#include "buttons.h"

#include <string.h>

void buttons_init(buttons_t *b)
{
    memset(b, 0, sizeof(*b));
}

void buttons_update(buttons_t *b, uint16_t raw)
{
    uint16_t prev = b->stable;

    for (unsigned i = 0; i < BUTTONS_MAX; i++) {
        uint16_t bit = (uint16_t)(1U << i);
        bool raw_on = (raw & bit) != 0;
        bool st_on = (b->stable & bit) != 0;

        if (raw_on == st_on) {
            b->count[i] = 0;
        } else if (++b->count[i] >= BUTTONS_DEBOUNCE_TICKS) {
            b->count[i] = 0;
            b->stable ^= bit;
        }
    }
    b->pressed = (uint16_t)(b->stable & ~prev);
    b->released = (uint16_t)(~b->stable & prev);
}
