#ifndef MM_ENHANCED_SAVE_H
#define MM_ENHANCED_SAVE_H

#include "../core/nes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENHANCED_METADATA_MAX 128u
typedef enum {
    ENHANCED_LOAD_FAILED = 0,
    ENHANCED_LOAD_LEGACY = 1, /* Core restored, metadata_len=0: history untrusted. */
    ENHANCED_LOAD_VERIFIED = 2
} enhanced_load_result_t;

/* Enhanced-only caller supplies its mode-domain snapshot ID and namespace.
 * Original continues to use the existing raw core save/load API. Metadata
 * is opaque, <=128 bytes; its semantic/version checks belong to the caller.
 * No directories/imports are created. Atomic replacement uses path.tmp.
 * FAILED reads preserve nes, metadata bytes and *metadata_len. */
bool enhanced_snapshot_save(const char *path, const uint8_t id[8], const nes_t *nes,
                            const void *metadata, size_t metadata_len);
enhanced_load_result_t enhanced_snapshot_load(const char *path, const uint8_t id[8], nes_t *nes,
                                              void *metadata, size_t metadata_capacity,
                                              size_t *metadata_len);

/* Sidecar trust requires checksum + SHA256 of all 8192 battery bytes.
 * Write after committing the corresponding Enhanced battery file. Any
 * missing/corrupt/mismatching read is false and preserves output metadata.
 * It does not infer native save/load events or activation history itself. */
bool enhanced_battery_metadata_write(const char *path, const uint8_t prgram[8192],
                                     const void *metadata, size_t metadata_len);
bool enhanced_battery_metadata_read(const char *path, const uint8_t prgram[8192],
                                    void *metadata, size_t metadata_capacity,
                                    size_t *metadata_len);

#endif
