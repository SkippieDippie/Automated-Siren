/*
 * Program player: steps through a parsed program in real time.
 *
 * Portable code - driven by an external millisecond clock, so it can be
 * unit-tested on the host. Step boundaries are scheduled from the previous
 * boundary (not from "now"), so a late tick never accumulates drift.
 */
#ifndef PLAYER_H
#define PLAYER_H

#include "program.h"

typedef enum {
    PLAYER_IDLE = 0,
    PLAYER_RUNNING,
    PLAYER_FINISHED
} player_state_t;

typedef struct {
    uint16_t repeat_idx;   /* index of the OP_REPEAT step   */
    uint32_t remaining;    /* iterations left, 0 = forever  */
} player_loop_t;

typedef struct {
    const program_t *prog;
    player_state_t   state;
    uint16_t         pc;          /* index of the current timed step */
    uint32_t         start_ms;
    uint32_t         step_end_ms;
    uint32_t         elapsed_at_step_end;  /* program time at step_end */
    player_loop_t    loops[PROGRAM_MAX_NESTING];
    uint8_t          depth;
} player_t;

void player_init(player_t *pl);
void player_start(player_t *pl, const program_t *prog, uint32_t now_ms);
void player_stop(player_t *pl);

/* Advance the program to now_ms. Call at least every few tens of ms. */
void player_tick(player_t *pl, uint32_t now_ms);

/* Acknowledge a FINISHED state (goes back to IDLE). */
void player_clear(player_t *pl);

player_state_t player_state(const player_t *pl);
bool     player_siren_on(const player_t *pl);
uint32_t player_step_remaining_ms(const player_t *pl, uint32_t now_ms);
/* PROGRAM_DURATION_INFINITE for never-ending programs. */
uint32_t player_total_remaining_ms(const player_t *pl, uint32_t now_ms);

#endif /* PLAYER_H */
