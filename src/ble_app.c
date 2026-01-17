/**
 * Copyright (c) 2024
 *
 * BLE Media Remote - BLE Application Implementation
 */

#include <stdint.h>
#include <string.h>
#include "ble_app.h"
#include "nrf_log.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_stack.h"
#include "app_error.h"
#include "app_timer.h"

#define DEVICE_NAME                     "nice!nano Remote"                /**< Name of device. Will be included in the advertising data. */
#define MANUFACTURER_NAME               "NordicSemiconductor"             /**< Manufacturer. Will be passed to Device Information Service. */
#define APP_ADV_INTERVAL                1600                               /**< The advertising interval (in units of 0.625 ms; this value corresponds to 1 second). */
#define APP_ADV_DURATION                18000                              /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)   /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)   /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                  /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)    /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)             /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)            /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                  /**< Number of attempts before giving up the connection parameter update. */

static ble_advertising_t m_advertising;                                    /**< Advertising module instance. */
static nrf_ble_gatt_t    m_gatt;                                           /**< GATT module instance. */
static nrf_ble_qwr_t     m_qwr;                                            /**< Queued writes module instance. */

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            NRF_LOG_INFO("Fast advertising");
            // TODO: Add LED indication if needed
            break;

        case BLE_ADV_EVT_IDLE:
            err_code = ble_app_advertising_start();
            APP_ERROR_CHECK(err_code);
            break;

        default:
            break;
    }
}

/**@brief Function for handling the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module which
 *          are passed to the application.
 *
 * @note All this function does is to disconnect. This could be changed to whatever you want.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(p_evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling a Connection Parameters error.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count    = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                   = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing GATT.
 */
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Queued Write module.
 */
static void qwr_init(void)
{
    uint32_t         err_code;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module
    qwr_init.error_handler = NULL;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance      = true;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete.uuid_cnt = 0;
    init.advdata.uuids_complete.p_uuids  = NULL;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval  = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.config.ble_adv_on_disconnect_disabled = false;

    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

/**@brief Function for handling BLE events.
 */
void ble_app_on_ble_evt(ble_evt_t const * p_ble_evt)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            // TODO: Add LED indication if needed
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            // TODO: Add LED indication if needed
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for initializing the BLE application.
 */
uint32_t ble_app_init(void)
{
    // Initialize GATT
    gatt_init();

    // Initialize Queued Write module
    qwr_init();

    // Initialize Connection Parameters
    conn_params_init();

    // Initialize Advertising
    advertising_init();

    NRF_LOG_INFO("BLE application initialized");

    return NRF_SUCCESS;
}

/**@brief Function for starting advertising.
 */
uint32_t ble_app_advertising_start(void)
{
    uint32_t err_code;

    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);

    return NRF_SUCCESS;
}

/**@brief Function for stopping advertising.
 */
uint32_t ble_app_advertising_stop(void)
{
    uint32_t err_code;

    err_code = sd_ble_gap_adv_stop(m_advertising.adv_handle);
    APP_ERROR_CHECK(err_code);

    return NRF_SUCCESS;
}
