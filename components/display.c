/**
 * @file display.c
 * @brief UI en pantalla OLED SSD1306.
 *
 * Pantallas:
 *   SCREEN_MENU        → Menú principal con opciones
 *   SCREEN_THERAPY     → Presión actual y setpoint en tiempo real
 *   SCREEN_CALIBRATION → Mensaje de espera durante calibración
 *   SCREEN_ALARM       → Alarma parpadeante con código de error
 *
 * La pantalla de alarma tiene prioridad absoluta sobre cualquier otra.
 * Puede descartarse de dos formas:
 *   - BLE: comando RESET_ALARMS  → llama sm_reset_error()
 *   - Físico: botón BTN_ENTER    → llama sm_reset_error() desde draw_alarm()
 *
 * En ambos casos sm_reset_error() también apaga el buzzer.
 */

#include "display.h"
#include "state_machine.h"
#include "sensor.h"
#include "app_config.h"
#include "SSD1306_MINI.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "DISPLAY";

// ─────────────────────────────────────────────────────────────
// ESTADO INTERNO
// ─────────────────────────────────────────────────────────────
static screen_t s_screen      = SCREEN_MENU;
static bool     s_redraw      = true;
static uint32_t s_blink_tick  = 0;
static bool     s_blink_state = false;

// ─────────────────────────────────────────────────────────────
// PANTALLAS PRIVADAS
// ─────────────────────────────────────────────────────────────

static void draw_menu(void)
{
    if (!s_redraw) return;

    oled_clear();
    oled_print(0, 20, "VAC THERAPY");
    oled_print(2,  0, ">Terapia  (ENTER)");
    oled_print(4,  0, " Calibrar (DOWN)");
    s_redraw = false;
}

static void draw_therapy(float pressure_mmhg, int target_mmhg)
{
    char buf[32];

    if (s_redraw) {
        oled_clear();
        oled_print(0, 0, "Terapia activa");
        oled_print(2, 0, "SET:");
        oled_print(4, 0, "ACT:");
        s_redraw = false;
    }

    snprintf(buf, sizeof(buf), "%d   ", target_mmhg);
    oled_print(2, 48, buf);

    snprintf(buf, sizeof(buf), "%.0f   ", pressure_mmhg);
    oled_print(4, 48, buf);

    oled_print(6, 0, sm_is_running() ? "[S:STOP ]" : "[S:START]");
}

static void draw_calibration(void)
{
    oled_clear();
    oled_print(2,  0, "Calibrando...");
    oled_print(4, 20, "espere");
}

static void draw_alarm(void)
{
    /*
     * Parpadeo: alterna entre mostrar el error y pantalla en negro.
     *
     * Reset físico: si el usuario presiona BTN_ENTER desde esta pantalla,
     * se llama sm_reset_error() que:
     *   1. Cambia STATE_ERROR → STATE_IDLE
     *   2. Apaga el buzzer (buzzer_off())
     *   3. Registra el reset en el logger
     *
     * La pantalla vuelve a SCREEN_MENU en el siguiente ciclo de ui_task
     * porque sm_is_error() ya no será true.
     */

    /* ── Verificar botón físico de reset ── */
    if (!gpio_get_level(PIN_BTN_ENTER)) {
        if (sm_reset_error()) {
            sensor_reset_faults();
            display_set_screen(SCREEN_MENU);
            display_force_redraw();
            ESP_LOGI(TAG, "Alarma reseteada por boton fisico");
            vTaskDelay(pdMS_TO_TICKS(300));  /* debounce */
            return;
        }
    }

    /* ── Lógica de parpadeo ── */
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if ((now - s_blink_tick) >= UI_BLINK_MS) {
        s_blink_tick  = now;
        s_blink_state = !s_blink_state;

        oled_clear();

        if (s_blink_state) {
            oled_print(0, 10, "!! ALARMA !!");
            oled_print(2,  0, sm_error_str(sm_get_error()));
            oled_print(4,  0, "ENTER o BLE:");
            oled_print(6,  0, "RESET_ALARMS");
        }
        /* Si s_blink_state es false: pantalla en negro (parpadeo) */
    }
}

// ─────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────

void display_init(void)
{
    oled_init();
    oled_clear();
    s_screen     = SCREEN_MENU;
    s_redraw     = true;
    s_blink_tick = 0;
    ESP_LOGI(TAG, "Display inicializado");
    draw_menu();
}

void display_update(float pressure_mmhg, int target_mmhg)
{
    /* La alarma siempre tiene prioridad */
    if (sm_is_error()) {
        draw_alarm();
        return;
    }

    switch (s_screen) {
        case SCREEN_MENU:        draw_menu();                               break;
        case SCREEN_THERAPY:     draw_therapy(pressure_mmhg, target_mmhg); break;
        case SCREEN_CALIBRATION: draw_calibration();                        break;
        case SCREEN_ALARM:       draw_alarm();                              break;
        default:                 break;
    }
}

void display_force_redraw(void)
{
    s_redraw = true;
}

screen_t display_get_screen(void)
{
    return s_screen;
}

void display_set_screen(screen_t screen)
{
    if (s_screen != screen) {
        s_screen = screen;
        s_redraw = true;
        oled_clear();
    }
}