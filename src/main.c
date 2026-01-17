/**
 * Copyright (c) 2024
 *
 * BLE Media Remote for iOS - Main Application
 * Target: NRF52840
 */

#include <stdint.h>
#include <string.h>
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_error.h"
#include "app_timer.h"
#include "ble_stack.h"
#include "ble_app.h"
#include "media_remote.h"
#include "nrf_pwr_mgmt.h"

// nice!nano blue LED on P0.15 (directly connected, active LOW)
#define LED_BLUE       NRF_GPIO_PIN_MAP(0, 15)

/**@brief Function for initializing LEDs. */
static void leds_init(void)
{
    nrf_gpio_cfg_output(LED_BLUE);
    nrf_gpio_pin_set(LED_BLUE);  // LED off (active low)
}

/**@brief Blink LED to show firmware is running. */
static void led_blink(int times)
{
    for (int i = 0; i < times; i++) {
        nrf_gpio_pin_clear(LED_BLUE);  // LED on
        nrf_delay_ms(100);
        nrf_gpio_pin_set(LED_BLUE);    // LED off
        nrf_delay_ms(100);
    }
}

/**@brief Function for application main entry. */
int main(void)
{
    uint32_t err_code;

    // Initialize LED first - blink 1x to show we started
    leds_init();
    led_blink(1);
    nrf_delay_ms(500);

    // Skip logging for now - blink 2x
    led_blink(2);
    nrf_delay_ms(500);

    // Initialize timer module - blink 3x before
    led_blink(3);
    nrf_delay_ms(500);
    err_code = app_timer_init();
    if (err_code != 0) { while(1) { led_blink(10); nrf_delay_ms(1000); } }

    // Initialize BLE stack - blink 4x before
    led_blink(4);
    nrf_delay_ms(500);
    err_code = ble_stack_init();
    if (err_code != 0) { 
        // Blink the sub-step that failed (high nibble of err_code >> 8)
        // 4A=sdh_enable, 4B=cfg_set, 4C=ble_enable, 4D=gatt_init, 4E=mtu, 4F=pwr
        uint8_t substep = (err_code >> 8) & 0x0F;
        while(1) { 
            led_blink(4);  // Step 4 failed
            nrf_delay_ms(300);
            led_blink(substep);  // Sub-step (A=10, B=11, C=12, D=13, E=14, F=15)
            nrf_delay_ms(1000); 
        } 
    }

    // Initialize BLE services - blink 5x before
    led_blink(5);
    nrf_delay_ms(500);
    err_code = ble_app_init();
    if (err_code != 0) { while(1) { led_blink(12); nrf_delay_ms(1000); } }

    // Initialize media remote - blink 6x before
    led_blink(6);
    nrf_delay_ms(500);
    err_code = media_remote_init();
    if (err_code != 0) { while(1) { led_blink(13); nrf_delay_ms(1000); } }

    // Start advertising - blink 7x before
    led_blink(7);
    nrf_delay_ms(500);
    err_code = ble_app_advertising_start();
    if (err_code != 0) { while(1) { led_blink(14); nrf_delay_ms(1000); } }

    // SUCCESS! Blink 8x
    led_blink(8);
    nrf_delay_ms(1000);

    // Enter main loop - LED stays off, device is advertising
    for (;;)
    {
        nrf_pwr_mgmt_run();
    }
}
