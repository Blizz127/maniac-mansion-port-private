#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/host/mode.h"
#include "../src/core/nes.h"

static const SDL_Scancode keys[8] = {SDL_SCANCODE_X, SDL_SCANCODE_Z, SDL_SCANCODE_RSHIFT,
    SDL_SCANCODE_RETURN, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT};

typedef struct { unsigned cursor; uint8_t buttons[5]; } scripted_pad_t;
static uint8_t scripted_pad(void *userdata)
{
    scripted_pad_t *pad = userdata;
    assert(pad->cursor < 5);
    return pad->buttons[pad->cursor++];
}

int main(void)
{
    mm_mode_t selected = MM_MODE_ENHANCED;
    assert(mode_parse("original", &selected) && selected == MM_MODE_ORIGINAL);
    assert(mode_parse("enhanced", &selected) && selected == MM_MODE_ENHANCED);
    assert(!mode_parse("remastered", &selected));
    assert(!mode_parse(NULL, &selected));
    char battery[256], quick[256];
    assert(mode_save_paths("/disposable", MM_MODE_ORIGINAL, battery, sizeof battery, quick, sizeof quick));
    assert(!strcmp(battery, "/disposable/saves/maniac-mansion-usa.sav"));
    assert(!strcmp(quick, "/disposable/quicksave/slot1.mmst"));
    assert(mode_save_paths("/disposable", MM_MODE_ENHANCED, battery, sizeof battery, quick, sizeof quick));
    assert(!strcmp(battery, "/disposable/saves/enhanced/maniac-mansion-usa.sav"));
    assert(!strcmp(quick, "/disposable/quicksave/enhanced/slot1.mmst"));
    assert(!mode_save_paths("/disposable", MM_MODE_ORIGINAL, battery, 2, quick, sizeof quick));
    mode_menu_t controller, keyboard;
    mode_menu_init(&controller);
    mode_menu_init(&keyboard);
    assert(controller.selected == MM_MODE_ORIGINAL && !controller.confirmed);
    mode_menu_input(&controller, PAD_A); /* Ignore A already held on entry. */
    assert(!controller.confirmed);
    mode_menu_input(&controller, 0);
    mode_menu_input(&keyboard, 0);
    uint8_t held[SDL_NUM_SCANCODES] = {0};
    held[SDL_SCANCODE_DOWN] = 1;
    mode_menu_input(&keyboard, mode_keyboard_buttons(held, keys));
    mode_menu_input(&controller, PAD_DOWN);
    assert(keyboard.selected == controller.selected && controller.selected == MM_MODE_ENHANCED);
    mode_menu_input(&controller, PAD_DOWN); /* Held direction never repeats. */
    assert(controller.selected == MM_MODE_ENHANCED);
    mode_menu_input(&controller, 0);
    memset(held, 0, sizeof held);
    mode_menu_input(&keyboard, mode_keyboard_buttons(held, keys));
    held[SDL_SCANCODE_RETURN] = 1;
    mode_menu_input(&keyboard, mode_keyboard_buttons(held, keys));
    mode_menu_input(&controller, PAD_A);
    assert(keyboard.confirmed && controller.confirmed && keyboard.selected == controller.selected);
    mode_menu_init(&keyboard); /* Every new boot starts Original. */
    mode_menu_input(&keyboard, 0);
    mode_menu_input(&keyboard, PAD_DOWN);
    mode_menu_input(&keyboard, 0);
    memset(held, 0, sizeof held);
    held[SDL_SCANCODE_ESCAPE] = 1;
    mode_menu_input(&keyboard, mode_keyboard_buttons(held, keys));
    assert(keyboard.confirmed && keyboard.selected == MM_MODE_ORIGINAL);

    /* Real snapshot loader rejects copied cross-mode states; original IDs
     * are unchanged so pre-existing Original snapshots remain compatible. */
    uint8_t retail[8] = {0, 1, 2, 3, 4, 5, 6, 7}, original[8], enhanced[8];
    mode_snapshot_id(retail, MM_MODE_ORIGINAL, original);
    mode_snapshot_id(retail, MM_MODE_ENHANCED, enhanced);
    assert(!memcmp(original, retail, sizeof retail));
    assert(memcmp(original, enhanced, sizeof original));
    nes_t *nes = calloc(1, sizeof *nes);
    uint8_t *prg = calloc(32768, 1), *snapshot = malloc(nes_state_size());
    assert(nes && prg && snapshot);
    nes_init(nes, prg, 32768);
    nes_power(nes, 0);
    nes_state_save(nes, snapshot, original);
    assert(!nes_state_load(nes, snapshot, nes_state_size(), enhanced));
    assert(nes_state_load(nes, snapshot, nes_state_size(), original));
    nes_state_save(nes, snapshot, enhanced);
    assert(!nes_state_load(nes, snapshot, nes_state_size(), original));
    assert(nes_state_load(nes, snapshot, nes_state_size(), enhanced));
    free(snapshot); free(prg); free(nes);

    /* The real chooser exits on SDL_QUIT without selecting a mode. It has
     * no ROM/save access itself; main invokes it before power/save_open. */
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    assert(!SDL_Init(SDL_INIT_VIDEO));
    SDL_Window *window = SDL_CreateWindow("isolated mode test", 0, 0, 256, 240, SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    scripted_pad_t pad = {.buttons = {0, PAD_DOWN, 0, PAD_A, 0}};
    selected = MM_MODE_ORIGINAL;
    assert(mode_choose(renderer, keys, scripted_pad, NULL, &pad, &selected));
    assert(selected == MM_MODE_ENHANCED && pad.cursor == 5);
    pad = (scripted_pad_t){.buttons = {0, PAD_DOWN, 0, PAD_B, 0}};
    assert(mode_choose(renderer, keys, scripted_pad, NULL, &pad, &selected));
    assert(selected == MM_MODE_ORIGINAL && pad.cursor == 5);
    SDL_Event quit = {.type = SDL_QUIT};
    assert(SDL_PushEvent(&quit) == 1);
    selected = MM_MODE_ENHANCED;
    assert(!mode_choose(renderer, keys, NULL, NULL, NULL, &selected));
    assert(selected == MM_MODE_ENHANCED);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    puts("mode: per-boot Original, neutral/edge input, keyboard/controller parity, namespaces, cross-mode snapshot rejection, chooser quit PASS");
    return 0;
}
