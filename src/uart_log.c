/**
 * Logging via hardware UART0 (blocking TX only)
 */
#include "uart_log.h"

#if APP_UART_LOG_ENABLED

#include "nrf_uart.h"
#include "nrf_gpio.h"
#include <stdio.h>

// TX on P0.17 (adjust if you prefer another pin)
#define UART_TX_PIN NRF_GPIO_PIN_MAP(0, 17)

static void uart_send_byte(uint8_t byte)
{
    nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
    nrf_uart_txd_set(NRF_UART0, byte);
    while (!nrf_uart_event_check(NRF_UART0, NRF_UART_EVENT_TXDRDY)) {
        /* wait for byte sent */
    }
}

void uart_log_init(void)
{
    nrf_uart_disable(NRF_UART0);

    // Configure TX pin
    nrf_gpio_cfg_output(UART_TX_PIN);
    NRF_UART0->PSELTXD = UART_TX_PIN;
    NRF_UART0->PSELRXD = NRF_UART_PSEL_DISCONNECTED;

    // Configure UART: no parity, no flow control, 9600 baud
    nrf_uart_configure(NRF_UART0, NRF_UART_PARITY_EXCLUDED, NRF_UART_HWFC_DISABLED);
    nrf_uart_baudrate_set(NRF_UART0, NRF_UART_BAUDRATE_9600);

    nrf_uart_enable(NRF_UART0);
    nrf_uart_task_trigger(NRF_UART0, NRF_UART_TASK_STARTTX);

    // Clear TXDRDY and ensure ready
    nrf_uart_event_clear(NRF_UART0, NRF_UART_EVENT_TXDRDY);
    uart_send_byte(0);

    uart_log("\r\n\r\n=== UART Log Started (9600 8N1) ===\r\n");
}

void uart_log(const char* str)
{
    if (!str) return;
    while (*str) {
        uart_send_byte((uint8_t)*str++);
    }
}

void uart_log_hex(const char* str, uint32_t val)
{
    if (str) {
        uart_log(str);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08lX\r\n", (unsigned long)val);
    uart_log(buf);
}

#else

void uart_log_init(void) {}
void uart_log(const char * str) { (void)str; }
void uart_log_hex(const char * prefix, uint32_t val) { (void)prefix; (void)val; }

#endif
