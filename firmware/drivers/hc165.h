/*
 * 74HC165 parallel-in / serial-out shift register chain (button inputs).
 *
 * Bit-banged on three GPIOs (load, clock, data). Chip 0 is the one whose
 * QH output goes to the MCU; further chips feed into the previous chip's
 * SER input. Input A of chip 0 is bit 0 of the result, H of chip 0 is
 * bit 7, A of chip 1 is bit 8, and so on.
 */
#ifndef HC165_H
#define HC165_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f1xx_hal.h"

typedef struct {
    GPIO_TypeDef *load_port;   /* SH/LD (active-low parallel load) */
    uint16_t      load_pin;
    GPIO_TypeDef *clk_port;    /* CLK                              */
    uint16_t      clk_pin;
    GPIO_TypeDef *data_port;   /* QH of chip 0                     */
    uint16_t      data_pin;
    uint8_t       chips;       /* 1..4                             */
} hc165_t;

void hc165_init(const hc165_t *hc);

/* Shift in all inputs. Returns the raw levels (bit set = input high). */
uint32_t hc165_read(const hc165_t *hc);

#endif /* HC165_H */
