/**
 * @file control.h
 * @brief Control PWM de la bomba de vacío y buzzer de alarma.
 *
 * ── BOMBA (PWM proporcional) ──────────────────────────────────────────
 *
 *   error     = pressure_mmhg - target_mmhg
 *
 *   Si error > 0 (falta vacío):
 *     duty = clamp(PWM_KP * error, PUMP_DUTY_MIN_PCT, PUMP_DUTY_MAX_PCT)
 *
 *   Si error <= 0 (vacío suficiente o en exceso leve):
 *     duty = PUMP_DUTY_MIN_PCT
 *
 *   La bomba nunca se apaga durante terapia activa.
 *   El duty mínimo compensa el leak permanente del sistema neumático.
 *   El slew rate evita cambios bruscos de potencia entre ciclos.
 *
 * ── BUZZER ────────────────────────────────────────────────────────────
 *   GPIO simple: HIGH = suena, LOW = silencio.
 *   Se activa en emergency_stop() y se apaga en sm_reset_error().
 */

#ifndef CONTROL_H
#define CONTROL_H

#include <stdbool.h>
#include <stdint.h>

/** Inicializa LEDC para la bomba y GPIO para el buzzer. */
void control_init(void);

/**
 * Ejecuta un ciclo de control completo.
 * Llamar desde control_task cada PERIOD_CONTROL_MS.
 *
 * @param pressure_mmhg  Presión actual [mmHg, negativo].
 * @param target_mmhg    Setpoint de vacío [mmHg, negativo].
 */
void control_update(float pressure_mmhg, int target_mmhg);

/**
 * Parada de emergencia: bomba OFF (0% PWM), buzzer ON.
 * Llamada desde cualquier módulo ante condición de fallo.
 */
void emergency_stop(void);

/** Activa el buzzer (pin HIGH). */
void buzzer_on(void);

/** Apaga el buzzer (pin LOW). */
void buzzer_off(void);

/** Retorna true si la bomba está activa (duty >= PUMP_DUTY_MIN_PCT). */
bool control_pump_is_on(void);

/** Retorna el duty cycle actual de la bomba [0.0 – 100.0 %]. */
float control_get_duty_pct(void);

/** Resetea el temporizador de detección de fugas al iniciar terapia. */
void control_reset_leak_timer(void);

#endif /* CONTROL_H */