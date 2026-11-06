#include "test.h"
#include "player.h"

static program_t p;
static player_t pl;

static void load(const char *text)
{
    program_result_t r = program_parse(text, &p);
    CHECK_EQ(r.code, PROG_OK);
}

/* Record the siren state at every 100 ms boundary into a string of
 * '1'/'0' characters, from t0 until the player stops. */
static void trace(uint32_t t0, char *out, size_t max)
{
    size_t n = 0;
    for (uint32_t t = t0; n + 1 < max; t += 100) {
        player_tick(&pl, t);
        if (player_state(&pl) != PLAYER_RUNNING) {
            break;
        }
        out[n++] = player_siren_on(&pl) ? '1' : '0';
    }
    out[n] = '\0';
}

static void test_sequence(void)
{
    char buf[128];
    load("ON 0.3\nOFF 0.2\nON 0.1\n");
    player_start(&pl, &p, 1000);
    trace(1000, buf, sizeof buf);
    CHECK_STR(buf, "111001");
    CHECK_EQ(player_state(&pl), PLAYER_FINISHED);
    CHECK(!player_siren_on(&pl));
    player_clear(&pl);
    CHECK_EQ(player_state(&pl), PLAYER_IDLE);
}

static void test_nested_repeat(void)
{
    char buf[128];
    load("REPEAT 2\nON 0.1\nREPEAT 2\nOFF 0.1\nON 0.1\nEND\nEND\nOFF 0.1\n");
    player_start(&pl, &p, 0);
    trace(0, buf, sizeof buf);
    CHECK_STR(buf, "10101101010");
}

static void test_forever_loops(void)
{
    char buf[64];
    load("REPEAT FOREVER\nON 0.2\nOFF 0.1\nEND\n");
    player_start(&pl, &p, 0);
    trace(0, buf, 31);
    CHECK_STR(buf, "110110110110110110110110110110");
    CHECK_EQ(player_state(&pl), PLAYER_RUNNING);
    CHECK_EQ(player_total_remaining_ms(&pl, 3000), PROGRAM_DURATION_INFINITE);
}

static void test_remaining_and_late_ticks(void)
{
    load("ON 10\nOFF 5\nON 5\n");
    player_start(&pl, &p, 0);
    CHECK_EQ(player_total_remaining_ms(&pl, 0), 20000);
    CHECK_EQ(player_step_remaining_ms(&pl, 4000), 6000);
    CHECK_EQ(player_total_remaining_ms(&pl, 4000), 16000);

    /* A very late tick skips over a whole step without drifting. */
    player_tick(&pl, 16000);
    CHECK(player_siren_on(&pl));
    CHECK_EQ(player_step_remaining_ms(&pl, 16000), 4000);
    CHECK_EQ(player_total_remaining_ms(&pl, 16000), 4000);

    player_tick(&pl, 20000);
    CHECK_EQ(player_state(&pl), PLAYER_FINISHED);
}

static void test_tick_wraparound(void)
{
    load("ON 2\nOFF 2\n");
    uint32_t t0 = 0xFFFFFFFFu - 1000u;   /* wraps 1 s into the program */
    player_start(&pl, &p, t0);
    player_tick(&pl, t0 + 1500);
    CHECK(player_siren_on(&pl));
    player_tick(&pl, t0 + 2500);
    CHECK(!player_siren_on(&pl));
    CHECK_EQ(player_state(&pl), PLAYER_RUNNING);
    CHECK_EQ(player_total_remaining_ms(&pl, t0 + 2500), 1500);
    player_tick(&pl, t0 + 4000);
    CHECK_EQ(player_state(&pl), PLAYER_FINISHED);
}

static void test_stop(void)
{
    load("ON 10\n");
    player_start(&pl, &p, 0);
    CHECK(player_siren_on(&pl));
    player_stop(&pl);
    CHECK(!player_siren_on(&pl));
    CHECK_EQ(player_state(&pl), PLAYER_IDLE);
}

void test_player_all(void)
{
    RUN(test_sequence);
    RUN(test_nested_repeat);
    RUN(test_forever_loops);
    RUN(test_remaining_and_late_ticks);
    RUN(test_tick_wraparound);
    RUN(test_stop);
}
