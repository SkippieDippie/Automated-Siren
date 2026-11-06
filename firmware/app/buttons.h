/*
 * Button debouncer (portable). Feed it the raw "pressed" bitmask at a
 * fixed rate (e.g. every 10 ms); it reports stable state and press edges.
 */
#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

#define BUTTONS_MAX            16
#define BUTTONS_DEBOUNCE_TICKS 3   /* stable samples required (3 x 10 ms) */

typedef struct {
    uint16_t stable;      /* debounced state, bit set = pressed      */
    uint16_t pressed;     /* edges seen by the last update           */
    uint16_t released;
    uint8_t  count[BUTTONS_MAX];
} buttons_t;

void buttons_init(buttons_t *b);
void buttons_update(buttons_t *b, uint16_t raw_pressed_mask);

static inline bool buttons_is_down(const buttons_t *b, unsigned id)
{
    return (b->stable >> id) & 1U;
}

static inline bool buttons_was_pressed(const buttons_t *b, unsigned id)
{
    return (b->pressed >> id) & 1U;
}

#endif /* BUTTONS_H */
