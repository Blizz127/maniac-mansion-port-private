#include "mode.h"
#include "../core/nes.h"
#include "font8x8.h"
#include <stdio.h>
#include <string.h>

bool mode_parse(const char *text, mm_mode_t *mode)
{
    if (!text || !mode) return false;
    if (!strcmp(text, "original")) *mode = MM_MODE_ORIGINAL;
    else if (!strcmp(text, "enhanced")) *mode = MM_MODE_ENHANCED;
    else return false;
    return true;
}

const char *mode_name(mm_mode_t mode)
{
    return mode == MM_MODE_ENHANCED ? "ENHANCED / REMASTERED" : "ORIGINAL ROM";
}

bool mode_save_paths(const char *data_dir, mm_mode_t mode,
                     char *battery, size_t battery_size, char *quick, size_t quick_size)
{
    if (!data_dir || !battery || !quick || !battery_size || !quick_size) return false;
    const char *lane = mode == MM_MODE_ENHANCED ? "/enhanced" : "";
    int b = snprintf(battery, battery_size, "%s/saves%s/maniac-mansion-usa.sav", data_dir, lane);
    int q = snprintf(quick, quick_size, "%s/quicksave%s/slot1.mmst", data_dir, lane);
    return b >= 0 && (size_t)b < battery_size && q >= 0 && (size_t)q < quick_size;
}

void mode_snapshot_id(const uint8_t retail_id[8], mm_mode_t mode, uint8_t out[8])
{
    static const uint8_t enhanced_domain[8] = {'E','N','H','A','N','C','E','D'};
    for (unsigned i = 0; i < 8; i++)
        out[i] = retail_id[i] ^ (mode == MM_MODE_ENHANCED ? enhanced_domain[i] : 0);
}

void mode_menu_init(mode_menu_t *menu)
{
    *menu = (mode_menu_t){.selected = MM_MODE_ORIGINAL, .neutral_required = true};
}

void mode_menu_input(mode_menu_t *menu, uint8_t buttons)
{
    if (menu->neutral_required) {
        if (!buttons) menu->neutral_required = false;
        menu->previous = buttons;
        return;
    }
    if (menu->confirmed) return;
    uint8_t edge = buttons & ~menu->previous;
    menu->previous = buttons;
    if (edge & PAD_B) { menu->selected = MM_MODE_ORIGINAL; menu->confirmed = true; }
    else if (edge & (PAD_A | PAD_START)) menu->confirmed = true;
    else if (edge & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT))
        menu->selected = menu->selected == MM_MODE_ORIGINAL ? MM_MODE_ENHANCED : MM_MODE_ORIGINAL;
}

uint8_t mode_keyboard_buttons(const uint8_t *keyboard, const SDL_Scancode keys[8])
{
    uint8_t buttons = 0;
    for (int i = 0; i < 8; i++) if (keyboard[keys[i]]) buttons |= (uint8_t)(1u << i);
    if (keyboard[SDL_SCANCODE_RETURN]) buttons |= PAD_START;
    if (keyboard[SDL_SCANCODE_ESCAPE]) buttons |= PAD_B;
    if (keyboard[SDL_SCANCODE_UP]) buttons |= PAD_UP;
    if (keyboard[SDL_SCANCODE_DOWN]) buttons |= PAD_DOWN;
    if (keyboard[SDL_SCANCODE_LEFT]) buttons |= PAD_LEFT;
    if (keyboard[SDL_SCANCODE_RIGHT]) buttons |= PAD_RIGHT;
    return buttons;
}

static void text(SDL_Renderer *r, int width, int y, int scale, const char *s)
{
    int x = (width - (int)strlen(s) * 8 * scale) / 2;
    for (; *s; s++, x += 8 * scale) {
        unsigned c = (unsigned char)*s;
        if (c >= 128) continue;
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                if (font8x8[c][row] & (1 << col)) {
                    SDL_Rect pixel = {x + col * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(r, &pixel);
                }
    }
}

void mode_menu_draw(SDL_Renderer *r, const mode_menu_t *menu)
{
    int w, h;
    SDL_GetRendererOutputSize(r, &w, &h);
    int scale = w / (38 * 8);
    if (scale < 1) scale = 1;
    if (scale > 3) scale = 3;
    int line = 12 * scale, y = h / 2 - line * 5;
    SDL_SetRenderDrawColor(r, 12, 12, 20, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawColor(r, 240, 240, 224, 255);
    text(r, w, y, scale, "MANIAC MANSION");
    text(r, w, y + line * 2, scale, "CHOOSE GAME MODE");
    SDL_SetRenderDrawColor(r, menu->selected == MM_MODE_ORIGINAL ? 255 : 140,
                           menu->selected == MM_MODE_ORIGINAL ? 220 : 140, 100, 255);
    text(r, w, y + line * 4, scale, menu->selected == MM_MODE_ORIGINAL ? "> ORIGINAL ROM <" : "ORIGINAL ROM");
    SDL_SetRenderDrawColor(r, menu->selected == MM_MODE_ENHANCED ? 255 : 140,
                           menu->selected == MM_MODE_ENHANCED ? 220 : 140, 100, 255);
    text(r, w, y + line * 6, scale, menu->selected == MM_MODE_ENHANCED ? "> ENHANCED / REMASTERED <" : "ENHANCED / REMASTERED");
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    text(r, w, y + line * 8, scale, "ARROWS / D-PAD: SELECT");
    text(r, w, y + line * 9, scale, "ENTER / A / START: PLAY");
    text(r, w, y + line * 10, scale, "ESC / B: PLAY ORIGINAL");
    /* Small detail text fits the same minimum content width. */
    text(r, w, y + line * 12, 1, "ENHANCED: SEPARATE SAVES");
    SDL_RenderPresent(r);
}

bool mode_choose(SDL_Renderer *renderer, const SDL_Scancode keys[8],
                 uint8_t (*pad_poll)(void *), void (*device_event)(void *, const SDL_Event *),
                 void *userdata, mm_mode_t *selected)
{
    mode_menu_t menu;
    mode_menu_init(&menu);
    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) return false;
            if (device_event) device_event(userdata, &event);
        }
        const uint8_t *keyboard = SDL_GetKeyboardState(NULL);
        uint8_t buttons = pad_poll ? pad_poll(userdata) : 0;
        /* Enter/arrows/Escape are also available with custom game bindings. */
        buttons |= mode_keyboard_buttons(keyboard, keys);
        mode_menu_input(&menu, buttons);
        mode_menu_draw(renderer, &menu);
        if (menu.confirmed && !buttons) { *selected = menu.selected; return true; }
        SDL_Delay(8);
    }
}
