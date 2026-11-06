/*
 * Siren program: representation and text parser.
 *
 * Portable code - no hardware dependencies, unit-tested on the host.
 * File format is documented in docs/program-format.md.
 */
#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROGRAM_NAME_LEN     16   /* fits one LCD line */
#define PROGRAM_MAX_STEPS    64
#define PROGRAM_MAX_NESTING  4
#define PROGRAM_MAX_STEP_MS  (24UL * 3600UL * 1000UL)  /* 24 h per step */
#define PROGRAM_MAX_REPEAT   9999U

/* Sentinel for "runs until stopped" (contains REPEAT FOREVER). */
#define PROGRAM_DURATION_INFINITE UINT32_MAX

typedef enum {
    OP_ON = 0,   /* arg = duration in ms, siren powered            */
    OP_OFF,      /* arg = duration in ms, siren unpowered          */
    OP_REPEAT,   /* arg = repeat count, 0 = forever                */
    OP_END       /* arg = index of the matching OP_REPEAT          */
} program_op_t;

typedef struct {
    uint8_t  op;
    uint32_t arg;
} program_step_t;

typedef struct {
    char           name[PROGRAM_NAME_LEN + 1];
    program_step_t steps[PROGRAM_MAX_STEPS];
    uint16_t       count;
    uint32_t       total_ms;   /* PROGRAM_DURATION_INFINITE if it never ends */
} program_t;

typedef enum {
    PROG_OK = 0,
    PROG_ERR_SYNTAX,       /* unknown keyword / malformed line        */
    PROG_ERR_VALUE,        /* bad or out-of-range number              */
    PROG_ERR_TOO_LONG,     /* more than PROGRAM_MAX_STEPS             */
    PROG_ERR_NESTING,      /* REPEAT nested too deep                  */
    PROG_ERR_UNMATCHED,    /* END without REPEAT / REPEAT without END */
    PROG_ERR_EMPTY,        /* no timed steps (or empty REPEAT body)   */
    PROG_ERR_TOTAL         /* total duration overflows                */
} program_err_t;

typedef struct {
    program_err_t code;
    uint16_t      line;    /* 1-based line number, 0 if not line-specific */
} program_result_t;

/* Parse a NUL-terminated program text. On error, *out is left in an
 * unspecified state and must not be played. */
program_result_t program_parse(const char *text, program_t *out);

/* Short (<= 16 chars) human-readable description of an error code. */
const char *program_err_str(program_err_t code);

#endif /* PROGRAM_H */
