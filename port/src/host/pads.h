/* Controller selection (DEV_MENU_SPEC §4 and §4a).
 *
 * Under Steam Game Mode or InputPlumber the process inherits an SDL ignore
 * list that hides the physical Deck / Legion Go pads, leaving only Steam's
 * virtual pad, which is silent unless Steam Input is on for the shortcut.
 * pads_fix_environment() removes those entries before SDL starts. The pad
 * set then picks the active pad: a physical pad that has produced input,
 * the Steam virtual pad only when it is the only one producing input, never
 * a silent device. Pure logic; the SDL glue is in main.c. */
#ifndef MM_PADS_H
#define MM_PADS_H

#include <stdbool.h>
#include <stdint.h>

/* Returns a malloc'd copy of an SDL_GAMECONTROLLER_IGNORE_DEVICES-style
 * list ("0xVVVV/0xPPPP,...") without the entries §4a says to un-hide. */
char *pads_filter_ignore_list(const char *list);

/* Rewrites SDL_GAMECONTROLLER_IGNORE_DEVICES(_EXCEPT) in the process
 * environment and logs before/after. Call before SDL_Init. */
void pads_fix_environment(void (*log)(const char *line));

bool pads_is_steam_virtual(uint16_t vid, uint16_t pid, const char *name);

enum { PADS_MAX = 16 };

typedef struct {
    int32_t instance;
    uint16_t vid, pid;
    bool is_virtual;
    bool had_input;
    bool need_neutral; /* connected (or re-added) with buttons held */
    char name[96];
} pad_dev_t;

typedef struct {
    pad_dev_t dev[PADS_MAX];
    int n;
    int32_t active; /* instance id, or -1 */
    void (*log)(const char *line);
} pad_set_t;

void pads_init(pad_set_t *s, void (*log)(const char *line));
void pads_added(pad_set_t *s, int32_t instance, uint16_t vid, uint16_t pid, const char *name, bool held);
void pads_removed(pad_set_t *s, int32_t instance);
/* Report the pad's current state; nonzero = something pressed/deflected.
 * Returns true if this pad's input should drive the game. */
bool pads_input(pad_set_t *s, int32_t instance, bool non_neutral);
const pad_dev_t *pads_get(const pad_set_t *s, int32_t instance);

#endif

/* Raw-joystick fallback for pads SDL has no controller mapping for: the Linux
 * X-Box 360 / xpad button order (0 A, 1 B, 2 X, 3 Y, 4 LB, 5 RB, 6 Back,
 * 7 Start, 8 Guide, 9 L3, 10 R3), hat 0 as the D-pad (SDL_HAT_* bits: 1 up,
 * 2 right, 4 down, 8 left) and axes 0/1 as the left stick. Returns the NES
 * pad bits (PAD_* order: A B SELECT START UP DOWN LEFT RIGHT) and reports
 * Back/Start and L3/R3 separately. Pure logic for the tests; SDL reads in main.c. */
enum { PADS_RAW_A = 0, PADS_RAW_B = 1, PADS_RAW_X = 2, PADS_RAW_BACK = 6, PADS_RAW_START = 7, PADS_RAW_L3 = 9, PADS_RAW_R3 = 10 };
uint8_t pads_raw_layout(uint32_t buttons, uint8_t hat, int ax, int ay, bool *back, bool *start, bool *l3, bool *r3);
