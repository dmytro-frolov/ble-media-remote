/**
 * Simple UART logging for nice!nano debug
 * TX pin: P0.06 (connect to USB-to-serial RX)
 * Baud: 115200 (bit-banged)
 */

#ifndef UART_LOG_H
#define UART_LOG_H

#include <stdint.h>

#ifndef APP_UART_LOG_ENABLED
#define APP_UART_LOG_ENABLED 1
#endif

void uart_log_init(void);
void uart_log(const char *str);
void uart_log_hex(const char *prefix, uint32_t val);

#endif // UART_LOG_H
