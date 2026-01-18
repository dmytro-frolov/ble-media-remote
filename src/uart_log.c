/**
 * Simple UART logging - bit-banged for simplicity
 */
#include "uart_log.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"

// TX pin - use P0.17
#define UART_TX_PIN    NRF_GPIO_PIN_MAP(0, 17)

// 9600 baud = ~104us per bit (more reliable for bit-banging)
#define BIT_DELAY_US   104

static void uart_send_byte(uint8_t byte)
{
    // Start bit (low)
    nrf_gpio_pin_clear(UART_TX_PIN);
    nrf_delay_us(BIT_DELAY_US);
    
    // 8 data bits, LSB first
    for (int i = 0; i < 8; i++) {
        if (byte & (1 << i)) {
            nrf_gpio_pin_set(UART_TX_PIN);
        } else {
            nrf_gpio_pin_clear(UART_TX_PIN);
        }
        nrf_delay_us(BIT_DELAY_US);
    }
    
    // Stop bit (high)
    nrf_gpio_pin_set(UART_TX_PIN);
    nrf_delay_us(BIT_DELAY_US);
}

void uart_log_init(void)
{
    nrf_gpio_cfg_output(UART_TX_PIN);
    nrf_gpio_pin_set(UART_TX_PIN);  // Idle high
    nrf_delay_ms(10);
    uart_log("\r\n\r\n=== UART Log Started ===\r\n");
}

void uart_log(const char* str)
{
    while (*str) {
        uart_send_byte(*str++);
    }
}

static char hex_char(uint8_t nibble)
{
    if (nibble < 10) return '0' + nibble;
    return 'A' + (nibble - 10);
}

void uart_log_hex(const char* str, uint32_t val)
{
    uart_log(str);
    uart_log("0x");
    
    // Print 8 hex digits
    for (int i = 7; i >= 0; i--) {
        uart_send_byte(hex_char((val >> (i * 4)) & 0x0F));
    }
    uart_log("\r\n");
}

