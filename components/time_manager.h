/**
 * @file time_manager.h
 * @brief Timestamps con sincronización BLE.
 *
 * Prototipos y documentación para gestión de timestamps y sincronización BLE.
 * El ESP32 no tiene RTC. Este módulo mantiene una base de tiempo:
 *   - Si se sincronizó via BLE (comando TIME:<epoch>):
 *     timestamp = epoch_base + (ticks_actuales - sync_tick) / 1000
 *   - Si no se sincronizó:
 *     timestamp = segundos de uptime desde el boot
 * Los logs y el historial de eventos usan este timestamp.
 */

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/** Inicializa el gestor de tiempo. Llamar una vez al inicio. */
void time_manager_init(void);

/**
 * Sincroniza con tiempo real enviado desde la app BLE.
 * @param epoch Tiempo Unix en segundos (ej. 1716415023).
 */
void time_manager_sync(uint32_t epoch);

/**
 * Retorna el timestamp actual en segundos.
 * Si está sincronizado: epoch Unix real.
 * Si no: uptime en segundos desde boot.
 */
uint32_t time_manager_get_epoch(void);

/** Retorna true si el tiempo fue sincronizado via BLE. */
bool time_manager_is_synced(void);

/** Retorna los segundos de uptime desde el boot (siempre disponible). */
uint32_t time_manager_get_uptime(void);

#endif /* TIME_MANAGER_H */
