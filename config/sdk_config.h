/**
 * Copyright (c) 2024
 *
 * SDK Configuration for BLE Media Remote
 * 
 * This is a minimal configuration file. For a full configuration,
 * copy sdk_config.h from the nRF5 SDK examples.
 */

#ifndef SDK_CONFIG_H
#define SDK_CONFIG_H

// Logging
#define NRF_LOG_ENABLED 1
#define NRF_LOG_DEFAULT_LEVEL 3
#define NRF_LOG_USES_UART 1
#define NRF_LOG_USES_RTT 0

// BLE
#define BLE_STACK_SUPPORTED 1
#define NRF_SDH_BLE_ENABLED 1
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE 23
#define NRF_SDH_BLE_VS_UUID_COUNT 0
#define NRF_SDH_BLE_PERIPHERAL_LINK_COUNT 1
#define NRF_SDH_BLE_CENTRAL_LINK_COUNT 0
#define NRF_SDH_BLE_TOTAL_LINK_COUNT 1
#define NRF_SDH_BLE_GAP_DATA_LENGTH 251
#define NRF_SDH_BLE_GAP_EVENT_LENGTH 3
#define NRF_SDH_BLE_GATT_MAX_MTU_SIZE 23
#define NRF_SDH_BLE_GAP_ENABLED 1
#define NRF_SDH_BLE_GATT_ENABLED 1

// Advertising
#define BLE_ADVERTISING_ENABLED 1

// Connection Parameters
#define BLE_CONN_PARAMS_ENABLED 1

// GATT
#define NRF_BLE_GATT_ENABLED 1

// Queued Write
#define NRF_BLE_QWR_ENABLED 1

// Power Management
#define NRF_PWR_MGMT_ENABLED 1

// Timer
#define APP_TIMER_ENABLED 1
#define APP_TIMER_KEEPS_RTC_ACTIVE 1

// GPIO
#define NRFX_GPIOTE_ENABLED 1

// UART (for logging)
#define NRF_LOG_BACKEND_UART_ENABLED 1
#define NRF_UART_ENABLED 1

// Note: This is a minimal configuration. For production use,
// copy the full sdk_config.h from an nRF5 SDK example project.

#endif // SDK_CONFIG_H
