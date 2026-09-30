#ifndef MM_PRESENTATION_H
#define MM_PRESENTATION_H

#include <stdbool.h>
#include <stdint.h>

/* Host-only deadline waiting. Call after presentation using elapsed time
 * since the accumulator's last clock sample. Never modifies game time.
 * Return 0 means yield/poll: the deadline is near or already passed. */
unsigned presentation_wait_ms(double accumulator_s, double elapsed_s, double frame_s);

/* Reversible scene-color SAMPLE. ARGB stays at the same pixel coordinate;
 * black/white endpoints, alpha and the bottom 80 UI rows stay unchanged.
 * Disabled mode is exactly the input pixel, including alpha. */
uint32_t presentation_pixel(uint32_t argb, bool scene_color_sample, int y);

#endif
