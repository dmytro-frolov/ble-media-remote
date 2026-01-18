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
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "uart_log.h"

// nice!nano LED - P0.15
#define LED_PIN        NRF_GPIO_PIN_MAP(0, 15)

static void led_on(void)  { nrf_gpio_pin_clear(LED_PIN); }
static void led_off(void) { nrf_gpio_pin_set(LED_PIN); }

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
    0x85, 0x01,        //   Report ID (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16) - 16 buttons
    0x09, 0xCD,        //   Usage (Play/Pause)
    0x09, 0xB5,        //   Usage (Scan Next Track)
    0x09, 0xB6,        //   Usage (Scan Previous Track)
    0x09, 0xE9,        //   Usage (Volume Up)
    0x09, 0xEA,        //   Usage (Volume Down)
    0x09, 0xE2,        //   Usage (Mute)
    0x09, 0xB0,        //   Usage (Play)
    0x09, 0xB1,        //   Usage (Pause)
    0x09, 0xB3,        //   Usage (Fast Forward)
    0x09, 0xB4,        //   Usage (Rewind)
    0x09, 0xB2,        //   Usage (Record)
    0x09, 0xB7,        //   Usage (Stop)
    0x0A, 0x23, 0x02,  //   Usage (Home) - AC Home
    0x0A, 0x24, 0x02,  //   Usage (Back) - AC Back
    0x0A, 0x25, 0x02,  //   Usage (Forward) - AC Forward
    0x0A, 0x26, 0x02,  //   Usage (Stop) - AC Stop
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
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
static uint8_t m_report_value[3] = {1, 0, 0};  // Report ID + 16 bits for media buttons

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
    report_ref[0] = report_id;   // Report ID
    report_ref[1] = report_type; // 0x01 = Input, 0x02 = Output, 0x03 = Feature
    
    ble_gatts_attr_md_t report_ref_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&report_ref_md.read_perm);
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
    err = add_char(m_hid_service_handle, BLE_UUID_HID_INFORMATION, 0x02,
                   (uint8_t*)HID_INFORMATION, sizeof(HID_INFORMATION), &m_hid_info_handles);
    uart_log_hex("     HID info: ", err);
    if (err) return err;
    
    // Report Map (read)
    err = add_char(m_hid_service_handle, BLE_UUID_REPORT_MAP, 0x02,
                   (uint8_t*)HID_REPORT_MAP, sizeof(HID_REPORT_MAP), &m_report_map_handles);
    uart_log_hex("     Report map: ", err);
    if (err) return err;
    
    // HID Control Point (write without response)
    err = add_char(m_hid_service_handle, BLE_UUID_HID_CONTROL_POINT, 0x04,
                   &m_hid_control_point, 1, &m_hid_control_handles);
    uart_log_hex("     Ctrl pt: ", err);
    if (err) return err;
    
    // Protocol Mode (read + write without response)
    err = add_char(m_hid_service_handle, BLE_UUID_PROTOCOL_MODE, 0x06,
                   &m_protocol_mode, 1, &m_protocol_mode_handles);
    uart_log_hex("     Proto mode: ", err);
    if (err) return err;
    
    // Input Report (read + notify) with Report Reference
    err = add_hid_report_char(m_hid_service_handle, 0x01, 0x01,
                              m_report_value, sizeof(m_report_value), &m_report_handles);
    uart_log_hex("     Report: ", err);
    uart_log_hex("     Report handle: ", m_report_handles.value_handle);
    uart_log_hex("     Report CCCD: ", m_report_handles.cccd_handle);
    
    return err;
}

// ============================================================================
// Send HID Report (media key press)
// ============================================================================
static uint32_t send_media_key(uint16_t key_bit)
{
    if (m_conn_handle == BLE_CONN_HANDLE_INVALID) {
        return 0xFFFF;
    }
    
    // Populate report: [Report ID][LSB][MSB]
    m_report_value[0] = 0x01;
    m_report_value[1] = key_bit & 0xFF;
    m_report_value[2] = (key_bit >> 8) & 0xFF;
    
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
    
    // Release key (keep report ID)
    m_report_value[0] = 0x01;
    m_report_value[1] = 0;
    m_report_value[2] = 0;
    len = sizeof(m_report_value);
    sd_ble_gatts_hvx(m_conn_handle, &hvx);
    
    return err;
}

// Media key bits (match Report Descriptor order)
#define MEDIA_PLAY_PAUSE   (1 << 0)
#define MEDIA_NEXT         (1 << 1)
#define MEDIA_PREV         (1 << 2)
#define MEDIA_VOL_UP       (1 << 3)
#define MEDIA_VOL_DOWN     (1 << 4)
#define MEDIA_MUTE         (1 << 5)

// ============================================================================
// Main
// ============================================================================
int main(void)
{
    uint32_t err_code;
    uint32_t ram_start = 0x20003000;

    leds_init();
    led_on();
    uart_log_init();
    uart_log("\r\n=== MEDIA REMOTE ===\r\n");

    // Step 1: Enable SoftDevice
    uart_log("1. Enabling SoftDevice...\r\n");
    uint8_t sd_enabled = 0;
    sd_softdevice_is_enabled(&sd_enabled);
    
    if (!sd_enabled) {
        static const nrf_clock_lf_cfg_t clock_cfg = {
            .source = NRF_CLOCK_LF_SRC_XTAL,
            .rc_ctiv = 0,
            .rc_temp_ctiv = 0,
            .accuracy = NRF_CLOCK_LF_ACCURACY_20_PPM
        };
        err_code = sd_softdevice_enable(&clock_cfg, sd_fault_handler);
        if (err_code) {
            static const nrf_clock_lf_cfg_t clock_cfg_rc = {
                .source = NRF_CLOCK_LF_SRC_RC,
                .rc_ctiv = 16,
                .rc_temp_ctiv = 2,
                .accuracy = NRF_CLOCK_LF_ACCURACY_500_PPM
            };
            err_code = sd_softdevice_enable(&clock_cfg_rc, sd_fault_handler);
        }
        uart_log_hex("   result: ", err_code);
    }

    // Step 2: Configure BLE
    uart_log("2. Configuring BLE...\r\n");
    ble_cfg_t ble_cfg;
    
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag = 1;
    ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = 1;
    ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 6;
    sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
    
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gap_cfg.role_count_cfg.periph_role_count = 1;
    sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
    
    // Increase attribute table size for HID (need lots of space)
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gatts_cfg.attr_tab_size.attr_tab_size = 2048;
    err_code = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &ble_cfg, ram_start);
    uart_log_hex("   ATTR_TAB: ", err_code);
    
    // Enable BLE
    uart_log("3. Enabling BLE...\r\n");
    err_code = sd_ble_enable(&ram_start);
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
    
    err_code = sd_ble_gap_adv_start(m_adv_handle, 1);
    uart_log_hex("   adv_start: ", err_code);

    uart_log("\r\n*** READY! Look for 'Media Remote' ***\r\n");
    led_off();

    // Main loop
    for (;;) {
        sd_app_evt_wait();
        
        uint8_t evt_buffer[256] __attribute__((aligned(4)));
        uint16_t evt_len = sizeof(evt_buffer);
        
        while (sd_ble_evt_get(evt_buffer, &evt_len) == 0) {
            ble_evt_t *p_evt = (ble_evt_t *)evt_buffer;
            
            switch (p_evt->header.evt_id) {
                case BLE_GAP_EVT_CONNECTED:
                    m_conn_handle = p_evt->evt.gap_evt.conn_handle;
                    uart_log(">>> CONNECTED!\r\n");
                    led_on();
                    break;
                    
                case BLE_GAP_EVT_DISCONNECTED:
                    uart_log(">>> DISCONNECTED\r\n");
                    m_conn_handle = BLE_CONN_HANDLE_INVALID;
                    led_off();
                    sd_ble_gap_adv_start(m_adv_handle, 1);
                    break;
                    
                case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
                    uart_log(">>> SEC_PARAMS_REQUEST\r\n");
                    // Accept pairing with encryption (required for HID on iOS)
                    {
                        ble_gap_sec_params_t sec_params = {0};
                        sec_params.bond = 1;           // Enable bonding
                        sec_params.mitm = 0;           // No MITM (Just Works)
                        sec_params.lesc = 0;           // No LE Secure Connections
                        sec_params.keypress = 0;
                        sec_params.io_caps = BLE_GAP_IO_CAPS_NONE;
                        sec_params.oob = 0;
                        sec_params.min_key_size = 7;
                        sec_params.max_key_size = 16;
                        sec_params.kdist_own.enc = 1;  // Distribute our encryption key
                        sec_params.kdist_own.id = 1;   // Distribute our identity
                        sec_params.kdist_peer.enc = 1; // Request peer's encryption key
                        sec_params.kdist_peer.id = 1;  // Request peer's identity
                        
                        ble_gap_sec_keyset_t keyset = {0};
                        static ble_gap_enc_key_t own_enc_key;
                        static ble_gap_id_key_t own_id_key;
                        static ble_gap_enc_key_t peer_enc_key;
                        static ble_gap_id_key_t peer_id_key;
                        
                        keyset.keys_own.p_enc_key = &own_enc_key;
                        keyset.keys_own.p_id_key = &own_id_key;
                        keyset.keys_peer.p_enc_key = &peer_enc_key;
                        keyset.keys_peer.p_id_key = &peer_id_key;
                        
                        uint32_t err = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_SUCCESS, &sec_params, &keyset);
                        uart_log_hex("    sec_reply: ", err);
                    }
                    break;
                    
                case BLE_GAP_EVT_AUTH_STATUS:
                    uart_log(">>> AUTH_STATUS\r\n");
                    uart_log_hex("    status: ", p_evt->evt.gap_evt.params.auth_status.auth_status);
                    uart_log_hex("    bonded: ", p_evt->evt.gap_evt.params.auth_status.bonded);
                    break;
                    
                case BLE_GAP_EVT_SEC_INFO_REQUEST:
                    uart_log(">>> SEC_INFO_REQUEST\r\n");
                    // Peer is asking for stored keys (for re-encryption)
                    // For now, we don't store keys, so reply with NULL
                    sd_ble_gap_sec_info_reply(m_conn_handle, NULL, NULL, NULL);
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
                        }
                    }
                    break;
                    
                case BLE_GATTS_EVT_HVN_TX_COMPLETE:
                    // Notification sent
                    break;
                    
                default:
                    break;
            }
            
            evt_len = sizeof(evt_buffer);
        }
        
        // Demo: Send Play/Pause every 10 seconds when connected
        static uint32_t counter = 0;
        counter++;
        if (counter > 1000000 && m_conn_handle != BLE_CONN_HANDLE_INVALID) {
            uint32_t err = send_media_key(MEDIA_PLAY_PAUSE);
            uart_log_hex("Play/Pause sent, err=", err);
            counter = 0;
        }
    }
}
