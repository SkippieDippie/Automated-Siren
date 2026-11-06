/*
 * Minimal blocking transmit-only log on USART1 (TX = PA9), 115200 8N1.
 * Used by the bring-up firmware; connect a 3.3 V USB-serial adapter's RX
 * to PA9 and GND to GND.
 */
#ifndef UART_LOG_H
#define UART_LOG_H

#include <stdint.h>

void uart_log_init(uint32_t baud);

/* printf-style, lines end with "\r\n" automatically. Max 160 chars. */
void uart_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* UART_LOG_H */
