/*
 * Hardware interface required by the application.
 *
 * Implemented by firmware/core/port_stm32.c on the target and by fakes in
 * tests/ on the host.
 */
#ifndef APP_PORT_H
#define APP_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "program.h"

/* Logical button ids (bit positions in port_buttons_raw()). */
enum {
    BTN_MANUAL = 0,
    BTN_STOP   = 1,
    BTN_PROG1  = 2   /* BTN_PROG1 + n for program n (0-based) */
};

typedef enum {
    LOAD_OK = 0,
    LOAD_NO_CARD,     /* card missing or not responding     */
    LOAD_NO_FS,       /* no FAT filesystem on the card      */
    LOAD_NO_FILE,     /* PROG<N>.TXT not found              */
    LOAD_READ_ERR,
    LOAD_TOO_BIG,     /* file larger than the text buffer   */
    LOAD_PARSE_ERR    /* see program_result_t for details   */
} load_status_t;

uint32_t port_millis(void);

/* Bitmask of currently pressed buttons (raw, not debounced). */
uint32_t port_buttons_raw(void);

/* Energise / de-energise the siren motor. */
void port_siren_set(bool on);

/* Write one 16-character LCD line (row 0 or 1). `text` is exactly 16
 * characters plus NUL, already padded. */
void port_lcd_line(uint8_t row, const char *text);

/* Try to mount the SD card; true if a readable FAT volume is present. */
bool port_sd_mount(void);

/* Load and parse program `slot` (0-based) from the SD card. */
load_status_t port_load_program(uint8_t slot, program_t *out, program_result_t *res);

#endif /* APP_PORT_H */
