#include "snapwalk.h"
#include <string.h>

#define DIRS (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)
/* Reach units: one column is 8 px, one row 2 px; a row step of twice the
 * column step keeps diagonals near 45 degrees on screen. */
#define UNIT_X 1
#define UNIT_Y 2

void sw_snap_reset(sw_snap_t *s) { memset(s, 0, sizeof(*s)); }
sw_snap_result_t sw_snap_step(sw_snap_t *s, uint8_t pad)
{
    sw_snap_result_t r = { .pad = pad };
    uint8_t edge = (uint8_t)(pad & ~s->previous);
    s->previous = pad;
    if (edge & PAD_B) { s->b_pending = true; s->b_used = false; }
    if (!s->b_pending) return r;
    r.pad &= (uint8_t)~(PAD_B | DIRS); /* B is a modifier; cursor holds still */
    if (pad & (PAD_SELECT | PAD_START)) s->b_used = true;
    uint8_t d = (uint8_t)((pad & PAD_B) ? edge & DIRS : 0); /* snap only with B held */
    if (d) {
        r.snap = true; s->b_used = true;
        if (d & PAD_UP) r.dy = -1; else if (d & PAD_DOWN) r.dy = 1;
        else if (d & PAD_LEFT) r.dx = -1; else r.dx = 1;
    }
    if (!(pad & PAD_B)) {
        r.pulse_b = !s->b_used;
        s->b_pending = s->b_used = false;
        r.pad = (uint8_t)(pad & ~PAD_B); /* directions free again on release */
    }
    return r;
}

void sw_walk_reset(sw_walk_t *s) { memset(s, 0, sizeof(*s)); }
sw_walk_result_t sw_walk_step(sw_walk_t *s, sw_walk_context_t c, uint8_t pad, uint64_t now)
{
    sw_walk_result_t r = { .pad = pad };
    bool eligible = c.walk_verb && c.pointer_in_scene && !c.sentence_pending && !c.blocked;
    uint8_t edge = (uint8_t)(pad & ~s->previous);
    s->previous = pad;
    if (!(pad & PAD_A)) s->swallow_a = false;
    /* The player's A press is itself a native click (walk to the pointer).
     * Hold its edge back briefly so A + direction never also sends one. */
    if ((edge & PAD_A) && eligible && !(pad & DIRS)) { s->a_pending = true; s->a_since = now; }
    if (s->a_pending) {
        r.pad &= (uint8_t)~PAD_A;
        if ((pad & PAD_A) && (pad & DIRS) && eligible) {
            s->a_pending = false; s->swallow_a = true; /* a walk gesture */
        } else if (!(pad & PAD_A) || !eligible || now - s->a_since >= SW_A_GRACE_MS) {
            s->a_pending = false; r.pulse_a = true; /* deliver the plain A tap/hold */
            r.pad = pad;
            return r;
        } else {
            return r;
        }
    } else if ((edge & PAD_A) && (pad & DIRS) && eligible) {
        s->swallow_a = true; /* A pressed with a direction already held */
    }
    if (!s->swallow_a) { s->walking = false; return r; }
    r.pad &= (uint8_t)~PAD_A; /* the gesture's A never reaches the game */
    if ((pad & DIRS) && eligible) {
        int dx = !!(pad & PAD_RIGHT) - !!(pad & PAD_LEFT);
        int dy = !!(pad & PAD_DOWN) - !!(pad & PAD_UP);
        r.pad &= (uint8_t)~DIRS;
        if (!dx && !dy) return r; /* opposite directions cancel; cursor still held */
        /* One leg at a time: a native request made during a running walk
         * stalls or is deferred by script 4, so wait for the kid to be idle. */
        if (c.kid_idle && (!s->walking || dx != s->dx || dy != s->dy || now - s->last_ms >= SW_WALK_REPEAT_MS)) {
            r.request = true; r.dx = dx; r.dy = dy;
            s->last_ms = now;
        }
        s->walking = true; s->dx = dx; s->dy = dy;
        return r;
    }
    s->walking = false;
    return r;
}

bool sw_walk_target(int ax, int ay, int dx, int dy, int camera, int reach,
                    bool (*occupied)(void *, int, int), void *ud, int *tx, int *ty)
{
    if (!tx || !ty || (!dx && !dy) || dx < -1 || dx > 1 || dy < -1 || dy > 1 || reach < 1) return false;
    for (int f = reach; f >= 1; --f) {
        int x = ax + dx * UNIT_X * f, y = ay + dy * UNIT_Y * f;
        /* Keep the request on the visible scene: screen columns 2..29 and
         * pointer rows inside the scene window (screen y 36..174). */
        int column = ((x - camera + 128) & 255) - 128;
        if (column < 2) x += 2 - column; else if (column > 29) x -= column - 29;
        if (y < 0) y = 0; else if (y > 69) y = 69;
        x &= 255;
        if (x == (ax & 255) && y == ay) return false;
        if (occupied && occupied(ud, x, y)) continue;
        *tx = x; *ty = y;
        return true;
    }
    return false;
}

static unsigned guest(const nes_t *n, unsigned address) { return n->prgram[address - 0x6000]; }
void sw_native_init(sw_native_t *s) { memset(s, 0, sizeof(*s)); }
bool sw_native_actor(const nes_t *n, int *x, int *y)
{
    unsigned ego = guest(n, 0x6100);
    if (ego > 25) return false;
    int slot = (int8_t)guest(n, 0x6d22 + ego);
    if (slot < 0 || slot > 3 || guest(n, 0x6d1a + (unsigned)slot) != ego) return false;
    *x = (int)guest(n, 0x6cbe + (unsigned)slot);
    *y = (int)guest(n, 0x6cc2 + (unsigned)slot);
    return true;
}
bool sw_native_request(nes_t *n, sw_native_t *s, int x, int y)
{
    /* One request at a time, and never over a native A edge the player made. */
    if (s->pending || guest(n, 0x710e) || x < 0 || x > 255 || y < 0 || y > 69) return false;
    s->pending = true; s->pending_frames = 0;
    s->target_x = x; s->target_y = y;
    s->room = guest(n, 0x63c6); s->actor = guest(n, 0x6100);
    n->prgram[0x710e - 0x6000] = 0x80; /* native A latch, as $FBAA sets for a press */
    s->requests++;
    return true;
}
void sw_native_exec(nes_t *n, sw_native_t *s)
{
    if (!s->pending) return;
    if (guest(n, 0x63c6) != s->room || guest(n, 0x6100) != s->actor ||
        !(guest(n, 0x6b0a) & 0x10) || guest(n, 0x6b08) || guest(n, 0x6b09)) {
        s->pending = false; /* the engine's queued click now reads the real pointer */
        s->cancels++;
        return;
    }
    uint16_t pc = n->cpu.pc;
    if (pc == 0x95b4 && n->mmc1.prg_lo == n->prg) {
        /* Bank-0 $95A1 converts the pointer to scene bytes $6CA5 (column on
         * screen) and $6CA4 ((y+1)/2); the engine reads only these for scene
         * clicks. Replace only that conversion result, so the drawn cursor
         * ($7116/$7118) and UI hover are untouched. */
        unsigned camera = guest(n, 0x6c31);
        n->prgram[0x6ca5 - 0x6000] = (uint8_t)(s->target_x - (int)camera);
        n->prgram[0x6ca4 - 0x6000] = (uint8_t)(s->target_y + 18);
    } else if (pc == 0xcf94 && n->ram[0xd7] == s->actor &&
               guest(n, 0x6c9d) == (unsigned)s->target_x && guest(n, 0x6c9e) == (unsigned)s->target_y) {
        /* walkActorTo ($CF7E) is starting the kid's walk to our point. Script 4
         * reads the pointer variables 30/31 (copied from $6CA5/$6CA4 by the
         * main loop at $C056) when it walks, which can be well after the
         * click if the kid is busy, so the override lasts until here. */
        s->pending = false;
        s->consumed++;
    }
}
