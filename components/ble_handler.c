/**
 * @file ble_handler.c
 * @brief Implementación del parser de comandos BLE.
 *
 * Este archivo implementa el parser y manejo de comandos recibidos por BLE,
 * así como la interacción con la máquina de estados y el logger.
 */

#include "ble_handler.h"
#include "state_machine.h"
#include "logger.h"
#include "time_manager.h"
#include "sensor.h"
#include "control.h"
#include "app_config.h"
#include "display.h"

#include "BMP_180.h"

#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "BLE_CMD";

static volatile int g_target_mmhg = THERAPY_DEFAULT_MMHG;

static int _clamp_target(int val)
{
    if (val > THERAPY_MAX_MMHG) val = THERAPY_MAX_MMHG;
    if (val < THERAPY_MIN_MMHG) val = THERAPY_MIN_MMHG;
    return val;
}

static void _reply(const char *msg)
{
    printf("BLE_REPLY | %s", msg);
}

void ble_on_rx(const char *data, uint16_t len)
{
    char reply[96];

    ESP_LOGI(TAG, "CMD: \"%s\"", data);

    /* ── START ── */
    if (strncasecmp(data, "START", 5) == 0) {
        if (sm_start_therapy()) {
            control_reset_leak_timer();
            sensor_reset_faults();
            _reply("OK:START\n");
        } else {
            snprintf(reply, sizeof(reply),
                     "ERR:START rechazado en estado %s\n",
                     sm_state_str(sm_get_state()));
            _reply(reply);
        }
    }

    /* ── STOP ── */
    else if (strncasecmp(data, "STOP", 4) == 0) {
        if (sm_stop_therapy()) {
            _reply("OK:STOP\n");
        } else {
            _reply("ERR:STOP rechazado\n");
        }
    }

    /* ── SET:<valor> ── */
    else if (strncasecmp(data, "SET:", 4) == 0) {
        int val = atoi(data + 4);
        val = _clamp_target(val);
        g_target_mmhg = val;
        snprintf(reply, sizeof(reply), "OK:SET:%d\n", val);
        _reply(reply);
        ESP_LOGI(TAG, "Setpoint -> %d mmHg", val);
    }

    /* ── CALIB ── */
    else if (strncasecmp(data, "CALIB", 5) == 0) {
        if (!sm_start_calibration()) {
            snprintf(reply, sizeof(reply),
                     "ERR:CALIB rechazado en estado %s\n",
                     sm_state_str(sm_get_state()));
            _reply(reply);
        } else {
            _reply("OK:CALIB iniciada\n");
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
        _reply(reply);
    }

    /* ── GET_STATE ── */
    else if (strncasecmp(data, "GET_STATE", 9) == 0) {
        snprintf(reply, sizeof(reply),
                 "STATE:%s\n", sm_state_str(sm_get_state()));
        _reply(reply);
    }

    /* ── GET_ERRORS ── */
    else if (strncasecmp(data, "GET_ERRORS", 10) == 0) {
        if (sm_is_error()) {
            snprintf(reply, sizeof(reply),
                     "ERR_ACTIVE:%s\n", sm_error_str(sm_get_error()));
        } else {
            snprintf(reply, sizeof(reply), "ERR_ACTIVE:NONE\n");
        }
        _reply(reply);
    }

    /* ── GET_LOGS ── */
    else if (strncasecmp(data, "GET_LOGS", 8) == 0) {
        logger_dump_uart();
        _reply("OK:GET_LOGS\n");
    }

    /* ── RESET_ALARMS ── */
    else if (strncasecmp(data, "RESET_ALARMS", 12) == 0) {
        if (sm_reset_error()) {
            sensor_reset_faults();
            control_reset_leak_timer();
            display_set_screen(SCREEN_MENU);
            display_force_redraw();
            _reply("OK:RESET_ALARMS\n");
        } else {
            _reply("ERR:RESET rechazado, no hay error activo\n");
        }
    }

    /* ── TIME:<epoch> ── */
    else if (strncasecmp(data, "TIME:", 5) == 0) {
        uint32_t epoch = (uint32_t)strtoul(data + 5, NULL, 10);
        if (epoch > 1000000000UL) {
            time_manager_sync(epoch);
            _reply("OK:TIME\n");
        } else {
            _reply("ERR:TIME epoch invalido\n");
        }
    }

    /* ── TEST:ON ── */
    else if (strncasecmp(data, "TEST:ON", 7) == 0) {
        sensor_set_test_mode(true);
        display_force_redraw();
        logger_add("Modo prueba activado");
        _reply("OK:TEST:ON\n");
    }

    /* ── TEST:OFF ── */
    else if (strncasecmp(data, "TEST:OFF", 8) == 0) {
        sensor_set_test_mode(false);
        display_force_redraw();
        logger_add("Modo prueba desactivado");
        _reply("OK:TEST:OFF\n");
    }

    /* ── TEST:PRESS:<valor> ── */
    else if (strncasecmp(data, "TEST:PRESS:", 11) == 0) {
        if (!sensor_get_test_mode()) {
            _reply("ERR:TEST no activo, enviar TEST:ON primero\n");
        } else {
            float val = strtof(data + 11, NULL);
            sensor_set_test_pressure(val);
            snprintf(reply, sizeof(reply), "OK:TEST:PRESS:%.1f\n", val);
            _reply(reply);
        }
    }

    /* ── TEST:ERR:<codigo> ── */
    else if (strncasecmp(data, "TEST:ERR:", 9) == 0) {
        if (!sensor_get_test_mode()) {
            _reply("ERR:TEST no activo, enviar TEST:ON primero\n");
        } else {
            int code = atoi(data + 9);
            if (code == 0) {
                sm_reset_error();
                sensor_reset_faults();
                control_reset_leak_timer();
                _reply("OK:TEST:ERR:NONE\n");
            } else if (code >= 1 && code <= 4) {
                emergency_stop();
                sm_set_error((error_code_t)code);
                snprintf(reply, sizeof(reply),
                         "OK:TEST:ERR:%s\n",
                         sm_error_str((error_code_t)code));
                _reply(reply);
            } else {
                _reply("ERR:codigo invalido (0-4)\n");
            }
        }
    }

    /* ── TEST:PUMP:<duty> ── */
    else if (strncasecmp(data, "TEST:PUMP:", 10) == 0) {
        if (!sensor_get_test_mode()) {
            _reply("ERR:TEST no activo, enviar TEST:ON primero\n");
        } else {
            float duty = strtof(data + 10, NULL);
            if (duty < 0.0f)   duty = 0.0f;
            if (duty > 100.0f) duty = 100.0f;
            control_set_duty_direct(duty);
            snprintf(reply, sizeof(reply), "OK:TEST:PUMP:%.1f\n", duty);
            _reply(reply);
        }
    }

    /* ── COMANDO DESCONOCIDO ── */
    else {
        snprintf(reply, sizeof(reply), "ERR:CMD_UNKNOWN [%s]\n", data);
        _reply(reply);
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