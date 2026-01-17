/**
 * Minimal blink test for nice!nano - NO BLE
 * Just blinks to verify firmware flashing works.
 */

#include <stdint.h>
#include "nrf_delay.h"
#include "nrf_gpio.h"

// nice!nano blue LED on P0.15 (active LOW)
#define LED_BLUE  NRF_GPIO_PIN_MAP(0, 15)

int main(void)
{
    // Configure LED pin as output
    nrf_gpio_cfg_output(LED_BLUE);
    
    // Infinite blink loop - 1 second on, 1 second off
    while (1)
    {
        nrf_gpio_pin_clear(LED_BLUE);  // LED on
        nrf_delay_ms(500);
        nrf_gpio_pin_set(LED_BLUE);    // LED off
        nrf_delay_ms(500);
    }
}

