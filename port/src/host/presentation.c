#include "presentation.h"

unsigned presentation_wait_ms(double accumulator_s, double elapsed_s, double frame_s)
{
    /* Comparisons also reject NaN; infinity cannot produce a valid remaining
     * interval. Leave the last millisecond for clock polling instead of
     * relying on SDL_Delay's coarse scheduler wakeup. */
    if (!(frame_s > 0 && frame_s < 1) || !(accumulator_s >= 0) || !(elapsed_s >= 0))
        return 0;
    double remaining_ms = (frame_s - accumulator_s - elapsed_s) * 1000.0 - 1.0;
    if (!(remaining_ms >= 1.0))
        return 0;
    if (remaining_ms > 8.0)
        return 8;
    return (unsigned)remaining_ms;
}

static uint32_t scene_channel(uint32_t c)
{
    /* A monotonic curve: at most +12 per channel, endpoints unchanged.
     * No neighbouring pixels are sampled and no temporal blending occurs. */
    return c + c * (255u - c) / 1350u;
}

uint32_t presentation_pixel(uint32_t argb, bool scene_color_sample, int y)
{
    if (!scene_color_sample || y < 0 || y >= 160)
        return argb;
    return (argb & 0xff000000u) |
           (scene_channel((argb >> 16) & 255u) << 16) |
           (scene_channel((argb >> 8) & 255u) << 8) |
           scene_channel(argb & 255u);
}
