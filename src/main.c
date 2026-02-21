/**
 * BLE Media Remote (nRF5 SDK 15.3.0, SoftDevice S140)
 *
 * Pairs to iOS and sends media controls via BLE HID over GATT (HOGP).
 * Based on the structure of the Nordic `ble_app_hids_keyboard` example:
 * - Peer Manager bonding + whitelist/direct advertising
 * - ble_hids service
 * - ble_advertising module
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

#include "app_error.h"
#include "app_timer.h"

#include "ble.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_srv_common.h"
#include "ble_hids.h"
#include "ble_bas.h"
#include "ble_dis.h"

#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"

#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "fds.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"

#include "nrfx_saadc.h"

#include "uart_log.h"

#define APP_BLE_CONN_CFG_TAG            1
#define APP_BLE_OBSERVER_PRIO           3

#define DEVICE_NAME                     "Media Remote"
#define MANUFACTURER_NAME               "nice!nano"
#define MODEL_NUMBER_STR                "ble-media-remote"

#define APP_ADV_FAST_INTERVAL           160   /* 100 ms (0.625 ms units). */
#define APP_ADV_FAST_DURATION           0     /* Never time out. */

#define BASE_USB_HID_SPEC_VERSION       0x0101

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(30, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(50, UNIT_1_25_MS)
#define SLAVE_LATENCY                   0
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(6000, UNIT_10_MS)

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)
#define MAX_CONN_PARAMS_UPDATE_COUNT    3

// Status LED: blink when not connected, off when connected.
#define STATUS_LED_BLINK_INTERVAL       APP_TIMER_TICKS(250)

// Battery Service updates (to iOS).
#define BATTERY_UPDATE_INTERVAL         APP_TIMER_TICKS(60000)
#define BATTERY_SAADC_CHANNEL           0
// nice!nano battery sense is wired to P0.04 (AIN2) through an on-board divider.
// Divider ratio depends on board revision; common values are 1/2 or 1/3.
// If iOS shows 0% while VBAT is ~4V, your divider is likely 1/3 => set NUM=3.
#define BATTERY_SAADC_INPUT             NRF_SAADC_INPUT_AIN2
#define BATTERY_DIVIDER_NUM             3
#define BATTERY_DIVIDER_DEN             1
#define BATTERY_MV_FULL                 4200
#define BATTERY_MV_EMPTY                3300
#define SAADC_FS_MV 15200u

// nice!nano LED - P0.15
#define LED_PIN                         NRF_GPIO_PIN_MAP(0, 15)

// Buttons (active low with internal pull-up)
// #define FREE_UNUSED_PIN                 NRF_GPIO_PIN_MAP(0, 22)

//  8 / 31 / 29 / 2
//         / 6


#define BUTTON_PLAY_PAUSE_PIN           NRF_GPIO_PIN_MAP(0, 8)
#define BUTTON_NEXT_PIN                 NRF_GPIO_PIN_MAP(0, 31)
#define BUTTON_PREV_PIN                 NRF_GPIO_PIN_MAP(0, 29)
#define BUTTON_VOL_UP_PIN               NRF_GPIO_PIN_MAP(0, 2)
#define BUTTON_VOL_DOWN_PIN             NRF_GPIO_PIN_MAP(0, 6)  

#define BUTTON_SAMPLE_MS                10
#define BUTTON_DEBOUNCE_SAMPLES         5      /* 50 ms */
#define BUTTON_HOLD_MS                  500
#define BUTTON_HOLD_SAMPLES             (BUTTON_HOLD_MS / BUTTON_SAMPLE_MS)
#define BUTTON_NEXT_LONG_HOLD_MS        3000
#define BUTTON_NEXT_LONG_HOLD_SAMPLES   (BUTTON_NEXT_LONG_HOLD_MS / BUTTON_SAMPLE_MS)

// Media key usage IDs (Consumer Page 0x0C)
#define MEDIA_PLAY_PAUSE                0x00CD
#define MEDIA_NEXT_TRACK                0x00B5
#define MEDIA_PREV_TRACK                0x00B6
#define MEDIA_VOL_UP                    0x00E9
#define MEDIA_VOL_DOWN                  0x00EA

typedef struct
{
    uint32_t     pin;
    uint16_t     usage;
    const char * label;
} button_cfg_t;

static const button_cfg_t BUTTONS[] =
{
    { BUTTON_PLAY_PAUSE_PIN, MEDIA_PLAY_PAUSE,    "Play/Pause" },
    { BUTTON_NEXT_PIN,       MEDIA_NEXT_TRACK,    "Next"       },
    // { BUTTON_PREV_PIN,       MEDIA_PREV_TRACK,    "Prev"       },
    { BUTTON_PREV_PIN,       MEDIA_VOL_UP,        "Vol Up"       },
    { BUTTON_VOL_UP_PIN,     MEDIA_VOL_UP,        "Vol Up"     },
    { BUTTON_VOL_DOWN_PIN,   MEDIA_VOL_DOWN,      "Vol Down"   },
};

#define BUTTON_COUNT (sizeof(BUTTONS) / sizeof(BUTTONS[0]))

// HID definitions
#define INPUT_REPORT_CONSUMER_MAX_LEN   2

#define INPUT_REP_REF_ID_CONSUMER       1

#define INPUT_REPORT_INDEX_CONSUMER     0

BLE_HIDS_DEF(m_hids,
	         NRF_SDH_BLE_TOTAL_LINK_COUNT,
	         INPUT_REPORT_CONSUMER_MAX_LEN);

BLE_BAS_DEF(m_bas);
NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);
BLE_ADVERTISING_DEF(m_advertising);
APP_TIMER_DEF(m_status_led_timer);
APP_TIMER_DEF(m_battery_timer);

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static pm_peer_id_t m_peer_id = PM_PEER_ID_INVALID;
static volatile bool m_battery_update_pending = false;
static uint8_t m_battery_level = 100;

static ble_uuid_t m_adv_uuids[] =
{
    { BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, BLE_UUID_TYPE_BLE },
    { BLE_UUID_BATTERY_SERVICE,                BLE_UUID_TYPE_BLE },
};

static void led_on(void)  { nrf_gpio_pin_set(LED_PIN); }
static void led_off(void) { nrf_gpio_pin_clear(LED_PIN); }

static void battery_timer_timeout_handler(void * p_context)
{
    (void)p_context;
    m_battery_update_pending = true;
}

static uint8_t battery_mv_to_percent(uint32_t mv)
{
    if (mv >= BATTERY_MV_FULL)
    {
        return 100;
    }
    if (mv <= BATTERY_MV_EMPTY)
    {
        return 0;
    }
    return (uint8_t)(((mv - BATTERY_MV_EMPTY) * 100u) / (BATTERY_MV_FULL - BATTERY_MV_EMPTY));
}

static void saadc_evt_handler(nrfx_saadc_evt_t const * p_event)
{
    (void)p_event;
}

static void battery_saadc_init(void)
{
    nrfx_saadc_config_t config = NRFX_SAADC_DEFAULT_CONFIG;
    APP_ERROR_CHECK(nrfx_saadc_init(&config, saadc_evt_handler));

    nrf_saadc_channel_config_t ch = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(BATTERY_SAADC_INPUT);
    // Higher acquisition time helps when using a high-impedance divider (e.g. 1M/1M).
    ch.acq_time = NRF_SAADC_ACQTIME_40US;
    APP_ERROR_CHECK(nrfx_saadc_channel_init(BATTERY_SAADC_CHANNEL, &ch));
}

static uint32_t battery_saadc_max_value(void)
{
#if (NRFX_SAADC_CONFIG_RESOLUTION == 0)
    return 255u;    // 8-bit
#elif (NRFX_SAADC_CONFIG_RESOLUTION == 1)
    return 1023u;   // 10-bit
#elif (NRFX_SAADC_CONFIG_RESOLUTION == 2)
    return 4095u;   // 12-bit
#elif (NRFX_SAADC_CONFIG_RESOLUTION == 3)
    return 16383u;  // 14-bit
#else
    return 4095u;
#endif
}


static uint32_t battery_measure_mv(void)
{
    nrf_saadc_value_t raw = 0;
    if (nrfx_saadc_sample_convert(BATTERY_SAADC_CHANNEL, &raw) != NRFX_SUCCESS)
    {
        return 0;
    }
    if (raw < 0)
    {
        raw = 0;
    }

    // With INTERNAL 0.6V reference and GAIN1_6, full-scale ~= 3.6V at max raw.
    uint32_t max_raw = battery_saadc_max_value();
    uint32_t mv = ((uint32_t)raw * SAADC_FS_MV + (max_raw / 2u)) / max_raw;
    mv = (mv * BATTERY_DIVIDER_NUM) / BATTERY_DIVIDER_DEN;
    if (mv > 5000) mv = 5000;
    return mv;
}

static uint32_t battery_measure_mv_avg(void)
{
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++)
    {
        sum += battery_measure_mv();
        nrf_delay_ms(1);
    }
    return sum / 8;
}


static void battery_measure_update_level(void)
{
    uint32_t mv = battery_measure_mv_avg();
    if (mv == 0)
    {
        return;
    }
    m_battery_level = battery_mv_to_percent(mv);
}

static void battery_update_process(void)
{
    if (!m_battery_update_pending)
    {
        return;
    }
    m_battery_update_pending = false;

    battery_measure_update_level();

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        (void)ble_bas_battery_level_update(&m_bas, m_battery_level, m_conn_handle);
    }
}

static void status_led_timeout_handler(void * p_context)
{
    (void)p_context;
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        nrf_gpio_pin_toggle(LED_PIN);
    }
    else
    {
        led_off();
    }
}

static void status_led_start(void)
{
    ret_code_t err = app_timer_start(m_status_led_timer, STATUS_LED_BLINK_INTERVAL, NULL);
    APP_ERROR_CHECK(err);
}

static void status_led_stop(void)
{
    (void)app_timer_stop(m_status_led_timer);
    led_off();
}

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    uart_log("\r\n*** APP ERROR ***\r\n");
    uart_log_hex("id=", id);
    uart_log_hex("pc=", pc);
    uart_log_hex("info=", info);
    while (1)
    {
        led_on();
        nrf_delay_ms(100);
        led_off();
        nrf_delay_ms(100);
    }
}

static void fds_evt_handler(fds_evt_t const * p_evt)
{
    if (p_evt == NULL)
    {
        return;
    }
    if (p_evt->result != FDS_SUCCESS)
    {
        uart_log_hex("FDS err=", p_evt->result);
    }
}

static void timers_init(void)
{
    ret_code_t err = app_timer_init();
    APP_ERROR_CHECK(err);

    err = app_timer_create(&m_status_led_timer, APP_TIMER_MODE_REPEATED, status_led_timeout_handler);
    APP_ERROR_CHECK(err);

    err = app_timer_create(&m_battery_timer, APP_TIMER_MODE_REPEATED, battery_timer_timeout_handler);
    APP_ERROR_CHECK(err);
}

static void power_management_init(void)
{
    ret_code_t err = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err);
}

static void ble_stack_init(void)
{
    ret_code_t err;

    err = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err);

    uint32_t ram_start = 0;
    err = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err);

    err = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err);
}

static void gap_params_init(void)
{
    ret_code_t err;
    ble_gap_conn_params_t gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err = sd_ble_gap_device_name_set(&sec_mode, (uint8_t const *)DEVICE_NAME, strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err);

    // Avoid iOS treating this as a "keyboard"; we only expose Consumer Control media keys.
    err = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_REMOTE_CONTROL);
    APP_ERROR_CHECK(err);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err);
}

static void gatt_init(void)
{
    ret_code_t err = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err);
}

static void qwr_init(void)
{
    nrf_ble_qwr_init_t qwr_init_obj;
    memset(&qwr_init_obj, 0, sizeof(qwr_init_obj));

    ret_code_t err = nrf_ble_qwr_init(&m_qwr, &qwr_init_obj);
    APP_ERROR_CHECK(err);
}

static void dis_init(void)
{
    ble_dis_init_t dis_init_obj;
    memset(&dis_init_obj, 0, sizeof(dis_init_obj));

    ble_srv_ascii_to_utf8(&dis_init_obj.manufact_name_str, (char *)MANUFACTURER_NAME);
    ble_srv_ascii_to_utf8(&dis_init_obj.model_num_str,     (char *)MODEL_NUMBER_STR);
    dis_init_obj.dis_char_rd_sec = SEC_JUST_WORKS;

    ret_code_t err = ble_dis_init(&dis_init_obj);
    APP_ERROR_CHECK(err);
}

static void bas_init(void)
{
    ble_bas_init_t bas_init_obj;
    memset(&bas_init_obj, 0, sizeof(bas_init_obj));

    bas_init_obj.evt_handler          = NULL;
    bas_init_obj.support_notification = true;
    bas_init_obj.p_report_ref         = NULL;
    bas_init_obj.initial_batt_level   = 100;

    bas_init_obj.bl_rd_sec        = SEC_JUST_WORKS;
    bas_init_obj.bl_cccd_wr_sec   = SEC_JUST_WORKS;
    bas_init_obj.bl_report_rd_sec = SEC_JUST_WORKS;

    ret_code_t err = ble_bas_init(&m_bas, &bas_init_obj);
    APP_ERROR_CHECK(err);
}

static void on_hids_evt(ble_hids_t * p_hids, ble_hids_evt_t * p_evt)
{
    (void)p_hids;
    if (p_evt == NULL)
    {
        return;
    }

    switch (p_evt->evt_type)
    {
        case BLE_HIDS_EVT_BOOT_MODE_ENTERED:
            uart_log("HIDS: boot mode\r\n");
            break;
        case BLE_HIDS_EVT_REPORT_MODE_ENTERED:
            uart_log("HIDS: report mode\r\n");
            break;
        case BLE_HIDS_EVT_NOTIF_ENABLED:
            uart_log("HIDS: notif enabled\r\n");
            break;
        case BLE_HIDS_EVT_NOTIF_DISABLED:
            uart_log("HIDS: notif disabled\r\n");
            break;
        case BLE_HIDS_EVT_REP_CHAR_WRITE:
            uart_log("HIDS: rep write\r\n");
            break;
        default:
            break;
    }
}

static void hids_init(void)
{
    static ble_hids_inp_rep_init_t input_report_array[1];

    // Consumer Control only (no keyboard report) to reduce iOS "keyboard" prompts/behavior.
    static uint8_t report_map_data[] =
    {
        0x05, 0x0C,       // Usage Page (Consumer)
        0x09, 0x01,       // Usage (Consumer Control)
        0xA1, 0x01,       // Collection (Application)
        0x85, 0x01,       //   Report ID (1)
        0x15, 0x00,       //   Logical Minimum (0)
        0x26, 0x9C, 0x02, //   Logical Maximum (0x029C)
        0x19, 0x00,       //   Usage Minimum (0)
        0x2A, 0x9C, 0x02, //   Usage Maximum (0x029C)
        0x75, 0x10,       //   Report Size (16)
        0x95, 0x01,       //   Report Count (1)
        0x81, 0x00,       //   Input (Data, Array)
        0xC0              // End Collection
    };

    memset(input_report_array, 0, sizeof(input_report_array));

    ble_hids_inp_rep_init_t * p_inp_cons = &input_report_array[INPUT_REPORT_INDEX_CONSUMER];
    p_inp_cons->max_len             = INPUT_REPORT_CONSUMER_MAX_LEN;
    p_inp_cons->rep_ref.report_id   = INPUT_REP_REF_ID_CONSUMER;
    p_inp_cons->rep_ref.report_type = BLE_HIDS_REP_TYPE_INPUT;
    p_inp_cons->sec.cccd_wr         = SEC_JUST_WORKS;
    p_inp_cons->sec.rd              = SEC_JUST_WORKS;
    p_inp_cons->sec.wr              = SEC_JUST_WORKS;

    uint8_t hid_info_flags = HID_INFO_FLAG_REMOTE_WAKE_MSK | HID_INFO_FLAG_NORMALLY_CONNECTABLE_MSK;

    ble_hids_init_t hids_init_obj;
    memset(&hids_init_obj, 0, sizeof(hids_init_obj));

    hids_init_obj.evt_handler                    = on_hids_evt;
    hids_init_obj.error_handler                  = NULL;
    hids_init_obj.is_kb                          = false;
    hids_init_obj.is_mouse                       = false;
    hids_init_obj.inp_rep_count                  = 1;
    hids_init_obj.p_inp_rep_array                = input_report_array;
    hids_init_obj.outp_rep_count                 = 0;
    hids_init_obj.p_outp_rep_array               = NULL;
    hids_init_obj.feature_rep_count              = 0;
    hids_init_obj.p_feature_rep_array            = NULL;
    hids_init_obj.rep_map.data_len               = sizeof(report_map_data);
    hids_init_obj.rep_map.p_data                 = report_map_data;
    hids_init_obj.hid_information.bcd_hid        = BASE_USB_HID_SPEC_VERSION;
    hids_init_obj.hid_information.b_country_code = 0;
    hids_init_obj.hid_information.flags          = hid_info_flags;
    hids_init_obj.included_services_count        = 0;
    hids_init_obj.p_included_services_array      = NULL;

    hids_init_obj.rep_map.rd_sec                 = SEC_JUST_WORKS;
    hids_init_obj.hid_information.rd_sec         = SEC_JUST_WORKS;
    hids_init_obj.ctrl_point_wr_sec              = SEC_JUST_WORKS;

    ret_code_t err = ble_hids_init(&m_hids, &hids_init_obj);
    APP_ERROR_CHECK(err);
}

static void services_init(void)
{
    qwr_init();
    dis_init();
    bas_init();
    hids_init();
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        ret_code_t err = sd_ble_gap_disconnect(p_evt->conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err);
    }
}

static void conn_params_init(void)
{
    ble_conn_params_init_t cp_init;
    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    ret_code_t err = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err);
}

static void whitelist_set(pm_peer_id_list_skip_t skip)
{
    pm_peer_id_t peer_ids[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
    uint32_t peer_id_count = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

    ret_code_t err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
    APP_ERROR_CHECK(err);

    err = pm_whitelist_set(peer_ids, peer_id_count);
    APP_ERROR_CHECK(err);
}

static void identities_set(pm_peer_id_list_skip_t skip)
{
    pm_peer_id_t peer_ids[BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT];
    uint32_t peer_id_count = BLE_GAP_DEVICE_IDENTITIES_MAX_COUNT;

    ret_code_t err = pm_peer_id_list(peer_ids, &peer_id_count, PM_PEER_ID_INVALID, skip);
    APP_ERROR_CHECK(err);

    err = pm_device_identities_list_set(peer_ids, peer_id_count);
    APP_ERROR_CHECK(err);
}

static void delete_bonds(void)
{
    uart_log("Deleting bonds...\r\n");
    ret_code_t err = pm_peers_delete();
    APP_ERROR_CHECK(err);
}

static void advertising_start(bool erase_bonds)
{
    if (erase_bonds)
    {
        delete_bonds();
        // Advertising is restarted from PM_EVT_PEERS_DELETE_SUCCEEDED.
        return;
    }

    whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
    ret_code_t err = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err);
}

static void pm_evt_handler(pm_evt_t const * p_evt)
{
    if (p_evt == NULL)
    {
        return;
    }

    pm_handler_on_pm_evt(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_BONDED_PEER_CONNECTED:
            m_peer_id = p_evt->peer_id;
            break;

        case PM_EVT_CONN_SEC_SUCCEEDED:
            m_peer_id = p_evt->peer_id;
            break;

        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            advertising_start(false);
            break;

        case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
            if (p_evt->params.peer_data_update_succeeded.flash_changed &&
                (p_evt->params.peer_data_update_succeeded.data_id == PM_PEER_DATA_ID_BONDING))
            {
                whitelist_set(PM_PEER_ID_LIST_SKIP_NO_ID_ADDR);
            }
            break;

        default:
            break;
    }
}

static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    memset(&sec_param, 0, sizeof(sec_param));

    sec_param.bond         = 1;
    sec_param.mitm         = 0;
    sec_param.lesc         = 0;
    sec_param.keypress     = 0;
    sec_param.io_caps      = BLE_GAP_IO_CAPS_NONE;
    sec_param.oob          = 0;
    sec_param.min_key_size = 7;
    sec_param.max_key_size = 16;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    ret_code_t err = pm_init();
    APP_ERROR_CHECK(err);

    err = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err);

    err = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err);

    // If there is a previously bonded peer, keep its id so directed advertising can request its address.
    m_peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            uart_log("ADV: fast\r\n");
            break;

        case BLE_ADV_EVT_IDLE:
            advertising_start(false);
            break;

        case BLE_ADV_EVT_WHITELIST_REQUEST:
        {
            ble_gap_addr_t whitelist_addrs[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
            ble_gap_irk_t  whitelist_irks[BLE_GAP_WHITELIST_ADDR_MAX_COUNT];
            uint32_t addr_cnt = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;
            uint32_t irk_cnt  = BLE_GAP_WHITELIST_ADDR_MAX_COUNT;

            ret_code_t err = pm_whitelist_get(whitelist_addrs, &addr_cnt, whitelist_irks, &irk_cnt);
            APP_ERROR_CHECK(err);

            identities_set(PM_PEER_ID_LIST_SKIP_NO_IRK);

            err = ble_advertising_whitelist_reply(&m_advertising,
                                                  whitelist_addrs,
                                                  addr_cnt,
                                                  whitelist_irks,
                                                  irk_cnt);
            APP_ERROR_CHECK(err);
        } break;

        case BLE_ADV_EVT_PEER_ADDR_REQUEST:
        {
            if (m_peer_id != PM_PEER_ID_INVALID)
            {
                pm_peer_data_bonding_t peer_bonding_data;
                ret_code_t err = pm_peer_data_bonding_load(m_peer_id, &peer_bonding_data);
                if (err == NRF_SUCCESS)
                {
                    identities_set(PM_PEER_ID_LIST_SKIP_ALL);
                    ble_gap_addr_t * p_peer_addr = &(peer_bonding_data.peer_ble_id.id_addr_info);
                    err = ble_advertising_peer_addr_reply(&m_advertising, p_peer_addr);
                    APP_ERROR_CHECK(err);
                }
            }
        } break;

        default:
            break;
    }
}

static void advertising_init(void)
{
    ble_advertising_init_t init;
    memset(&init, 0, sizeof(init));

    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance      = true;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_whitelist_enabled          = true;
    init.config.ble_adv_directed_high_duty_enabled = true;
    init.config.ble_adv_directed_enabled           = false;
    init.config.ble_adv_fast_enabled               = true;
    init.config.ble_adv_fast_interval              = APP_ADV_FAST_INTERVAL;
    init.config.ble_adv_fast_timeout               = APP_ADV_FAST_DURATION;
    init.config.ble_adv_slow_enabled               = false;

    init.evt_handler = on_adv_evt;
    init.error_handler = NULL;

    ret_code_t err = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    (void)p_context;
    if (p_ble_evt == NULL)
    {
        return;
    }

    pm_handler_secure_on_connection(p_ble_evt);
    pm_handler_secure_on_error(p_ble_evt);

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            uart_log("BLE: connected\r\n");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            APP_ERROR_CHECK(nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle));
            status_led_stop();
            battery_measure_update_level();
            (void)ble_bas_battery_level_update(&m_bas, m_battery_level, m_conn_handle);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            uart_log("BLE: disconnected\r\n");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            m_peer_id = PM_PEER_ID_INVALID;
            status_led_start();
            advertising_start(false);
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            ble_gap_phys_t const phys = { .rx_phys = BLE_GAP_PHY_AUTO, .tx_phys = BLE_GAP_PHY_AUTO };
            APP_ERROR_CHECK(sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys));
        } break;

        case BLE_GATTC_EVT_TIMEOUT:
            APP_ERROR_CHECK(sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                                  BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION));
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            APP_ERROR_CHECK(sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                                  BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION));
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            APP_ERROR_CHECK(sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0));
            break;

        default:
            break;
    }
}

NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

static void gpio_init(void)
{
    nrf_gpio_cfg_output(LED_PIN);
    led_off();

    for (uint32_t i = 0; i < BUTTON_COUNT; i++)
    {
        nrf_gpio_cfg_input(BUTTONS[i].pin, NRF_GPIO_PIN_PULLUP);
    }
}

static ret_code_t consumer_control_send(uint16_t usage)
{
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    uint8_t report[2] = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };

    ret_code_t err = ble_hids_inp_rep_send(&m_hids,
                                          INPUT_REPORT_INDEX_CONSUMER,
                                          sizeof(report),
                                          report,
                                          m_conn_handle);
    if (err != NRF_SUCCESS)
    {
        return err;
    }

    // Release
    nrf_delay_ms(20);
    uint8_t release[2] = { 0x00, 0x00 };
    (void)ble_hids_inp_rep_send(&m_hids,
                               INPUT_REPORT_INDEX_CONSUMER,
                               sizeof(release),
                               release,
                               m_conn_handle);
    return NRF_SUCCESS;
}

static void button_poll_and_send(void)
{
    static bool last_raw[BUTTON_COUNT] = {0};
    static uint8_t stable_cnt[BUTTON_COUNT] = {0};
    static bool stable_pressed[BUTTON_COUNT] = {0};
    static uint16_t hold_cnt[BUTTON_COUNT] = {0};
    static bool fired[BUTTON_COUNT] = {0};

    for (uint32_t i = 0; i < BUTTON_COUNT; i++)
    {
        bool raw_pressed = (nrf_gpio_pin_read(BUTTONS[i].pin) == 0);
        bool is_next_btn = (BUTTONS[i].usage == MEDIA_NEXT_TRACK);

        if (raw_pressed == last_raw[i])
        {
            if (stable_cnt[i] < BUTTON_DEBOUNCE_SAMPLES)
            {
                stable_cnt[i]++;
            }
        }
        else
        {
            stable_cnt[i] = 0;
            last_raw[i] = raw_pressed;
        }

        if (stable_cnt[i] == BUTTON_DEBOUNCE_SAMPLES)
        {
            if (raw_pressed != stable_pressed[i])
            {
                if (raw_pressed)
                {
                    stable_pressed[i] = true;
                    hold_cnt[i] = 0;
                    fired[i] = false;
                }
                else
                {
                    // Release: for NEXT, decide between short (Next) and long (Prev).
                    stable_pressed[i] = false;
                    if (is_next_btn && !fired[i] && (hold_cnt[i] >= BUTTON_HOLD_SAMPLES))
                    {
                        fired[i] = true;
                        ret_code_t err = consumer_control_send(MEDIA_NEXT_TRACK);
                        uart_log("BTN: Next\r\n");
                        uart_log_hex(" err=", err);
                    }
                    hold_cnt[i] = 0;
                    fired[i] = false;
                }
            }

            if (stable_pressed[i] && !fired[i])
            {
                if (hold_cnt[i] < 0xFFFF)
                {
                    hold_cnt[i]++;
                }

                if (is_next_btn)
                {
                    if (hold_cnt[i] >= BUTTON_NEXT_LONG_HOLD_SAMPLES)
                    {
                        fired[i] = true;
                        ret_code_t err = consumer_control_send(MEDIA_PREV_TRACK);
                        uart_log("BTN: Next(long)->Prev\r\n");
                        uart_log_hex(" err=", err);
                    }
                }
                else if (hold_cnt[i] >= BUTTON_HOLD_SAMPLES)
                {
                    fired[i] = true;
                    ret_code_t err = consumer_control_send(BUTTONS[i].usage);
                    uart_log("BTN: ");
                    uart_log(BUTTONS[i].label);
                    uart_log_hex(" usage=", BUTTONS[i].usage);
                    uart_log_hex(" err=", err);
                }
            }
        }
    }
}

int main(void)
{
    uart_log_init();
    uart_log("\r\n=== BLE Media Remote (HID) ===\r\n");

    gpio_init();

    bool erase_bonds = (nrf_gpio_pin_read(BUTTON_PLAY_PAUSE_PIN) == 0);
    if (erase_bonds)
    {
        uart_log("Boot: erase bonds requested\r\n");
    }

    timers_init();
    power_management_init();
    battery_saadc_init();
    battery_measure_update_level();

    fds_register(fds_evt_handler);
    APP_ERROR_CHECK(fds_init());

    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
    peer_manager_init();

    advertising_start(erase_bonds);
    uart_log("*** READY: pair from iOS Bluetooth settings ***\r\n");
    status_led_start();
    APP_ERROR_CHECK(app_timer_start(m_battery_timer, BATTERY_UPDATE_INTERVAL, NULL));

    for (;;)
    {
        button_poll_and_send();
        battery_update_process();
        nrf_delay_ms(BUTTON_SAMPLE_MS);
    }
}
