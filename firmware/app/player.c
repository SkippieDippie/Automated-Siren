#include "player.h"

#include <string.h>

void player_init(player_t *pl)
{
    memset(pl, 0, sizeof(*pl));
    pl->state = PLAYER_IDLE;
}

/* Starting at pl->pc, execute control ops (REPEAT/END) until pc points at
 * a timed step (ON/OFF). Returns false if the program has ended. */
static bool seek_timed_step(player_t *pl)
{
    const program_t *p = pl->prog;

    while (pl->pc < p->count) {
        const program_step_t *s = &p->steps[pl->pc];
        switch (s->op) {
        case OP_ON:
        case OP_OFF:
            return true;

        case OP_REPEAT:
            /* Parser guarantees depth <= PROGRAM_MAX_NESTING. */
            pl->loops[pl->depth].repeat_idx = pl->pc;
            pl->loops[pl->depth].remaining = s->arg;
            pl->depth++;
            pl->pc++;
            break;

        case OP_END: {
            player_loop_t *lp = &pl->loops[pl->depth - 1];
            if (lp->remaining == 0) {             /* forever */
                pl->pc = (uint16_t)(lp->repeat_idx + 1);
            } else if (--lp->remaining > 0) {     /* more iterations */
                pl->pc = (uint16_t)(lp->repeat_idx + 1);
            } else {                              /* loop done */
                pl->depth--;
                pl->pc++;
            }
            break;
        }

        default:
            pl->pc++;
            break;
        }
    }
    return false;
}

void player_start(player_t *pl, const program_t *prog, uint32_t now_ms)
{
    player_init(pl);
    pl->prog = prog;
    pl->start_ms = now_ms;

    if (!seek_timed_step(pl)) {
        pl->state = PLAYER_FINISHED;
        return;
    }
    pl->state = PLAYER_RUNNING;
    pl->step_end_ms = now_ms + prog->steps[pl->pc].arg;
    pl->elapsed_at_step_end = prog->steps[pl->pc].arg;
}

void player_stop(player_t *pl)
{
    pl->state = PLAYER_IDLE;
    pl->prog = NULL;
}

void player_clear(player_t *pl)
{
    if (pl->state == PLAYER_FINISHED) {
        pl->state = PLAYER_IDLE;
        pl->prog = NULL;
    }
}

void player_tick(player_t *pl, uint32_t now_ms)
{
    if (pl->state != PLAYER_RUNNING) {
        return;
    }
    /* Signed difference handles 32-bit tick wrap-around (~49 days). */
    while ((int32_t)(now_ms - pl->step_end_ms) >= 0) {
        pl->pc++;
        if (!seek_timed_step(pl)) {
            pl->state = PLAYER_FINISHED;
            return;
        }
        uint32_t d = pl->prog->steps[pl->pc].arg;
        pl->step_end_ms += d;
        pl->elapsed_at_step_end += d;
    }
}

player_state_t player_state(const player_t *pl)
{
    return pl->state;
}

bool player_siren_on(const player_t *pl)
{
    return pl->state == PLAYER_RUNNING && pl->prog->steps[pl->pc].op == OP_ON;
}

uint32_t player_step_remaining_ms(const player_t *pl, uint32_t now_ms)
{
    if (pl->state != PLAYER_RUNNING) {
        return 0;
    }
    int32_t r = (int32_t)(pl->step_end_ms - now_ms);
    return r > 0 ? (uint32_t)r : 0;
}

uint32_t player_total_remaining_ms(const player_t *pl, uint32_t now_ms)
{
    if (pl->state != PLAYER_RUNNING) {
        return 0;
    }
    if (pl->prog->total_ms == PROGRAM_DURATION_INFINITE) {
        return PROGRAM_DURATION_INFINITE;
    }
    uint32_t elapsed = pl->elapsed_at_step_end - player_step_remaining_ms(pl, now_ms);
    return pl->prog->total_ms > elapsed ? pl->prog->total_ms - elapsed : 0;
}
