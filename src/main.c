/**
 * Copyright (c) 2024
 *
 * BLE Media Remote for iOS
 * Target: NRF52840 / nice!nano
 * 
 * Features:
 * - HID Service for media control (Play/Pause, Next, Prev, Volume)
 * - Device Information Service (shows in iOS info button)
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "fds.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "uart_log.h"

#define APP_BLE_CONN_CFG_TAG 1

// nice!nano LED - P0.15
#define LED_PIN        NRF_GPIO_PIN_MAP(0, 15)
// Buttons (active low with pull-up)
#define BUTTON_PLAY_PAUSE_PIN  NRF_GPIO_PIN_MAP(0, 22)
#define BUTTON_NEXT_PIN        NRF_GPIO_PIN_MAP(0, 6)
#define BUTTON_VOL_UP_PIN      NRF_GPIO_PIN_MAP(0, 8)
#define BUTTON_VOL_DOWN_PIN    NRF_GPIO_PIN_MAP(0, 31)
#define BUTTON_PREV_PIN        NRF_GPIO_PIN_MAP(0, 29)
#define BUTTON_PAUSE_PIN       NRF_GPIO_PIN_MAP(0, 2)

// Media key usage IDs (Consumer Page)
#define MEDIA_PLAY_PAUSE   0x00CD
#define MEDIA_NEXT         0x00B5
#define MEDIA_PREV         0x00B6
#define MEDIA_VOL_UP       0x00E9
#define MEDIA_VOL_DOWN     0x00EA
#define MEDIA_MUTE         0x00E2
#define MEDIA_PAUSE        0x00B1

typedef struct {
    uint32_t pin;
    uint16_t usage;
    const char *label;
} button_cfg_t;

static const button_cfg_t BUTTONS[] = {
    { BUTTON_PLAY_PAUSE_PIN, MEDIA_PLAY_PAUSE, "Play/Pause" },
    { BUTTON_NEXT_PIN,       MEDIA_NEXT, "Next" },
    { BUTTON_VOL_UP_PIN,     MEDIA_VOL_UP, "Vol Up" },
    { BUTTON_VOL_DOWN_PIN,   MEDIA_VOL_DOWN, "Vol Down" },
    { BUTTON_PREV_PIN,       MEDIA_PREV, "Prev" },
    { BUTTON_PAUSE_PIN,      MEDIA_PAUSE, "Pause" },
};
#define BUTTON_COUNT (sizeof(BUTTONS) / sizeof(BUTTONS[0]))

static ble_gap_sec_params_t m_sec_params;

static void led_on(void)  { nrf_gpio_pin_set(LED_PIN); }
static void led_off(void) { nrf_gpio_pin_clear(LED_PIN); }

// SoftDevice fault handler
void sd_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    while(1) { led_on(); nrf_delay_ms(50); led_off(); nrf_delay_ms(50); }
}

static void leds_init(void)
{
    nrf_gpio_cfg_output(LED_PIN);
    led_off();
}

static void button_init(void)
{
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        nrf_gpio_cfg_input(BUTTONS[i].pin, NRF_GPIO_PIN_PULLUP);
    }
}

// ============================================================================
// Device Information Service (DIS) - Shows in iOS "i" button
// ============================================================================
#define BLE_UUID_DEVICE_INFORMATION_SERVICE  0x180A
#define BLE_UUID_MANUFACTURER_NAME_STRING    0x2A29
#define BLE_UUID_MODEL_NUMBER_STRING         0x2A24
#define BLE_UUID_FIRMWARE_REVISION_STRING    0x2A26
#define BLE_UUID_SOFTWARE_REVISION_STRING    0x2A28
#define BLE_UUID_PNP_ID                      0x2A50

static uint16_t m_dis_service_handle;
static ble_gatts_char_handles_t m_manufacturer_handles;
static ble_gatts_char_handles_t m_model_handles;
static ble_gatts_char_handles_t m_firmware_handles;
static ble_gatts_char_handles_t m_pnp_handles;
static const char MANUFACTURER_NAME[] = "nice!nano";
static const char MODEL_NUMBER[] = "Media Remote v1";
static const char FIRMWARE_REV[] = "1.0.0";

// PnP ID: Vendor ID Source (USB), Vendor ID, Product ID, Product Version
static const uint8_t PNP_ID[] = {
    0x02,              // Vendor ID Source: USB Implementer's Forum
    0x9A, 0x10,        // Vendor ID: 0x109A (placeholder - would need real VID)
    0x01, 0x00,        // Product ID: 0x0001
    0x01, 0x00         // Product Version: 0.0.1
};

static void fds_evt_handler(fds_evt_t const *p_evt)
{
    if (p_evt == NULL) {
        return;
    }
    if (p_evt->result != FDS_SUCCESS) {
        uart_log_hex("FDS err: ", p_evt->result);
    }
}


// ============================================================================
// HID Service - Media Control
// ============================================================================
#define BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE  0x1812
#define BLE_UUID_HID_INFORMATION                 0x2A4A
#define BLE_UUID_REPORT_MAP                      0x2A4B
#define BLE_UUID_HID_CONTROL_POINT               0x2A4C
#define BLE_UUID_REPORT                          0x2A4D
#define BLE_UUID_PROTOCOL_MODE                   0x2A4E

// HID Report Descriptor for Consumer Control (Media Keys)
static const uint8_t HID_REPORT_MAP[] = {
    0x05, 0x0C,        // Usage Page (Consumer Devices)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0x9C, 0x02,  //   Logical Maximum (0x029C)
    0x19, 0x00,        //   Usage Minimum (0)
    0x2A, 0x9C, 0x02,  //   Usage Maximum (0x029C)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data, Array, Absolute)
    0xC0               // End Collection
};

// HID Information: bcdHID (1.11), Country Code (0), Flags (Remote Wake + Normally Connectable)
static const uint8_t HID_INFORMATION[] = { 0x11, 0x01, 0x00, 0x02 };

static uint16_t m_hid_service_handle;
static ble_gatts_char_handles_t m_hid_info_handles;
static ble_gatts_char_handles_t m_report_map_handles;
static ble_gatts_char_handles_t m_hid_control_handles;
static ble_gatts_char_handles_t m_report_handles;
static ble_gatts_char_handles_t m_protocol_mode_handles;

static uint8_t m_protocol_mode = 1;  // 0=Boot Protocol, 1=Report Protocol
static uint8_t m_hid_control_point = 0;
static uint8_t m_report_value[2] = {0, 0};  // 16-bit usage
static bool m_report_notif_enabled = false;

// ============================================================================
// Battery Service (Optional but nice to have)
// ============================================================================
#define BLE_UUID_BATTERY_SERVICE  0x180F
#define BLE_UUID_BATTERY_LEVEL    0x2A19

static uint16_t m_bas_service_handle;
static ble_gatts_char_handles_t m_battery_handles;
static uint8_t m_battery_level = 100;  // Fake 100% for now

// ============================================================================
// Connection state
// ============================================================================
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static ble_gap_sec_params_t m_sec_params;

// Advertising data with HID Keyboard appearance (iOS recognizes this)
static uint8_t m_adv_data[] = {
    // Flags
    0x02, 0x01, 0x06,
    // Appearance: HID Keyboard (0x03C1) - iOS recognizes this as input device
    0x03, 0x19, 0xC1, 0x03,
    // 16-bit Service UUIDs: DIS (0x180A) + HID (0x1812)
    0x05, 0x03, 0x0A, 0x18, 0x12, 0x18
};

// Scan response with full name
static uint8_t m_scan_rsp[] = {
    // Complete Local Name: "Media Remote"
    0x0D, 0x09, 'M', 'e', 'd', 'i', 'a', ' ', 'R', 'e', 'm', 'o', 't', 'e'
};

static ble_gap_adv_data_t m_gap_adv_data;
static ble_gap_adv_params_t m_adv_params;

// static void restart_advertising(void)
// {
//     sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
// }

static void pm_evt_handler(pm_evt_t const *p_evt)
{
    if (p_evt == NULL) {
        return;
    }

    uart_log_hex("PM evt: ", p_evt->evt_id);
    uart_log_hex("    !!! conn: ", p_evt->conn_handle);
    switch (p_evt->evt_id) {
        case PM_EVT_BONDED_PEER_CONNECTED: uart_log("    bonded peer connected\r\n"); break;
        case PM_EVT_CONN_SEC_START: uart_log("    sec start\r\n"); break;
        case PM_EVT_CONN_SEC_SUCCEEDED: uart_log("    sec succeeded\r\n"); break;
        case PM_EVT_CONN_SEC_FAILED: uart_log("    sec failed\r\n"); break;
        case PM_EVT_CONN_SEC_CONFIG_REQ: uart_log("    sec config req\r\n"); break;
        case PM_EVT_CONN_SEC_PARAMS_REQ: uart_log("    sec params req\r\n"); break;
        default: break;
    }
    if (p_evt->evt_id == PM_EVT_CONN_SEC_CONFIG_REQ) {
        pm_conn_sec_config_t cfg = { .allow_repairing = true };
        pm_conn_sec_config_reply(p_evt->conn_handle, &cfg);
        uart_log("PM sec cfg reply\r\n");
    }

    if (p_evt->evt_id == PM_EVT_CONN_SEC_PARAMS_REQ) {
        ret_code_t err = pm_conn_sec_params_reply(
            p_evt->conn_handle,
            &m_sec_params,
            p_evt->params.conn_sec_params_req.p_context);
        uart_log_hex("PM sec params reply: ", err);
    }

    pm_handler_on_pm_evt(p_evt);
    pm_handler_flash_clean(p_evt);
    pm_handler_disconnect_on_sec_failure(p_evt);

    if (p_evt->evt_id == PM_EVT_CONN_SEC_FAILED) {
        uart_log_hex("PM sec failed, err: ", p_evt->params.conn_sec_failed.error);
    }
    if (p_evt->evt_id == PM_EVT_CONN_SEC_SUCCEEDED) {
        uart_log("PM sec succeeded\r\n");
    }
}

static void ble_evt_handler(ble_evt_t const *p_evt, void *p_context);
NRF_SDH_BLE_OBSERVER(m_ble_observer, 3, ble_evt_handler, NULL);

static void ble_evt_handler(ble_evt_t const *p_evt, void *p_context)
{
    (void)p_context;
    if (p_evt == NULL) {
        uart_log("no p_evt\r\n");
        return;
    }

    uart_log_hex("BLE evt: ", p_evt->header.evt_id);
    pm_handler_secure_on_error(p_evt);

    switch (p_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_evt->evt.gap_evt.conn_handle;
            uart_log(">>> CONNECTED!\r\n");
            uart_log_hex("    role: ", p_evt->evt.gap_evt.params.connected.role);
            uart_log_hex("    addr type: ", p_evt->evt.gap_evt.params.connected.peer_addr.addr_type);
            led_on();
            {
                ret_code_t err = pm_conn_secure(m_conn_handle, true);
                uart_log_hex("    pm_conn_secure: ", err);
                if (err == NRF_ERROR_BUSY || err == NRF_ERROR_INVALID_STATE) {
                    ret_code_t err2 = sd_ble_gap_authenticate(m_conn_handle, &m_sec_params);
                    uart_log_hex("    gap_authenticate fallback: ", err2);
                }
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            uart_log(">>> DISCONNECTED\r\n");
            uart_log_hex("    reason: ", p_evt->evt.gap_evt.params.disconnected.reason);
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            led_off();
            m_report_notif_enabled = false;
            sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            uart_log(">>> SEC_PARAMS_REQUEST\r\n");
            break;

        case BLE_GAP_EVT_AUTH_STATUS:
            uart_log(">>> AUTH_STATUS\r\n");
            uart_log_hex("    status: ", p_evt->evt.gap_evt.params.auth_status.auth_status);
            uart_log_hex("    bonded: ", p_evt->evt.gap_evt.params.auth_status.bonded);
            break;

        case BLE_GAP_EVT_SEC_INFO_REQUEST:
            uart_log(">>> SEC_INFO_REQUEST\r\n");
            {
                sd_ble_gap_sec_info_reply(m_conn_handle, NULL, NULL, NULL);
            }
            break;

        case BLE_GAP_EVT_CONN_SEC_UPDATE:
            uart_log(">>> CONN_SEC_UPDATE\r\n");
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
            {
                ble_gap_phys_t phys = { .rx_phys = BLE_GAP_PHY_AUTO, .tx_phys = BLE_GAP_PHY_AUTO };
                sd_ble_gap_phy_update(m_conn_handle, &phys);
            }
            break;

        case BLE_GAP_EVT_PHY_UPDATE:
        case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
            break;

        case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
            sd_ble_gap_data_length_update(m_conn_handle, NULL, NULL);
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
            sd_ble_gap_conn_param_update(m_conn_handle, 
                &p_evt->evt.gap_evt.params.conn_param_update_request.conn_params);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            break;

        case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
            sd_ble_gatts_exchange_mtu_reply(m_conn_handle, 23);
            break;

        case BLE_GATTS_EVT_WRITE:
            {
                uint16_t handle = p_evt->evt.gatts_evt.params.write.handle;
                uart_log_hex(">>> WRITE h:", handle);

                // Check if this is our HID report CCCD
                if (handle == m_report_handles.cccd_handle) {
                    uint16_t cccd = *(uint16_t*)p_evt->evt.gatts_evt.params.write.data;
                    uart_log_hex("    HID CCCD enabled: ", cccd);
                    m_report_notif_enabled = (cccd & BLE_GATT_HVX_NOTIFICATION) != 0;
                }
            }
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            // Notification sent
            break;

        default:
            break;
    }
}

// ============================================================================
// Add a characteristic to a service
// ============================================================================
static uint32_t add_char(uint16_t service_handle, uint16_t uuid, 
                         uint8_t props, uint8_t *p_value, uint16_t len,
                         ble_gatts_char_handles_t *p_handles)
{
    ble_uuid_t char_uuid = { .uuid = uuid, .type = BLE_UUID_TYPE_BLE };
    
    ble_gatts_char_md_t char_md = {0};
    char_md.char_props.read = (props & 0x02) ? 1 : 0;
    char_md.char_props.write = (props & 0x08) ? 1 : 0;
    char_md.char_props.write_wo_resp = (props & 0x04) ? 1 : 0;
    char_md.char_props.notify = (props & 0x10) ? 1 : 0;
    
    // Add CCCD if notify is enabled
    ble_gatts_attr_md_t cccd_md = {0};
    if (char_md.char_props.notify) {
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        char_md.p_cccd_md = &cccd_md;
    }
    
    ble_gatts_attr_md_t attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 0;
    
    ble_gatts_attr_t attr = {0};
    attr.p_uuid = &char_uuid;
    attr.p_attr_md = &attr_md;
    attr.init_len = len;
    attr.init_offs = 0;
    attr.max_len = len;
    attr.p_value = p_value;
    
    return sd_ble_gatts_characteristic_add(service_handle, &char_md, &attr, p_handles);
}

// ============================================================================
// Add a characteristic to a service (encrypted link required)
// ============================================================================
static uint32_t add_char_enc(uint16_t service_handle, uint16_t uuid, 
                             uint8_t props, uint8_t *p_value, uint16_t len,
                             ble_gatts_char_handles_t *p_handles)
{
    ble_uuid_t char_uuid = { .uuid = uuid, .type = BLE_UUID_TYPE_BLE };
    
    ble_gatts_char_md_t char_md = {0};
    char_md.char_props.read = (props & 0x02) ? 1 : 0;
    char_md.char_props.write = (props & 0x08) ? 1 : 0;
    char_md.char_props.write_wo_resp = (props & 0x04) ? 1 : 0;
    char_md.char_props.notify = (props & 0x10) ? 1 : 0;
    
    // Add CCCD if notify is enabled
    ble_gatts_attr_md_t cccd_md = {0};
    if (char_md.char_props.notify) {
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
        cccd_md.vloc = BLE_GATTS_VLOC_STACK;
        char_md.p_cccd_md = &cccd_md;
    }
    
    ble_gatts_attr_md_t attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen = 0;
    
    ble_gatts_attr_t attr = {0};
    attr.p_uuid = &char_uuid;
    attr.p_attr_md = &attr_md;
    attr.init_len = len;
    attr.init_offs = 0;
    attr.max_len = len;
    attr.p_value = p_value;
    
    return sd_ble_gatts_characteristic_add(service_handle, &char_md, &attr, p_handles);
}

// ============================================================================
// Add HID Report characteristic with Report Reference descriptor
// ============================================================================
static uint32_t add_hid_report_char(uint16_t service_handle, uint8_t report_id, uint8_t report_type,
                                    uint8_t *p_value, uint16_t len,
                                    ble_gatts_char_handles_t *p_handles)
{
    ble_uuid_t char_uuid = { .uuid = BLE_UUID_REPORT, .type = BLE_UUID_TYPE_BLE };
    
    ble_gatts_char_md_t char_md = {0};
    char_md.char_props.read = 1;
    char_md.char_props.notify = 1;
    
    // CCCD
    ble_gatts_attr_md_t cccd_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    char_md.p_cccd_md = &cccd_md;
    
    // Report Reference Descriptor
    static uint8_t report_ref[2];
    report_ref[0] = report_id;   // Report ID (0 if none)
    report_ref[1] = report_type; // 0x01 = Input, 0x02 = Output, 0x03 = Feature
    
    ble_gatts_attr_md_t report_ref_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_ENC_NO_MITM(&report_ref_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&report_ref_md.write_perm);
    report_ref_md.vloc = BLE_GATTS_VLOC_STACK;
    
    ble_uuid_t report_ref_uuid = { .uuid = 0x2908, .type = BLE_UUID_TYPE_BLE };  // Report Reference
    ble_gatts_attr_t report_ref_attr = {
        .p_uuid = &report_ref_uuid,
        .p_attr_md = &report_ref_md,
        .init_len = 2,
        .max_len = 2,
        .p_value = report_ref
    };
    
    char_md.p_char_user_desc = NULL;
    
    ble_gatts_attr_md_t attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;
    
    ble_gatts_attr_t attr = {
        .p_uuid = &char_uuid,
        .p_attr_md = &attr_md,
        .init_len = len,
        .max_len = len,
        .p_value = p_value
    };
    
    uint32_t err = sd_ble_gatts_characteristic_add(service_handle, &char_md, &attr, p_handles);
    if (err != 0) return err;
    
    // Add Report Reference descriptor
    uint16_t desc_handle;
    return sd_ble_gatts_descriptor_add(p_handles->value_handle, &report_ref_attr, &desc_handle);
}

// ============================================================================
// Security parameters
// ============================================================================
static void sec_params_init(void)
{
    memset(&m_sec_params, 0, sizeof(m_sec_params));
    m_sec_params.bond = 1;
    m_sec_params.mitm = 0;
    m_sec_params.lesc = 0;
    m_sec_params.keypress = 0;
    m_sec_params.io_caps = BLE_GAP_IO_CAPS_NONE;
    m_sec_params.oob = 0;
    m_sec_params.min_key_size = 7;
    m_sec_params.max_key_size = 16;
    m_sec_params.kdist_own.enc = 1;
    m_sec_params.kdist_own.id = 1;
    m_sec_params.kdist_peer.enc = 1;
    m_sec_params.kdist_peer.id = 1;
}

static uint32_t peer_manager_init(void)
{
    ret_code_t err = pm_init();
    if (err != NRF_SUCCESS) {
        return err;
    }

    err = pm_sec_params_set(&m_sec_params);
    if (err != NRF_SUCCESS) {
        return err;
    }

    return pm_register(pm_evt_handler);
}

// ============================================================================
// Initialize Device Information Service
// ============================================================================
static uint32_t dis_init(void)
{
    uint32_t err;
    ble_uuid_t uuid = { .uuid = BLE_UUID_DEVICE_INFORMATION_SERVICE, .type = BLE_UUID_TYPE_BLE };
    
    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &uuid, &m_dis_service_handle);
    if (err) return err;
    
    // Manufacturer Name
    err = add_char(m_dis_service_handle, BLE_UUID_MANUFACTURER_NAME_STRING, 0x02,
                   (uint8_t*)MANUFACTURER_NAME, strlen(MANUFACTURER_NAME), &m_manufacturer_handles);
    if (err) return err;
    
    // Model Number
    err = add_char(m_dis_service_handle, BLE_UUID_MODEL_NUMBER_STRING, 0x02,
                   (uint8_t*)MODEL_NUMBER, strlen(MODEL_NUMBER), &m_model_handles);
    if (err) return err;
    
    // Firmware Revision
    err = add_char(m_dis_service_handle, BLE_UUID_FIRMWARE_REVISION_STRING, 0x02,
                   (uint8_t*)FIRMWARE_REV, strlen(FIRMWARE_REV), &m_firmware_handles);
    if (err) return err;
    
    // PnP ID
    err = add_char(m_dis_service_handle, BLE_UUID_PNP_ID, 0x02,
                   (uint8_t*)PNP_ID, sizeof(PNP_ID), &m_pnp_handles);
    
    return err;
}

// ============================================================================
// Initialize Battery Service
// ============================================================================
static uint32_t bas_init(void)
{
    uint32_t err;
    ble_uuid_t uuid = { .uuid = BLE_UUID_BATTERY_SERVICE, .type = BLE_UUID_TYPE_BLE };
    
    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &uuid, &m_bas_service_handle);
    if (err) return err;
    
    // Battery Level (read + notify)
    err = add_char(m_bas_service_handle, BLE_UUID_BATTERY_LEVEL, 0x12,
                   &m_battery_level, 1, &m_battery_handles);
    
    return err;
}

// ============================================================================
// Initialize HID Service
// ============================================================================
static uint32_t hids_init(void)
{
    uint32_t err;
    ble_uuid_t uuid = { .uuid = BLE_UUID_HUMAN_INTERFACE_DEVICE_SERVICE, .type = BLE_UUID_TYPE_BLE };
    
    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &uuid, &m_hid_service_handle);
    uart_log_hex("     HID svc: ", err);
    if (err) return err;
    
    // HID Information (read)
    err = add_char_enc(m_hid_service_handle, BLE_UUID_HID_INFORMATION, 0x02,
                   (uint8_t*)HID_INFORMATION, sizeof(HID_INFORMATION), &m_hid_info_handles);
    uart_log_hex("     HID info: ", err);
    if (err) return err;
    
    // Report Map (read)
    err = add_char_enc(m_hid_service_handle, BLE_UUID_REPORT_MAP, 0x02,
                   (uint8_t*)HID_REPORT_MAP, sizeof(HID_REPORT_MAP), &m_report_map_handles);
    uart_log_hex("     Report map: ", err);
    if (err) return err;
    
    // HID Control Point (write without response)
    err = add_char_enc(m_hid_service_handle, BLE_UUID_HID_CONTROL_POINT, 0x04,
                   &m_hid_control_point, 1, &m_hid_control_handles);
    uart_log_hex("     Ctrl pt: ", err);
    if (err) return err;
    
    // Protocol Mode (read + write without response)
    err = add_char_enc(m_hid_service_handle, BLE_UUID_PROTOCOL_MODE, 0x06,
                   &m_protocol_mode, 1, &m_protocol_mode_handles);
    uart_log_hex("     Proto mode: ", err);
    if (err) return err;
    
    // Input Report (read + notify) with Report Reference
    err = add_hid_report_char(m_hid_service_handle, 0x00, 0x01,
                              m_report_value, sizeof(m_report_value), &m_report_handles);
    uart_log_hex("     Report: ", err);
    uart_log_hex("     Report handle: ", m_report_handles.value_handle);
    uart_log_hex("     Report CCCD: ", m_report_handles.cccd_handle);
    
    return err;
}

// ============================================================================
// Send HID Report (media key press)
// ============================================================================
static uint32_t send_media_key(uint16_t usage)
{
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return NRF_ERROR_INVALID_STATE;
    }

    if (!m_report_notif_enabled) {
        return NRF_ERROR_INVALID_STATE;
    }
    
    // Populate report: [LSB][MSB]
    m_report_value[0] = usage & 0xFF;
    m_report_value[1] = (usage >> 8) & 0xFF;
    
    // Send notification
    uint16_t len = sizeof(m_report_value);
    ble_gatts_hvx_params_t hvx = {
        .handle = m_report_handles.value_handle,
        .type = BLE_GATT_HVX_NOTIFICATION,
        .offset = 0,
        .p_len = &len,
        .p_data = m_report_value
    };
    
    uint32_t err = sd_ble_gatts_hvx(m_conn_handle, &hvx);
    
    // Small delay then release key
    nrf_delay_ms(50);
    
    // Release key
    m_report_value[0] = 0;
    m_report_value[1] = 0;
    len = sizeof(m_report_value);
    sd_ble_gatts_hvx(m_conn_handle, &hvx);
    
    return err;
}

// ============================================================================
// Main
// ============================================================================
int main(void)
{
    uint32_t err_code;
    uint32_t ram_start = 0x20003000;

    leds_init();
    button_init();
    led_on();
    uart_log_init();
    uart_log("\r\n=== MEDIA REMOTE ===\r\n");

    // Step 1: Enable SoftDevice via nrf_sdh (required for observers/PM)
    uart_log("1. Enabling SoftDevice...\r\n");
    err_code = nrf_sdh_enable_request();
    uart_log_hex("   result: ", err_code);

    // Step 2: Configure BLE
    uart_log("2. Configuring BLE...\r\n");
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    uart_log_hex("   default cfg: ", err_code);
    uart_log_hex("   ram: ", ram_start);

    // Increase attribute table size for HID (need lots of space)
    ble_cfg_t ble_cfg;
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gatts_cfg.attr_tab_size.attr_tab_size = 2048;
    err_code = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &ble_cfg, ram_start);
    uart_log_hex("   ATTR_TAB: ", err_code);

    // Enable BLE
    uart_log("3. Enabling BLE...\r\n");
    err_code = nrf_sdh_ble_enable(&ram_start);
    uart_log_hex("   result: ", err_code);
    uart_log_hex("   ram: ", ram_start);
    if (err_code) {
        while(1) { led_on(); nrf_delay_ms(200); led_off(); nrf_delay_ms(200); }
    }

    // Step 4: Set device name
    uart_log("4. Setting device name...\r\n");
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    sd_ble_gap_device_name_set(&sec_mode, (uint8_t*)"Media Remote", 12);
    
    // Set appearance to HID Keyboard (0x03C1) - iOS recognizes this
    sd_ble_gap_appearance_set(0x03C1);

    // Security params for HID (encrypted link)
    sec_params_init();
    fds_register(fds_evt_handler);
    err_code = fds_init();
    uart_log_hex("   FDS init: ", err_code);
    err_code = peer_manager_init();
    uart_log_hex("   PM: ", err_code);

    // Step 5: Initialize Services
    uart_log("5. Adding services...\r\n");
    
    err_code = dis_init();
    uart_log_hex("   DIS: ", err_code);
    
    err_code = bas_init();
    uart_log_hex("   BAS: ", err_code);
    
    err_code = hids_init();
    uart_log_hex("   HIDS: ", err_code);

    // Step 6: Start advertising
    uart_log("6. Starting advertising...\r\n");
    
    memset(&m_gap_adv_data, 0, sizeof(m_gap_adv_data));
    m_gap_adv_data.adv_data.p_data = m_adv_data;
    m_gap_adv_data.adv_data.len = sizeof(m_adv_data);
    m_gap_adv_data.scan_rsp_data.p_data = m_scan_rsp;
    m_gap_adv_data.scan_rsp_data.len = sizeof(m_scan_rsp);

    memset(&m_adv_params, 0, sizeof(m_adv_params));
    m_adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    m_adv_params.interval = 160;
    m_adv_params.duration = 0;
    m_adv_params.primary_phy = BLE_GAP_PHY_1MBPS;

    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_gap_adv_data, &m_adv_params);
    uart_log_hex("   adv_config: ", err_code);
    err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    uart_log_hex("   adv_start: ", err_code);

    uart_log("\r\n*** READY! Look for 'Media Remote' ***\r\n");
    led_off();

    // Main loop
    for (;;) {
        nrf_sdh_evts_poll();
        // uart_log("after nrf_sdh_evts_poll\r\n");

        // Button handling with simple debounce
        static bool last_raw_pressed[BUTTON_COUNT] = {0};
        static bool stable_pressed[BUTTON_COUNT] = {0};
        static uint8_t stable_count[BUTTON_COUNT] = {0};

        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            bool raw_pressed = (nrf_gpio_pin_read(BUTTONS[i].pin) == 0);
            if (raw_pressed == last_raw_pressed[i]) {
                if (stable_count[i] < 5) {
                    stable_count[i]++;
                }
            } else {
                stable_count[i] = 0;
                last_raw_pressed[i] = raw_pressed;
            }

            if (stable_count[i] == 5 && raw_pressed != stable_pressed[i]) {
                stable_pressed[i] = raw_pressed;
                if (stable_pressed[i] && m_conn_handle != BLE_CONN_HANDLE_INVALID) {
                    uint32_t err = send_media_key(BUTTONS[i].usage);
                    uart_log("Button ");
                    uart_log(BUTTONS[i].label);
                    uart_log_hex(" sent, err=", err);
                }
            }
        }

        nrf_delay_ms(5);
    }
}
