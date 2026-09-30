/* DEV_MENU_SPEC §4a tests: the ignore-list filter, the pad selection
 * policy, and an SDL virtual-device case under the Steam hint environment
 * (a physical Deck/InputPlumber pad hidden by the inherited ignore list
 * plus a silent Steam virtual pad). */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/host/pads.h"
#include "../src/host/chord.h"

static int failures;
#define CHECK(c)                                                          \
    do {                                                                  \
        if (!(c)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);  \
            failures++;                                                   \
        }                                                                 \
    } while (0)

static char last_log[512];
static void cap(const char *s)
{
    snprintf(last_log, sizeof last_log, "%s", s);
    printf("  log: %s\n", s);
}

static void test_filter(void)
{
    char *f = pads_filter_ignore_list("0x28de/0x1205,0x28de/0x11ff,0x045e/0x028e,0x28de/0x12fb,0x17ef/0x6182,"
                                      "0x17ef/*,0x28de/0x1206,0x28de/0x1142");
    CHECK(!strcmp(f, "0x28de/0x11ff,0x045e/0x028e,0x28de/0x1142"));
    free(f);
    f = pads_filter_ignore_list("");
    CHECK(!strcmp(f, ""));
    free(f);
}

static void test_policy(void)
{
    pad_set_t s;
    pads_init(&s, cap);
    pads_added(&s, 10, 0x28DE, 0x11FF, "Steam Virtual Gamepad", false);
    pads_added(&s, 11, 0x28DE, 0x1205, "Steam Deck", false);
    /* silent devices are never chosen */
    CHECK(!pads_input(&s, 10, false));
    CHECK(!pads_input(&s, 11, false));
    CHECK(s.active == -1);
    /* physical input wins */
    CHECK(pads_input(&s, 11, true));
    CHECK(strstr(last_log, "reason=physical-active") && strstr(last_log, "28de:1205"));
    /* the virtual pad producing input later does not steal it */
    CHECK(!pads_input(&s, 10, true));
    CHECK(s.active == 11);
    /* hotplug: removing the active pad clears it; re-added pad held -> neutral first */
    pads_removed(&s, 11);
    CHECK(s.active == -1);
    pads_added(&s, 12, 0x28DE, 0x1205, "Steam Deck", true);
    CHECK(!pads_input(&s, 12, true)); /* still held from before */
    CHECK(!pads_input(&s, 12, false)); /* neutral */
    CHECK(pads_input(&s, 12, true));
    /* virtual-only case */
    pad_set_t v;
    pads_init(&v, cap);
    pads_added(&v, 20, 0x28DE, 0x11FF, "Steam Virtual Gamepad", false);
    CHECK(pads_input(&v, 20, true));
    CHECK(strstr(last_log, "reason=virtual-only"));
    /* a physical pad's first input re-evaluates */
    pads_added(&v, 21, 0x045E, 0x028E, "Xbox 360 Controller", false);
    CHECK(pads_input(&v, 21, true));
    CHECK(!pads_input(&v, 20, true));
}

/* SDL-level: the Steam hints hide the physical pad until the filter runs. */
static void test_sdl_virtual(void)
{
    setenv("SDL_GAMECONTROLLER_IGNORE_DEVICES", "0x28de/0x1205,0x17ef/0x61eb", 1);
    unsetenv("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD"); /* the launcher does not set it; we must */
    pads_fix_environment(cap);
    CHECK(!strcmp(getenv("SDL_GAMECONTROLLER_IGNORE_DEVICES"), ""));
    CHECK(getenv("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD") && !strcmp(getenv("SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD"), "1"));
    CHECK(!strcmp(getenv("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"), "1"));
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) {
        printf("  skip SDL case: %s\n", SDL_GetError());
        return;
    }
#if SDL_VERSION_ATLEAST(2, 24, 0)
    SDL_VirtualJoystickDesc d;
    memset(&d, 0, sizeof d);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = SDL_CONTROLLER_AXIS_MAX;
    d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    d.vendor_id = 0x28DE;
    d.product_id = 0x1205;
    d.name = "Steam Deck (virtual test device)";
    int idx = SDL_JoystickAttachVirtualEx(&d);
    CHECK(idx >= 0);
    CHECK(SDL_IsGameController(idx)); /* would be hidden without the filter */
    SDL_GameController *gc = SDL_GameControllerOpen(idx);
    CHECK(gc != NULL);
    if (gc) {
        SDL_Joystick *j = SDL_GameControllerGetJoystick(gc);
        SDL_JoystickSetVirtualButton(j, SDL_CONTROLLER_BUTTON_A, 1);
        SDL_GameControllerUpdate();
        CHECK(SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A) == 1);
        pad_set_t s;
        pads_init(&s, cap);
        pads_added(&s, 99, 0x28DE, 0x11FF, "Steam Virtual Gamepad", false); /* silent */
        int32_t id = SDL_JoystickInstanceID(j);
        pads_added(&s, id, SDL_GameControllerGetVendor(gc), SDL_GameControllerGetProduct(gc),
                   SDL_GameControllerName(gc), false);
        CHECK(!pads_input(&s, 99, false));
        CHECK(pads_input(&s, id, SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_A)));
        CHECK(s.active == id);
        SDL_GameControllerClose(gc);
    }
    SDL_JoystickDetachVirtual(idx);
#else
    printf("  skip SDL virtual device: SDL < 2.24\n");
#endif
    SDL_Quit();
}


/* Back+Start chord (owner 2026-09-30): either order, within ~300 ms, or held; a lone
 * Select/Start still reaches the game; a chord never leaks into the game. */
enum { SEL = 0x04, STA = 0x08 };
static int run_chord(chord_t *c, const char *script, int *fires, uint8_t *leaked, int *delivered_sel, int *delivered_sta)
{
    /* script: one char per frame: '.' none, 'b' back, 's' start, 'x' both */
    int n = 0;
    for (const char *q = script; *q; q++, n++) {
        bool f;
        uint8_t out = chord_step(c, *q == 'b' || *q == 'x', *q == 's' || *q == 'x', SEL, STA, &f);
        if (f) (*fires)++;
        if (c->fired && out) *leaked |= out;
        if (out & SEL) (*delivered_sel)++;
        if (out & STA) (*delivered_sta)++;
    }
    return n;
}
static void test_chord(void)
{
    chord_t c; int fires; uint8_t leaked; int ds, dt;
    /* 1. held together (same frame) */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..............................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 1); CHECK(leaked == 0); CHECK(ds == 0 && dt == 0);
    /* 2. Back first, Start 10 frames later while Back is held (either order) */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "bbbbbbbbbbxxxxxxxxxx..............................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 1); CHECK(ds == 0 && dt == 0);
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "ssssssssssxxxxxxxxxx..............................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 1); CHECK(ds == 0 && dt == 0);
    /* 3. tap Back (3 frames), release, tap Start 8 frames later: sequence within the window */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "bbb........sss......................................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 1); CHECK(ds == 0 && dt == 0);
    /* 4. a lone 3-frame Start tap reaches the game (replayed after the window), no fire */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "sss.....................................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 0); CHECK(dt == 3); CHECK(ds == 0);
    /* 5. a lone 1-frame Select tap is stretched to CHORD_MIN_TAP */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "b.......................................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 0); CHECK(ds == CHORD_MIN_TAP);
    /* 6. a lone held Start passes live after the window, for the rest of the hold */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "ssssssssssssssssssssssssssssssssssssssss....................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 0); CHECK(dt == 40 - CHORD_WINDOW);
    /* 7. Back tap, then Start 25 frames later: two separate presses, both delivered */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "bbb..........................sss..............................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 0); CHECK(ds == 3 && dt == 3);
    /* 8. one fire per gesture; a second chord after a quiet window fires again */
    chord_init(&c); fires = 0; leaked = 0; ds = dt = 0;
    run_chord(&c, "xxxx.x.xxx........................xxxx..............................", &fires, &leaked, &ds, &dt);
    CHECK(fires == 2); CHECK(leaked == 0); CHECK(ds == 0 && dt == 0);
}

static void test_raw_layout(void)
{
    bool back, start, l3, r3;
    uint8_t b = pads_raw_layout((1u << PADS_RAW_BACK) | (1u << PADS_RAW_START) | (1u << PADS_RAW_A), 0, 0, 0, &back, &start, &l3, &r3);
    CHECK(b == (SEL | STA | 0x01)); CHECK(back && start && !l3 && !r3);
    b = pads_raw_layout(1u << PADS_RAW_X, 1 | 8 /* hat up+left */, 0, 0, &back, &start, &l3, &r3);
    CHECK(b == (0x02 | 0x10 | 0x40)); CHECK(!back && !start);
    b = pads_raw_layout(1u << PADS_RAW_L3, 0, 20000, -20000, &back, &start, &l3, &r3);
    CHECK(b == (0x80 | 0x10)); CHECK(l3 && !r3);
    b = pads_raw_layout(0, 1 | 4, 0, 0, NULL, NULL, NULL, NULL); /* opposing directions cancel */
    CHECK(b == 0);
}

/* SDL-level: a virtual X-Box 360 pad (what Steam Input presents under Game Mode)
 * drives the chord through the real controller API: Back then Start within the
 * window opens the menu; a lone Start reaches the game. */
static void test_sdl_chord(void)
{
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) != 0) { printf("  skip: %s\n", SDL_GetError()); return; }
#if SDL_VERSION_ATLEAST(2, 24, 0)
    SDL_VirtualJoystickDesc d;
    memset(&d, 0, sizeof d);
    d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    d.naxes = SDL_CONTROLLER_AXIS_MAX;
    d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    d.vendor_id = 0x045E;
    d.product_id = 0x028E;
    d.name = "Microsoft X-Box 360 pad (Steam virtual, test)";
    int idx = SDL_JoystickAttachVirtualEx(&d);
    CHECK(idx >= 0 && SDL_IsGameController(idx));
    SDL_GameController *gc = SDL_GameControllerOpen(idx);
    CHECK(gc != NULL);
    if (gc) {
        SDL_Joystick *j = SDL_GameControllerGetJoystick(gc);
        chord_t c; chord_init(&c);
        int fires = 0, dt = 0; bool f;
        /* frames 1-5: Back held; frame 6: Start pressed too -> fire */
        for (int fr = 1; fr <= 30; fr++) {
            SDL_JoystickSetVirtualButton(j, SDL_CONTROLLER_BUTTON_BACK, fr <= 12);
            SDL_JoystickSetVirtualButton(j, SDL_CONTROLLER_BUTTON_START, fr >= 6 && fr <= 12);
            SDL_GameControllerUpdate();
            uint8_t out = chord_step(&c, SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK),
                                     SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_START), SEL, STA, &f);
            if (f) { fires++; CHECK(fr == 6); }
            CHECK(out == 0);
        }
        CHECK(fires == 1);
        /* quiet, then a lone Start tap of 4 frames reaches the game */
        for (int fr = 0; fr < 25; fr++) { SDL_GameControllerUpdate(); chord_step(&c, false, false, SEL, STA, &f); }
        for (int fr = 1; fr <= 40; fr++) {
            SDL_JoystickSetVirtualButton(j, SDL_CONTROLLER_BUTTON_START, fr <= 4);
            SDL_GameControllerUpdate();
            uint8_t out = chord_step(&c, SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_BACK),
                                     SDL_GameControllerGetButton(gc, SDL_CONTROLLER_BUTTON_START), SEL, STA, &f);
            CHECK(!f);
            if (out & STA) dt++;
        }
        CHECK(dt == 4);
        SDL_GameControllerClose(gc);
    }
    SDL_JoystickDetachVirtual(idx);
    /* raw fallback: a virtual joystick with no controller mapping is read with the xpad layout */
    SDL_VirtualJoystickDesc r;
    memset(&r, 0, sizeof r);
    r.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    r.type = SDL_JOYSTICK_TYPE_UNKNOWN;
    r.naxes = 2; r.nbuttons = 11; r.nhats = 1;
    r.vendor_id = 0x1234; r.product_id = 0x5678;
    r.name = "Unmapped test joystick";
    int ridx = SDL_JoystickAttachVirtualEx(&r);
    CHECK(ridx >= 0);
    if (ridx >= 0) {
        SDL_Joystick *rj = SDL_JoystickOpen(ridx);
        CHECK(rj != NULL);
        if (rj) {
            printf("  raw: IsGameController=%d buttons=%d\n", SDL_IsGameController(ridx), SDL_JoystickNumButtons(rj));
            SDL_JoystickSetVirtualButton(rj, PADS_RAW_BACK, 1);
            SDL_JoystickSetVirtualButton(rj, PADS_RAW_START, 1);
            SDL_JoystickSetVirtualHat(rj, 0, SDL_HAT_LEFT);
            SDL_JoystickUpdate();
            uint32_t bt = 0;
            for (int i = 0; i < SDL_JoystickNumButtons(rj); i++) if (SDL_JoystickGetButton(rj, i)) bt |= 1u << i;
            bool back, start;
            uint8_t b = pads_raw_layout(bt, SDL_JoystickGetHat(rj, 0), SDL_JoystickGetAxis(rj, 0), SDL_JoystickGetAxis(rj, 1), &back, &start, NULL, NULL);
            CHECK(back && start); CHECK(b == (SEL | STA | 0x40));
            SDL_JoystickClose(rj);
        }
        SDL_JoystickDetachVirtual(ridx);
    }
#else
    printf("  skip: SDL < 2.24\n");
#endif
    SDL_Quit();
}

int main(void)
{
    printf("filter\n");
    test_filter();
    printf("policy\n");
    test_policy();
    printf("sdl virtual device under Steam hints\n");
    test_sdl_virtual();
    printf("back+start chord\n");
    test_chord();
    printf("raw joystick layout\n");
    test_raw_layout();
    printf("sdl virtual x360 pad drives the chord; raw fallback\n");
    test_sdl_chord();
    printf(failures ? "FAILED (%d)\n" : "all pad tests passed\n", failures);
    return failures ? 1 : 0;
}
