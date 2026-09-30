# Optional presentation enhancements

Both settings are OFF by default and belong in the host Enhancements menu.
Suggested labels are **Deadline display pacing** and **Scene color boost SAMPLE**.
The integrator persists `enhancements.smooth_display` and
`enhancements.graphics_sample`; neither setting belongs in an NES save/state.

## Existing timing and limits

The SDL host already runs the core from a wall-clock accumulator at
`1789772.7272 * 3 / (341 * 262 - 0.5)` frames per second (approximately
60.10 NTSC frames/s). A core frame runs CPU steps until the PPU signals frame
ready. CPU/PPU/APU master clocks, script execution, NPCs and puzzle events run
inside that core. The host presents when it has run a frame, uses VSync by
default, and otherwise polls with a fixed 1 ms delay when no frame is due.
Audio queue correction changes the resampling ratio, not CPU game time.

The optional pacing helper calculates a bounded sleep until the next existing
NTSC deadline, accounting for time spent emulating and presenting. It sleeps
at most 8 ms, leaves a 1 ms scheduler margin, and yields when the deadline is
near. It replaces blind 1 ms polling with deadline-aware waiting. It does not
modify the accumulator or insert/drop core frames, interpolate images, change
VSync, or advertise higher game-animation FPS. Recording and fast-forward
bypass this helper and preserve their current timing.

The helper's tests prove deadline bounds; they do not prove improved physical
display smoothness. A fixed 60 Hz display and NTSC's approximately 60.10 Hz
refresh still have different cadences. VSync blocking and OS scheduling can
still introduce jitter. Actual monitor frame-time captures and subjective
assessment remain necessary before claiming a smoothness improvement.
Independent faster player movement requires game-semantic work and must not
be implemented by changing these clocks.

## Reversible graphics sample

`presentation_pixel()` applies a gentle, monotonic per-channel scene-color
curve `c + c * (255 - c) / 1350`, at most +12 of 255. Black and white stay
fixed, alpha stays identical, and the bottom 80 NES pixel rows stay untouched
to preserve verb/inventory UI. It applies only to the final host ARGB copy
of rows 0–159; the core framebuffer, palette, tile/sprite data, state, hitboxes,
and sprite/background priority stay unchanged. Every output pixel uses only
the corresponding input pixel: no spatial filtering or temporal blending.
Switching it OFF restores the original host colors on the next presentation.

This is a small color-polish feasibility sample, not a replacement asset set
or a finished art upgrade. The renderer uses the same destination/crop rectangles,
so cursor coordinates and occlusion are preserved structurally. Physical display
legibility and additional room/cutscene coverage still need owner review.

## Integrator wiring

Add `src/host/presentation.c` to `mmhost` and include `presentation.h` in the
host main source. Pass `graphics_sample` to `video_present`, or store it in
`video_t`, and change only the texture-copy expression:

```c
row[x] = presentation_pixel(v->lut[src[x] & 0x1FF], graphics_sample, y);
```

After presenting, use the helper only in the optional normal-time branch:

```c
if (to_run)
    video_present(&v, nes.ppu.fb, dm);
if (dms.smooth_display && !recording && !ff) {
    double elapsed = (double)(SDL_GetPerformanceCounter() - last) / (double)freq;
    SDL_Delay(presentation_wait_ms(acc, elapsed, frame_time));
} else if (!to_run) {
    SDL_Delay(1);
}
```

Retain the original `video_present` call signature as appropriate to the
chosen toggle wiring. The disabled branch is equivalent to the prior loop.
Do not add a simulation-speed control here. Developer-menu paused paths
already bypass the normal timing loop and should retain their existing behavior.

For CTest, add `test-presentation` from `tests/test_presentation.c`, link to
`mmhost` and `mmhost_common`, and register a `presentation` test.
Optionally add `presentation-proof` from `tests/presentation_proof.c`, linked
to `mmhost`, `mmcore`, and `mmhost_common`. The proof runner is owner-local:

```sh
presentation-proof /owner/local/original.nes port/traces/newgame.mmin build/enh-display/gameplay 8200
```

## Evidence from this lane

`test_presentation.c` passed with C11, `-Wall -Wextra -Werror`: deadline
bounds/elapsed accounting/invalid inputs, disabled identity, alpha and UI
preservation, bounded monotonic channels, and no new color collisions among
all 512 NES palette/emphasis entries.

The owner-local proof runner executed the existing `newgame.mmin` trace for
8,200 frames using the verified supported ROM. The moon/path/children/fence
scene was visually inspected before and after:

- 7,838 scene pixels changed; zero UI pixels changed.
- The disabled conversion changed zero pixels.
- The entire `nes_t` was byte-identical before and after both conversions.
- Output dimensions remained 256×240 and the same per-pixel coordinates.

The local pair is in the display worktree at
`build/enh-display/gameplay-before.png` and `gameplay-after.png`. These images
are derived game data: keep them owner-local, excluded from commits/packages.
The proof runner's state comparison proves conversion isolation, not a full
playthrough or timer/music equivalence at arbitrary monitor refresh rates.
The integrator must perform host toggle/persistence and disabled-regression
checks after wiring. No push, deployment or publication is part of this lane.

## Follow-up timing and core-state evidence

`tests/presentation_timing.c` provides a standalone timer benchmark and
simulated-display-clock state check, without opening an SDL window. Build it
with the current worktree's `mmcore`, `presentation.c` and SDL2, then run:

```sh
presentation-timing /owner/local/original.nes port/traces/smoke.mmin
```

A local 120-deadline SDL timer sample measured the following. There was no
renderer, VSync or core workload in the timer benchmark, so these numbers
characterize waiting behavior on this machine, not end-to-end frame pacing.
CPU time is process CPU time; elapsed time was about 1,997 ms per mode.

| Wait mode | Mean lateness ms | p95 lateness ms | Maximum lateness ms | CPU ms | Delay calls |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original fixed 1 ms | 0.6112 | 1.4866 | 2.5799 | 30.888 | 1,753 |
| Deadline helper | 0.1879 | 1.2166 | 4.0772 | 20.077 | 2,427 |

The sample improved mean/p95 lateness and CPU time, but its worst lateness
increased. The extra calls include `SDL_Delay(0)` yielding near the deadline.
Scheduling noise, other processes and workload affect these values; this
single sample does not establish consistent smoothness or CPU improvement.

With the current-source core, the first 300 native frames of `smoke.mmin`
were executed using simulated host ticks at 60, 120 and 144 Hz. Inputs were
indexed by native frame, never by presentation tick. All 300 normalized
`nes_state_save()` hashes and generated audio PCM hashes matched in each run;
final normalized snapshots were also compared directly and were byte-identical.
The common final state hash was `e88c41c71c259420` (64-bit FNV-1a).
Host tick counts were 300, 600 and 719 respectively. The state includes
CPU/PPU/APU clocks, RAM and framebuffer, so the comparison checks those
components' identity for this short trace. It does not validate every timed
puzzle, chase input, audio-device resampling, monitor refresh or heard music.
There is no higher-FPS simulation in the test.
