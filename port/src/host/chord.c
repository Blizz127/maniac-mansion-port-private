#include "chord.h"

#include <string.h>

void chord_init(chord_t *c)
{
    memset(c, 0, sizeof *c);
    c->press[0] = c->press[1] = -1;
}

uint8_t chord_step(chord_t *c, bool back, bool start, uint8_t bit_select, uint8_t bit_start, bool *fire)
{
    const bool now[2] = {back, start};
    const uint8_t bits[2] = {bit_select, bit_start};
    bool edge[2] = {false, false};
    c->frame++;
    *fire = false;
    for (int i = 0; i < 2; i++) {
        if (now[i]) {
            if (c->held[i] == 0) {
                edge[i] = true;
                c->press[i] = c->frame;
                c->pending[i] = 0;
                c->replay[i] = 0;
            }
            c->held[i]++;
        } else if (c->held[i] > 0) {
            /* released: a press the game has not seen yet becomes a replayed tap */
            if (!c->fired && c->held[i] <= CHORD_WINDOW)
                c->pending[i] = c->held[i];
            c->held[i] = 0;
        }
    }
    if (!c->fired) {
        bool together = now[0] && now[1];
        bool seq = false;
        for (int i = 0; i < 2; i++)
            if (edge[i] && c->press[1 - i] >= 0 && c->frame - c->press[1 - i] <= CHORD_WINDOW)
                seq = true;
        if (together || seq) {
            c->fired = true;
            *fire = true;
            c->pending[0] = c->pending[1] = 0;
            c->replay[0] = c->replay[1] = 0;
        }
    }
    uint8_t out = 0;
    if (!c->fired) {
        for (int i = 0; i < 2; i++) {
            if (c->held[i] > CHORD_WINDOW)
                out |= bits[i]; /* a lone hold: live after the window */
            if (c->pending[i] && c->frame >= c->press[i] + CHORD_WINDOW) {
                c->replay[i] = c->pending[i] < CHORD_MIN_TAP ? CHORD_MIN_TAP : c->pending[i];
                c->pending[i] = 0;
            }
            if (c->replay[i] > 0) {
                out |= bits[i];
                c->replay[i]--;
            }
        }
    }
    /* gesture over: both up, nothing pending, quiet for a window */
    if (!now[0] && !now[1] && !c->pending[0] && !c->pending[1] && !c->replay[0] && !c->replay[1]) {
        long last = c->press[0] > c->press[1] ? c->press[0] : c->press[1];
        if (last < 0 || c->frame - last > CHORD_WINDOW) {
            c->fired = false;
            c->press[0] = c->press[1] = -1;
        }
    }
    return out;
}
