#ifndef MM_RESTORATION_H
#define MM_RESTORATION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Partial, source-supported references; no claim of a complete beta restoration. */
typedef struct { bool ed_dialogue, scumm_credits; } restoration_options_t;
typedef enum {
    RESTORATION_OFF, RESTORATION_APPLIED, RESTORATION_BAD_SIZE,
    RESTORATION_WRONG_ROM, RESTORATION_BAD_SIGNATURE
} restoration_result_t;
/* Boot only, on a disposable host-owned PRG copy, never cart.file/cart.prg.
 * Original ignores every option. All selected signatures are checked before
 * any write; resource lengths, instructions and control bytes stay unchanged. */
restoration_result_t restoration_apply(uint8_t *prg, size_t size,
                                      bool enhanced, restoration_options_t options);
const char *restoration_result_name(restoration_result_t result);
/* Bind Enhanced snapshots to the actual optional text profile. */
void restoration_identity(const uint8_t *prg, size_t size, uint8_t out[8]);
#endif
