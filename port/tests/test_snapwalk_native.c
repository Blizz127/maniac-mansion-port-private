/* Owner-local ROM test for the snap combo and direct walk.
 * Usage: test-snapwalk-native ROM FIELD_STATE [--png DIR]
 * The field state must be in native field play with Walk to selected and
 * the pointer in the scene (for example the porch/road fixtures). --png
 * writes before/after captures for local review only; never package them.
 * The retail engine (walking, scripts, rendering) still runs interpreted;
 * this verifies the native host policy and request through that engine. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/cart.h"
#include "../src/core/nes.h"
#include "../src/host/png.h"
#include "../src/host/qol_input.h"
static nes_t base, n, baseline;
static qol_native_t q;
static unsigned failures, b_events, b_base_events;
static const char *png_dir;
static unsigned clicks_at_pointer; static int pointer_x = -1;
static bool trace;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); failures++; } } while(0)
static unsigned g(nes_t *v, unsigned a) { return bus_peek(v, a); }
static void copy(nes_t *d, const nes_t *s)
{ memcpy(d, s, sizeof *d); d->apu.out = NULL; d->apu.out_cap = d->apu.out_len = 0; d->exec_hook = NULL; }
static bool identical(void)
{
    size_t z = nes_state_size(); void *a = malloc(z), *b = malloc(z); uint8_t id[8] = {0};
    if (!a || !b) exit(5);
    nes_state_save(&n, a, id); nes_state_save(&baseline, b, id);
    bool same = memcmp(a, b, z) == 0; free(a); free(b); return same;
}
/* Observes native B/Select/Start consumption ($F7B1 stores the edge). */
static void (*inner)(nes_t *, void *); static void *inner_ud;
static void observe(nes_t *v, void *ud)
{
    if (v->cpu.pc == 0xf7b4 && v->cpu.a == 0x40) b_events++;
    if (v->cpu.pc == 0xcf94 && pointer_x >= 0 && v->prgram[0x6c9d - 0x6000] == (uint8_t)pointer_x) clicks_at_pointer++;
    if (trace && (v->cpu.pc == 0xcf94 || v->cpu.pc == 0xf7b4 || (v->cpu.pc == 0xfbb3 && v->cpu.a)))
        fprintf(stderr, "    pc=%04x a=%02x d7=%02x 6c9d=%02x 6c9e=%02x 6ca5=%02x 6ca4=%02x 710e=%02x\n", v->cpu.pc, v->cpu.a, v->ram[0xd7], v->prgram[0x6c9d - 0x6000], v->prgram[0x6c9e - 0x6000],
                v->prgram[0x6ca5 - 0x6000], v->prgram[0x6ca4 - 0x6000], v->prgram[0x710e - 0x6000]);
    if (inner) inner(v, ud);
}
static void observe_base(nes_t *v, void *ud) { (void)ud; if (v->cpu.pc == 0xf7b4 && v->cpu.a == 0x40) b_base_events++; }
static unsigned bad_requests; /* a walk request made while the verb wasn't Walk to */
static void frame(qol_options_t o, uint8_t pad, bool permitted)
{
    unsigned verb_before = g(&n, 0x611a), requests_before = q.sw.requests;
    if (trace) {
        int x = -1, y = -1; sw_native_actor(&n, &x, &y);
        unsigned slot = (unsigned)(int8_t)g(&n, 0x6d22 + g(&n, 0x6100));
        fprintf(stderr, "  pad=%02x perm=%d kid=%d,%d fl=%02x pend=%d req=%u con=%u to=%u can=%u tgt=%d,%d\n",
                pad, permitted, x, y, g(&n, 0x6cce + slot), q.sw.pending, q.sw.requests, q.sw.consumed,
                q.sw.timeouts, q.sw.cancels, q.sw.target_x, q.sw.target_y);
    }
    qol_native_update(&n, &q, o, pad, permitted);
    inner = n.exec_hook; inner_ud = n.exec_hook_ud;
    n.exec_hook = observe; n.exec_hook_ud = inner_ud;
    nes_set_pad(&n, 0, pad); nes_run_frame(&n);
    n.exec_hook = inner; n.exec_hook_ud = inner_ud;
    if (q.sw.requests != requests_before && verb_before != 13) bad_requests++;
}
static void frames(qol_options_t o, uint8_t pad, unsigned k) { while (k--) frame(o, pad, true); }
static void base_frames(uint8_t pad, unsigned k)
{
    baseline.exec_hook = observe_base;
    while (k--) { nes_set_pad(&baseline, 0, pad); nes_run_frame(&baseline); }
    baseline.exec_hook = NULL;
}
static int cursor_sprite(nes_t *v, unsigned *x, unsigned *y)
{
    for (int k = 0; k < 64; k++) if (v->ppu.oam[k * 4 + 1] == 0xfa) {
        *x = v->ppu.oam[k * 4 + 3]; *y = v->ppu.oam[k * 4]; return k;
    }
    return -1;
}
static void shot(const char *name)
{
    if (!png_dir) return;
    char p[1024]; snprintf(p, sizeof p, "%s/%s.png", png_dir, name);
    if (!png_write_nes(p, n.ppu.fb)) { fprintf(stderr, "capture failed %s\n", p); failures++; }
}
static void actor(int *x, int *y) { if (!sw_native_actor(&n, x, y)) { *x = *y = -1; failures++; } }

/* Hold A + direction; return displacement and verify the drawn cursor stays. */
static void walk(const char *label, uint8_t dir, unsigned hold, int *dx, int *dy, int *after_x, int *after_y)
{
    qol_options_t o = {.direct_walk = true};
    copy(&n, &base); qol_native_init(&q);
    CHECK(qol_native_cursor(&n, 24, 100));
    frames(o, 0, 30);
    unsigned cx = g(&n, 0x7116), cy = g(&n, 0x7118), sx0 = 0, sy0 = 0, sx, sy;
    int x0, y0, x1, y1; actor(&x0, &y0);
    char name[64]; snprintf(name, sizeof name, "%s-before", label); shot(name);
    cursor_sprite(&n, &sx0, &sy0);
    unsigned moved_cursor = 0, moved_sprite = 0;
    clicks_at_pointer = 0; pointer_x = (int)((cx / 8 + g(&n, 0x6c31)) & 255);
    frames(o, PAD_A, 4); /* A first, direction inside the grace: no native click */
    for (unsigned f = 0; f < hold; f++) {
        frame(o, (uint8_t)(PAD_A | dir), true);
        if (g(&n, 0x7116) != cx || g(&n, 0x7118) != cy) moved_cursor++;
        if (cursor_sprite(&n, &sx, &sy) < 0 || sx != sx0 || sy != sy0) moved_sprite++;
    }
    actor(&x1, &y1);
    snprintf(name, sizeof name, "%s-held", label); shot(name);
    frames(o, 0, 40);
    pointer_x = -1;
    actor(after_x, after_y);
    *dx = x1 - x0; *dy = y1 - y0;
    unsigned slot = (unsigned)(int8_t)g(&n, 0x6d22 + g(&n, 0x6100));
    CHECK(clicks_at_pointer == 0);
    printf("%s: kid %d,%d -> %d,%d (held %u frames) -> %d,%d after release; requests=%u consumed=%u "
           "timeouts=%u cancels=%u cursor_moved=%u sprite_moved=%u walking=%02x\n",
           label, x0, y0, x1, y1, hold, *after_x, *after_y, q.sw.requests, q.sw.consumed,
           q.sw.timeouts, q.sw.cancels, moved_cursor, moved_sprite, g(&n, 0x6cce + slot));
    CHECK(moved_cursor == 0); CHECK(moved_sprite == 0);
    CHECK(q.sw.timeouts == 0); CHECK(q.sw.cancels == 0);
    CHECK(q.sw.consumed == q.sw.requests);
    CHECK(!q.sw.pending);
    CHECK(g(&n, 0x6cce + slot) == 0); /* stopped */
    CHECK(abs(*after_x - x1) <= 4 && abs(*after_y - y1) <= 8); /* finishes at most one leg */
    snprintf(name, sizeof name, "%s-released", label); shot(name);
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    if (argc > 4 && !strcmp(argv[3], "--png")) png_dir = argv[4];
    trace = getenv("SW_TRACE") != NULL;
    cart_t c; char err[512];
    if (!cart_load(&c, argv[1], false, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 2; }
    uint8_t id[8]; for (int i = 0; i < 8; i++) { unsigned v; sscanf(c.prg_sha256 + i * 2, "%2x", &v); id[i] = v; }
    nes_init(&base, c.prg, c.prg_size);
    FILE *f = fopen(argv[2], "rb"); if (!f) return 2;
    size_t z = nes_state_size(); void *b = malloc(z); size_t got = fread(b, 1, z, f); fclose(f);
    if (!nes_state_load(&base, b, got, id)) { fprintf(stderr, "state rejected\n"); return 3; }
    free(b);
    if (!qol_native_ready(&base) || g(&base, 0x611a) != 13) { fprintf(stderr, "fixture not in field play with Walk to\n"); return 3; }

    /* 1. Free cursor and parity: with both new options ON, 900 frames of
     * d-pad and plain A taps (never A with a direction, never B) leave the
     * whole machine state identical to the unhooked retail run. */
    qol_options_t both = {.object_snap = true};
    copy(&n, &base); copy(&baseline, &base); qol_native_init(&q);
    unsigned rng = 12345, diverged = 0;
    for (unsigned i = 0; i < 900; i++) {
        if (i % 15 == 0) rng = rng * 1103515245u + 12345u;
        if (i < 2) rng = 0; /* enabled options start from neutral input, as in play */
        unsigned pick = (rng >> 16) % 6;
        uint8_t pad = pick == 0 ? 0 : pick == 1 ? PAD_UP : pick == 2 ? PAD_DOWN :
                      pick == 3 ? PAD_LEFT : pick == 4 ? PAD_RIGHT : ((i % 15) < 4 ? PAD_A : 0);
        frame(both, pad, true);
        nes_set_pad(&baseline, 0, pad); nes_run_frame(&baseline);
        if (!identical()) diverged++;
    }
    printf("parity: 900 frames free d-pad/A taps with snap ON, full-state diverged frames=%u\n", diverged);
    CHECK(diverged == 0);
    qol_options_t walk_on = {.object_snap = true, .direct_walk = true};
    copy(&n, &base); copy(&baseline, &base); qol_native_init(&q); diverged = 0;
    for (unsigned i = 0; i < 900; i++) {
        static const uint8_t d[5] = {0, PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT};
        uint8_t pad = i < 2 ? 0 : d[(i / 13) % 5];
        frame(walk_on, pad, true);
        nes_set_pad(&baseline, 0, pad); nes_run_frame(&baseline);
        if (!identical()) diverged++;
    }
    printf("parity: 900 frames free d-pad with snap+walk ON, full-state diverged frames=%u\n", diverged);
    CHECK(diverged == 0);
    /* A plain A tap with direct walk ON reaches the same native outcome,
     * delivered on release (here 4 frames, within the 100 ms grace). */
    copy(&n, &base); copy(&baseline, &base); qol_native_init(&q);
    CHECK(qol_native_cursor(&n, 200, 150));
    for (unsigned a = 0x710f; a <= 0x7118; ++a) baseline.prgram[a - 0x6000] = n.prgram[a - 0x6000];
    frames(walk_on, 0, 4); frames(walk_on, PAD_A, 4); frames(walk_on, 0, 90);
    base_frames(0, 4); base_frames(PAD_A, 4); base_frames(0, 90);
    {
        nes_t keep; copy(&keep, &n); int ax0, ay0, ax1, ay1, sx0, sy0;
        copy(&n, &base); sw_native_actor(&n, &sx0, &sy0); copy(&n, &keep);
        sw_native_actor(&n, &ax0, &ay0); copy(&n, &baseline); sw_native_actor(&n, &ax1, &ay1); copy(&n, &keep);
        printf("A tap with direct walk ON: kid %d,%d vs retail %d,%d\n", ax0, ay0, ax1, ay1);
        CHECK(ax0 == ax1 && ay0 == ay1 && (ax0 != sx0 || ay0 != sy0));
    }
    qol_options_t off = {0};
    copy(&n, &base); copy(&baseline, &base); qol_native_init(&q); diverged = 0;
    for (unsigned i = 0; i < 900; i++) {
        uint8_t pad = (uint8_t)((i / 7) * 37);
        frame(off, pad, true);
        nes_set_pad(&baseline, 0, pad); nes_run_frame(&baseline);
        if (!identical()) diverged++;
    }
    printf("parity: 900 frames arbitrary input with all options OFF, diverged=%u hook_bound=%d\n", diverged, n.exec_hook != NULL);
    CHECK(diverged == 0); CHECK(!n.exec_hook);

    /* 2. Lone B tap keeps its native action with the snap combo on. */
    qol_options_t snap = {.object_snap = true};
    copy(&n, &base); copy(&baseline, &base); qol_native_init(&q);
    b_events = b_base_events = 0;
    frames(snap, 0, 4); frames(snap, PAD_B, 4); frames(snap, 0, 20);
    base_frames(0, 4); base_frames(PAD_B, 4); base_frames(0, 20);
    printf("lone B: native B events snap=%u retail=%u\n", b_events, b_base_events);
    CHECK(b_events == 1); CHECK(b_base_events == 1);

    /* 3. B + direction snaps to a visible object and does not fire B. */
    copy(&n, &base); qol_native_init(&q); b_events = 0;
    CHECK(qol_native_cursor(&n, 24, 100));
    frames(snap, 0, 4); shot("snap-before");
    qol_target_t t[34]; size_t count = qol_native_targets(&n, t, 34);
    unsigned snaps = 0;
    const uint8_t dirs[4] = {PAD_RIGHT, PAD_DOWN, PAD_LEFT, PAD_UP};
    for (unsigned k = 0; k < 4; k++) {
        frames(snap, PAD_B, 2); frames(snap, (uint8_t)(PAD_B | dirs[k]), 3);
        bool on = false;
        for (size_t i = 0; i < count; i++)
            on |= g(&n, 0x7116) == (unsigned)t[i].x && g(&n, 0x7118) == (unsigned)t[i].y;
        snaps += on && q.highlight;
        if (k == 0) shot("snap-b-right");
        frames(snap, 0, 6);
    }
    printf("B+direction: %u/4 presses landed on visible native targets (%zu visible), native B events=%u\n",
           snaps, count, b_events);
    CHECK(snaps >= 2); CHECK(b_events == 0);

    /* 4. Direct walk: A + direction walks the kid; cursor never moves; release stops. */
    int dx, dy, ax, ay;
    walk("walk-right", PAD_RIGHT, 90, &dx, &dy, &ax, &ay); CHECK(dx >= 0);
    int right = dx;
    walk("walk-left", PAD_LEFT, 60, &dx, &dy, &ax, &ay); CHECK(dx <= 0);
    CHECK(right >= 6 || dx <= -6); /* walk boxes may block one side at a fixture */
    uint8_t open_dir = right >= 6 ? PAD_RIGHT : PAD_LEFT;
    walk("walk-up", PAD_UP, 60, &dx, &dy, &ax, &ay); CHECK(dy <= 0);
    walk("walk-down", PAD_DOWN, 60, &dx, &dy, &ax, &ay); CHECK(dy >= 0);

    /* 5. Not Walk to, or pointer outside the scene: A + d-pad stays native. */
    qol_options_t dw = {.direct_walk = true};
    copy(&n, &base); copy(&baseline, &base); qol_native_init(&q);
    CHECK(qol_native_cursor(&n, 100, 182)); /* verb panel */
    for (unsigned a = 0x710f; a <= 0x7115; ++a) baseline.prgram[a - 0x6000] = n.prgram[a - 0x6000];
    baseline.prgram[0x7116 - 0x6000] = 100; baseline.prgram[0x7117 - 0x6000] = 0; baseline.prgram[0x7118 - 0x6000] = 182;
    diverged = 0;
    for (unsigned i = 0; i < 40; i++) {
        uint8_t pad = i < 2 ? 0 : (uint8_t)(PAD_A | PAD_RIGHT);
        frame(dw, pad, true); nes_set_pad(&baseline, 0, pad); nes_run_frame(&baseline);
        if (!identical()) diverged++;
    }
    printf("pointer in verb panel: requests=%u diverged=%u verb=%u\n", q.sw.requests, diverged, g(&n, 0x611a));
    CHECK(q.sw.requests == 0); CHECK(diverged == 0);
    /* With Open selected, A + d-pad keeps native cursor motion, no walk. */
    copy(&n, &base); qol_native_init(&q);
    frames(dw, 0, 2); CHECK(qol_native_verb_cursor(&n, 1));
    frames(dw, 0, 2); frames(dw, PAD_A, 3); frames(dw, 0, 20);
    CHECK(g(&n, 0x611a) == 1);
    CHECK(qol_native_cursor(&n, 24, 100)); frames(dw, 0, 4);
    unsigned cx = g(&n, 0x7116); bad_requests = 0;
    frame(dw, PAD_A, true); /* native Open click on the floor */
    for (unsigned i = 0; i < 6 && g(&n, 0x611a) == 1; i++) frame(dw, PAD_A | PAD_RIGHT, true);
    unsigned moved = g(&n, 0x7116) - cx, verb_now = g(&n, 0x611a);
    frames(dw, PAD_A | PAD_RIGHT, 30);
    printf("verb Open: A+Right moved native cursor %u px while Open stayed; verb then %u; "
           "requests made while not Walk to=%u\n", moved, verb_now, bad_requests);
    CHECK(bad_requests == 0);

    /* 6. A request pending when the host menu opens still reads its own target. */
    copy(&n, &base); qol_native_init(&q);
    CHECK(qol_native_cursor(&n, 24, 100)); frames(dw, 0, 30);
    int x0, y0; actor(&x0, &y0);
    frames(dw, PAD_A, 4);
    frame(dw, (uint8_t)(PAD_A | open_dir), true);
    CHECK(q.sw.pending);
    for (unsigned i = 0; i < 40; i++) frame(dw, 0, false); /* menu: not permitted */
    int x1, y1; actor(&x1, &y1);
    printf("menu during pending: kid %d -> %d consumed=%u cancels=%u hook=%d\n", x0, x1, q.sw.consumed, q.sw.cancels, n.exec_hook != NULL);
    CHECK(q.sw.consumed == 1 && !q.sw.pending); CHECK(open_dir == PAD_RIGHT ? x1 > x0 : x1 < x0); CHECK(!n.exec_hook);

    /* 7. Stale pending is dropped when the selected kid or room changes. */
    copy(&n, &base);
    sw_native_t s; sw_native_init(&s);
    CHECK(sw_native_request(&n, &s, 40, 57));
    n.prgram[0x6100 - 0x6000] ^= 1; /* host test write only: pretend another kid */
    sw_native_exec(&n, &s);
    CHECK(!s.pending && s.cancels == 1);
    copy(&n, &base); sw_native_init(&s);
    CHECK(sw_native_request(&n, &s, 40, 57));
    n.prgram[0x63c6 - 0x6000] ^= 1;
    sw_native_exec(&n, &s);
    CHECK(!s.pending && s.cancels == 1);
    copy(&n, &base); sw_native_init(&s);
    CHECK(sw_native_request(&n, &s, 40, 57));
    CHECK(!sw_native_request(&n, &s, 41, 57)); /* one request at a time */
    n.prgram[0x6b0a - 0x6000] &= (uint8_t)~0x10; /* input disabled (cutscene) */
    sw_native_exec(&n, &s);
    CHECK(!s.pending && s.cancels == 1);

    printf("snapwalk native: failures=%u\n", failures);
    return failures ? 1 : 0;
}
