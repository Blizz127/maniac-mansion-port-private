/* Back+Start dev-menu chord (DEV_MENU_SPEC §4a, owner 2026-09-30: "Select and Start
 * open the dev menu on controllers"). Pure per-frame logic, no SDL.
 *
 * Fires once per gesture when Back and Start are down together (either order,
 * held) or when the second is pressed within CHORD_WINDOW frames (~300 ms) of the
 * first, even if the first was already released. While a chord is possible the
 * two buttons are held back from the game; a lone Select or Start reaches the
 * game after the window (a short tap is replayed for its own length, at least
 * CHORD_MIN_TAP frames), so single presses are never lost and a chord never
 * leaks into the game. */
#ifndef MM_CHORD_H
#define MM_CHORD_H

#include <stdbool.h>
#include <stdint.h>

enum { CHORD_WINDOW = 18, CHORD_MIN_TAP = 2, CHORD_BACK = 0, CHORD_START = 1 };

typedef struct {
    long frame;
    long press[2];  /* frame of the latest press of Back/Start in this gesture, -1 none */
    int held[2];    /* frames held so far, 0 = up */
    int pending[2]; /* completed short tap awaiting replay (its length), 0 none */
    int replay[2];  /* replay frames left */
    bool fired;     /* chord fired in this gesture */
} chord_t;

void chord_init(chord_t *c);
/* One frame: current Back/Start state of the active pad. Returns the mask of
 * PAD_SELECT / PAD_START to deliver to the game this frame; *fire is true on
 * the frame the menu should toggle. bit_select/bit_start are the pad bits. */
uint8_t chord_step(chord_t *c, bool back, bool start, uint8_t bit_select, uint8_t bit_start, bool *fire);


/* Wall-clock adjudication for optional enhancements. The legacy per-loop
 * chord API remains unchanged when every enhancement is OFF. */
typedef struct {
    uint64_t press[2], duration[2], replay_until[2];
    bool held[2], seen[2], pending[2], fired;
} chord_timed_t;
void chord_timed_init(chord_timed_t *c);
uint8_t chord_timed_step(chord_timed_t *c, bool back, bool start,
                         uint8_t bit_select, uint8_t bit_start,
                         uint64_t now_ms, bool *fire);

#endif
