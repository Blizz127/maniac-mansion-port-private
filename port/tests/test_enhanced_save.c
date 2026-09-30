#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/host/enhanced_save.h"
#include "../src/core/sha256.h"

static void write_file(const char *path, const uint8_t *bytes, size_t length)
{
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, length, file) == length && !fclose(file));
}

static uint8_t *read_file(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size > 0 && !fseek(file, 0, SEEK_SET));
    uint8_t *bytes = malloc((size_t)size);
    assert(bytes && fread(bytes, 1, (size_t)size, file) == (size_t)size && !fclose(file));
    *length = (size_t)size;
    return bytes;
}

static void failed_snapshot_preserves(const char *path, nes_t *nes, const uint8_t id[8], size_t capacity)
{
    nes_t *before = malloc(sizeof *before);
    assert(before);
    memcpy(before, nes, sizeof *before);
    uint8_t metadata[ENHANCED_METADATA_MAX], sentinel[ENHANCED_METADATA_MAX];
    memset(metadata, 0xa5, sizeof metadata);
    memcpy(sentinel, metadata, sizeof sentinel);
    size_t length = 117;
    assert(enhanced_snapshot_load(path, id, nes, metadata, capacity, &length) == ENHANCED_LOAD_FAILED);
    assert(length == 117 && !memcmp(metadata, sentinel, sizeof metadata));
    assert(!memcmp(nes, before, sizeof *nes));
    free(before);
}

static void failed_battery_preserves(const char *path, const uint8_t prgram[8192], size_t capacity)
{
    uint8_t metadata[ENHANCED_METADATA_MAX], before[ENHANCED_METADATA_MAX];
    memset(metadata, 0x5a, sizeof metadata);
    memcpy(before, metadata, sizeof before);
    size_t length = 119;
    assert(!enhanced_battery_metadata_read(path, prgram, metadata, capacity, &length));
    assert(length == 119 && !memcmp(metadata, before, sizeof metadata));
}

int main(void)
{
    char dir[] = "/tmp/mm-enhanced-save-XXXXXX";
    assert(mkdtemp(dir));
    char snapshot_path[256], sidecar_path[256], missing_path[256];
    snprintf(snapshot_path, sizeof snapshot_path, "%s/snapshot", dir);
    snprintf(sidecar_path, sizeof sidecar_path, "%s/sidecar", dir);
    snprintf(missing_path, sizeof missing_path, "%s/missing", dir);
    uint8_t id[8] = {1,2,3,4,5,6,7,8}, wrong_id[8] = {8,7,6,5,4,3,2,1};
    uint8_t metadata[ENHANCED_METADATA_MAX], loaded[ENHANCED_METADATA_MAX];
    for (unsigned i = 0; i < sizeof metadata; i++) metadata[i] = (uint8_t)(i ^ 0x55);
    uint8_t *prg = calloc(32768, 1);
    nes_t *nes = calloc(1, sizeof *nes);
    assert(prg && nes);
    nes_init(nes, prg, 32768);
    nes_power(nes, 0);
    nes->ram[123] = 41;
    nes->prgram[8191] = 99;
    assert(enhanced_snapshot_save(snapshot_path, id, nes, metadata, sizeof metadata));
    assert(!enhanced_snapshot_save(snapshot_path, id, nes, metadata, sizeof metadata + 1));
    failed_snapshot_preserves(missing_path, nes, id, sizeof loaded);
    failed_snapshot_preserves(snapshot_path, nes, wrong_id, sizeof loaded);
    failed_snapshot_preserves(snapshot_path, nes, id, sizeof loaded - 1);
    nes->ram[123] = 7;
    nes->prgram[8191] = 3;
    size_t length = 0;
    assert(enhanced_snapshot_load(snapshot_path, id, nes, loaded, sizeof loaded, &length) == ENHANCED_LOAD_VERIFIED);
    assert(length == sizeof metadata && !memcmp(loaded, metadata, length));
    assert(nes->ram[123] == 41 && nes->prgram[8191] == 99);
    size_t size;
    uint8_t *valid = read_file(snapshot_path, &size), *modified = malloc(size + 1);
    assert(modified);
    /* Independently corrupt header, embedded core, metadata and checksum. */
    size_t offsets[] = {8, 20 + 40, 20 + nes_state_size(), size - 1};
    for (unsigned i = 0; i < sizeof offsets / sizeof offsets[0]; i++) {
        memcpy(modified, valid, size);
        modified[offsets[i]] ^= 0x80;
        write_file(snapshot_path, modified, size);
        failed_snapshot_preserves(snapshot_path, nes, id, sizeof loaded);
    }
    memcpy(modified, valid, size);
    modified[8] = 2; /* Unknown version rejected even with valid digest. */
    sha256(modified, size - 32, modified + size - 32);
    write_file(snapshot_path, modified, size);
    failed_snapshot_preserves(snapshot_path, nes, id, sizeof loaded);
    memcpy(modified, valid, size);
    modified[16] = 129; /* Opaque metadata cap is a format constraint. */
    sha256(modified, size - 32, modified + size - 32);
    write_file(snapshot_path, modified, size);
    failed_snapshot_preserves(snapshot_path, nes, id, sizeof loaded);
    memcpy(modified, valid, size);
    modified[20] ^= 0x80; /* Valid envelope cannot hide invalid core magic. */
    sha256(modified, size - 32, modified + size - 32);
    write_file(snapshot_path, modified, size);
    failed_snapshot_preserves(snapshot_path, nes, id, sizeof loaded);
    write_file(snapshot_path, valid, size - 1);
    failed_snapshot_preserves(snapshot_path, nes, id, sizeof loaded);
    memcpy(modified, valid, size); modified[size] = 1;
    write_file(snapshot_path, modified, size + 1);
    failed_snapshot_preserves(snapshot_path, nes, id, sizeof loaded);
    free(valid); free(modified);

    /* Existing raw Enhanced snapshots restore core, never invent history. */
    size_t raw_size = nes_state_size();
    uint8_t *raw_allocation = malloc(raw_size + 4);
    assert(raw_allocation);
    uint8_t *raw = raw_allocation + 4; /* Align embedded nes_t after 20-byte header. */
    nes_state_save(nes, raw, id);
    write_file(snapshot_path, raw, raw_size);
    failed_snapshot_preserves(snapshot_path, nes, wrong_id, sizeof loaded);
    memset(loaded, 0xa5, sizeof loaded);
    nes->ram[123] = 0;
    assert(enhanced_snapshot_load(snapshot_path, id, nes, loaded, sizeof loaded, &length) == ENHANCED_LOAD_LEGACY);
    assert(!length && nes->ram[123] == 41);
    for (unsigned i = 0; i < sizeof loaded; i++) assert(loaded[i] == 0xa5);
    free(raw_allocation);

    failed_battery_preserves(missing_path, nes->prgram, sizeof loaded);
    assert(enhanced_battery_metadata_write(sidecar_path, nes->prgram, metadata, sizeof metadata));
    assert(!enhanced_battery_metadata_write(sidecar_path, nes->prgram, metadata, sizeof metadata + 1));
    assert(enhanced_battery_metadata_read(sidecar_path, nes->prgram, loaded, sizeof loaded, &length));
    assert(length == sizeof metadata && !memcmp(loaded, metadata, length));
    failed_battery_preserves(sidecar_path, nes->prgram, sizeof loaded - 1);
    /* Binding includes both ends of the complete 8192-byte battery image. */
    nes->prgram[0] ^= 1;
    failed_battery_preserves(sidecar_path, nes->prgram, sizeof loaded);
    nes->prgram[0] ^= 1;
    nes->prgram[8191] ^= 1;
    failed_battery_preserves(sidecar_path, nes->prgram, sizeof loaded);
    nes->prgram[8191] ^= 1;
    valid = read_file(sidecar_path, &size);
    modified = malloc(size + 1);
    assert(modified);
    size_t battery_offsets[] = {8, 16, 48, size - 1};
    for (unsigned i = 0; i < sizeof battery_offsets / sizeof battery_offsets[0]; i++) {
        memcpy(modified, valid, size);
        modified[battery_offsets[i]] ^= 0x80;
        write_file(sidecar_path, modified, size);
        failed_battery_preserves(sidecar_path, nes->prgram, sizeof loaded);
    }
    write_file(sidecar_path, valid, size - 1);
    failed_battery_preserves(sidecar_path, nes->prgram, sizeof loaded);
    memcpy(modified, valid, size); modified[size] = 1;
    write_file(sidecar_path, modified, size + 1);
    failed_battery_preserves(sidecar_path, nes->prgram, sizeof loaded);
    free(valid); free(modified);
    assert(enhanced_battery_metadata_write(sidecar_path, nes->prgram, NULL, 0));
    assert(enhanced_battery_metadata_read(sidecar_path, nes->prgram, NULL, 0, &length) && !length);
    assert(enhanced_snapshot_save(snapshot_path, id, nes, NULL, 0));
    assert(enhanced_snapshot_load(snapshot_path, id, nes, NULL, 0, &length) == ENHANCED_LOAD_VERIFIED && !length);
    free(prg); free(nes);
    assert(!unlink(snapshot_path) && !unlink(sidecar_path) && !rmdir(dir));
    puts("enhanced-save: bounded opaque metadata, verified core roundtrip, corruption/truncation/version/mode-ID rejection, legacy no-history, battery SHA binding, failed-read no-mutation PASS");
    return 0;
}
