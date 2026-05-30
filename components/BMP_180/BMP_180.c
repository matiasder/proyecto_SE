
/**
 * @file BMP_180.c
 * @brief Driver para sensor de presión BMP180.
 *
 * Implementa la inicialización, lectura y calibración del sensor BMP180.
 */

#include "BMP_180.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include <string.h>

// ─────────────────────────────
// Registros internos BMP180
// ─────────────────────────────
#define REG_CALIB       0xAA
#define REG_CONTROL     0xF4
#define REG_RESULT      0xF6

#define CMD_TEMP        0x2E
#define CMD_PRESSURE    0x34

static const char *TAG = "BMP180";

// ─────────────────────────────
// Estructura de calibración
// ─────────────────────────────
typedef struct
{
    int16_t  AC1, AC2, AC3;
    uint16_t AC4, AC5, AC6;
    int16_t  B1, B2, MB, MC, MD;

} bmp180_calib_t;

// ─────────────────────────────
// Estado interno del módulo
// ─────────────────────────────
static i2c_port_t    s_port = I2C_NUM_0;
static bmp180_calib_t s_cal;

// ─────────────────────────────
// Helpers I2C (privados)
// ─────────────────────────────
static esp_err_t bmp_write(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = {reg, val};

    return i2c_master_write_to_device(
        s_port, BMP180_ADDR,
        data, 2,
        pdMS_TO_TICKS(100)
    );
}

static esp_err_t bmp_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return i2c_master_write_read_device(
        s_port, BMP180_ADDR,
        &reg, 1,
        buf, len,
        pdMS_TO_TICKS(100)
    );
}

// ─────────────────────────────
// Lectura de calibración
// ─────────────────────────────
static void bmp180_read_calibration(void)
{
    uint8_t b[22];

    bmp_read(REG_CALIB, b, 22);

    s_cal.AC1 = (b[0]  << 8) | b[1];
    s_cal.AC2 = (b[2]  << 8) | b[3];
    s_cal.AC3 = (b[4]  << 8) | b[5];
    s_cal.AC4 = (b[6]  << 8) | b[7];
    s_cal.AC5 = (b[8]  << 8) | b[9];
    s_cal.AC6 = (b[10] << 8) | b[11];
    s_cal.B1  = (b[12] << 8) | b[13];
    s_cal.B2  = (b[14] << 8) | b[15];
    s_cal.MB  = (b[16] << 8) | b[17];
    s_cal.MC  = (b[18] << 8) | b[19];
    s_cal.MD  = (b[20] << 8) | b[21];

    ESP_LOGI(TAG, "Calibración leída correctamente");
}

// ─────────────────────────────
// Lecturas raw
// ─────────────────────────────
static int32_t bmp180_read_ut(void)
{
    uint8_t b[2];

    bmp_write(REG_CONTROL, CMD_TEMP);
    vTaskDelay(pdMS_TO_TICKS(10));
    bmp_read(REG_RESULT, b, 2);

    return ((int32_t)b[0] << 8) | b[1];
}

static int32_t bmp180_read_up(void)
{
    uint8_t b[3];

    bmp_write(REG_CONTROL, CMD_PRESSURE);
    vTaskDelay(pdMS_TO_TICKS(10));
    bmp_read(REG_RESULT, b, 3);

    return ((((int32_t)b[0] << 16) |
              ((int32_t)b[1] <<  8) |
              b[2]) >> 8);
}

// ═════════════════════════════════════════════════════════════
// API pública
// ═════════════════════════════════════════════════════════════

void bmp180_init(i2c_port_t port)
{
    s_port = port;
    bmp180_read_calibration();
}

float bmp180_get_pressure_hpa(void)
{
    int32_t UT = bmp180_read_ut();
    int32_t UP = bmp180_read_up();

    /* ── Compensación de temperatura ── */
    int32_t X1 = ((UT - s_cal.AC6) * s_cal.AC5) >> 15;
    int32_t X2 = (s_cal.MC << 11) / (X1 + s_cal.MD);
    int32_t B5 = X1 + X2;

    /* ── Compensación de presión ── */
    int32_t B6 = B5 - 4000;

    X1 = (s_cal.B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = (s_cal.AC2 * B6) >> 11;

    int32_t X3 = X1 + X2;
    int32_t B3 = (((s_cal.AC1 * 4 + X3) + 2) >> 2);

    X1 = (s_cal.AC3 * B6) >> 13;
    X2 = (s_cal.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;

    uint32_t B4 = (s_cal.AC4 * (uint32_t)(X3 + 32768)) >> 15;
    uint32_t B7 = ((uint32_t)UP - B3) * 50000;

    int32_t p;

    if (B7 < 0x80000000)
        p = (B7 * 2) / B4;
    else
        p = (B7 / B4) * 2;

    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;

    p += (X1 + X2 + 3791) >> 4;

    return p / 100.0f;
}

void bmp180_calibrate(float *offset_hpa)
{
    float sum = 0;

    for (int i = 0; i < 64; i++)
    {
        sum += bmp180_get_pressure_hpa();
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    *offset_hpa = sum / 64.0f;

    ESP_LOGI(TAG, "Offset calculado: %.2f hPa", *offset_hpa);
}

float bmp180_pressure_to_mmhg(float pressure_hpa, float offset_hpa)
{
    float mmhg = (pressure_hpa - offset_hpa) * 0.75006f;

    if (mmhg > -5.0f && mmhg < 5.0f)
        mmhg = 0.0f;

    return mmhg;
}