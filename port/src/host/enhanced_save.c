#include "enhanced_save.h"
#include "../core/sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Explicit little-endian lengths/version: no host struct padding in the
 * wrapper. The embedded nes_state_save bytes retain their existing format.
 * SHA256 covers header + both payloads, followed by a 32-byte digest. */
static const uint8_t snapshot_magic[8] = {'M','M','E','N','H','S','1',0};
static const uint8_t battery_magic[8] = {'M','M','E','H','B','A','T','1'};
enum { SNAPSHOT_HEADER = 20, BATTERY_HEADER = 48, DIGEST_SIZE = 32, FORMAT_VERSION = 1 };

static void put32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* Same temporary-file/rename operation as the existing host write_atomic.
 * Its main.c helper is private; this independent module keeps a bounded,
 * dynamically sized path instead of exposing or modifying that helper. */
static bool write_atomic(const char *path, const void *data, size_t n)
{
    if (!path || !*path || strlen(path) > 4096) return false;
    size_t size = strlen(path) + 5;
    char *tmp = malloc(size);
    if (!tmp) return false;
    snprintf(tmp, size, "%s.tmp", path);
    FILE *file = fopen(tmp, "wb");
    if (!file) { free(tmp); return false; }
    bool ok = fwrite(data, 1, n, file) == n;
    ok = (fclose(file) == 0) && ok;
    if (ok) ok = rename(tmp, path) == 0;
    if (!ok) remove(tmp);
    free(tmp);
    return ok;
}

static uint8_t *read_bounded(const char *path, size_t limit, size_t *size)
{
    if (!path) return NULL;
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    uint8_t *data = malloc(limit + 1);
    if (!data) { fclose(file); return NULL; }
    size_t n = fread(data, 1, limit + 1, file);
    bool ok = !ferror(file) && n <= limit;
    ok = (fclose(file) == 0) && ok;
    if (!ok) { free(data); return NULL; }
    *size = n;
    return data;
}

static bool metadata_valid(const void *metadata, size_t length)
{
    return length <= ENHANCED_METADATA_MAX && (!length || metadata);
}

static bool checksum_valid(const uint8_t *data, size_t size)
{
    if (size < DIGEST_SIZE) return false;
    uint8_t digest[DIGEST_SIZE];
    sha256(data, size - DIGEST_SIZE, digest);
    return !memcmp(digest, data + size - DIGEST_SIZE, DIGEST_SIZE);
}

bool enhanced_snapshot_save(const char *path, const uint8_t id[8], const nes_t *nes,
                            const void *metadata, size_t metadata_len)
{
    size_t core_len = nes_state_size();
    if (!id || !nes || core_len > UINT32_MAX || !metadata_valid(metadata, metadata_len)) return false;
    size_t size = SNAPSHOT_HEADER + core_len + metadata_len + DIGEST_SIZE;
    uint8_t *data = malloc(size);
    if (!data) return false;
    memcpy(data, snapshot_magic, sizeof snapshot_magic);
    put32(data + 8, FORMAT_VERSION);
    put32(data + 12, (uint32_t)core_len);
    put32(data + 16, (uint32_t)metadata_len);
    nes_state_save(nes, data + SNAPSHOT_HEADER, id);
    if (metadata_len) memcpy(data + SNAPSHOT_HEADER + core_len, metadata, metadata_len);
    sha256(data, size - DIGEST_SIZE, data + size - DIGEST_SIZE);
    bool ok = write_atomic(path, data, size);
    free(data);
    return ok;
}

enhanced_load_result_t enhanced_snapshot_load(const char *path, const uint8_t id[8], nes_t *nes,
                                              void *metadata, size_t metadata_capacity,
                                              size_t *metadata_len)
{
    if (!id || !nes || !metadata_len) return ENHANCED_LOAD_FAILED;
    size_t core_len = nes_state_size(), size;
    uint8_t *data = read_bounded(path, SNAPSHOT_HEADER + core_len + ENHANCED_METADATA_MAX + DIGEST_SIZE, &size);
    if (!data) return ENHANCED_LOAD_FAILED;
    enhanced_load_result_t result = ENHANCED_LOAD_FAILED;
    if (size == core_len && !memcmp(data, "MMSTATE1", 8)) {
        if (nes_state_load(nes, data, size, id)) {
            *metadata_len = 0;
            result = ENHANCED_LOAD_LEGACY;
        }
    } else if (size >= SNAPSHOT_HEADER + DIGEST_SIZE &&
               !memcmp(data, snapshot_magic, sizeof snapshot_magic) &&
               get32(data + 8) == FORMAT_VERSION && get32(data + 12) == core_len) {
        size_t length = get32(data + 16);
        if (metadata_valid(metadata, length) && length <= metadata_capacity &&
            size == SNAPSHOT_HEADER + core_len + length + DIGEST_SIZE && checksum_valid(data, size) &&
            nes_state_load(nes, data + SNAPSHOT_HEADER, core_len, id)) {
            if (length) memcpy(metadata, data + SNAPSHOT_HEADER + core_len, length);
            *metadata_len = length;
            result = ENHANCED_LOAD_VERIFIED;
        }
    }
    free(data);
    return result;
}

bool enhanced_battery_metadata_write(const char *path, const uint8_t prgram[8192],
                                     const void *metadata, size_t metadata_len)
{
    if (!prgram || !metadata_valid(metadata, metadata_len)) return false;
    uint8_t data[BATTERY_HEADER + ENHANCED_METADATA_MAX + DIGEST_SIZE];
    size_t size = BATTERY_HEADER + metadata_len + DIGEST_SIZE;
    memcpy(data, battery_magic, sizeof battery_magic);
    put32(data + 8, FORMAT_VERSION);
    put32(data + 12, (uint32_t)metadata_len);
    sha256(prgram, 8192, data + 16);
    if (metadata_len) memcpy(data + BATTERY_HEADER, metadata, metadata_len);
    sha256(data, size - DIGEST_SIZE, data + size - DIGEST_SIZE);
    return write_atomic(path, data, size);
}

bool enhanced_battery_metadata_read(const char *path, const uint8_t prgram[8192],
                                    void *metadata, size_t metadata_capacity, size_t *metadata_len)
{
    if (!prgram || !metadata_len) return false;
    size_t size;
    uint8_t *data = read_bounded(path, BATTERY_HEADER + ENHANCED_METADATA_MAX + DIGEST_SIZE, &size);
    if (!data) return false;
    bool ok = false;
    if (size >= BATTERY_HEADER + DIGEST_SIZE && !memcmp(data, battery_magic, sizeof battery_magic) &&
        get32(data + 8) == FORMAT_VERSION) {
        size_t length = get32(data + 12);
        uint8_t battery_digest[DIGEST_SIZE];
        sha256(prgram, 8192, battery_digest);
        if (metadata_valid(metadata, length) && length <= metadata_capacity &&
            size == BATTERY_HEADER + length + DIGEST_SIZE && checksum_valid(data, size) &&
            !memcmp(battery_digest, data + 16, DIGEST_SIZE)) {
            if (length) memcpy(metadata, data + BATTERY_HEADER, length);
            *metadata_len = length;
            ok = true;
        }
    }
    free(data);
    return ok;
}
