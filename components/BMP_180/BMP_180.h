#pragma once

#include <stdint.h>
#include "driver/i2c.h"

// ─────────────────────────────────────────────────────────────
//  BMP180 – Librería de driver para ESP-IDF
//
//  Uso típico:
//
//    bmp180_init(I2C_NUM_0);        // leer calibración del sensor
//    bmp180_calibrate(&offset_hpa); // promedia 64 muestras → offset
//    float hpa = bmp180_get_pressure_hpa();
// ─────────────────────────────────────────────────────────────

// Dirección I2C del BMP180
#define BMP180_ADDR     0x77

// ─────────────────────────────
// Inicialización
// ─────────────────────────────

/**
 * @brief Lee los coeficientes de calibración del BMP180.
 *        Debe llamarse una vez después de inicializar el bus I2C.
 *
 * @param port  Puerto I2C ya configurado (ej. I2C_NUM_0).
 */
void bmp180_init(i2c_port_t port);

// ─────────────────────────────
// Lectura de presión
// ─────────────────────────────

/**
 * @brief Retorna la presión absoluta en hPa.
 *        Internamente lee temperatura y presión raw,
 *        aplica la compensación de Bosch y devuelve el resultado.
 *
 * @return Presión en hPa (ej. 850.2).
 */
float bmp180_get_pressure_hpa(void);

// ─────────────────────────────
// Calibración de offset
// ─────────────────────────────

/**
 * @brief Toma 64 muestras consecutivas y promedia la presión.
 *        El resultado se guarda en *offset_hpa y debe usarse como
 *        referencia de "presión atmosférica local".
 *
 * @param offset_hpa  Puntero donde se escribe el offset calculado.
 */
void bmp180_calibrate(float *offset_hpa);

// ─────────────────────────────
// Conversión a mmHg relativos
// ─────────────────────────────

/**
 * @brief Convierte presión absoluta + offset a vacío en mmHg.
 *
 *        vacuum_mmhg = (pressure_hpa - offset_hpa) * 0.75006
 *        Valores entre -5 y +5 se redondean a 0.
 *
 * @param pressure_hpa  Presión actual (de bmp180_get_pressure_hpa).
 * @param offset_hpa    Referencia atmosférica (de bmp180_calibrate).
 * @return Vacío relativo en mmHg (negativo = vacío).
 */
float bmp180_pressure_to_mmhg(float pressure_hpa, float offset_hpa);