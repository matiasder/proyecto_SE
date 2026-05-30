/**
 * @file sensor.h
 * @brief Capa de validación sobre el driver BMP180.
 *
 * Prototipos y documentación para validación y filtrado de lecturas del sensor BMP180.
 * Envuelve bmp180_get_pressure_hpa() con:
 *   - Verificación de rango físico
 *   - Detección de timeout (sin lectura válida por mucho tiempo)
 *   - Contador de fallos consecutivos
 * No reemplaza BMP_180.h, solo agrega validación encima.
 * El offset de calibración se actualiza via sensor_set_offset().
 */

#ifndef SENSOR_H
#define SENSOR_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c.h"

/** Inicializa el sensor y el validador. Llama a bmp180_init() internamente. */
void sensor_init(i2c_port_t port, float initial_offset_hpa);

/**
 * Lee el sensor, valida la lectura y actualiza el estado interno.
 *
 * Si la lectura es inválida, incrementa el contador de fallos y
 * dispara sm_set_error() cuando supera SENSOR_FAULT_THRESHOLD.
 *
 * @param out_mmhg  [out] Vacío relativo en mmHg. Solo válido si retorna true.
 * @return true si la lectura es válida, false si hay error.
 */
bool sensor_read(float *out_mmhg);

/** Actualiza el offset de calibración (llamar después de bmp180_calibrate). */
void sensor_set_offset(float offset_hpa);

/** Retorna el offset de calibración actual. */
float sensor_get_offset(void);

/** Retorna true si el sensor está en estado de fallo. */
bool sensor_is_faulted(void);

/** Resetea el contador de fallos (luego de resolver el error). */
void sensor_reset_faults(void);

/** Activa/desactiva el modo de prueba. */
void sensor_set_test_mode(bool enabled);

/** Retorna true si el modo prueba está activo. */
bool sensor_get_test_mode(void);

/**
 * Inyecta una presión manual en mmHg.
 * Solo tiene efecto si el modo prueba está activo.
 */
void sensor_set_test_pressure(float mmhg);

#endif /* SENSOR_H */
