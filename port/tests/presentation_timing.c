/* Standalone local evidence: SDL timer waits and simulated display clocks.
 * Does not open a window, access saves, or claim physical-display validation.
 * usage: presentation-timing ROM TRACE */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/core/cart.h"
#include "../src/core/nes.h"
#include "../src/core/trace.h"
#include "../src/host/presentation.h"

#define NATIVE_FRAMES 300
#define WAIT_FRAMES 120

static const double native_frame = 1.0 / (1789772.7272 * 3.0 / (341.0 * 262.0 - 0.5));

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

static void wait_benchmark(bool enhanced)
{
    double late[WAIT_FRAMES], total = 0;
    unsigned polls = 0;
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t begin = SDL_GetPerformanceCounter();
    clock_t cpu_begin = clock();
    for (unsigned frame = 1; frame <= WAIT_FRAMES; frame++) {
        double deadline = frame * native_frame;
        double elapsed;
        for (;;) {
            elapsed = (double)(SDL_GetPerformanceCounter() - begin) / freq;
            if (elapsed >= deadline) break;
            polls++;
            SDL_Delay(enhanced ? presentation_wait_ms(elapsed - (frame - 1) * native_frame,
                                                     0, native_frame) : 1);
        }
        late[frame - 1] = (elapsed - deadline) * 1000;
        total += late[frame - 1];
    }
    double cpu_ms = (double)(clock() - cpu_begin) * 1000 / CLOCKS_PER_SEC;
    double wall_ms = (double)(SDL_GetPerformanceCounter() - begin) * 1000 / freq;
    qsort(late, WAIT_FRAMES, sizeof late[0], compare_double);
    printf("SDL_wait mode=%s frames=%u mean_late_ms=%.4f p95_late_ms=%.4f max_late_ms=%.4f cpu_ms=%.3f wall_ms=%.3f delay_calls=%u\n",
           enhanced ? "deadline" : "old_1ms", WAIT_FRAMES, total / WAIT_FRAMES,
           late[(WAIT_FRAMES * 95 / 100) - 1], late[WAIT_FRAMES - 1], cpu_ms, wall_ms, polls);
}

static uint64_t hash_bytes(const void *data, size_t size)
{
    const unsigned char *p = data;
    uint64_t h = 0xcbf29ce484222325ULL;
    while (size--) { h ^= *p++; h *= 0x100000001b3ULL; }
    return h;
}

static bool state_schedule(const cart_t *cart, const char *trace_path, unsigned host_hz,
                           uint64_t states[NATIVE_FRAMES], uint64_t audio[NATIVE_FRAMES],
                           unsigned char *final, bool reference)
{
    trace_t trace = {0};
    char err[512];
    if (!trace_load(&trace, trace_path, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return false;
    }
    nes_t *nes = calloc(1, sizeof *nes);
    unsigned char *snapshot = malloc(nes_state_size());
    if (!nes || !snapshot) return false;
    nes_init(nes, cart->prg, cart->prg_size);
    nes_audio_config(nes, 48000);
    nes_power(nes, 0);
    uint8_t id[8];
    for (unsigned i = 0; i < 8; i++)
        id[i] = (uint8_t)strtoul((char[3]){cart->prg_sha256[2 * i], cart->prg_sha256[2 * i + 1], 0}, NULL, 16);
    unsigned frame = 0, host_ticks = 0, differences = 0;
    double accumulator = 0;
    while (frame < NATIVE_FRAMES) {
        host_ticks++;
        accumulator += 1.0 / host_hz;
        while (accumulator >= native_frame && frame < NATIVE_FRAMES) {
            accumulator -= native_frame;
            uint8_t pads[2];
            trace_at(&trace, frame, pads);
            nes_set_pad(nes, 0, pads[0]);
            nes_set_pad(nes, 1, pads[1]);
            nes_run_frame(nes);
            int16_t samples[2048];
            size_t count = nes_audio_take(nes, samples, 2048);
            uint64_t sound = hash_bytes(samples, count * sizeof samples[0]);
            nes_state_save(nes, snapshot, id);
            uint64_t state = hash_bytes(snapshot, nes_state_size());
            if (reference) { states[frame] = state; audio[frame] = sound; }
            else if (state != states[frame] || sound != audio[frame]) differences++;
            frame++;
        }
    }
    bool exact_final = reference || !memcmp(final, snapshot, nes_state_size());
    if (reference) memcpy(final, snapshot, nes_state_size());
    printf("simulated_host hz=%u host_ticks=%u native_frames=%u per_frame_state_and_PCM_differences=%u final_normalized_state_exact=%s final_state_hash=%016llx\n",
           host_hz, host_ticks, frame, differences, exact_final ? "yes" : "NO",
           (unsigned long long)hash_bytes(snapshot, nes_state_size()));
    free(nes->apu.out);
    free(nes);
    free(snapshot);
    trace_free(&trace);
    return !differences && exact_final;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: presentation-timing ROM TRACE\n"); return 2; }
    if (SDL_Init(SDL_INIT_TIMER)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    wait_benchmark(false);
    wait_benchmark(true);
    SDL_Quit();
    cart_t cart = {0};
    char err[512];
    if (!cart_load(&cart, argv[1], false, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 1; }
    uint64_t states[NATIVE_FRAMES], audio[NATIVE_FRAMES];
    unsigned char *final = malloc(nes_state_size());
    if (!final) return 1;
    bool ok = state_schedule(&cart, argv[2], 60, states, audio, final, true) &&
              state_schedule(&cart, argv[2], 120, states, audio, final, false) &&
              state_schedule(&cart, argv[2], 144, states, audio, final, false);
    free(final);
    cart_free(&cart);
    return ok ? 0 : 1;
}
