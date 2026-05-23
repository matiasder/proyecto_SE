#pragma once

/*
 * ble_nus.h  –  Nordic UART Service (NUS) sobre NimBLE para ESP32
 *
 * Uso básico:
 *   1. ble_nus_init("MiDispositivo", my_rx_callback);
 *   2. ble_nus_send("Hola desde ESP32\n");
 *
 * El callback se invoca desde el contexto de la tarea NimBLE;
 * mantenerlo corto y no bloquear.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────
// TIPOS
// ─────────────────────────────────────────────────────────

/**
 * @brief Callback invocado cuando el cliente escribe en RX.
 *
 * @param data   Datos recibidos (terminados en '\0').
 * @param len    Longitud del payload (sin el '\0').
 */
typedef void (*ble_nus_rx_cb_t)(const char *data, uint16_t len);

// ─────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────

/**
 * @brief Inicializa el stack NimBLE y arranca el NUS.
 *
 * Llama a nvs_flash_init() internamente si todavía no se hizo.
 * Debe llamarse una sola vez, antes de cualquier otra función.
 *
 * @param device_name  Nombre BLE visible al escanear (NULL → "ESP32-NUS").
 * @param rx_cb        Función que recibe los datos escritos por el cliente.
 *                     Puede ser NULL si solo se quiere transmitir.
 */
void ble_nus_init(const char *device_name, ble_nus_rx_cb_t rx_cb);

/**
 * @brief Envía una cadena al cliente conectado (notificación TX).
 *
 * @param msg  String terminado en '\0'.  No enviar >512 bytes.
 * @return true  Si la notificación fue encolada correctamente.
 * @return false Si no hay cliente conectado/suscrito, o error interno.
 */
bool ble_nus_send(const char *msg);

/**
 * @brief Envía un bloque binario/raw al cliente conectado.
 *
 * @param data  Puntero al buffer.
 * @param len   Bytes a enviar.
 * @return true / false igual que ble_nus_send.
 */
bool ble_nus_send_raw(const uint8_t *data, uint16_t len);

/**
 * @brief Indica si hay un cliente BLE conectado en este momento.
 */
bool ble_nus_connected(void);

/**
 * @brief Indica si el cliente está suscrito a notificaciones TX.
 *
 * Solo cuando esto devuelve true ble_nus_send() funciona.
 */
bool ble_nus_notify_enabled(void);

#ifdef __cplusplus
}
#endif