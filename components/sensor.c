/**
 * @file sensor.c
 * @brief Implementación de la capa de validación del BMP180.
 *
 * Lógica de fallos:
 *   - Cada lectura inválida incrementa s_fault_count.
 *   - Cada lectura válida resetea s_fault_count.
 *   - Cuando s_fault_count >= SENSOR_FAULT_THRESHOLD → error declarado.
 *   - Esto evita falsas alarmas por glitches I2C puntuales.
 *
 * Este archivo valida y filtra las lecturas del sensor BMP180.
 */

#include "sensor.h"
#include "state_machine.h"
#include "logger.h"
#include "app_config.h"

#include "BMP_180.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <math.h>
#include <stddef.h>

static const char *TAG = "SENSOR";

// ─────────────────────────────────────────────────────────────
// ESTADO INTERNO
// ─────────────────────────────────────────────────────────────
static float    s_offset_hpa   = 0.0f;
static uint8_t  s_fault_count  = 0;
static bool     s_faulted      = false;
static uint32_t s_last_ok_tick = 0;    /* Tick de la última lectura válida */

static bool s_test_mode = false; /* Si true, sensor_read() devuelve valores simulados para pruebas */
static float s_test_pressure_mmhg = 0.0f; /* Presión simulada en modo test */

// ─────────────────────────────────────────────────────────────
// HELPERS PRIVADOS
// ─────────────────────────────────────────────────────────────

static bool _is_valid(float hpa)
{
    if (isnan(hpa) || isinf(hpa))           return false;
    if (hpa < SENSOR_MIN_HPA)               return false;
    if (hpa > SENSOR_MAX_HPA)               return false;
    return true;
}

static bool _has_timed_out(void)
{
    /* Si nunca hubo lectura válida, no aplica timeout */
    if (s_last_ok_tick == 0) return false;

    uint32_t elapsed_ms =
        (xTaskGetTickCount() - s_last_ok_tick) * portTICK_PERIOD_MS;

    return (elapsed_ms > SENSOR_TIMEOUT_MS);
}

// ─────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────

void sensor_init(i2c_port_t port, float initial_offset_hpa)
{
    bmp180_init(port);
    s_offset_hpa   = initial_offset_hpa;
    s_fault_count  = 0;
    s_faulted      = false;
    s_last_ok_tick = 0;
    ESP_LOGI(TAG, "Sensor iniciado. Offset=%.2f hPa", initial_offset_hpa);
}

bool sensor_read(float *out_mmhg)
{
     if (!out_mmhg) return false;

    // ── Modo prueba: retorna presión inyectada por BLE ──
    if (s_test_mode) {
        *out_mmhg = s_test_pressure_mmhg;
        return true;
    }
    
    if (!out_mmhg) return false;

    /* ── Verificar timeout antes de intentar leer ── */
    if (_has_timed_out()) {
        ESP_LOGE(TAG, "Timeout: sin lectura válida en >%d ms", SENSOR_TIMEOUT_MS);
        s_fault_count++;
        if (!s_faulted && s_fault_count >= SENSOR_FAULT_THRESHOLD) {
            s_faulted = true;
            logger_add("FALLO: timeout de sensor");
            sm_set_error(ERR_SENSOR_TIMEOUT);
        }
        return false;
    }

    /* ── Leer presión cruda ── */
    float hpa = bmp180_get_pressure_hpa();

    /* ── Validar rango físico ── */
    if (!_is_valid(hpa)) {
        ESP_LOGE(TAG, "Lectura inválida: %.2f hPa", hpa);
        s_fault_count++;
        if (!s_faulted && s_fault_count >= SENSOR_FAULT_THRESHOLD) {
            s_faulted = true;
            logger_add("FALLO: sensor fuera de rango");
            sm_set_error(ERR_SENSOR_RANGE);
        }
        return false;
    }

    /* ── Lectura válida ── */
    s_fault_count  = 0;
    s_faulted      = false;
    s_last_ok_tick = xTaskGetTickCount();

    *out_mmhg = bmp180_pressure_to_mmhg(hpa, s_offset_hpa);
    return true;
}

void sensor_set_offset(float offset_hpa)
{
    s_offset_hpa = offset_hpa;
    ESP_LOGI(TAG, "Offset actualizado: %.2f hPa", offset_hpa);
}

float sensor_get_offset(void)
{
    return s_offset_hpa;
}

bool sensor_is_faulted(void)
{
    return s_faulted;
}

void sensor_reset_faults(void)
{
    s_fault_count  = 0;
    s_faulted      = false;
    s_last_ok_tick = 0;
    ESP_LOGI(TAG, "Contador de fallos reseteado");
}

void sensor_set_test_mode(bool enabled)
{
    s_test_mode = enabled;
    ESP_LOGI(TAG, "Modo prueba: %s", enabled ? "ON" : "OFF");
}

bool sensor_get_test_mode(void)
{
    return s_test_mode;
}

void sensor_set_test_pressure(float mmhg)
{
    s_test_pressure_mmhg = mmhg;
    ESP_LOGI(TAG, "Presion de prueba: %.1f mmHg", mmhg);
}