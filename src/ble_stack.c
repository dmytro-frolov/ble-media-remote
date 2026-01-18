/**
 * Copyright (c) 2024
 *
 * BLE Media Remote - BLE Stack Implementation
 */

#include <stdbool.h>
#include <string.h>
#include "ble_stack.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_soc.h"
#include "ble_gatt.h"
#include "app_error.h"
#include "nrf_log.h"
#include "nrf_delay.h"
#include "ble_app.h"
#include "uart_log.h"

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

    // Check if SoftDevice is already enabled
    uart_log("  Checking SD state...\r\n");
    bool sd_enabled = nrf_sdh_is_enabled();
    uart_log_hex("      nrf_sdh_is_enabled: ", sd_enabled ? 1 : 0);

    // Step 3a: Enable SoftDevice (if not already enabled)
    if (!sd_enabled) {
        uart_log("  3a: nrf_sdh_enable_request...\r\n");
        err_code = nrf_sdh_enable_request();
        uart_log_hex("      result: ", err_code);
        if (err_code != NRF_SUCCESS) return 0x3A00 | (err_code & 0xFF);
    } else {
        uart_log("  3a: SoftDevice already enabled, skipping\r\n");
    }

    // Step 3b: Configure BLE stack with default settings
    uart_log("  3b: nrf_sdh_ble_default_cfg_set...\r\n");
    uint32_t ram_start = 0x20003000;  // Start with more RAM
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    uart_log_hex("      result: ", err_code);
    uart_log_hex("      ram_start: ", ram_start);
    if (err_code != NRF_SUCCESS) return 0x3B00 | (err_code & 0xFF);

    // Step 3b2: Configure advertising set count
    uart_log("  3b2: sd_ble_cfg_set (DEVICE_NAME config)...\r\n");
    ble_cfg_t ble_cfg;
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gap_cfg.device_name_cfg.write_perm.sm = 1;
    ble_cfg.gap_cfg.device_name_cfg.write_perm.lv = 1;
    ble_cfg.gap_cfg.device_name_cfg.vloc = BLE_GATTS_VLOC_STACK;
    ble_cfg.gap_cfg.device_name_cfg.p_value = NULL;
    ble_cfg.gap_cfg.device_name_cfg.current_len = 0;
    ble_cfg.gap_cfg.device_name_cfg.max_len = 32;
    err_code = sd_ble_cfg_set(BLE_GAP_CFG_DEVICE_NAME, &ble_cfg, ram_start);
    uart_log_hex("      result: ", err_code);
    // Ignore errors here - it might already be configured

    // Step 3c: Enable BLE stack
    uart_log("  3c: nrf_sdh_ble_enable...\r\n");
    uart_log_hex("      using ram_start: ", ram_start);
    
    uart_log("      calling nrf_sdh_ble_enable now...\r\n");
    err_code = nrf_sdh_ble_enable(&ram_start);
    uart_log_hex("      result: ", err_code);
    uart_log_hex("      new ram_start: ", ram_start);
    if (err_code != NRF_SUCCESS) return 0x3C00 | (err_code & 0xFF);

    // Step 3d: Init GATT
    uart_log("  3d: nrf_ble_gatt_init...\r\n");
    err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    uart_log_hex("      result: ", err_code);
    if (err_code != NRF_SUCCESS) return 0x3D00 | (err_code & 0xFF);

    // Step 3e: Set MTU
    uart_log("  3e: nrf_ble_gatt_att_mtu_periph_set...\r\n");
    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_BLE_MAX_MTU_SIZE);
    uart_log_hex("      result: ", err_code);
    if (err_code != NRF_SUCCESS) return 0x3E00 | (err_code & 0xFF);

    // Step 3f: Power management init
    uart_log("  3f: nrf_pwr_mgmt_init...\r\n");
    err_code = nrf_pwr_mgmt_init();
    uart_log_hex("      result: ", err_code);
    if (err_code != NRF_SUCCESS) return 0x3F00 | (err_code & 0xFF);

    uart_log("  BLE stack init complete!\r\n");
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
