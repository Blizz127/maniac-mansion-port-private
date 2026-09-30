#define _POSIX_C_SOURCE 200809L
#include "../src/host/config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char dir[] = "./mm-config-test-XXXXXX";
    assert(mkdtemp(dir));
    char path[1024], err[256] = "";
    snprintf(path, sizeof path, "%s/settings.ini", dir);
    FILE *f = fopen(path, "w"); assert(f);
    const char *original = "# keep this comment\n[game]\nrom = owner-local-game\n[input]\na = X\n[enhancements]\nsmooth_display = off\n# keep another comment\n[video]\nvsync = on";
    fputs(original, f); fclose(f);
    config_t *c = config_load(path, err, sizeof err); assert(c);
    assert(!config_bool(c, "enhancements", "smooth_display", false));
    assert(config_save_bool(c, "enhancements", "smooth_display", true));
    assert(config_save_bool(c, "enhancements", "graphics_sample", true));
    assert(config_save_bool(c, "enhancements", "graphics_sample", false));
    config_free(c);
    c = config_load(path, err, sizeof err); assert(c);
    assert(config_bool(c, "enhancements", "smooth_display", false));
    assert(!config_bool(c, "enhancements", "graphics_sample", true));
    assert(!strcmp(config_str(c, "game", "rom", ""), "owner-local-game"));
    assert(!strcmp(config_str(c, "input", "a", ""), "X"));
    assert(config_bool(c, "video", "vsync", false));
    f = fopen(path, "r"); assert(f); char contents[4096]; size_t n = fread(contents, 1, sizeof contents-1, f); contents[n]=0; fclose(f);
    assert(strstr(contents, "# keep this comment\n"));
    assert(strstr(contents, "# keep another comment\n"));
    assert(!config_save_bool(c, "bad\nsection", "key", true));
    assert(!config_save_bool(c, "enhancements", "bad=key", true));
    config_free(c);
    unlink(path); rmdir(dir);
    puts("config persistence: PASS (roundtrip, comments, ROM path, mappings, missing final newline, invalid keys)");
    return 0;
}
