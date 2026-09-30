#ifndef MM_QOL_INPUT_H
#define MM_QOL_INPUT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Host policy only. A verified guest adapter must supply these facts and execute
 * resulting intents through native input/UI. Pure policy never writes NES RAM;
 * the explicitly bound USA adapter below updates verified cursor/input fields. */
typedef struct { bool hold_walk, object_snap, quick_verbs; } qol_options_t;
typedef struct {
    bool gameplay, input_enabled, menu, cutscene, chord, walk_verb;
    bool free_walk, sentence_pending;
    uint32_t epoch; /* change on room, actor, load, focus, or control-owner change */
    int cursor_x, cursor_y;
} qol_context_t;
typedef struct {
    uint8_t previous;
    bool blocked, holding_walk, select_pending, chooser;
    uint64_t a_since, select_since;
    uint32_t epoch;
    int walk_x, walk_y;
    unsigned verb_index;
} qol_input_t;
typedef enum {
    QOL_WALK, QOL_PUSH, QOL_PULL, QOL_GIVE, QOL_OPEN, QOL_CLOSE,
    QOL_PICK_UP, QOL_USE, QOL_READ, QOL_TURN_ON, QOL_TURN_OFF, QOL_NEW_KID, QOL_COUNT
} qol_verb_t;
typedef struct {
    uint8_t pad; /* native input; no generated repeated A edges */
    bool walk_update, chooser_open, chooser_closed, select_verb;
    int walk_x, walk_y;
    qol_verb_t verb;
} qol_result_t;
void qol_input_reset(qol_input_t *state);
qol_result_t qol_input_step(qol_input_t *state, qol_options_t options,
                           qol_context_t context, uint8_t pad, uint64_t now_ms);
const char *qol_verb_name(qol_verb_t verb);

typedef struct {
    uint32_t id;
    int x, y; /* point confirmed by native hit-test, not guessed object center */
    bool visible, interactive, hit_visible;
} qol_target_t;
typedef enum { QOL_UP, QOL_DOWN, QOL_LEFT, QOL_RIGHT } qol_direction_t;
/* Returns false without a valid visible candidate in the requested half-plane.
 * Coordinates and viewport are screen pixels; never supply undiscovered objects. */
bool qol_snap(const qol_target_t *targets, size_t count, uint32_t current_id,
              int x, int y, int left, int top, int right, int bottom,
              qol_direction_t direction, qol_target_t *selected);
/* USA-only adapter. Caller verifies ROM identity before use, runs once per
 * emulated frame, and never invokes during host menus, trace playback or load. */
#include "../core/nes.h"
bool qol_native_ready(const nes_t *nes);
size_t qol_native_targets(const nes_t *nes, qol_target_t *out, size_t capacity);
bool qol_native_cursor(nes_t *nes, int x, int y);
bool qol_native_verb_cursor(nes_t *nes, unsigned native_verb_id);
typedef struct {
    qol_input_t policy;
    qol_options_t options;
    uint8_t input;
    bool permitted, restore_cursor;
    int saved_x, saved_y;
    uint32_t selected_id;
    uint64_t last_walk_ms;
    qol_target_t selected;
    bool highlight;
} qol_native_t;
void qol_native_init(qol_native_t *state);
/* Call with already adjudicated keyboard|pad input. permitted=false for menus,
 * focus drain, traces, unknown ROM, loads, and other host-owned contexts. */
void qol_native_update(nes_t *nes, qol_native_t *state, qol_options_t options,
                       uint8_t input, bool permitted);
#endif
