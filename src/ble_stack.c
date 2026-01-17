/**
 * Copyright (c) 2024
 *
 * BLE Media Remote - BLE Stack Implementation
 */

#include "ble_stack.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_pwr_mgmt.h"
#include "ble_gatt.h"
#include "app_error.h"
#include "nrf_log.h"
#include "ble_app.h"

#define NRF_BLE_MAX_MTU_SIZE   BLE_GATT_ATT_MTU_DEFAULT                     /**< MTU size used in the softdevice enabling and in the BLE GATT module. */
#define APP_BLE_OBSERVER_PRIO  3                                            /**< Application's BLE observer priority. */

static nrf_ble_gatt_t m_gatt;                                              /**< GATT module instance. */

// BLE observer declaration (will be defined after ble_stack_on_ble_evt)
NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_stack_on_ble_evt, NULL);

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
void ble_stack_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_app_on_ble_evt(p_ble_evt);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
uint32_t ble_stack_init(void)
{
    uint32_t err_code;

    // Step 4a: Enable SoftDevice
    err_code = nrf_sdh_enable_request();
    if (err_code != NRF_SUCCESS) return 0x4A00 | (err_code & 0xFF);

    // Step 4b: Configure BLE stack with default settings
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    if (err_code != NRF_SUCCESS) return 0x4B00 | (err_code & 0xFF);

    // Step 4c: Enable BLE stack
    err_code = nrf_sdh_ble_enable(&ram_start);
    if (err_code != NRF_SUCCESS) return 0x4C00 | (err_code & 0xFF);

    // Step 4d: Init GATT
    err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    if (err_code != NRF_SUCCESS) return 0x4D00 | (err_code & 0xFF);

    // Step 4e: Set MTU
    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_BLE_MAX_MTU_SIZE);
    if (err_code != NRF_SUCCESS) return 0x4E00 | (err_code & 0xFF);

    // Step 4f: Power management init
    err_code = nrf_pwr_mgmt_init();
    if (err_code != NRF_SUCCESS) return 0x4F00 | (err_code & 0xFF);

    return NRF_SUCCESS;
}

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
void power_manage(void)
{
    nrf_pwr_mgmt_run();
}
