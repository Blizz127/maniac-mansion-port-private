/* Pure snap-combo and direct-walk policy (no ROM). Synthetic input only;
 * native execution evidence is test_snapwalk_native.c. */
#include "../src/host/snapwalk.h"
#include <assert.h>
#include <stdio.h>

#define DIRS (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)
static bool wall(void *ud, int x, int y) { (void)y; return x >= *(int *)ud; }

int main(void)
{
    sw_snap_t s;
    /* Without B every pad value passes through and never snaps or pulses. */
    for (unsigned p = 0; p < 256; ++p) {
        if (p & PAD_B) continue;
        sw_snap_reset(&s);
        sw_snap_result_t r = sw_snap_step(&s, (uint8_t)p);
        assert(r.pad == p && !r.snap && !r.pulse_b);
    }
    /* Plain d-pad moves the cursor: directions are never consumed. */
    sw_snap_reset(&s);
    for (int i = 0; i < 10; ++i) assert(sw_snap_step(&s, PAD_RIGHT).pad == PAD_RIGHT);
    /* Lone B tap: withheld while held, one pulse on release. */
    sw_snap_reset(&s);
    sw_snap_result_t r = sw_snap_step(&s, PAD_B);
    assert(r.pad == 0 && !r.pulse_b);
    assert(sw_snap_step(&s, PAD_B).pad == 0);
    r = sw_snap_step(&s, 0);
    assert(r.pulse_b && r.pad == 0);
    assert(!sw_snap_step(&s, 0).pulse_b);
    /* B + direction edge snaps once, consumes both, no B pulse after. */
    sw_snap_reset(&s);
    sw_snap_step(&s, PAD_B);
    r = sw_snap_step(&s, PAD_B | PAD_LEFT);
    assert(r.snap && r.dx == -1 && r.dy == 0 && r.pad == 0);
    r = sw_snap_step(&s, PAD_B | PAD_LEFT); /* held: no repeat, cursor still */
    assert(!r.snap && r.pad == 0);
    r = sw_snap_step(&s, PAD_B | PAD_LEFT | PAD_UP);
    assert(r.snap && r.dy == -1 && r.dx == 0);
    r = sw_snap_step(&s, PAD_LEFT);
    assert(!r.pulse_b && r.pad == PAD_LEFT);
    /* Releasing B on the same frame as a new direction: no snap, the lone
     * B still pulses once and the direction reaches the free cursor. */
    sw_snap_reset(&s); sw_snap_step(&s, PAD_B);
    r = sw_snap_step(&s, PAD_RIGHT);
    assert(!r.snap && r.pulse_b && r.pad == PAD_RIGHT);
    r = sw_snap_step(&s, PAD_RIGHT);
    assert(!r.snap && !r.pulse_b && r.pad == PAD_RIGHT);
    /* After a combo snap, releasing B with a new direction neither snaps
     * again nor pulses. */
    sw_snap_reset(&s); sw_snap_step(&s, PAD_B); sw_snap_step(&s, PAD_B | PAD_UP);
    r = sw_snap_step(&s, PAD_DOWN);
    assert(!r.snap && !r.pulse_b && r.pad == PAD_DOWN);
    /* Each direction maps to one axis. */
    const uint8_t d[4] = {PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT};
    const int ex[4] = {0, 0, -1, 1}, ey[4] = {-1, 1, 0, 0};
    for (int i = 0; i < 4; ++i) {
        sw_snap_reset(&s); sw_snap_step(&s, PAD_B);
        r = sw_snap_step(&s, (uint8_t)(PAD_B | d[i]));
        assert(r.snap && r.dx == ex[i] && r.dy == ey[i]);
    }
    /* Select/Start during a held B cancel the pulse and pass through. */
    sw_snap_reset(&s); sw_snap_step(&s, PAD_B);
    r = sw_snap_step(&s, PAD_B | PAD_START);
    assert(r.pad == PAD_START);
    assert(!sw_snap_step(&s, 0).pulse_b);

    sw_walk_t w;
    sw_walk_context_t ok = {.walk_verb = true, .pointer_in_scene = true, .kid_idle = true};
    /* Directions alone and every non-walk context pass through untouched. */
    for (unsigned p = 0; p < 256; ++p) {
        if (p & PAD_A) continue;
        sw_walk_reset(&w);
        sw_walk_result_t x = sw_walk_step(&w, ok, (uint8_t)p, 0);
        assert(x.pad == p && !x.request && !x.pulse_a);
    }
    sw_walk_context_t bad[4] = {{.pointer_in_scene = true, .kid_idle = true}, {.walk_verb = true, .kid_idle = true},
        {.walk_verb = true, .pointer_in_scene = true, .sentence_pending = true, .kid_idle = true},
        {.walk_verb = true, .pointer_in_scene = true, .blocked = true, .kid_idle = true}};
    for (int i = 0; i < 4; ++i) {
        sw_walk_reset(&w);
        for (unsigned t = 0; t < 500; t += 16) {
            sw_walk_result_t x = sw_walk_step(&w, bad[i], t < 32 ? PAD_A : PAD_A | PAD_RIGHT, t);
            assert(x.pad == (t < 32 ? PAD_A : PAD_A | PAD_RIGHT) && !x.request && !x.pulse_a);
        }
    }
    /* Plain A tap: withheld, then one native edge on release. */
    sw_walk_reset(&w);
    sw_walk_result_t x = sw_walk_step(&w, ok, PAD_A, 1000);
    assert(x.pad == 0 && !x.pulse_a);
    x = sw_walk_step(&w, ok, 0, 1050);
    assert(x.pulse_a && !x.request);
    assert(!sw_walk_step(&w, ok, 0, 1066).pulse_a);
    /* Plain A hold: delivered at the grace limit, then passes through. */
    sw_walk_reset(&w);
    sw_walk_step(&w, ok, PAD_A, 1000);
    assert(!sw_walk_step(&w, ok, PAD_A, 1000 + SW_A_GRACE_MS - 1).pulse_a);
    x = sw_walk_step(&w, ok, PAD_A, 1000 + SW_A_GRACE_MS);
    assert(x.pulse_a && x.pad == PAD_A);
    x = sw_walk_step(&w, ok, PAD_A | PAD_RIGHT, 1200); /* late direction: native cursor */
    assert(x.pad == (PAD_A | PAD_RIGHT) && !x.request && !x.pulse_a);
    /* A then direction within the grace: a gesture, A never delivered. */
    sw_walk_reset(&w);
    sw_walk_step(&w, ok, PAD_A, 1000);
    x = sw_walk_step(&w, ok, PAD_A | PAD_RIGHT, 1050);
    assert(x.request && x.dx == 1 && x.dy == 0 && x.pad == 0 && !x.pulse_a);
    /* Next leg only once the kid is idle again and the period passed. */
    sw_walk_context_t busy = ok; busy.kid_idle = false;
    for (unsigned t = 1066; t < 3000; t += 50)
        assert(!sw_walk_step(&w, busy, PAD_A | PAD_RIGHT, t).request);
    assert(sw_walk_step(&w, ok, PAD_A | PAD_RIGHT, 3000).request);
    assert(!sw_walk_step(&w, ok, PAD_A | PAD_RIGHT, 3000 + SW_WALK_REPEAT_MS - 1).request);
    assert(sw_walk_step(&w, ok, PAD_A | PAD_RIGHT, 3000 + SW_WALK_REPEAT_MS).request);
    /* Direction change while idle requests at once; diagonals combine. */
    x = sw_walk_step(&w, ok, PAD_A | PAD_UP | PAD_LEFT, 3000 + SW_WALK_REPEAT_MS + 1);
    assert(x.request && x.dx == -1 && x.dy == -1);
    /* Releasing the direction while A stays held: A stays swallowed, no click. */
    x = sw_walk_step(&w, ok, PAD_A, 3300);
    assert(x.pad == 0 && !x.pulse_a && !x.request);
    x = sw_walk_step(&w, ok, 0, 3320);
    assert(!x.pulse_a && x.pad == 0);
    /* A pressed while a direction is already held is a gesture too. */
    sw_walk_reset(&w);
    sw_walk_step(&w, ok, PAD_RIGHT, 0);
    x = sw_walk_step(&w, ok, PAD_A | PAD_RIGHT, 16);
    assert(x.request && x.pad == 0);
    /* Opposite directions cancel without walking, cursor still held. */
    sw_walk_reset(&w);
    sw_walk_step(&w, ok, PAD_A, 0);
    x = sw_walk_step(&w, ok, PAD_A | PAD_LEFT | PAD_RIGHT, 16);
    assert(!x.request && x.pad == 0);

    /* Target: a step ahead, clamped to the visible scene, shrunk to avoid
     * objects, never the kid's own spot. */
    int tx, ty;
    assert(sw_walk_target(30, 40, 1, 0, 14, 4, NULL, NULL, &tx, &ty) && tx == 34 && ty == 40);
    assert(sw_walk_target(30, 40, 0, -1, 14, 4, NULL, NULL, &tx, &ty) && tx == 30 && ty == 32);
    assert(sw_walk_target(30, 2, -1, -1, 14, 4, NULL, NULL, &tx, &ty) && tx == 26 && ty == 0);
    assert(sw_walk_target(30, 40, 1, 1, 14, SW_REACH_WALK, NULL, NULL, &tx, &ty) && tx == 34 && ty == 48);
    assert(sw_walk_target(42, 40, 1, 0, 14, 4, NULL, NULL, &tx, &ty) && tx == 43); /* column 29 */
    assert(!sw_walk_target(43, 40, 1, 0, 14, 4, NULL, NULL, &tx, &ty));
    assert(!sw_walk_target(30, 69, 0, 1, 14, 4, NULL, NULL, &tx, &ty));
    int edge = 33;
    assert(sw_walk_target(30, 40, 1, 0, 14, 4, wall, &edge, &tx, &ty) && tx == 32);
    edge = 31;
    assert(!sw_walk_target(30, 40, 1, 0, 14, 4, wall, &edge, &tx, &ty));
    assert(!sw_walk_target(30, 40, 0, 0, 14, 4, NULL, NULL, &tx, &ty));
    assert(!sw_walk_target(30, 40, 2, 0, 14, 4, NULL, NULL, &tx, &ty));
    assert(!sw_walk_target(30, 40, 1, 0, 14, 0, NULL, NULL, &tx, &ty));
    puts("snapwalk policy: PASS (B combo snap/pulse, free d-pad, A grace, idle-only walk legs, targets)");
    return 0;
}
