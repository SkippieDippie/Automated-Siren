/*
 * End-to-end tests of the control logic against a fake hardware port.
 */
#include "test.h"
#include "app.h"

/* ---------------- fake port ---------------- */

static uint32_t    f_now;
static uint32_t    f_buttons;
static bool        f_siren;
static char        f_lcd[2][17];
static bool        f_card;
static const char *f_files[APP_NUM_PROGRAMS];
static uint32_t    f_load_delay_ms;

uint32_t port_millis(void)        { return f_now; }
uint32_t port_buttons_raw(void)   { return f_buttons; }
void port_siren_set(bool on)      { f_siren = on; }
void port_lcd_line(uint8_t row, const char *text) { memcpy(f_lcd[row], text, 17); }
bool port_sd_mount(void)          { return f_card; }

load_status_t port_load_program(uint8_t slot, program_t *out, program_result_t *res)
{
    f_now += f_load_delay_ms;
    if (!f_card) {
        return LOAD_NO_CARD;
    }
    if (!f_files[slot]) {
        return LOAD_NO_FILE;
    }
    *res = program_parse(f_files[slot], out);
    return res->code == PROG_OK ? LOAD_OK : LOAD_PARSE_ERR;
}

/* ---------------- helpers ---------------- */

static app_t app;

static void reset(void)
{
    f_now = 5000;
    f_buttons = 0;
    f_siren = false;
    f_card = true;
    f_load_delay_ms = 0;
    memset(f_files, 0, sizeof f_files);
    memset(f_lcd, 0, sizeof f_lcd);
    app_init(&app);
}

/* Run the app for `ms` milliseconds of simulated time. */
static void run(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += APP_TICK_MS) {
        f_now += APP_TICK_MS;
        app_tick(&app);
    }
}

static void press(unsigned id)
{
    f_buttons |= 1UL << id;
    run(50);
    f_buttons &= ~(1UL << id);
    run(50);
}

/* ---------------- tests ---------------- */

static void test_idle_screen(void)
{
    reset();
    run(20);
    CHECK_STR(f_lcd[0], "SIREN READY     ");
    CHECK_STR(f_lcd[1], "SD OK  P1-P6    ");
    CHECK(!f_siren);

    f_card = false;
    app_init(&app);
    CHECK_STR(f_lcd[1], "NO SD CARD      ");
}

static void test_manual(void)
{
    reset();
    f_buttons = 1U << BTN_MANUAL;
    run(20);
    CHECK(!f_siren);              /* still debouncing */
    run(30);
    CHECK(f_siren);
    CHECK_STR(f_lcd[0], "MANUAL CONTROL  ");
    CHECK_STR(f_lcd[1], "SIREN ON        ");
    run(5000);
    CHECK(f_siren);
    f_buttons = 0;
    run(50);
    CHECK(!f_siren);
    CHECK_STR(f_lcd[0], "SIREN READY     ");
}

static void test_manual_ignores_bounce(void)
{
    reset();
    for (int i = 0; i < 20; i++) {   /* contact bounce: 10 ms on/off */
        f_buttons = (i & 1) ? 0 : (1U << BTN_MANUAL);
        run(10);
        CHECK(!f_siren);
    }
}

static void test_manual_timeout(void)
{
    reset();
    f_buttons = 1U << BTN_MANUAL;
    run(APP_MANUAL_MAX_ON_MS - 100);
    CHECK(f_siren);
    run(200);
    CHECK(!f_siren);
    CHECK_STR(f_lcd[1], "TIMEOUT-RELEASE ");
    run(10000);
    CHECK(!f_siren);              /* stays off while still held */
    f_buttons = 0;
    run(100);
    f_buttons = 1U << BTN_MANUAL; /* re-press works again */
    run(100);
    CHECK(f_siren);
}

static void test_program_runs_to_completion(void)
{
    reset();
    f_files[1] = "NAME Alert\nON 2\nOFF 1\nON 1\n";
    press(BTN_PROG1 + 1);
    CHECK(f_siren);
    CHECK_STR(f_lcd[0], "P2 Alert        ");
    CHECK_STR(f_lcd[1], "ON   0:02 T 0:04");

    run(1950);                     /* now in OFF step */
    CHECK(!f_siren);
    CHECK_STR(f_lcd[1], "OFF  0:01 T 0:02");

    run(2000);                     /* finished */
    CHECK(!f_siren);
    CHECK_STR(f_lcd[0], "P2 DONE         ");
    CHECK_STR(f_lcd[1], "Alert           ");

    run(APP_MESSAGE_MS);
    CHECK_STR(f_lcd[0], "SIREN READY     ");
}

static void test_default_name(void)
{
    reset();
    f_files[0] = "ON 5\n";
    press(BTN_PROG1);
    CHECK_STR(f_lcd[0], "P1 Program 1    ");
}

static void test_stop_and_toggle(void)
{
    reset();
    f_files[0] = "NAME Long\nON 60\n";
    press(BTN_PROG1);
    CHECK(f_siren);
    press(BTN_STOP);
    CHECK(!f_siren);
    CHECK_STR(f_lcd[0], "STOPPED         ");

    press(BTN_PROG1);
    CHECK(f_siren);
    press(BTN_PROG1);             /* same button again stops it */
    CHECK(!f_siren);
    CHECK_STR(f_lcd[0], "STOPPED         ");
}

static void test_switch_program(void)
{
    reset();
    f_files[0] = "NAME One\nON 60\n";
    f_files[2] = "NAME Three\nOFF 60\n";
    press(BTN_PROG1);
    CHECK(f_siren);
    press(BTN_PROG1 + 2);
    CHECK(!f_siren);
    CHECK_STR(f_lcd[0], "P3 Three        ");
}

static void test_manual_overrides_program(void)
{
    reset();
    f_files[0] = "NAME Quiet\nOFF 60\n";
    press(BTN_PROG1);
    CHECK(!f_siren);
    f_buttons = 1U << BTN_MANUAL;
    run(50);
    CHECK(f_siren);
    CHECK_STR(f_lcd[0], "MANUAL CONTROL  ");
    f_buttons = 0;
    run(50);
    CHECK(!f_siren);
    CHECK_STR(f_lcd[0], "P1 Quiet        ");   /* program kept running */
}

static void test_errors_shown(void)
{
    reset();
    press(BTN_PROG1 + 3);
    CHECK_STR(f_lcd[0], "P4 ERROR        ");
    CHECK_STR(f_lcd[1], "No PROG4.TXT    ");
    CHECK(!f_siren);

    f_files[1] = "ON 1\nFOO\n";
    press(BTN_PROG1 + 1);
    CHECK_STR(f_lcd[0], "P2 LINE 2       ");
    CHECK_STR(f_lcd[1], "Bad command     ");

    f_card = false;
    press(BTN_PROG1);
    CHECK_STR(f_lcd[0], "NO SD CARD      ");
    run(APP_MESSAGE_MS);
    CHECK_STR(f_lcd[1], "NO SD CARD      ");
}

static void test_timing_accounts_for_load_delay(void)
{
    reset();
    f_files[0] = "ON 1\nOFF 1\n";
    f_load_delay_ms = 300;        /* slow SD card */
    press(BTN_PROG1);             /* press() runs 100 ms after the load */
    run(850);                     /* ~0.95 s since the program started */
    CHECK(f_siren);
    run(100);
    CHECK(!f_siren);
}

static void test_forever_display(void)
{
    reset();
    f_files[0] = "NAME Loop\nREPEAT FOREVER\nON 3\nOFF 2\nEND\n";
    press(BTN_PROG1);
    CHECK_STR(f_lcd[1], "ON   0:03 T--:--");
    run(60000);
    CHECK_EQ(player_state(&app.player), PLAYER_RUNNING);
}

static void test_button_decode(void)
{
    bool fault;

    /* One 165, nothing pressed: all inputs pulled high. */
    CHECK_EQ(buttons_decode_active_low(0xFFu, 8, &fault), 0);
    CHECK(!fault);

    /* MANUAL (input A, bit 0) and PROG3 (input E, bit 4) pressed. */
    CHECK_EQ(buttons_decode_active_low(0xFFu & ~0x11u, 8, &fault), 0x11);
    CHECK(!fault);

    /* Bits above the chain are ignored. */
    CHECK_EQ(buttons_decode_active_low(0x000000FEu, 8, &fault), 0x01);

    /* Stuck-low data line: everything reads pressed -> fault, nothing pressed. */
    CHECK_EQ(buttons_decode_active_low(0x00u, 8, &fault), 0);
    CHECK(fault);
    CHECK_EQ(buttons_decode_active_low(0x00u, 32, &fault), 0);
    CHECK(fault);

    /* Two chips, PROG14 = chip 1 input H = bit 15. */
    CHECK_EQ(buttons_decode_active_low(0x7FFFu, 16, &fault), 0x8000);
    CHECK(!fault);
}

static void test_stuck_data_line_keeps_siren_off(void)
{
    bool fault;
    reset();
    /* Simulate the port with a stuck-low 165 data line for 1 second. */
    f_buttons = buttons_decode_active_low(0x00u, 8U * APP_BUTTON_CHIPS, &fault);
    run(1000);
    CHECK(!f_siren);
    CHECK_STR(f_lcd[0], "SIREN READY     ");
}

static void test_last_program_button(void)
{
    reset();
    f_files[APP_NUM_PROGRAMS - 1] = "NAME Last\nON 5\n";
    press(BTN_PROG1 + APP_NUM_PROGRAMS - 1);
    CHECK(f_siren);
    CHECK_STR(f_lcd[0], "P6 Last         ");
}

static void test_format_duration(void)
{
    char b[6];
    app_format_duration(0, b);          CHECK_STR(b, " 0:00");
    app_format_duration(1, b);          CHECK_STR(b, " 0:01");
    app_format_duration(59999, b);      CHECK_STR(b, " 1:00");
    app_format_duration(605000, b);     CHECK_STR(b, "10:05");
    app_format_duration(5999000, b);    CHECK_STR(b, "99:59");
    app_format_duration(6000000, b);    CHECK_STR(b, " 1h40");
    app_format_duration(86400000, b);   CHECK_STR(b, "24h00");
    app_format_duration(PROGRAM_DURATION_INFINITE, b); CHECK_STR(b, "--:--");
}

void test_app_all(void)
{
    RUN(test_idle_screen);
    RUN(test_manual);
    RUN(test_manual_ignores_bounce);
    RUN(test_manual_timeout);
    RUN(test_program_runs_to_completion);
    RUN(test_default_name);
    RUN(test_stop_and_toggle);
    RUN(test_switch_program);
    RUN(test_manual_overrides_program);
    RUN(test_errors_shown);
    RUN(test_timing_accounts_for_load_delay);
    RUN(test_forever_display);
    RUN(test_format_duration);
    RUN(test_button_decode);
    RUN(test_stuck_data_line_keeps_siren_off);
    RUN(test_last_program_button);
}
