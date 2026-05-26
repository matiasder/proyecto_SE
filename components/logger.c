/**
 * @file logger.c
 * @brief Implementación del logger circular.
 *
 * Buffer circular de tamaño fijo (LOGGER_BUFFER_SIZE entradas).
 * Cuando se llena, la siguiente escritura sobreescribe la entrada
 * más antigua (comportamiento típico de logs en embedded).
 *
 * El mutex protege escrituras concurrentes desde distintas tareas.
 */

#include "logger.h"
#include "time_manager.h"
#include "ble_nus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#include <string.h>
#include <stdio.h>

static const char *TAG = "LOGGER";

// ─────────────────────────────────────────────────────────────
// ESTADO INTERNO
// ─────────────────────────────────────────────────────────────
static log_entry_t      s_buf[LOGGER_BUFFER_SIZE];
static uint8_t          s_head  = 0;    /* Próxima posición de escritura */
static uint8_t          s_count = 0;    /* Entradas almacenadas (hasta BUFFER_SIZE) */
static SemaphoreHandle_t s_mutex = NULL;

// ─────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────

void logger_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex != NULL);
    memset(s_buf, 0, sizeof(s_buf));
    s_head  = 0;
    s_count = 0;
    ESP_LOGI(TAG, "Logger inicializado (%d entradas)", LOGGER_BUFFER_SIZE);
}

void logger_add(const char *msg)
{
    if (!msg) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    log_entry_t *entry = &s_buf[s_head];

    entry->timestamp = time_manager_get_epoch();
    strncpy(entry->msg, msg, LOGGER_MSG_LEN - 1);
    entry->msg[LOGGER_MSG_LEN - 1] = '\0';

    /* Avanzar cabeza circular */
    s_head = (s_head + 1) % LOGGER_BUFFER_SIZE;

    if (s_count < LOGGER_BUFFER_SIZE) {
        s_count++;
    }

    xSemaphoreGive(s_mutex);

    //ESP_LOGI(TAG, "[%lu] %s", (unsigned long)entry->timestamp, msg);
}

void logger_dump_ble(void)
{
    char line[96];

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_count == 0) {
        xSemaphoreGive(s_mutex);
        ble_nus_send("LOG:EMPTY\n");
        return;
    }

    /*
     * El buffer circular puede no empezar en índice 0.
     * Si está lleno, la entrada más antigua está en s_head.
     * Si no está lleno, la más antigua está en índice 0.
     */
    uint8_t start = (s_count < LOGGER_BUFFER_SIZE) ? 0 : s_head;

    for (uint8_t i = 0; i < s_count; i++) {
        uint8_t idx = (start + i) % LOGGER_BUFFER_SIZE;
        snprintf(line, sizeof(line),
                 "LOG:%d,%lu,%s\n",
                 i,
                 (unsigned long)s_buf[idx].timestamp,
                 s_buf[idx].msg);

        xSemaphoreGive(s_mutex);
        ble_nus_send(line);
        vTaskDelay(pdMS_TO_TICKS(20));   /* Pequeña pausa entre paquetes BLE */
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }

    xSemaphoreGive(s_mutex);

    ble_nus_send("LOG:END\n");
}

uint8_t logger_count(void)
{
    return s_count;
}

const log_entry_t *logger_get(uint8_t index)
{
    if (index >= s_count) return NULL;

    uint8_t start = (s_count < LOGGER_BUFFER_SIZE) ? 0 : s_head;
    uint8_t real  = (start + index) % LOGGER_BUFFER_SIZE;

    return &s_buf[real];
}

void logger_clear(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(s_buf, 0, sizeof(s_buf));
    s_head  = 0;
    s_count = 0;
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "Logs borrados");
}
