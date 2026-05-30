/**
 * @file time_manager.c
 * @brief Implementación del gestor de timestamps.
 *
 * Proporciona funciones para obtener y manipular marcas de tiempo en el sistema.
 */

#include "time_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "TIME";

// ─────────────────────────────────────────────────────────────
// ESTADO INTERNO
// ─────────────────────────────────────────────────────────────
static bool     s_synced     = false;
static uint32_t s_epoch_base = 0;   /* Epoch Unix recibido por BLE */
static uint32_t s_sync_tick  = 0;   /* Tick FreeRTOS en el momento de sync */

// ─────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────

void time_manager_init(void)
{
    s_synced     = false;
    s_epoch_base = 0;
    s_sync_tick  = 0;
    ESP_LOGI(TAG, "Time manager iniciado (sin sincronizar)");
}

void time_manager_sync(uint32_t epoch)
{
    s_epoch_base = epoch;
    s_sync_tick  = xTaskGetTickCount();
    s_synced     = true;
    ESP_LOGI(TAG, "Sincronizado: epoch=%lu", (unsigned long)epoch);
}

uint32_t time_manager_get_epoch(void)
{
    /* Milisegundos transcurridos desde el boot */
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (s_synced) {
        uint32_t sync_ms  = s_sync_tick * portTICK_PERIOD_MS;
        uint32_t elapsed  = (now_ms - sync_ms) / 1000;
        return s_epoch_base + elapsed;
    } else {
        /* Fallback: uptime en segundos */
        return now_ms / 1000;
    }
}

bool time_manager_is_synced(void)
{
    return s_synced;
}

uint32_t time_manager_get_uptime(void)
{
    return (xTaskGetTickCount() * portTICK_PERIOD_MS) / 1000;
}
