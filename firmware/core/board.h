/*
 * Board pin mapping - STM32F103C8T6 "Blue Pill" (8 MHz HSE crystal).
 * See docs/hardware.md for wiring.
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

/* ---- Buttons: 74HC165 shift register chain ----
 * Buttons connect each 165 input to GND, with a 10k pull-up per input.
 * Input mapping (chip 0 = the one whose QH goes to the MCU):
 *   chip 0: A = MANUAL, B = STOP, C..H = PROG1..PROG6
 *   chip 1: A..H = PROG7..PROG14, and so on.
 * Number of chips: APP_BUTTON_CHIPS in firmware/app/app_config.h. */
#define BTN_LOAD_PORT       GPIOA           /* 165 pin 1  SH/LD  */
#define BTN_LOAD_PIN        GPIO_PIN_0
#define BTN_CLK_PORT        GPIOA           /* 165 pin 2  CLK    */
#define BTN_CLK_PIN         GPIO_PIN_1
#define BTN_DATA_PORT       GPIOA           /* 165 pin 9  QH     */
#define BTN_DATA_PIN        GPIO_PIN_2

/* ---- LCD 1602, 4-bit parallel. LCD RW (pin 5) tied to GND. ---- */
#define LCD_PINS {                         \
    .rs = { GPIOB, GPIO_PIN_10 },          \
    .e  = { GPIOB, GPIO_PIN_11 },          \
    .d  = { { GPIOB, GPIO_PIN_12 },  /* D4 */ \
            { GPIOB, GPIO_PIN_13 },  /* D5 */ \
            { GPIOB, GPIO_PIN_14 },  /* D6 */ \
            { GPIOB, GPIO_PIN_15 } } /* D7 */ \
}

/* ---- SD card on SPI1 ---- */
#define SD_SPI              SPI1
#define SD_CS_PORT          GPIOA
#define SD_CS_PIN           GPIO_PIN_4      /* SCK=PA5 MISO=PA6 MOSI=PA7 */
#define SD_SPI_SLOW_PRESC   SPI_BAUDRATEPRESCALER_256   /* 281 kHz for init */
#define SD_SPI_FAST_PRESC   SPI_BAUDRATEPRESCALER_8     /* 9 MHz           */

/* ---- Independent watchdog: resets the MCU (siren off) if firmware hangs ---- */
#define WATCHDOG_TIMEOUT_MS 2000

#endif /* BOARD_H */
