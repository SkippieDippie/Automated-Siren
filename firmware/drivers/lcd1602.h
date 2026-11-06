/*
 * HD44780-compatible 16x2 character LCD driven directly in 4-bit mode.
 *
 * Six GPIOs: RS, E, D4..D7. The LCD's RW pin must be tied to GND
 * (write-only), so the display never drives the MCU pins and the busy
 * flag is never read; fixed delays are used instead.
 */
#ifndef LCD1602_H
#define LCD1602_H

#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} lcd_pin_t;

typedef struct {
    lcd_pin_t rs;
    lcd_pin_t e;
    lcd_pin_t d[4];   /* D4, D5, D6, D7 */
} lcd_pins_t;

void lcd_init(const lcd_pins_t *pins);
void lcd_clear(void);

/* Write up to 16 characters to row 0 or 1 (padded with spaces). */
void lcd_write_line(uint8_t row, const char *text);

#endif /* LCD1602_H */
