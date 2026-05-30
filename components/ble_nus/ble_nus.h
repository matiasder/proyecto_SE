#ifndef BLE_NUS_H
#define BLE_NUS_H

/**
 * @file ble_nus.h
 * @brief Servicio BLE NUS (Nordic UART Service) - prototipos.
 *
 * Prototipos para inicialización y uso del servicio BLE NUS.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ble_nus_rx_cb_t)(const char *data, uint16_t len);

void ble_nus_init(const char *device_name, ble_nus_rx_cb_t rx_cb);
bool ble_nus_send(const char *msg);
bool ble_nus_send_raw(const uint8_t *data, uint16_t len);
bool ble_nus_connected(void);
bool ble_nus_notify_enabled(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_NUS_H