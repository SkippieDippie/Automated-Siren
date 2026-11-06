/*
 * MCU-level setup shared by the main firmware and the bring-up firmware.
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdbool.h>

typedef enum {
    CLOCK_HSE_72MHZ = 0,   /* 8 MHz crystal x9                 */
    CLOCK_HSI_64MHZ        /* crystal failed: internal fallback */
} clock_source_t;

/* Siren output (forced off), status LED and GPIO clocks. Call first. */
void system_gpio_init(void);

/* Configure the system clock; falls back to HSI if the crystal fails. */
clock_source_t system_clock_init(void);

/* Fatal error: siren off, LED blinks fast, never returns. */
void Error_Handler(void);

#endif /* SYSTEM_H */
