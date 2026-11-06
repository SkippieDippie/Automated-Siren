#include "test.h"
#include "program.h"

static program_t p;

static void test_basic(void)
{
    program_result_t r = program_parse(
        "# comment line\n"
        "NAME Air Raid\n"
        "ON 10\n"
        "off 2.5   ; trailing comment\n"
        "WAIT 0.25s\r\n", &p);
    CHECK_EQ(r.code, PROG_OK);
    CHECK_STR(p.name, "Air Raid");
    CHECK_EQ(p.count, 3);
    CHECK_EQ(p.steps[0].op, OP_ON);
    CHECK_EQ(p.steps[0].arg, 10000);
    CHECK_EQ(p.steps[1].op, OP_OFF);
    CHECK_EQ(p.steps[1].arg, 2500);
    CHECK_EQ(p.steps[2].op, OP_OFF);
    CHECK_EQ(p.steps[2].arg, 250);
    CHECK_EQ(p.total_ms, 12750);
}

static void test_repeat_total(void)
{
    program_result_t r = program_parse(
        "REPEAT 3\n"
        "  ON 2\n"
        "  REPEAT 2\n"
        "    OFF 1\n"
        "  END\n"
        "END\n"
        "ON 5\n", &p);
    CHECK_EQ(r.code, PROG_OK);
    CHECK_EQ(p.total_ms, 3 * (2000 + 2 * 1000) + 5000);
}

static void test_forever(void)
{
    program_result_t r = program_parse("REPEAT FOREVER\nON 1\nOFF 1\nEND\n", &p);
    CHECK_EQ(r.code, PROG_OK);
    CHECK_EQ(p.total_ms, PROGRAM_DURATION_INFINITE);
}

static void test_name_truncated_and_bom(void)
{
    program_result_t r = program_parse("\xEF\xBB\xBFNAME A very long program name here\nON 1", &p);
    CHECK_EQ(r.code, PROG_OK);
    CHECK_STR(p.name, "A very long prog");
}

static void test_errors(void)
{
    program_result_t r;

    r = program_parse("ON 1\nBLAH 3\n", &p);
    CHECK_EQ(r.code, PROG_ERR_SYNTAX);
    CHECK_EQ(r.line, 2);

    r = program_parse("ON abc\n", &p);
    CHECK_EQ(r.code, PROG_ERR_VALUE);

    r = program_parse("ON 0\n", &p);
    CHECK_EQ(r.code, PROG_ERR_VALUE);

    r = program_parse("ON 5 extra\n", &p);
    CHECK_EQ(r.code, PROG_ERR_VALUE);

    r = program_parse("ONX 5\n", &p);
    CHECK_EQ(r.code, PROG_ERR_SYNTAX);

    r = program_parse("REPEAT 0\nON 1\nEND\n", &p);
    CHECK_EQ(r.code, PROG_ERR_VALUE);

    r = program_parse("ON 1\nEND\n", &p);
    CHECK_EQ(r.code, PROG_ERR_UNMATCHED);
    CHECK_EQ(r.line, 2);

    r = program_parse("\nREPEAT 2\nON 1\n", &p);
    CHECK_EQ(r.code, PROG_ERR_UNMATCHED);
    CHECK_EQ(r.line, 2);

    r = program_parse("REPEAT 2\nEND\n", &p);
    CHECK_EQ(r.code, PROG_ERR_EMPTY);

    r = program_parse("NAME nothing\n# only comments\n", &p);
    CHECK_EQ(r.code, PROG_ERR_EMPTY);

    r = program_parse("REPEAT 2\nREPEAT 2\nREPEAT 2\nREPEAT 2\nREPEAT 2\nON 1\n", &p);
    CHECK_EQ(r.code, PROG_ERR_NESTING);
    CHECK_EQ(r.line, 5);

    r = program_parse("REPEAT 9999\nREPEAT 9999\nON 86400\nEND\nEND\n", &p);
    CHECK_EQ(r.code, PROG_ERR_TOTAL);
}

static void test_too_many_steps(void)
{
    static char buf[4096];
    buf[0] = '\0';
    for (int i = 0; i < PROGRAM_MAX_STEPS + 1; i++) {
        strcat(buf, "ON 1\n");
    }
    program_result_t r = program_parse(buf, &p);
    CHECK_EQ(r.code, PROG_ERR_TOO_LONG);
    CHECK_EQ(r.line, PROGRAM_MAX_STEPS + 1);
}

/* The example programs shipped in sd-card/ must always be valid. */
static void test_example_files(void)
{
    static char buf[4096];
    for (int i = 1; i <= 4; i++) {
        char path[64];
        snprintf(path, sizeof path, "../sd-card/PROG%d.TXT", i);
        FILE *f = fopen(path, "rb");
        CHECK(f != NULL);
        if (!f) {
            continue;
        }
        size_t n = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        CHECK(n <= 2048);   /* PROGRAM_TEXT_MAX in port_stm32.c */
        buf[n] = '\0';
        program_result_t r = program_parse(buf, &p);
        CHECK_EQ(r.code, PROG_OK);
        CHECK(p.name[0] != '\0');
    }
}

void test_program_all(void)
{
    RUN(test_example_files);
    RUN(test_basic);
    RUN(test_repeat_total);
    RUN(test_forever);
    RUN(test_name_truncated_and_bom);
    RUN(test_errors);
    RUN(test_too_many_steps);
}
