/*
 * Siren control panel application logic (portable).
 *
 * Call app_init() once, then app_tick() every APP_TICK_MS milliseconds.
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"
#include "app_port.h"
#include "buttons.h"
#include "player.h"
#include "program.h"

typedef enum {
    SD_UNKNOWN = 0,
    SD_OK,
    SD_MISSING
} sd_state_t;

typedef struct {
    buttons_t  buttons;
    player_t   player;
    program_t  program;
    int8_t     program_slot;      /* slot of loaded program, -1 = none   */

    bool       manual_active;     /* manual button currently driving siren */
    bool       manual_lockout;    /* thermal timeout, wait for release     */
    uint32_t   manual_start_ms;

    sd_state_t sd;
    bool       siren_on;

    /* Temporary message (DONE / errors) shown instead of the idle screen. */
    char       msg[2][17];
    uint32_t   msg_until_ms;
    bool       msg_active;

    char       shown[2][17];      /* what the LCD currently displays */
} app_t;

void app_init(app_t *app);
void app_tick(app_t *app);

/* Format a duration as 5 characters: " m:ss", "mm:ss" or "hhhmm" style
 * ("12h05"). Rounds up to whole seconds. `out` must hold 6 bytes. */
void app_format_duration(uint32_t ms, char *out);

#endif /* APP_H */
