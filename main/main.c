/**
 * @file main.c
 * @brief Punto de entrada del firmware VAC Therapy.
 *
 * Responsabilidades de este archivo:
 *   1. Inicializar hardware (I2C, GPIO, periféricos)
 *   2. Inicializar módulos de software
 *   3. Crear tareas FreeRTOS
 *   4. Arrancar el sistema
 *
 * Tareas:
 *   sensor_task   (prio 8) → Lee BMP180 y valida lectura
 *   control_task  (prio 7) → Controla bomba, detecta fugas
 *   ui_task       (prio 4) → Botones + OLED + calibración
 *   ble_task      (prio 5) → Envío periódico de datos por BLE
 *
 * Archivo principal del sistema embebido.
 */
 *
 * Variables compartidas entre tareas:
 *   g_pressure_mmhg  → Escrita por sensor_task, leída por control/ui/ble
 *   g_target_mmhg    → Leída/escrita por ble_handler y ui_task
 *
 * Nota: Las escrituras de float/int en Xtensa de 32 bits son atómicas,
 * por lo que no se requiere mutex para estas variables simples.
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* Módulos propios */
#include "app_config.h"
#include "state_machine.h"
#include "sensor.h"
#include "control.h"
#include "display.h"
#include "logger.h"
#include "time_manager.h"
#include "ble_handler.h"

/* Librerías de hardware */
#include "BMP_180.h"
#include "ble_nus.h"
#include "SSD1306_MINI.h"

static const char *TAG = "MAIN";

// ─────────────────────────────────────────────────────────────
// VARIABLE COMPARTIDA: presión actual
// Escrita por sensor_task, leída por control_task y ui_task
// ─────────────────────────────────────────────────────────────
static volatile float g_pressure_mmhg = 0.0f;

// ─────────────────────────────────────────────────────────────
// INICIALIZACIÓN DE HARDWARE
// ─────────────────────────────────────────────────────────────

static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = PIN_I2C_SDA,
        .scl_io_num       = PIN_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    ESP_LOGI(TAG, "I2C inicializado (SDA=%d, SCL=%d)", PIN_I2C_SDA, PIN_I2C_SCL);
}

static void buttons_init(void)
{
    gpio_config_t io = {
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pin_bit_mask =
            (1ULL << PIN_BTN_UP)    |
            (1ULL << PIN_BTN_DOWN)  |
            (1ULL << PIN_BTN_ENTER) |
            (1ULL << PIN_BTN_BACK),
    };
    gpio_config(&io);
    ESP_LOGI(TAG, "Botones inicializados");
}

// ─────────────────────────────────────────────────────────────
// TAREA: SENSOR
// Lee el sensor BMP180 y actualiza g_pressure_mmhg
// ─────────────────────────────────────────────────────────────
static void sensor_task(void *arg)
{
    float reading = 0.0f;

    for (;;) {
        if (sensor_read(&reading)) {
            g_pressure_mmhg = reading;
        }
        vTaskDelay(pdMS_TO_TICKS(PERIOD_SENSOR_MS));
    }
}

// ─────────────────────────────────────────────────────────────
// TAREA: CONTROL
// Ejecuta el algoritmo de control con la presión actual
// ─────────────────────────────────────────────────────────────
static void control_task(void *arg)
{
    for (;;) {
        control_update(g_pressure_mmhg, ble_get_target());
        vTaskDelay(pdMS_TO_TICKS(PERIOD_CONTROL_MS));
    }
}

// ─────────────────────────────────────────────────────────────
// TAREA: UI
// Maneja botones, actualiza OLED y ejecuta calibración
// ─────────────────────────────────────────────────────────────
static void ui_task(void *arg)
{
    for (;;) {
        system_state_t state = sm_get_state();

        /* ── Calibración: se ejecuta aquí al entrar en STATE_CALIBRATION ── */
        if (state == STATE_CALIBRATION) {
            display_set_screen(SCREEN_CALIBRATION);
            display_update(g_pressure_mmhg, ble_get_target());

            float offset = 0.0f;
            bmp180_calibrate(&offset);      /* Bloquea ~3 segundos internamente */
            sensor_set_offset(offset);

            sm_finish_calibration();
            ble_nus_send("OK:CALIB_DONE\n");

            display_set_screen(SCREEN_MENU);
            display_force_redraw();
            vTaskDelay(pdMS_TO_TICKS(PERIOD_UI_MS));
            continue;
        }

        /* ── Manejo de botones según pantalla activa ── */
        screen_t screen = display_get_screen();

        if (screen == SCREEN_MENU) {
            /* ENTER → ir a terapia */
            if (!gpio_get_level(PIN_BTN_ENTER)) {
                display_set_screen(SCREEN_THERAPY);
                display_force_redraw();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            /* DOWN → calibrar */
            if (!gpio_get_level(PIN_BTN_DOWN)) {
                sm_start_calibration();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }

        else if (screen == SCREEN_THERAPY) {
            /* BACK → volver al menú */
            if (!gpio_get_level(PIN_BTN_BACK)) {
                if (sm_is_running()) sm_stop_therapy();
                display_set_screen(SCREEN_MENU);
                display_force_redraw();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            /* ENTER → toggle terapia */
            if (!gpio_get_level(PIN_BTN_ENTER)) {
                if (sm_is_running()) {
                    sm_stop_therapy();
                } else {
                    sm_start_therapy();
                    control_reset_leak_timer();
                    sensor_reset_faults();
                }
                display_force_redraw();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            /* UP/DOWN → ajustar setpoint */
            if (!gpio_get_level(PIN_BTN_UP)) {
                ble_set_target(ble_get_target() + 5);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
            if (!gpio_get_level(PIN_BTN_DOWN)) {
                ble_set_target(ble_get_target() - 5);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
        }

        /* Si hay error activo, mostrar pantalla de alarma */
        if (sm_is_error() && screen != SCREEN_ALARM) {
            display_set_screen(SCREEN_ALARM);
        }

        /* ── Actualizar pantalla OLED ── */
        display_update(g_pressure_mmhg, ble_get_target());

        vTaskDelay(pdMS_TO_TICKS(PERIOD_UI_MS));
    }
}

static void uart_task(void *arg)
{
    for (;;) {
        printf("DATA | state=%-12s | press=%6.1f mmHg | target=%4d mmHg | pump=%s | err=%s\n",
               sm_state_str(sm_get_state()),
               (double)g_pressure_mmhg,
               ble_get_target(),
               control_pump_is_on() ? "ON " : "OFF",
               sm_error_str(sm_get_error()));

        vTaskDelay(pdMS_TO_TICKS(PERIOD_BLE_MS));
    }
}

// ─────────────────────────────────────────────────────────────
// PUNTO DE ENTRADA
// ─────────────────────────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "VAC Therapy Firmware v%s arrancando...", FW_VERSION);

    /* ── 1. Hardware ── */
    i2c_init();
    buttons_init();

    /* ── 2. Módulos de software ── */
    time_manager_init();
    logger_init();
    sm_init();

    /* Offset inicial = 850 hPa (valor típico de altitud de laboratorio).
       Se actualiza con la calibración real. */
    sensor_init(I2C_NUM_0, 850.0f);

    control_init();
    display_init();

    /* ── 3. BLE ── */
    ble_nus_init(BLE_DEVICE_NAME, ble_on_rx);

    /* ── 4. Sistema listo ── */
    logger_add("Sistema iniciado v" FW_VERSION);
    ESP_LOGI(TAG, "Sistema listo. Creando tareas...");

    /* ── 5. Tareas FreeRTOS ── */
    xTaskCreate(sensor_task,  "sensor",  TASK_SENSOR_STACK,  NULL, TASK_SENSOR_PRIO,  NULL);
    xTaskCreate(control_task, "control", TASK_CONTROL_STACK, NULL, TASK_CONTROL_PRIO, NULL);
    xTaskCreate(ui_task,      "ui",      TASK_UI_STACK,      NULL, TASK_UI_PRIO,      NULL);
    xTaskCreate(uart_task,    "uart_tx",    TASK_UART_STACK,    NULL, TASK_UART_PRIO,    NULL);
    ESP_LOGI(TAG, "Tareas creadas. Firmware corriendo.");
}