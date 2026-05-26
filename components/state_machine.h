/**
 * @file state_machine.h
 * @brief Máquina de estados global del sistema VAC Therapy.
 *
 * Estados:
 *   IDLE        → Sistema listo, sin terapia activa
 *   RUNNING     → Terapia en curso, bomba controlada
 *   CALIBRATION → Proceso de calibración del sensor
 *   ERROR       → Fallo detectado, bomba detenida
 *
 * Transiciones válidas:
 *   IDLE        → RUNNING     (START por BLE o botón)
 *   IDLE        → CALIBRATION (CALIB por BLE o botón)
 *   RUNNING     → IDLE        (STOP por BLE o botón)
 *   RUNNING     → ERROR       (fallo sensor o fuga detectada)
 *   CALIBRATION → IDLE        (calibración completada o cancelada)
 *   ERROR       → IDLE        (RESET_ALARMS por BLE)
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

// ─────────────────────────────────────────────────────────────
// ESTADOS
// ─────────────────────────────────────────────────────────────
typedef enum {
    STATE_IDLE        = 0,
    STATE_RUNNING     = 1,
    STATE_CALIBRATION = 2,
    STATE_ERROR       = 3,
} system_state_t;

// ─────────────────────────────────────────────────────────────
// TIPOS DE ERROR
// Indican qué provocó la entrada a STATE_ERROR
// ─────────────────────────────────────────────────────────────
typedef enum {
    ERR_NONE              = 0x00,
    ERR_SENSOR_TIMEOUT    = 0x01,   /* Sin lectura del sensor */
    ERR_SENSOR_RANGE      = 0x02,   /* Valor fuera del rango físico */
    ERR_LEAK_DETECTED     = 0x03,   /* Fuga neumática detectada */
    ERR_PRESSURE_LOW      = 0x04,   /* Presión excede límite inferior */
} error_code_t;

// ─────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────

/** Inicializa la máquina de estados. Llamar una vez al inicio. */
void sm_init(void);

/** Retorna el estado actual. */
system_state_t sm_get_state(void);

/** Retorna el código de error activo (ERR_NONE si no hay error). */
error_code_t sm_get_error(void);

/** Retorna nombre del estado como string (para BLE y logs). */
const char *sm_state_str(system_state_t state);

/** Retorna nombre del error como string. */
const char *sm_error_str(error_code_t err);

/** Transición a RUNNING. Solo válida desde IDLE. */
bool sm_start_therapy(void);

/** Transición a IDLE. Válida desde RUNNING. */
bool sm_stop_therapy(void);

/** Transición a CALIBRATION. Solo válida desde IDLE. */
bool sm_start_calibration(void);

/** Transición a IDLE al terminar calibración. */
bool sm_finish_calibration(void);

/** Transición a ERROR con código específico. Válida desde RUNNING o IDLE. */
void sm_set_error(error_code_t code);

/** Limpia el error y vuelve a IDLE. Solo válida desde ERROR. */
bool sm_reset_error(void);

/** true si la terapia está activa (STATE_RUNNING). */
bool sm_is_running(void);

/** true si hay un error activo (STATE_ERROR). */
bool sm_is_error(void);

#endif /* STATE_MACHINE_H */
