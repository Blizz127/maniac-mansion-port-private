#ifndef MM_MODE_H
#define MM_MODE_H

#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { MM_MODE_ORIGINAL, MM_MODE_ENHANCED } mm_mode_t;
typedef struct {
    mm_mode_t selected;
    uint8_t previous;
    bool neutral_required, confirmed;
} mode_menu_t;

bool mode_parse(const char *text, mm_mode_t *mode);
const char *mode_name(mm_mode_t mode);
bool mode_save_paths(const char *data_dir, mm_mode_t mode,
                     char *battery, size_t battery_size, char *quick, size_t quick_size);
void mode_snapshot_id(const uint8_t retail_id[8], mm_mode_t mode, uint8_t out[8]);
void mode_menu_init(mode_menu_t *menu);
/* Standard NES pad bits; B/Escape confirms Original. Confirmation is on an
 * edge after neutral, preventing an already-held key from selecting a mode. */
void mode_menu_input(mode_menu_t *menu, uint8_t buttons);
uint8_t mode_keyboard_buttons(const uint8_t *keyboard, const SDL_Scancode keys[8]);
void mode_menu_draw(SDL_Renderer *renderer, const mode_menu_t *menu);
/* Call before powering the core/opening saves. Uses host's existing device
 * handling, supports hotplug, and drains selection buttons before returning. */
bool mode_choose(SDL_Renderer *renderer, const SDL_Scancode keys[8],
                 uint8_t (*pad_poll)(void *), void (*device_event)(void *, const SDL_Event *),
                 void *userdata, mm_mode_t *selected);

#endif
