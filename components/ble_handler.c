/**
 * @file ble_handler.c
 * @brief Implementación del parser de comandos BLE.
 *
 * Diseño:
 *   - ble_on_rx() es el callback registrado en ble_nus_init().
 *   - Se ejecuta desde la tarea NimBLE → mantenerse corto.
 *   - Las acciones que toman tiempo (calibración) se delegan
 *     al estado de la SM; la tarea UI/control las ejecuta.
 *   - g_target_mmhg es la variable compartida entre BLE y control.
 */

#include "ble_handler.h"
#include "state_machine.h"
#include "logger.h"
#include "time_manager.h"
#include "sensor.h"
#include "control.h"
#include "app_config.h"

#include "ble_nus.h"
#include "BMP_180.h"

#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "BLE_CMD";

// ─────────────────────────────────────────────────────────────
// SETPOINT COMPARTIDO
// Escrito desde BLE y botones, leído desde control_task
// ─────────────────────────────────────────────────────────────
static volatile int g_target_mmhg = THERAPY_DEFAULT_MMHG;

// ─────────────────────────────────────────────────────────────
// HELPER: validar y clampear setpoint
// ─────────────────────────────────────────────────────────────
static int _clamp_target(int val)
{
    if (val > THERAPY_MAX_MMHG) val = THERAPY_MAX_MMHG;
    if (val < THERAPY_MIN_MMHG) val = THERAPY_MIN_MMHG;
    return val;
}

// ─────────────────────────────────────────────────────────────
// CALLBACK PRINCIPAL
// ─────────────────────────────────────────────────────────────
void ble_on_rx(const char *data, uint16_t len)
{
    char reply[96];

    ESP_LOGI(TAG, "CMD: \"%s\"", data);

    /* ── START ── */
    if (strncasecmp(data, "START", 5) == 0) {
        if (sm_start_therapy()) {
            control_reset_leak_timer();
            sensor_reset_faults();
            ble_nus_send("OK:START\n");
        } else {
            snprintf(reply, sizeof(reply),
                     "ERR:START rechazado en estado %s\n",
                     sm_state_str(sm_get_state()));
            ble_nus_send(reply);
        }
    }

    /* ── STOP ── */
    else if (strncasecmp(data, "STOP", 4) == 0) {
        if (sm_stop_therapy()) {
            ble_nus_send("OK:STOP\n");
        } else {
            ble_nus_send("ERR:STOP rechazado\n");
        }
    }

    /* ── SET:<valor> ── */
    else if (strncasecmp(data, "SET:", 4) == 0) {
        int val = atoi(data + 4);
        val = _clamp_target(val);
        g_target_mmhg = val;
        snprintf(reply, sizeof(reply), "OK:SET:%d\n", val);
        ble_nus_send(reply);
        ESP_LOGI(TAG, "Setpoint -> %d mmHg", val);
    }

    /* ── CALIB ── */
    else if (strncasecmp(data, "CALIB", 5) == 0) {
        if (!sm_start_calibration()) {
            snprintf(reply, sizeof(reply),
                     "ERR:CALIB rechazado en estado %s\n",
                     sm_state_str(sm_get_state()));
            ble_nus_send(reply);
        } else {
            ble_nus_send("OK:CALIB iniciada\n");
            /* La tarea UI ejecuta la calibración al detectar STATE_CALIBRATION */
        }
    }

    /* ── STATUS ── */
    else if (strncasecmp(data, "STATUS", 6) == 0) {
        snprintf(reply, sizeof(reply),
                 "STATUS:state=%s,target=%d,pump=%s,err=%s,ts=%lu\n",
                 sm_state_str(sm_get_state()),
                 g_target_mmhg,
                 control_pump_is_on() ? "ON" : "OFF",
                 sm_error_str(sm_get_error()),
                 (unsigned long)time_manager_get_epoch());
        ble_nus_send(reply);
    }

    /* ── GET_STATE ── */
    else if (strncasecmp(data, "GET_STATE", 9) == 0) {
        snprintf(reply, sizeof(reply),
                 "STATE:%s\n", sm_state_str(sm_get_state()));
        ble_nus_send(reply);
    }

    /* ── GET_ERRORS ── */
    else if (strncasecmp(data, "GET_ERRORS", 10) == 0) {
        if (sm_is_error()) {
            snprintf(reply, sizeof(reply),
                     "ERR_ACTIVE:%s\n", sm_error_str(sm_get_error()));
        } else {
            snprintf(reply, sizeof(reply), "ERR_ACTIVE:NONE\n");
        }
        ble_nus_send(reply);
    }

    /* ── GET_LOGS ── */
    else if (strncasecmp(data, "GET_LOGS", 8) == 0) {
        logger_dump_ble();  /* Envía todas las entradas del buffer circular */
    }

    /* ── RESET_ALARMS ── */
    else if (strncasecmp(data, "RESET_ALARMS", 12) == 0) {
        if (sm_reset_error()) {
            sensor_reset_faults();
            ble_nus_send("OK:RESET_ALARMS\n");
        } else {
            ble_nus_send("ERR:RESET rechazado, no hay error activo\n");
        }
    }

    /* ── TIME:<epoch> ── */
    else if (strncasecmp(data, "TIME:", 5) == 0) {
        uint32_t epoch = (uint32_t)strtoul(data + 5, NULL, 10);
        if (epoch > 1000000000UL) {   /* Sanity check: epoch razonable */
            time_manager_sync(epoch);
            ble_nus_send("OK:TIME\n");
        } else {
            ble_nus_send("ERR:TIME epoch invalido\n");
        }
    }

    /* ── COMANDO DESCONOCIDO ── */
    else {
        snprintf(reply, sizeof(reply), "ERR:CMD_UNKNOWN [%s]\n", data);
        ble_nus_send(reply);
        ESP_LOGW(TAG, "Comando desconocido: %s", data);
    }
}

int ble_get_target(void)
{
    return g_target_mmhg;
}

void ble_set_target(int target_mmhg)
{
    g_target_mmhg = _clamp_target(target_mmhg);
}
