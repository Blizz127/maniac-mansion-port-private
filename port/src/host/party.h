#ifndef MM_PARTY_H
#define MM_PARTY_H
#include "../core/nes.h"
#include <stddef.h>
#include <stdint.h>

#define PARTY_HISTORY_BYTES 8
#define PARTY_HISTORY_VERSION 1
/* Stable wire order is exactly these eight bytes. Bit n identifies actor n+1. */
typedef struct {
    uint8_t version, valid, activated_mask, retired_mask, starter_issued_mask;
    uint8_t initial_roster[3];
} party_history_t;
typedef enum {
    PARTY_OK, PARTY_ORIGINAL, PARTY_DISABLED, PARTY_HISTORY_UNKNOWN,
    PARTY_BAD_ROSTER, PARTY_DAVE_REQUIRED, PARTY_NOT_TEAMMATE, PARTY_ALREADY_SELECTED,
    PARTY_RETURN_TO_ROAD, PARTY_SELECT_DAVE, PARTY_ACTOR_UNAVAILABLE,
    PARTY_STORY_BUSY, PARTY_SENTENCE_BUSY, PARTY_MOVING, PARTY_INPUT_HELD,
    PARTY_STARTER_UNSAFE, PARTY_PENDING, PARTY_NATIVE_FAILED
} party_result_t;
typedef struct {
    party_history_t history;
    bool enhanced, enabled, neutral;
    bool pending, executing, changed, fresh_observed, load_observed;
    party_result_t result;
    uint8_t outgoing, incoming, roster_slot, constructor_phase, call_phase;
    uint8_t spawn_x, spawn_y, old_bank, old_ego;
    uint8_t saved_a, saved_x, saved_y, saved_s, saved_p, saved_stack[2];
    uint8_t saved_zp[256], saved_scratch[256];
    uint64_t call_cycles;
    nes_t *rollback; /* allocated only during a queued Enhanced transaction */
} party_state_t;
void party_init(party_state_t *state);
/* Cancel with the NES first if a transaction is executing. */
void party_destroy(party_state_t *state);
/* Cancel/roll back an unfinished transaction before exit or state replacement. */
void party_cancel(nes_t *nes, party_state_t *state);
const char *party_result_name(party_result_t result);
const char *party_result_short(party_result_t result);
const char *party_actor_name(unsigned actor);
bool party_history_encode(const party_history_t *history, uint8_t out[PARTY_HISTORY_BYTES]);
bool party_history_decode(party_history_t *history, const uint8_t *bytes, size_t size);
/* Explicit trusted metadata restore; cancels pending transaction state. */
bool party_restore_history(party_state_t *state, const uint8_t *bytes, size_t size);
/* Host state only. Called after q input adjudication, never binds exec_hook. */
void party_update(party_state_t *state, bool enhanced, bool enabled, bool neutral);
party_result_t party_check(const nes_t *nes, const party_state_t *state,
                           unsigned outgoing, unsigned incoming);
party_result_t party_request(nes_t *nes, party_state_t *state,
                             unsigned outgoing, unsigned incoming);
/* Host dispatcher's every-instruction observer; Original and idle OFF never write NES.
 * Enhanced observer establishes history only from actual native construction. */
void party_exec(nes_t *nes, party_state_t *state);
#endif
