/* Owner-local evidence only. Never add generated PNGs or game state to Git.
 * usage: presentation-proof ROM TRACE OUTPUT_PREFIX FRAME_COUNT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/cart.h"
#include "../src/core/nes.h"
#include "../src/core/trace.h"
#include "../src/host/palette.h"
#include "../src/host/png.h"
#include "../src/host/presentation.h"

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: presentation-proof ROM TRACE OUTPUT_PREFIX FRAME_COUNT\n");
        return 2;
    }
    unsigned frames = (unsigned)strtoul(argv[4], NULL, 10);
    if (!frames || frames > 100000) return 2;
    cart_t cart = {0};
    trace_t trace = {0};
    char err[512];
    if (!cart_load(&cart, argv[1], false, err, sizeof err) ||
        !trace_load(&trace, argv[2], err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        cart_free(&cart);
        return 1;
    }
    nes_t *nes = calloc(1, sizeof *nes);
    if (!nes) return 1;
    nes_init(nes, cart.prg, cart.prg_size);
    nes_power(nes, 0);
    for (unsigned f = 0; f < frames; f++) {
        uint8_t buttons[2];
        trace_at(&trace, f, buttons);
        nes_set_pad(nes, 0, buttons[0]);
        nes_set_pad(nes, 1, buttons[1]);
        nes_run_frame(nes);
    }
    nes_t *original = malloc(sizeof *original);
    uint8_t *before = malloc(NES_W * NES_H * 3);
    uint8_t *after = malloc(NES_W * NES_H * 3);
    if (!original || !before || !after) return 1;
    memcpy(original, nes, sizeof *original);
    unsigned changed = 0, ui_changed = 0, off_changed = 0;
    for (int y = 0; y < NES_H; y++) {
        for (int x = 0; x < NES_W; x++) {
            size_t i = (size_t)y * NES_W + x;
            uint32_t source = 0xff000000u | nes_rgb(nes->ppu.fb[i]);
            uint32_t retail = presentation_pixel(source, false, y);
            uint32_t sample = presentation_pixel(source, true, y);
            if (retail != source) off_changed++;
            if (sample != retail) {
                changed++;
                if (y >= 160) ui_changed++;
            }
            for (unsigned c = 0; c < 3; c++) {
                before[i * 3 + c] = (uint8_t)(retail >> (16 - 8 * c));
                after[i * 3 + c] = (uint8_t)(sample >> (16 - 8 * c));
            }
        }
    }
    char path_before[2048], path_after[2048];
    snprintf(path_before, sizeof path_before, "%s-before.png", argv[3]);
    snprintf(path_after, sizeof path_after, "%s-after.png", argv[3]);
    bool unchanged = !memcmp(original, nes, sizeof *nes);
    bool written = png_write_rgb(path_before, before, NES_W, NES_H) &&
                   png_write_rgb(path_after, after, NES_W, NES_H);
    printf("frame_count=%u changed_pixels=%u ui_changed=%u off_changed=%u entire_nes_state_unchanged=%s written=%s\n",
           frames, changed, ui_changed, off_changed, unchanged ? "yes" : "NO", written ? "yes" : "NO");
    free(original);
    free(before);
    free(after);
    free(nes);
    trace_free(&trace);
    cart_free(&cart);
    return written && unchanged && changed && !ui_changed && !off_changed ? 0 : 1;
}
