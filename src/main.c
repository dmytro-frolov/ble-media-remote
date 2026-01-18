/**
 * Copyright (c) 2024
 *
 * BLE Media Remote for iOS - Main Application
 * Target: NRF52840 / nice!nano
 * 
 * MINIMAL TEST VERSION - Raw SoftDevice API
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
#include "uart_log.h"

// nice!nano LED - P0.15
#define LED_PIN        NRF_GPIO_PIN_MAP(0, 15)

static void led_on(void)  { nrf_gpio_pin_clear(LED_PIN); }
static void led_off(void) { nrf_gpio_pin_set(LED_PIN); }

// SoftDevice fault handler
void sd_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    // Blink rapidly on fault
    while(1) {
        led_on(); nrf_delay_ms(50); led_off(); nrf_delay_ms(50);
    }
}

static void leds_init(void)
{
    nrf_gpio_cfg_output(LED_PIN);
    led_off();
}

// Advertising data: Flags + Complete Local Name
static uint8_t m_adv_data[] = {
    0x02, 0x01, 0x06,  // Length=2, Type=Flags, Value=LE General Disc + BR/EDR not supported
    0x0B, 0x09,        // Length=11, Type=Complete Local Name
    'n', 'i', 'c', 'e', '!', 'n', 'a', 'n', 'o', 'B'  // "nice!nanoB"
};

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static ble_gap_adv_data_t m_gap_adv_data;
static ble_gap_adv_params_t m_adv_params;

int main(void)
{
    uint32_t err_code;
    uint32_t ram_start = 0x20002000;  // RAM start for application

    // Initialize LED
    leds_init();
    led_on();
    
    // Initialize UART logging
    uart_log_init();
    uart_log("\r\n=== MINIMAL BLE TEST ===\r\n");

    // Step 1: Check if SoftDevice is already enabled (nice!nano bootloader leaves it on)
    uart_log("1. Checking SoftDevice state...\r\n");
    uint8_t sd_enabled = 0;
    err_code = sd_softdevice_is_enabled(&sd_enabled);
    uart_log_hex("   sd_enabled: ", sd_enabled);
    
    if (!sd_enabled) {
        uart_log("   SD not enabled, enabling...\r\n");
        // Use static to ensure proper memory alignment
        static const nrf_clock_lf_cfg_t clock_cfg = {
            .source = NRF_CLOCK_LF_SRC_XTAL,  // nice!nano has 32.768kHz crystal
            .rc_ctiv = 0,
            .rc_temp_ctiv = 0,
            .accuracy = NRF_CLOCK_LF_ACCURACY_20_PPM
        };
        err_code = sd_softdevice_enable(&clock_cfg, sd_fault_handler);
        uart_log_hex("   sd_softdevice_enable: ", err_code);
        if (err_code != 0) {
            uart_log("   Trying RC oscillator...\r\n");
            // Fallback to RC oscillator
            static const nrf_clock_lf_cfg_t clock_cfg_rc = {
                .source = NRF_CLOCK_LF_SRC_RC,
                .rc_ctiv = 16,
                .rc_temp_ctiv = 2,
                .accuracy = NRF_CLOCK_LF_ACCURACY_500_PPM
            };
            err_code = sd_softdevice_enable(&clock_cfg_rc, sd_fault_handler);
            uart_log_hex("   sd_softdevice_enable (RC): ", err_code);
            if (err_code != 0) {
                uart_log("   FAILED!\r\n");
                while(1) { led_on(); nrf_delay_ms(100); led_off(); nrf_delay_ms(100); }
            }
        }
    } else {
        uart_log("   SoftDevice already enabled by bootloader!\r\n");
    }

    // Step 2: Configure BLE before enabling
    uart_log("2. Configuring BLE...\r\n");
    // Use linker script RAM start (matches nicenano.ld)
    ram_start = 0x20003000;
    
    ble_cfg_t ble_cfg;
    
    // Configure connection count - minimal
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.conn_cfg.conn_cfg_tag = 1;
    ble_cfg.conn_cfg.params.gap_conn_cfg.conn_count = 1;
    ble_cfg.conn_cfg.params.gap_conn_cfg.event_length = 3;
    err_code = sd_ble_cfg_set(BLE_CONN_CFG_GAP, &ble_cfg, ram_start);
    uart_log_hex("   BLE_CONN_CFG_GAP: ", err_code);
    
    // Configure role count (peripheral only)
    memset(&ble_cfg, 0, sizeof(ble_cfg));
    ble_cfg.gap_cfg.role_count_cfg.periph_role_count = 1;
    ble_cfg.gap_cfg.role_count_cfg.central_role_count = 0;
    ble_cfg.gap_cfg.role_count_cfg.central_sec_count = 0;
    err_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &ble_cfg, ram_start);
    uart_log_hex("   BLE_GAP_CFG_ROLE_COUNT: ", err_code);
    
    // Step 3: Enable BLE with enough RAM
    uart_log("3. Enabling BLE (ram_start=0x20003000)...\r\n");
    err_code = sd_ble_enable(&ram_start);
    uart_log_hex("   sd_ble_enable: ", err_code);
    uart_log_hex("   actual ram_start: ", ram_start);
    
    // NRF_ERROR_INVALID_STATE (0x08) means BLE is already enabled
    if (err_code == 0x08) {
        uart_log("   BLE already enabled!\r\n");
        err_code = 0;
    } else if (err_code != 0) {
        uart_log("   FAILED!\r\n");
        while(1) { led_on(); nrf_delay_ms(200); led_off(); nrf_delay_ms(200); }
    }

    // Step 3: Set device name
    uart_log("3. Setting device name...\r\n");
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err_code = sd_ble_gap_device_name_set(&sec_mode, (uint8_t *)"nice!nanoB", 10);
    uart_log_hex("   sd_ble_gap_device_name_set: ", err_code);
    if (err_code != 0) {
        uart_log("   FAILED!\r\n");
    }

    // Step 4: Configure advertising data
    uart_log("4. Configuring advertising...\r\n");
    memset(&m_gap_adv_data, 0, sizeof(m_gap_adv_data));
    m_gap_adv_data.adv_data.p_data = m_adv_data;
    m_gap_adv_data.adv_data.len = sizeof(m_adv_data);
    m_gap_adv_data.scan_rsp_data.p_data = NULL;
    m_gap_adv_data.scan_rsp_data.len = 0;

    memset(&m_adv_params, 0, sizeof(m_adv_params));
    m_adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    m_adv_params.p_peer_addr = NULL;
    m_adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval = 160;  // 100ms
    m_adv_params.duration = 0;    // Forever
    m_adv_params.primary_phy = BLE_GAP_PHY_1MBPS;

    uart_log("   sd_ble_gap_adv_set_configure...\r\n");
    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_gap_adv_data, &m_adv_params);
    uart_log_hex("   result: ", err_code);
    uart_log_hex("   adv_handle: ", m_adv_handle);
    if (err_code != 0) {
        uart_log("   FAILED!\r\n");
        while(1) { led_on(); nrf_delay_ms(300); led_off(); nrf_delay_ms(300); }
    }

    // Step 5: Start advertising
    uart_log("5. Starting advertising...\r\n");
    err_code = sd_ble_gap_adv_start(m_adv_handle, 1);  // conn_cfg_tag = 1
    uart_log_hex("   sd_ble_gap_adv_start: ", err_code);
    if (err_code != 0) {
        uart_log("   FAILED! Trying tag=0...\r\n");
        err_code = sd_ble_gap_adv_start(m_adv_handle, 0);  // Try default tag
        uart_log_hex("   sd_ble_gap_adv_start (tag=0): ", err_code);
        if (err_code != 0) {
            uart_log("   FAILED!\r\n");
            while(1) { led_on(); nrf_delay_ms(500); led_off(); nrf_delay_ms(500); }
        }
    }

    uart_log("\r\n*** SUCCESS! Advertising as 'nice!nanoB' ***\r\n");
    uart_log("Check your phone's Bluetooth settings.\r\n");
    led_off();

    // Main loop
    for (;;) {
        sd_app_evt_wait();
    }
}
