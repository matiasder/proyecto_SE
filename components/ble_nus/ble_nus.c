
/**
 * @file ble_nus.c
 * @brief Servicio BLE NUS (Nordic UART Service).
 *
 * Implementa el servicio BLE NUS para comunicación serie sobre BLE.
 */

#include "ble_nus.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_log.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"

#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define NUS_TAG         "BLE_NUS"
#define NUS_MAX_PAYLOAD  512U

static const ble_uuid128_t s_svc_uuid =
    BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
                     0x93,0xf3,0xa3,0xb5, 0x01,0x00,0x40,0x6e);

static const ble_uuid128_t s_rx_uuid =
    BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
                     0x93,0xf3,0xa3,0xb5, 0x02,0x00,0x40,0x6e);

static const ble_uuid128_t s_tx_uuid =
    BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
                     0x93,0xf3,0xa3,0xb5, 0x03,0x00,0x40,0x6e);

// ─────────────────────────────────────────────────────────
// ESTADO INTERNO
// ─────────────────────────────────────────────────────────
static uint8_t         s_addr_type;
static uint16_t        s_conn_handle    = BLE_HS_CONN_HANDLE_NONE;
static uint16_t        s_tx_val_handle;
static bool            s_notify_enabled = false;
static ble_nus_rx_cb_t s_rx_cb          = NULL;
static char            s_device_name[32] = "ESP32-NUS";

// ─────────────────────────────────────────────────────────
// FORWARD DECLARATIONS
// ─────────────────────────────────────────────────────────
static void _nus_advertise(void);
static int  _gap_event_cb(struct ble_gap_event *ev, void *arg);

// ─────────────────────────────────────────────────────────
// CALLBACKS GATT
// ─────────────────────────────────────────────────────────
static int _rx_access_cb(uint16_t conn_handle,
                          uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)arg;

    uint8_t  buf[NUS_MAX_PAYLOAD];
    uint16_t len = 0;

    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf) - 1, &len);
    if (rc != 0) return BLE_ATT_ERR_UNLIKELY;

    buf[len] = '\0';
    ESP_LOGI(NUS_TAG, "RX <- \"%s\" (%u B)", buf, len);

    if (s_rx_cb) s_rx_cb((const char *)buf, len);

    return 0;
}

static int _tx_access_cb(uint16_t conn_handle,
                          uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt,
                          void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)ctxt; (void)arg;
    return 0;
}

// ─────────────────────────────────────────────────────────
// TABLA DE SERVICIOS GATT
// ─────────────────────────────────────────────────────────
static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid      = &s_rx_uuid.u,
                .access_cb = _rx_access_cb,
                .flags     = BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid       = &s_tx_uuid.u,
                .access_cb  = _tx_access_cb,
                .val_handle = &s_tx_val_handle,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 }
        },
    },
    { 0 }
};

// ─────────────────────────────────────────────────────────
// ADVERTISING
// ─────────────────────────────────────────────────────────
static void _nus_advertise(void)
{
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp;
    struct ble_gap_adv_params params;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags                = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128             = (ble_uuid128_t *)&s_svc_uuid;
    fields.num_uuids128         = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(NUS_TAG, "adv_set_fields error: %d", rc); return; }

    memset(&rsp, 0, sizeof(rsp));
    rsp.name             = (uint8_t *)s_device_name;
    rsp.name_len         = strlen(s_device_name);
    rsp.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) { ESP_LOGE(NUS_TAG, "adv_rsp_set_fields error: %d", rc); return; }

    memset(&params, 0, sizeof(params));
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER,
                            &params, _gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(NUS_TAG, "adv_start error: %d", rc);
    } else {
        ESP_LOGI(NUS_TAG, "Advertising como \"%s\"", s_device_name);
    }
}

// ─────────────────────────────────────────────────────────
// GAP EVENTS
// ─────────────────────────────────────────────────────────
static int _gap_event_cb(struct ble_gap_event *ev, void *arg)
{
    (void)arg;

    switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status == 0) {
            s_conn_handle = ev->connect.conn_handle;
            ESP_LOGI(NUS_TAG, "Cliente conectado (handle=%d)", s_conn_handle);
        } else {
            ESP_LOGW(NUS_TAG, "Conexion fallida (%d), re-advertising...",
                     ev->connect.status);
            _nus_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(NUS_TAG, "Cliente desconectado (reason=%d)",
                 ev->disconnect.reason);
        s_conn_handle    = BLE_HS_CONN_HANDLE_NONE;
        s_notify_enabled = false;
        _nus_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (ev->subscribe.attr_handle == s_tx_val_handle) {
            s_notify_enabled = (bool)ev->subscribe.cur_notify;
            ESP_LOGI(NUS_TAG, "TX Notify: %s", s_notify_enabled ? "ON" : "OFF");
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        _nus_advertise();
        break;

    default:
        break;
    }

    return 0;
}

// ─────────────────────────────────────────────────────────
// SYNC / RESET CALLBACKS
// ─────────────────────────────────────────────────────────
static void _on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &s_addr_type);
    if (rc != 0) {
        ESP_LOGE(NUS_TAG, "ble_hs_id_infer_auto: %d", rc);
        return;
    }
    _nus_advertise();
}

static void _on_reset(int reason)
{
    ESP_LOGE(NUS_TAG, "BLE host reset (reason=%d)", reason);
}

// ─────────────────────────────────────────────────────────
// TAREA NIMBLE
// ─────────────────────────────────────────────────────────
static void _ble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ─────────────────────────────────────────────────────────
// API PÚBLICA
// ─────────────────────────────────────────────────────────
void ble_nus_init(const char *device_name, ble_nus_rx_cb_t rx_cb)
{
    if (device_name && device_name[0]) {
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
        s_device_name[sizeof(s_device_name) - 1] = '\0';
    }

    s_rx_cb = rx_cb;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(nimble_port_init());

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(s_device_name);

    ble_hs_cfg.sync_cb  = _on_sync;
    ble_hs_cfg.reset_cb = _on_reset;

    ESP_ERROR_CHECK(ble_gatts_count_cfg(s_gatt_svcs));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(s_gatt_svcs));

    nimble_port_freertos_init(_ble_host_task);

    ESP_LOGI(NUS_TAG, "NUS inicializado como \"%s\"", s_device_name);
}

// BLE ahora es solo recepción — send es no-op
bool ble_nus_send(const char *msg)
{
    (void)msg;
    return false;
}

bool ble_nus_send_raw(const uint8_t *data, uint16_t len)
{
    (void)data; (void)len;
    return false;
}

bool ble_nus_connected(void)
{
    return (s_conn_handle != BLE_HS_CONN_HANDLE_NONE);
}

bool ble_nus_notify_enabled(void)
{
    return s_notify_enabled;
}