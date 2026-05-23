#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "esp_log.h"

#include "SSD1306_MINI.h"
#include "ble_nus.h"
#include "BMP_180.h"          

// ─────────────────────────────
// I2C
// ─────────────────────────────
#define I2C_PORT        I2C_NUM_0
#define SDA_PIN         21
#define SCL_PIN         22
#define I2C_FREQ_HZ     100000

// ─────────────────────────────
// BOTONES
// ─────────────────────────────
#define BTN_UP      25
#define BTN_DOWN    26
#define BTN_ENTER   27
#define BTN_BACK    33
#define LOG_BOMBA   18

#define HYSTERESIS 5

static const char *TAG = "TERAPIA";

float pressure_offset_hpa = 850.0f;

int target_vacuum = -75;

uint8_t terapia_running = 0;
uint8_t bomba_on        = 0;

// ─────────────────────────────
// UI STATES
// ─────────────────────────────
typedef enum
{
    SCREEN_MENU,
    SCREEN_TERAPIA,
    SCREEN_CALIBRACION

} screen_t;

screen_t current_screen = SCREEN_MENU;

uint8_t menu_index = 0;

uint8_t terapia_redraw = 1;
uint8_t calib_redraw   = 1;
uint8_t menu_redraw    = 1;

// ─────────────────────────────────────────────────────────────
// CALLBACK BLE  –  comandos recibidos desde la app móvil
// ─────────────────────────────────────────────────────────────
static void on_ble_rx(const char *data, uint16_t len)
{
    char reply[64];

    /* ── START ── */
    if (strncasecmp(data, "START", 5) == 0)
    {
        terapia_running = 1;
        terapia_redraw  = 1;
        ble_nus_send("OK:START\n");
        ESP_LOGI(TAG, "BLE: terapia iniciada");
    }

    /* ── STOP ── */
    else if (strncasecmp(data, "STOP", 4) == 0)
    {
        terapia_running = 0;
        terapia_redraw  = 1;
        ble_nus_send("OK:STOP\n");
        ESP_LOGI(TAG, "BLE: terapia detenida");
    }

    /* ── SET:<valor> ── */
    else if (strncasecmp(data, "SET:", 4) == 0)
    {
        int val = atoi(data + 4);

        if (val > -75)  val = -75;
        if (val < -125) val = -125;

        target_vacuum  = val;
        terapia_redraw = 1;

        snprintf(reply, sizeof(reply), "OK:SET:%d\n", target_vacuum);
        ble_nus_send(reply);
        ESP_LOGI(TAG, "BLE: setpoint → %d mmHg", target_vacuum);
    }

    /* ── CALIB ── */
    else if (strncasecmp(data, "CALIB", 5) == 0)
    {
        current_screen = SCREEN_CALIBRACION;
        calib_redraw   = 1;
        ble_nus_send("OK:CALIB\n");
        ESP_LOGI(TAG, "BLE: calibracion solicitada");
    }

    /* ── STATUS ── */
    else if (strncasecmp(data, "STATUS", 6) == 0)
    {
        snprintf(reply, sizeof(reply),
                 "STATE:%s,SET:%d,BOMBA:%s\n",
                 terapia_running ? "RUN" : "STOP",
                 target_vacuum,
                 bomba_on ? "ON" : "OFF");
        ble_nus_send(reply);
    }

    /* ── COMANDO DESCONOCIDO ── */
    else
    {
        ble_nus_send("ERR:CMD_UNKNOWN\n");
        ESP_LOGW(TAG, "BLE: comando desconocido: \"%s\"", data);
    }
}

// ─────────────────────────────
// I2C INIT
// ─────────────────────────────
void i2c_init(void)
{
    i2c_config_t conf = {
        .mode           = I2C_MODE_MASTER,
        .sda_io_num     = SDA_PIN,
        .scl_io_num     = SCL_PIN,
        .sda_pullup_en  = GPIO_PULLUP_ENABLE,
        .scl_pullup_en  = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ
    };

    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

// ─────────────────────────────
// GPIO INIT
// ─────────────────────────────
void buttons_init(void)
{
    gpio_config_t io = {
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = 1,
        .pin_bit_mask =
            (1ULL << BTN_UP)    |
            (1ULL << BTN_DOWN)  |
            (1ULL << BTN_ENTER) |
            (1ULL << BTN_BACK)
    };

    gpio_config(&io);
}

void pin_init(void)
{
    gpio_config_t out_cfg = {
        .pin_bit_mask  = (1ULL << LOG_BOMBA),
        .mode          = GPIO_MODE_OUTPUT,
        .pull_up_en    = GPIO_PULLUP_DISABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE
    };

    gpio_config(&out_cfg);
    gpio_set_level(LOG_BOMBA, 0);
}

// ─────────────────────────────
// DRAW FUNCTIONS
// ─────────────────────────────
void draw_menu(void)
{
    if (menu_redraw)
    {
        oled_clear();
        oled_print(0, 30, "MENU");
        oled_print(2, 12, "Terapia(T)");
        oled_print(4, 12, "Calibracion(C)");
        menu_redraw = 0;
    }

    oled_print(2, 0, " ");
    oled_print(4, 0, " ");

    if (menu_index == 0) oled_print(2, 0, ">");
    if (menu_index == 1) oled_print(4, 0, ">");
}

void draw_terapia(float actual_mmhg)
{
    char buf[32];

    if (terapia_redraw)
    {
        oled_clear();
        oled_print(0, 0, "Terapia(T)");
        oled_print(2, 0, "SET:");
        oled_print(4, 0, "ACT:");
        terapia_redraw = 0;
    }

    sprintf(buf, "%d", target_vacuum);
    oled_print(2, 48, "      ");
    oled_print(2, 48, buf);

    sprintf(buf, "%.0f", actual_mmhg);
    oled_print(4, 48, "      ");
    oled_print(4, 48, buf);

    if (terapia_running)
        oled_print(6, 0, "[STOP(S)] ");
    else
        oled_print(6, 0, "[START(S)]");
}

void draw_calibrado(void)
{
    oled_clear();
    oled_print(2, 20, "CALIBRADO");
    oled_print(4, 40, "OK");
}

void draw_calibrando(void)
{
    oled_clear();
    oled_print(2, 0, "Calibrando");
    oled_print(4, 0, "espere...");
}

// ─────────────────────────────
// CALIBRATE
// ─────────────────────────────
void calibrate_pressure(void)
{
    draw_calibrando();

    /* La librería toma 64 muestras y escribe el offset */
    bmp180_calibrate(&pressure_offset_hpa);

    draw_calibrado();

    ble_nus_send("OK:CALIB_DONE\n");

    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ─────────────────────────────────────────────────────────────
// TAREA BLE STATUS
// ─────────────────────────────────────────────────────────────
static float s_last_mmhg = 0.0f;

static void ble_status_task(void *arg)
{
    char buf[48];

    for (;;)
    {
        if (ble_nus_notify_enabled() && terapia_running)
        {
            snprintf(buf, sizeof(buf),
                     "DATA:%.0f,%d,%s\n",
                     s_last_mmhg,
                     target_vacuum,
                     bomba_on ? "1" : "0");

            ble_nus_send(buf);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ─────────────────────────────
// MAIN
// ─────────────────────────────
void app_main(void)
{
    /* Periféricos físicos */
    i2c_init();
    buttons_init();
    pin_init();

    /* Inicializar librería BMP180 (lee calibración interna del sensor) */
    bmp180_init(I2C_PORT);

    oled_init();
    draw_menu();

    ble_nus_init("ESP32-TERAPIA", on_ble_rx);

    xTaskCreate(ble_status_task, "ble_status", 2048, NULL, 5, NULL);

    /* ─── Loop principal ─── */
    while (1)
    {
        /* Obtener presión y convertir a mmHg relativos usando la librería */
        float pressure_hpa  = bmp180_get_pressure_hpa();
        float pressure_mmhg = bmp180_pressure_to_mmhg(pressure_hpa, pressure_offset_hpa);

        s_last_mmhg = pressure_mmhg;

        // ───── MENU ─────
        if (current_screen == SCREEN_MENU)
        {
            if (!gpio_get_level(BTN_UP))
            {
                menu_index = 0;
                draw_menu();
                vTaskDelay(pdMS_TO_TICKS(200));
            }

            if (!gpio_get_level(BTN_DOWN))
            {
                menu_index = 1;
                draw_menu();
                vTaskDelay(pdMS_TO_TICKS(200));
            }

            if (!gpio_get_level(BTN_ENTER))
            {
                if (menu_index == 0)
                {
                    current_screen = SCREEN_TERAPIA;
                    terapia_redraw = 1;
                }

                if (menu_index == 1)
                {
                    current_screen = SCREEN_CALIBRACION;
                    calib_redraw   = 1;
                }

                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }

        // ───── TERAPIA ─────
        if (current_screen == SCREEN_TERAPIA)
        {
            draw_terapia(pressure_mmhg);

            if (!gpio_get_level(BTN_BACK))
            {
                current_screen = SCREEN_MENU;
                menu_redraw    = 1;
                draw_menu();
                terapia_redraw = 1;
                calib_redraw   = 1;
                vTaskDelay(pdMS_TO_TICKS(200));
            }

            if (!gpio_get_level(BTN_UP))
            {
                target_vacuum += 5;
                if (target_vacuum > -75) target_vacuum = -75;
                vTaskDelay(pdMS_TO_TICKS(150));
            }

            if (!gpio_get_level(BTN_DOWN))
            {
                target_vacuum -= 5;
                if (target_vacuum < -125) target_vacuum = -125;
                vTaskDelay(pdMS_TO_TICKS(150));
            }

            if (!gpio_get_level(BTN_ENTER))
            {
                terapia_running = !terapia_running;
                terapia_redraw  = 1;
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }

        // ───── CALIBRACION ─────
        if (current_screen == SCREEN_CALIBRACION)
        {
            calibrate_pressure();

            current_screen = SCREEN_MENU;
            menu_redraw    = 1;
            draw_menu();
        }

        // ───── CONTROL BOMBA ─────
        if (terapia_running)
        {
            if (pressure_mmhg > (target_vacuum + HYSTERESIS))
                bomba_on = 1;

            if (pressure_mmhg < (target_vacuum - HYSTERESIS))
                bomba_on = 0;
        }
        else
        {
            bomba_on = 0;
        }

        gpio_set_level(LOG_BOMBA, bomba_on);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}