/**
 * Copyright (c) 2024
 *
 * BLE Media Remote - BLE Application Header
 */

#ifndef BLE_APP_H__
#define BLE_APP_H__

#include <stdint.h>
#include "ble.h"
#include "ble_gap.h"

/**@brief Function for initializing the BLE application. */
uint32_t ble_app_init(void);

/**@brief Function for starting advertising. */
uint32_t ble_app_advertising_start(void);

/**@brief Function for stopping advertising. */
uint32_t ble_app_advertising_stop(void);

/**@brief Function for handling BLE events. */
void ble_app_on_ble_evt(ble_evt_t const * p_ble_evt);

#endif // BLE_APP_H__
