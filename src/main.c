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

#define LED_1          NRF_GPIO_PIN_MAP(0, 13)  // LED1 on nRF52840 DK
#define LED_2          NRF_GPIO_PIN_MAP(0, 14)  // LED2 on nRF52840 DK
#define LED_3          NRF_GPIO_PIN_MAP(0, 15)  // LED3 on nRF52840 DK
#define LED_4          NRF_GPIO_PIN_MAP(0, 16)  // LED4 on nRF52840 DK

/**@brief Function for initializing LEDs. */
static void leds_init(void)
{
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_cfg_output(LED_2);
    nrf_gpio_cfg_output(LED_3);
    nrf_gpio_cfg_output(LED_4);
    
    nrf_gpio_pin_clear(LED_1);
    nrf_gpio_pin_clear(LED_2);
    nrf_gpio_pin_clear(LED_3);
    nrf_gpio_pin_clear(LED_4);
}

/**@brief Function for application main entry. */
int main(void)
{
    uint32_t err_code;

    // Initialize log
    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("BLE Media Remote starting...");

    // Initialize LEDs
    leds_init();
    nrf_gpio_pin_set(LED_1);  // Indicate startup

    // Initialize timer module
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // Initialize BLE stack
    err_code = ble_stack_init();
    APP_ERROR_CHECK(err_code);

    // Initialize BLE services
    err_code = ble_app_init();
    APP_ERROR_CHECK(err_code);

    // Initialize media remote
    err_code = media_remote_init();
    APP_ERROR_CHECK(err_code);

    // Start advertising
    err_code = ble_app_advertising_start();
    APP_ERROR_CHECK(err_code);

    NRF_LOG_INFO("BLE Media Remote ready");

    // Turn off startup LED
    nrf_gpio_pin_clear(LED_1);

    // Enter main loop
    for (;;)
    {
        // Process BLE events
        if (NRF_LOG_PROCESS() == false)
        {
            power_manage();
        }
    }
}
