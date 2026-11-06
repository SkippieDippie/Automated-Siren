#include "program.h"

#include <ctype.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Small lexing helpers                                                */
/* ------------------------------------------------------------------ */

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t')) {
        p++;
    }
    return p;
}

/* Case-insensitive keyword match; keyword must be followed by
 * whitespace or end of line. Advances *pp past the keyword. */
static bool match_kw(const char **pp, const char *end, const char *kw)
{
    const char *p = *pp;
    while (*kw) {
        if (p >= end || toupper((unsigned char)*p) != *kw) {
            return false;
        }
        p++;
        kw++;
    }
    if (p < end && !(*p == ' ' || *p == '\t')) {
        return false;
    }
    *pp = p;
    return true;
}

/* Parse seconds with optional fractional part ("5", "2.5", "0.25")
 * into milliseconds. Resolution: 1 ms. */
static bool parse_seconds(const char **pp, const char *end, uint32_t *ms_out)
{
    const char *p = *pp;
    uint64_t ms = 0;
    bool digits = false;

    while (p < end && isdigit((unsigned char)*p)) {
        ms = ms * 10U + (uint64_t)(*p - '0');
        if (ms > PROGRAM_MAX_STEP_MS / 1000U) {
            return false;
        }
        digits = true;
        p++;
    }
    ms *= 1000U;
    if (p < end && *p == '.') {
        uint32_t scale = 100;
        p++;
        while (p < end && isdigit((unsigned char)*p)) {
            ms += (uint64_t)(*p - '0') * scale;
            scale /= 10U;
            digits = true;
            p++;
        }
    }
    if (!digits || ms == 0 || ms > PROGRAM_MAX_STEP_MS) {
        return false;
    }
    /* Optional unit suffix "s" for readability: "ON 5s". */
    if (p < end && (*p == 's' || *p == 'S')) {
        p++;
    }
    *ms_out = (uint32_t)ms;
    *pp = p;
    return true;
}

static bool parse_uint(const char **pp, const char *end, uint32_t max, uint32_t *out)
{
    const char *p = *pp;
    uint32_t v = 0;
    if (p >= end || !isdigit((unsigned char)*p)) {
        return false;
    }
    while (p < end && isdigit((unsigned char)*p)) {
        v = v * 10U + (uint32_t)(*p - '0');
        if (v > max) {
            return false;
        }
        p++;
    }
    *out = v;
    *pp = p;
    return true;
}

/* Rest of line must be blank (comments already stripped). */
static bool at_eol(const char *p, const char *end)
{
    return skip_ws(p, end) == end;
}

/* ------------------------------------------------------------------ */
/* Duration computation                                                */
/* ------------------------------------------------------------------ */

/* Computes total duration of steps[from..to) where the range is a
 * well-formed sequence. Returns false on overflow. Sets *infinite if a
 * REPEAT FOREVER is encountered. */
static bool sum_duration(const program_t *p, uint16_t from, uint16_t to,
                         uint64_t *total, bool *infinite)
{
    uint64_t sum = 0;
    uint16_t i = from;

    while (i < to) {
        const program_step_t *s = &p->steps[i];
        if (s->op == OP_ON || s->op == OP_OFF) {
            sum += s->arg;
            i++;
        } else if (s->op == OP_REPEAT) {
            /* find matching END: its arg points back at i */
            uint16_t j = (uint16_t)(i + 1);
            while (!(p->steps[j].op == OP_END && p->steps[j].arg == i)) {
                j++;
            }
            uint64_t body = 0;
            if (!sum_duration(p, (uint16_t)(i + 1), j, &body, infinite)) {
                return false;
            }
            if (s->arg == 0) {
                *infinite = true;
            } else {
                sum += body * s->arg;
            }
            i = (uint16_t)(j + 1);
        } else {
            i++;
        }
        if (sum >= PROGRAM_DURATION_INFINITE) {
            return false;
        }
    }
    *total = sum;
    return true;
}

/* ------------------------------------------------------------------ */
/* Parser                                                              */
/* ------------------------------------------------------------------ */

static program_result_t fail(program_err_t code, uint16_t line)
{
    program_result_t r = { code, line };
    return r;
}

static bool push_step(program_t *out, uint8_t op, uint32_t arg)
{
    if (out->count >= PROGRAM_MAX_STEPS) {
        return false;
    }
    out->steps[out->count].op = op;
    out->steps[out->count].arg = arg;
    out->count++;
    return true;
}

program_result_t program_parse(const char *text, program_t *out)
{
    uint16_t stack[PROGRAM_MAX_NESTING];
    uint16_t stack_line[PROGRAM_MAX_NESTING];
    uint8_t depth = 0;
    uint16_t line_no = 0;
    const char *line = text;

    memset(out, 0, sizeof(*out));

    while (*line != '\0') {
        const char *eol = line;
        while (*eol != '\0' && *eol != '\n') {
            eol++;
        }
        const char *next = (*eol == '\n') ? eol + 1 : eol;
        line_no++;

        /* Strip comments ('#' or ';') and trailing CR/whitespace. */
        const char *end = line;
        while (end < eol && *end != '#' && *end != ';') {
            end++;
        }
        while (end > line && isspace((unsigned char)end[-1])) {
            end--;
        }
        const char *p = skip_ws(line, end);

        /* UTF-8 BOM on the first line (Windows Notepad). */
        if (line_no == 1 && end - p >= 3 && (unsigned char)p[0] == 0xEF &&
            (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) {
            p = skip_ws(p + 3, end);
        }

        if (p == end) {
            line = next;
            continue;
        }

        if (match_kw(&p, end, "NAME")) {
            p = skip_ws(p, end);
            size_t n = (size_t)(end - p);
            if (n > PROGRAM_NAME_LEN) {
                n = PROGRAM_NAME_LEN;
            }
            memcpy(out->name, p, n);
            out->name[n] = '\0';
        } else if (match_kw(&p, end, "ON") || match_kw(&p, end, "OFF") ||
                   match_kw(&p, end, "WAIT")) {
            /* p was advanced past the keyword; look back to see which. */
            uint8_t op = (toupper((unsigned char)line[(p - line) - 1]) == 'N')
                             ? OP_ON : OP_OFF;
            uint32_t ms;
            p = skip_ws(p, end);
            if (!parse_seconds(&p, end, &ms) || !at_eol(p, end)) {
                return fail(PROG_ERR_VALUE, line_no);
            }
            if (!push_step(out, op, ms)) {
                return fail(PROG_ERR_TOO_LONG, line_no);
            }
        } else if (match_kw(&p, end, "REPEAT")) {
            uint32_t count;
            p = skip_ws(p, end);
            if (match_kw(&p, end, "FOREVER")) {
                count = 0;
            } else if (!parse_uint(&p, end, PROGRAM_MAX_REPEAT, &count) || count == 0) {
                return fail(PROG_ERR_VALUE, line_no);
            }
            if (!at_eol(p, end)) {
                return fail(PROG_ERR_VALUE, line_no);
            }
            if (depth >= PROGRAM_MAX_NESTING) {
                return fail(PROG_ERR_NESTING, line_no);
            }
            stack[depth] = out->count;
            stack_line[depth] = line_no;
            depth++;
            if (!push_step(out, OP_REPEAT, count)) {
                return fail(PROG_ERR_TOO_LONG, line_no);
            }
        } else if (match_kw(&p, end, "END")) {
            if (!at_eol(p, end)) {
                return fail(PROG_ERR_SYNTAX, line_no);
            }
            if (depth == 0) {
                return fail(PROG_ERR_UNMATCHED, line_no);
            }
            depth--;
            uint16_t rep = stack[depth];
            /* A loop body must contain time, otherwise playback would
             * spin without ever advancing the clock. */
            bool has_time = false;
            for (uint16_t i = (uint16_t)(rep + 1); i < out->count; i++) {
                if (out->steps[i].op == OP_ON || out->steps[i].op == OP_OFF) {
                    has_time = true;
                    break;
                }
            }
            if (!has_time) {
                return fail(PROG_ERR_EMPTY, line_no);
            }
            if (!push_step(out, OP_END, rep)) {
                return fail(PROG_ERR_TOO_LONG, line_no);
            }
        } else {
            return fail(PROG_ERR_SYNTAX, line_no);
        }

        line = next;
    }

    if (depth != 0) {
        return fail(PROG_ERR_UNMATCHED, stack_line[depth - 1]);
    }

    uint64_t total = 0;
    bool infinite = false;
    if (!sum_duration(out, 0, out->count, &total, &infinite)) {
        return fail(PROG_ERR_TOTAL, 0);
    }
    if (total == 0 && !infinite) {
        return fail(PROG_ERR_EMPTY, 0);
    }
    out->total_ms = infinite ? PROGRAM_DURATION_INFINITE : (uint32_t)total;
    return fail(PROG_OK, 0);
}

const char *program_err_str(program_err_t code)
{
    switch (code) {
    case PROG_OK:            return "OK";
    case PROG_ERR_SYNTAX:    return "Bad command";
    case PROG_ERR_VALUE:     return "Bad value";
    case PROG_ERR_TOO_LONG:  return "Too many steps";
    case PROG_ERR_NESTING:   return "Nested too deep";
    case PROG_ERR_UNMATCHED: return "REPEAT/END";
    case PROG_ERR_EMPTY:     return "No ON/OFF steps";
    case PROG_ERR_TOTAL:     return "Too long";
    default:                 return "Error";
    }
}
