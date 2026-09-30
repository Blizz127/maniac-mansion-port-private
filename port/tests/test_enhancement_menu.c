#include "../src/host/devmenu.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    dm_state_t s = {.enhanced_mode = true};
    devmenu_t *m = devmenu_create(&s, NULL); assert(m);
    assert(!s.smooth_display && !s.graphics_sample);
    devmenu_toggle(m);
    for (int i=0; i<3; ++i) devmenu_nav(m, DM_DOWN);
    devmenu_nav(m, DM_OK); /* Options */
    devmenu_nav(m, DM_OK); /* Enhancements */
    bool *choices[] = {&s.hold_walk, &s.object_snap, &s.quick_verbs, &s.smooth_display, &s.graphics_sample};
    for (int i=0; i<5; ++i) {
        assert(!*choices[i]);
        devmenu_nav(m, DM_OK); assert(*choices[i]);
        devmenu_nav(m, DM_OK); assert(!*choices[i]);
        devmenu_nav(m, DM_DOWN);
    }
    devmenu_nav(m, DM_BACK); devmenu_nav(m, DM_BACK); devmenu_nav(m, DM_BACK);
    assert(!devmenu_is_open(m));
    assert(SDL_setenv("SDL_VIDEODRIVER", "dummy", 1) == 0);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_Window *w = SDL_CreateWindow("isolated menu test", 0, 0, 320, 240, 0); assert(w);
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE); assert(r);
    devmenu_qol_draw(m, r, (SDL_Rect){0, 0, 256, 240}, 0, true, "Push", false, 0, 0);
    devmenu_qol_draw(m, r, (SDL_Rect){0, 0, 256, 240}, 0, false, "", true, 120, 80);
    SDL_RenderPresent(r);
    devmenu_free(m); /* texture cleanup must precede renderer/video teardown */
    /* Original never offers an enhancement entry, even with stale ON values. */
    dm_state_t original = {.hold_walk = true, .object_snap = true, .quick_verbs = true,
                           .smooth_display = true, .graphics_sample = true};
    m = devmenu_create(&original, NULL); assert(m);
    devmenu_toggle(m);
    for (int i=0; i<3; ++i) devmenu_nav(m, DM_DOWN);
    devmenu_nav(m, DM_OK); /* Options */
    devmenu_nav(m, DM_OK); /* Original's first item is fast-forward, not Enhancements. */
    assert(original.ff_on);
    assert(original.hold_walk && original.object_snap && original.quick_verbs);
    assert(original.smooth_display && original.graphics_sample);
    devmenu_free(m);
    SDL_DestroyRenderer(r); SDL_DestroyWindow(w); SDL_Quit();
    puts("enhancements menu: PASS (default OFF, independent toggles, return OFF, navigation)");
    return 0;
}
