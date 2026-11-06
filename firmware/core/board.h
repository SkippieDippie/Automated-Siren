/*
 * Board pin mapping - STM32F103C8T6 "Blue Pill" (8 MHz HSE crystal).
 * See docs/hardware.md for wiring.
 *
 * All buttons are wired between the pin and GND (internal pull-ups),
 * i.e. active-low.
 */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx_hal.h"

/* ---- Siren output (drives relay / SSR / contactor coil driver) ---- */
#define SIREN_PORT          GPIOB
#define SIREN_PIN           GPIO_PIN_1
#define SIREN_ACTIVE_HIGH   1

/* ---- Status LED (on-board, active-low) ---- */
#define LED_PORT            GPIOC
#define LED_PIN             GPIO_PIN_13

/* ---- Buttons (active-low, internal pull-up) ---- */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} board_pin_t;

/* Order must match the logical ids in app_port.h:
 * MANUAL, STOP, PROG1..PROGn */
#define BOARD_BUTTONS {            \
    { GPIOA, GPIO_PIN_0 },  /* MANUAL */ \
    { GPIOA, GPIO_PIN_1 },  /* STOP   */ \
    { GPIOB, GPIO_PIN_12 }, /* PROG 1 */ \
    { GPIOB, GPIO_PIN_13 }, /* PROG 2 */ \
    { GPIOB, GPIO_PIN_14 }, /* PROG 3 */ \
    { GPIOB, GPIO_PIN_15 }, /* PROG 4 */ \
}

/* ---- SD card on SPI1 ---- */
#define SD_SPI              SPI1
#define SD_CS_PORT          GPIOA
#define SD_CS_PIN           GPIO_PIN_4      /* SCK=PA5 MISO=PA6 MOSI=PA7 */
#define SD_SPI_SLOW_PRESC   SPI_BAUDRATEPRESCALER_256   /* 281 kHz for init */
#define SD_SPI_FAST_PRESC   SPI_BAUDRATEPRESCALER_8     /* 9 MHz           */

/* ---- LCD 1602 with PCF8574 I2C backpack on I2C1 (SCL=PB6 SDA=PB7) ---- */
#define LCD_I2C             I2C1
#define LCD_I2C_SPEED_HZ    100000
/* 7-bit addresses tried in order: PCF8574 (0x27) and PCF8574A (0x3F). */
#define LCD_I2C_ADDRS       { 0x27, 0x3F }

/* ---- Independent watchdog: resets the MCU (siren off) if firmware hangs ---- */
#define WATCHDOG_TIMEOUT_MS 2000

#endif /* BOARD_H */
