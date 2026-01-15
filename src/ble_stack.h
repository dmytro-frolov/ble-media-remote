/**
 * Copyright (c) 2024
 *
 * BLE Media Remote - BLE Stack Header
 */

#ifndef BLE_STACK_H__
#define BLE_STACK_H__

#include <stdint.h>
#include "ble.h"
#include "ble_gap.h"

#define APP_BLE_CONN_CFG_TAG    1                                           /**< A tag identifying the SoftDevice BLE configuration. */

/**@brief Function for initializing the BLE stack. */
uint32_t ble_stack_init(void);

/**@brief Function for handling BLE events. */
void ble_stack_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
void power_manage(void);

#endif // BLE_STACK_H__
