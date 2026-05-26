/**
 * @file state_machine.c
 * @brief Implementación de la máquina de estados global.
 */

#include "state_machine.h"
#include "logger.h"
#include "control.h"     /* para buzzer_off() en sm_reset_error() */
#include "esp_log.h"

static const char *TAG = "SM";

// ─────────────────────────────────────────────────────────────
// ESTADO INTERNO
// ─────────────────────────────────────────────────────────────
static system_state_t s_state = STATE_IDLE;
static error_code_t   s_error = ERR_NONE;

// ─────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────

void sm_init(void)
{
    s_state = STATE_IDLE;
    s_error = ERR_NONE;
    ESP_LOGI(TAG, "Iniciado en STATE_IDLE");
}

system_state_t sm_get_state(void)
{
    return s_state;
}

error_code_t sm_get_error(void)
{
    return s_error;
}

const char *sm_state_str(system_state_t state)
{
    switch (state) {
        case STATE_IDLE:        return "IDLE";
        case STATE_RUNNING:     return "RUNNING";
        case STATE_CALIBRATION: return "CALIBRATION";
        case STATE_ERROR:       return "ERROR";
        default:                return "UNKNOWN";
    }
}

const char *sm_error_str(error_code_t err)
{
    switch (err) {
        case ERR_NONE:           return "ERR_NONE";
        case ERR_SENSOR_TIMEOUT: return "ERR_SENSOR_TIMEOUT";
        case ERR_SENSOR_RANGE:   return "ERR_SENSOR_RANGE";
        case ERR_LEAK_DETECTED:  return "ERR_LEAK_DETECTED";
        case ERR_PRESSURE_LOW:   return "ERR_PRESSURE_LOW";
        default:                 return "ERR_UNKNOWN";
    }
}

bool sm_start_therapy(void)
{
    if (s_state != STATE_IDLE) {
        ESP_LOGW(TAG, "START rechazado en estado %s", sm_state_str(s_state));
        return false;
    }
    s_state = STATE_RUNNING;
    s_error = ERR_NONE;
    ESP_LOGI(TAG, "IDLE -> RUNNING");
    logger_add("Terapia iniciada");
    return true;
}

bool sm_stop_therapy(void)
{
    if (s_state != STATE_RUNNING) {
        ESP_LOGW(TAG, "STOP rechazado en estado %s", sm_state_str(s_state));
        return false;
    }
    s_state = STATE_IDLE;
    ESP_LOGI(TAG, "RUNNING -> IDLE");
    logger_add("Terapia detenida");
    return true;
}

bool sm_start_calibration(void)
{
    if (s_state != STATE_IDLE) {
        ESP_LOGW(TAG, "CALIB rechazado en estado %s", sm_state_str(s_state));
        return false;
    }
    s_state = STATE_CALIBRATION;
    ESP_LOGI(TAG, "IDLE -> CALIBRATION");
    logger_add("Calibracion iniciada");
    return true;
}

bool sm_finish_calibration(void)
{
    if (s_state != STATE_CALIBRATION) return false;
    s_state = STATE_IDLE;
    ESP_LOGI(TAG, "CALIBRATION -> IDLE");
    logger_add("Calibracion completada");
    return true;
}

void sm_set_error(error_code_t code)
{
    if (s_state == STATE_ERROR && s_error == code) return;

    char msg[64];
    snprintf(msg, sizeof(msg), "ERROR: %s", sm_error_str(code));

    s_error = code;
    s_state = STATE_ERROR;

    ESP_LOGE(TAG, "-> ERROR [%s]", sm_error_str(code));
    logger_add(msg);
}

bool sm_reset_error(void)
{
    if (s_state != STATE_ERROR) {
        ESP_LOGW(TAG, "RESET rechazado: no estamos en ERROR");
        return false;
    }
    s_state = STATE_IDLE;
    s_error = ERR_NONE;

    /* Apagar buzzer al salir del estado de error */
    buzzer_off();

    ESP_LOGI(TAG, "ERROR -> IDLE (reset)");
    logger_add("Error reseteado");
    return true;
}

bool sm_is_running(void)
{
    return (s_state == STATE_RUNNING);
}

bool sm_is_error(void)
{
    return (s_state == STATE_ERROR);
}