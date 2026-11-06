/*
 * Button debouncer (portable). Feed it the raw "pressed" bitmask at a
 * fixed rate (e.g. every 10 ms); it reports stable state and press edges.
 */
#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

#define BUTTONS_MAX            32
#define BUTTONS_DEBOUNCE_TICKS 3   /* stable samples required (3 x 10 ms) */

typedef struct {
    uint32_t stable;      /* debounced state, bit set = pressed      */
    uint32_t pressed;     /* edges seen by the last update           */
    uint32_t released;
    uint8_t  count[BUTTONS_MAX];
} buttons_t;

void buttons_init(buttons_t *b);

/* Convert raw input levels from active-low buttons (pressed = 0) into a
 * pressed mask for the lowest `n_inputs` bits.
 *
 * Fault check: if *every* input reads low, the data line is almost
 * certainly stuck low (shorted, chip unpowered or missing), not every
 * button held at once. Since that would include MANUAL and turn the siren
 * on, it is reported as "nothing pressed" and *fault is set. */
uint32_t buttons_decode_active_low(uint32_t raw_levels, unsigned n_inputs, bool *fault);
void buttons_update(buttons_t *b, uint32_t raw_pressed_mask);

static inline bool buttons_is_down(const buttons_t *b, unsigned id)
{
    return (b->stable >> id) & 1U;
}

static inline bool buttons_was_pressed(const buttons_t *b, unsigned id)
{
    return (b->pressed >> id) & 1U;
}

#endif /* BUTTONS_H */
