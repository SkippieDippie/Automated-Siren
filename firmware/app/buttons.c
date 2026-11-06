#include "buttons.h"

#include <string.h>

void buttons_init(buttons_t *b)
{
    memset(b, 0, sizeof(*b));
}

uint32_t buttons_decode_active_low(uint32_t raw_levels, unsigned n_inputs, bool *fault)
{
    uint32_t mask = (n_inputs >= 32U) ? 0xFFFFFFFFUL : ((1UL << n_inputs) - 1U);
    uint32_t pressed = ~raw_levels & mask;

    *fault = (n_inputs > 1U && pressed == mask);
    return *fault ? 0U : pressed;
}

void buttons_update(buttons_t *b, uint32_t raw)
{
    uint32_t prev = b->stable;

    for (unsigned i = 0; i < BUTTONS_MAX; i++) {
        uint32_t bit = 1UL << i;
        bool raw_on = (raw & bit) != 0;
        bool st_on = (b->stable & bit) != 0;

        if (raw_on == st_on) {
            b->count[i] = 0;
        } else if (++b->count[i] >= BUTTONS_DEBOUNCE_TICKS) {
            b->count[i] = 0;
            b->stable ^= bit;
        }
    }
    b->pressed = b->stable & ~prev;
    b->released = ~b->stable & prev;
}
