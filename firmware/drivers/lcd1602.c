#include "lcd1602.h"

#include <stdbool.h>

/* HD44780 timing (worst case, at the slowest 190 kHz internal clock):
 * most instructions 37 us -> use 50 us; clear/home 1.52 ms -> use 2 ms. */
#define LCD_CMD_US    50U
#define LCD_CLEAR_MS  2U

static const lcd_pins_t *s_pins;

/* ------------------------------------------------------------------ */
/* Microsecond delay using the Cortex-M3 DWT cycle counter              */
/* ------------------------------------------------------------------ */

static void delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while (DWT->CYCCNT - start < cycles) {
    }
}

/* ------------------------------------------------------------------ */

static void pin_write(const lcd_pin_t *p, bool high)
{
    HAL_GPIO_WritePin(p->port, p->pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Put 4 bits on D4..D7 and pulse E (data latched on the falling edge). */
static void write_nibble(uint8_t nibble)
{
    for (uint8_t i = 0; i < 4; i++) {
        pin_write(&s_pins->d[i], (nibble >> i) & 1U);
    }
    pin_write(&s_pins->e, true);
    delay_us(1);                  /* E high >= 450 ns */
    pin_write(&s_pins->e, false);
    delay_us(1);                  /* E cycle >= 1 us  */
}

static void write_byte(uint8_t value, bool is_data)
{
    pin_write(&s_pins->rs, is_data);
    write_nibble((uint8_t)(value >> 4));
    write_nibble((uint8_t)(value & 0x0FU));
    delay_us(LCD_CMD_US);
}

void lcd_init(const lcd_pins_t *pins)
{
    GPIO_InitTypeDef g = {0};

    s_pins = pins;
    delay_init();

    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    const lcd_pin_t *all[6] = { &pins->rs, &pins->e, &pins->d[0],
                                &pins->d[1], &pins->d[2], &pins->d[3] };
    for (uint8_t i = 0; i < 6; i++) {
        HAL_GPIO_WritePin(all[i]->port, all[i]->pin, GPIO_PIN_RESET);
        g.Pin = all[i]->pin;
        HAL_GPIO_Init(all[i]->port, &g);
    }

    /* Reset by instruction (HD44780 datasheet, figure 24): works whatever
     * state the controller was left in, e.g. after an MCU-only reset. */
    HAL_Delay(50);                /* > 40 ms after Vcc rises to 2.7 V */
    write_nibble(0x3);
    HAL_Delay(5);                 /* > 4.1 ms */
    write_nibble(0x3);
    delay_us(150);                /* > 100 us */
    write_nibble(0x3);
    delay_us(LCD_CMD_US);
    write_nibble(0x2);            /* switch to 4-bit interface */
    delay_us(LCD_CMD_US);

    write_byte(0x28, false);      /* 4-bit, 2 lines, 5x8 font */
    write_byte(0x08, false);      /* display off              */
    lcd_clear();
    write_byte(0x06, false);      /* entry mode: increment    */
    write_byte(0x0C, false);      /* display on, no cursor    */
}

void lcd_clear(void)
{
    write_byte(0x01, false);
    HAL_Delay(LCD_CLEAR_MS);
}

void lcd_write_line(uint8_t row, const char *text)
{
    /* ~17 x 55 us = under 1 ms per line. */
    write_byte((uint8_t)(0x80U | (row ? 0x40U : 0x00U)), false);
    bool ended = false;
    for (uint8_t i = 0; i < 16; i++) {
        ended = ended || text[i] == '\0';
        write_byte((uint8_t)(ended ? ' ' : text[i]), true);
    }
}
