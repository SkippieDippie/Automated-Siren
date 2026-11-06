#include "app.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Text helpers                                                        */
/* ------------------------------------------------------------------ */

/* Copy `src` into a 16-char LCD line, padding with spaces. */
static void set_line(char dst[17], const char *src)
{
    size_t n = strlen(src);
    if (n > 16) {
        n = 16;
    }
    memcpy(dst, src, n);
    memset(dst + n, ' ', 16 - n);
    dst[16] = '\0';
}

void app_format_duration(uint32_t ms, char *out)
{
    if (ms == PROGRAM_DURATION_INFINITE) {
        memcpy(out, "--:--", 6);
        return;
    }
    uint32_t s = (ms + 999U) / 1000U;   /* round up: never show 0:00 early */
    if (s < 100U * 60U) {
        snprintf(out, 6, "%2u:%02u", (unsigned)(s / 60U), (unsigned)(s % 60U));
    } else {
        uint32_t h = s / 3600U;
        if (h > 99U) {
            h = 99U;
        }
        snprintf(out, 6, "%2uh%02u", (unsigned)h, (unsigned)((s / 60U) % 60U));
    }
}

static void show_message(app_t *app, const char *l1, const char *l2)
{
    set_line(app->msg[0], l1);
    set_line(app->msg[1], l2);
    app->msg_active = true;
    app->msg_until_ms = port_millis() + APP_MESSAGE_MS;
}

/* ------------------------------------------------------------------ */
/* Program control                                                     */
/* ------------------------------------------------------------------ */

static void stop_program(app_t *app)
{
    player_stop(&app->player);
    app->program_slot = -1;
}

static void start_program(app_t *app, uint8_t slot)
{
    char l1[24];
    char l2[24];
    program_result_t res = { PROG_OK, 0 };

    /* The player references app->program, so stop before reloading it. */
    stop_program(app);
    app->msg_active = false;

    load_status_t st = port_load_program(slot, &app->program, &res);
    app->sd = (st == LOAD_NO_CARD || st == LOAD_NO_FS) ? SD_MISSING : SD_OK;

    snprintf(l1, sizeof l1, "P%u ERROR", (unsigned)(slot + 1));
    switch (st) {
    case LOAD_OK:
        break;
    case LOAD_NO_CARD:
        show_message(app, "NO SD CARD", "Insert card");
        return;
    case LOAD_NO_FS:
        show_message(app, "SD CARD ERROR", "Need FAT/FAT32");
        return;
    case LOAD_NO_FILE:
        snprintf(l2, sizeof l2, "No PROG%u.TXT", (unsigned)(slot + 1));
        show_message(app, l1, l2);
        return;
    case LOAD_TOO_BIG:
        show_message(app, l1, "File too big");
        return;
    case LOAD_PARSE_ERR:
        if (res.line > 0) {
            snprintf(l1, sizeof l1, "P%u LINE %u", (unsigned)(slot + 1), (unsigned)res.line);
        }
        show_message(app, l1, program_err_str(res.code));
        return;
    case LOAD_READ_ERR:
    default:
        show_message(app, l1, "Read error");
        return;
    }

    if (app->program.name[0] == '\0') {
        snprintf(app->program.name, sizeof app->program.name, "Program %u",
                 (unsigned)(slot + 1));
    }
    app->program_slot = (int8_t)slot;
    player_start(&app->player, &app->program, port_millis());
}

/* ------------------------------------------------------------------ */
/* Display                                                             */
/* ------------------------------------------------------------------ */

static void compose_screen(app_t *app, uint32_t now, char out[2][17])
{
    char buf[24];

    if (app->manual_active || app->manual_lockout) {
        set_line(out[0], "MANUAL CONTROL");
        set_line(out[1], app->manual_lockout ? "TIMEOUT-RELEASE" : "SIREN ON");
        return;
    }

    if (app->msg_active) {
        memcpy(out[0], app->msg[0], 17);
        memcpy(out[1], app->msg[1], 17);
        return;
    }

    if (player_state(&app->player) == PLAYER_RUNNING) {
        char step[6];
        char total[6];
        app_format_duration(player_step_remaining_ms(&app->player, now), step);
        app_format_duration(player_total_remaining_ms(&app->player, now), total);

        snprintf(buf, sizeof buf, "P%d %s", app->program_slot + 1, app->program.name);
        set_line(out[0], buf);
        /* "ON   0:05 T 3:25" */
        snprintf(buf, sizeof buf, "%-3s %s T%s",
                 player_siren_on(&app->player) ? "ON" : "OFF", step, total);
        set_line(out[1], buf);
        return;
    }

    set_line(out[0], "SIREN READY");
    switch (app->sd) {
    case SD_OK:      set_line(out[1], "SD OK  P1-P4"); break;
    case SD_MISSING: set_line(out[1], "NO SD CARD"); break;
    default:         set_line(out[1], "SD ?"); break;
    }
}

static void refresh_display(app_t *app, uint32_t now)
{
    char screen[2][17];
    compose_screen(app, now, screen);
    for (uint8_t row = 0; row < 2; row++) {
        if (memcmp(screen[row], app->shown[row], 17) != 0) {
            memcpy(app->shown[row], screen[row], 17);
            port_lcd_line(row, screen[row]);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void app_init(app_t *app)
{
    memset(app, 0, sizeof(*app));
    app->program_slot = -1;
    buttons_init(&app->buttons);
    player_init(&app->player);

    port_siren_set(false);
    app->sd = port_sd_mount() ? SD_OK : SD_MISSING;
    refresh_display(app, port_millis());
}

void app_tick(app_t *app)
{
    uint32_t now = port_millis();

    buttons_update(&app->buttons, port_buttons_raw());

    /* --- Manual control: siren runs while the button is held. --- */
    bool manual_down = buttons_is_down(&app->buttons, BTN_MANUAL);
    if (!manual_down) {
        app->manual_active = false;
        app->manual_lockout = false;
    } else if (!app->manual_lockout) {
        if (!app->manual_active) {
            app->manual_active = true;
            app->manual_start_ms = now;
        }
#if APP_MANUAL_MAX_ON_MS > 0
        if (now - app->manual_start_ms >= APP_MANUAL_MAX_ON_MS) {
            app->manual_active = false;
            app->manual_lockout = true;
        }
#endif
    }

    /* --- Stop button. --- */
    if (buttons_was_pressed(&app->buttons, BTN_STOP)) {
        if (player_state(&app->player) == PLAYER_RUNNING) {
            show_message(app, "STOPPED", app->program.name);
        }
        stop_program(app);
    }

    /* --- Program buttons. --- */
    for (uint8_t i = 0; i < APP_NUM_PROGRAMS; i++) {
        if (!buttons_was_pressed(&app->buttons, (unsigned)(BTN_PROG1 + i))) {
            continue;
        }
#if APP_SAME_BUTTON_STOPS
        if (player_state(&app->player) == PLAYER_RUNNING && app->program_slot == (int8_t)i) {
            show_message(app, "STOPPED", app->program.name);
            stop_program(app);
            break;
        }
#endif
        start_program(app, i);
        now = port_millis();   /* loading from SD takes time */
        break;                 /* one program change per tick */
    }

    /* --- Advance the running program. --- */
    player_tick(&app->player, now);
    if (player_state(&app->player) == PLAYER_FINISHED) {
        char l1[24];
        snprintf(l1, sizeof l1, "P%d DONE", app->program_slot + 1);
        show_message(app, l1, app->program.name);
        player_clear(&app->player);
        app->program_slot = -1;
    }

    /* --- Output: manual overrides (ORs with) the program. --- */
    /* Written every tick (idempotent) so a glitched output self-heals. */
    app->siren_on = app->manual_active || player_siren_on(&app->player);
    port_siren_set(app->siren_on);

    if (app->msg_active && (int32_t)(now - app->msg_until_ms) >= 0) {
        app->msg_active = false;
    }
    refresh_display(app, now);
}
