#ifndef MM_SNAPWALK_H
#define MM_SNAPWALK_H
#include <stdbool.h>
#include <stdint.h>

/* Enhanced snap combo and direct walk: native host policy plus a semantic
 * pointer/walk request. New code here never executes or redirects guest
 * code; the interpreted retail engine consumes the request through its own
 * pointer and A-latch input path exactly as it consumes a player click.
 *
 * Snap combo (object_snap): the d-pad always moves the native free cursor.
 * Holding B and pressing a direction snaps to the nearest visible object in
 * that direction. B is withheld while held; a lone B tap is delivered as one
 * native B edge on release, so B keeps its original action.
 *
 * Direct walk (direct_walk): while A is held with native Walk to selected and
 * the pointer in the scene, the d-pad walks the current kid in that direction
 * and the cursor stays put. Each request is an ordinary native walk-to-point
 * a short leg ahead in that direction, so the retail walk speed, walk boxes
 * and path logic decide the motion. A leg is requested only while the kid is
 * idle: script 4 stalls or defers a click made during a running walk. The
 * A press of the gesture is held back (at most SW_A_GRACE_MS) so it never
 * also clicks at the pointer. After release the kid finishes its leg. */

typedef struct {
    uint8_t previous;
    bool b_pending, b_used;
} sw_snap_t;
typedef struct {
    uint8_t pad;      /* input after consuming B and/or directions */
    bool pulse_b;     /* deliver one native B edge (lone tap released) */
    bool snap;        /* snap in direction `dx`/`dy` */
    int dx, dy;
} sw_snap_result_t;
void sw_snap_reset(sw_snap_t *s);
sw_snap_result_t sw_snap_step(sw_snap_t *s, uint8_t pad);

typedef struct {
    bool walk_verb, pointer_in_scene, sentence_pending, blocked;
    bool kid_idle; /* native walk flags clear: previous request finished */
} sw_walk_context_t;
typedef struct {
    uint8_t previous;
    bool a_pending, swallow_a, walking;
    uint64_t a_since, last_ms;
    int dx, dy;
} sw_walk_t;
typedef struct {
    uint8_t pad;      /* input with A and/or directions consumed */
    bool pulse_a;     /* deliver the withheld native A edge now */
    bool request;     /* issue a walk leg in dx/dy */
    int dx, dy;
} sw_walk_result_t;
#define SW_WALK_REPEAT_MS 100u
#define SW_A_GRACE_MS 100u /* a plain A tap is delivered at most this late */
void sw_walk_reset(sw_walk_t *s);
sw_walk_result_t sw_walk_step(sw_walk_t *s, sw_walk_context_t c, uint8_t pad, uint64_t now_ms);

/* Pure target choice in native scene units (8 px columns, 2 px rows): up to
 * `reach` steps of 4 columns / 8 rows ahead, clamped to the visible scene.
 * occupied(x, y) reports a native object hit at that scene point; the target
 * shrinks so a floor walk never becomes "walk to object". */
#define SW_REACH_WALK 4
bool sw_walk_target(int ax, int ay, int dx, int dy, int camera, int reach,
                    bool (*occupied)(void *, int, int), void *ud, int *tx, int *ty);

/* USA-only native adapter. Caller verifies ROM identity. */
#include "../core/nes.h"
typedef struct {
    sw_snap_t snap;
    sw_walk_t walk;
    bool pending;          /* pointer override active until the engine reads it */
    int target_x, target_y;
    unsigned room, actor;  /* origin of the pending request */
    unsigned pending_frames;
    unsigned requests, consumed, timeouts, cancels; /* diagnostics */
} sw_native_t;
#define SW_PENDING_FRAMES 60 /* script 4 may defer a click while the kid is busy */
void sw_native_init(sw_native_t *s);
bool sw_native_actor(const nes_t *n, int *x, int *y);
/* Every instruction: applies the pointer override at the native pointer
 * conversion return and observes the engine's walk-target read. Reads PC and
 * writes only pointer scene bytes; never changes CPU registers or PC. A
 * pending request is dropped if the room, selected kid or input control
 * changes, so it never steers another scene or kid. */
void sw_native_exec(nes_t *n, sw_native_t *s);
/* Issue a native walk-to-point request at scene (x, y). */
bool sw_native_request(nes_t *n, sw_native_t *s, int x, int y);
#endif
