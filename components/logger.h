/**
 * @file logger.h
 * @brief Logger de eventos con buffer circular y timestamps.
 *
 * Mantiene un historial de los últimos LOGGER_BUFFER_SIZE eventos
 * en RAM. Cada entrada tiene timestamp (epoch Unix si sincronizado,
 * uptime en segundos si no) y un mensaje de texto.
 *
 * El buffer es circular: cuando se llena, sobrescribe la entrada
 * más antigua.
 *
 * Thread-safe: usa mutex interno.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include "app_config.h"

// ─────────────────────────────────────────────────────────────
// ESTRUCTURA DE ENTRADA DE LOG
// ─────────────────────────────────────────────────────────────
typedef struct {
    uint32_t timestamp;             /* Epoch Unix o uptime [segundos] */
    char     msg[LOGGER_MSG_LEN];   /* Mensaje del evento */
} log_entry_t;

// ─────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────

/** Inicializa el logger. Llamar una vez al inicio. */
void logger_init(void);

/**
 * Agrega un evento al log.
 * @param msg Mensaje de texto (se trunca si supera LOGGER_MSG_LEN).
 */
void logger_add(const char *msg);

/**
 * Envía todos los logs por BLE como texto.
 * Formato de cada línea: "LOG:<idx>,<ts>,<msg>\n"
 * Última línea: "LOG:END\n"
 */
void logger_dump_ble(void);

/**
 * Retorna el número de entradas actualmente almacenadas.
 */
uint8_t logger_count(void);

/**
 * Acceso directo a una entrada por índice (0 = más antigua).
 * Retorna NULL si el índice está fuera de rango.
 */
const log_entry_t *logger_get(uint8_t index);

/** Borra todos los logs. */
void logger_clear(void);

#endif /* LOGGER_H */
