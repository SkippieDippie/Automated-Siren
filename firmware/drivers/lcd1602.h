/*
 * HD44780 16x2 character LCD behind a PCF8574 I2C expander
 * (the common "LCD1602 I2C" module).
 */
#ifndef LCD1602_H
#define LCD1602_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

/* Probes the given 7-bit addresses, initialises the display.
 * Returns false if no backpack answered. */
bool lcd_init(I2C_HandleTypeDef *i2c, const uint8_t *addrs, uint8_t n_addrs);

void lcd_clear(void);
void lcd_backlight(bool on);

/* Write exactly 16 characters to row 0 or 1. */
void lcd_write_line(uint8_t row, const char *text);

#endif /* LCD1602_H */
