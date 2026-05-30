/**
 * @file ble_handler.h
 * @brief Parser de comandos BLE y protocolo de comunicación.
 *
 * Prototipos y documentación para el parser y manejo de comandos BLE.
 * Comandos soportados:
 *   START           → Inicia terapia
 *   STOP            → Detiene terapia
 *   SET:<mmHg>      → Cambia setpoint (rango -80 a -125)
 *   CALIB           → Inicia calibración
 *   STATUS          → Estado general del sistema
 *   GET_STATE       → Solo el estado de la SM
 *   GET_ERRORS      → Error activo actual
 *   GET_LOGS        → Dump del historial de eventos
 *   RESET_ALARMS    → Limpia el error y vuelve a IDLE
 *   TIME:<epoch>    → Sincroniza el timestamp con hora real
 *
 *
 * Respuestas:
 *   OK:<CMD>        → Comando ejecutado exitosamente
 *   ERR:<razón>     → Comando rechazado o inválido
 */

#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <stdint.h>

/**
 * Callback para conectar con ble_nus_init().
 * Procesa el comando recibido y envía la respuesta.
 *
 * @param data Datos recibidos (string terminado en '\0').
 * @param len  Longitud del payload.
 */
void ble_on_rx(const char *data, uint16_t len);

/**
 * Retorna el setpoint actual configurado via BLE o botones.
 * Esta variable es compartida con la tarea de control.
 */
int ble_get_target(void);

/**
 * Actualiza el setpoint desde la UI de botones.
 * @param target_mmhg Nuevo setpoint (se valida internamente).
 */
void ble_set_target(int target_mmhg);

#endif /* BLE_HANDLER_H */
