#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../src/host/presentation.h"
#include "../src/host/palette.h"

int main(void)
{
    const double frame = 1.0 / (1789772.7272 * 3.0 / (341.0 * 262.0 - 0.5));
    assert(presentation_wait_ms(0, 0, frame) == 8);
    assert(presentation_wait_ms(0.008, 0.003, frame) == 4);
    assert(presentation_wait_ms(frame, 0, frame) == 0);
    assert(presentation_wait_ms(0, frame, frame) == 0);
    assert(presentation_wait_ms(frame - 0.0019, 0, frame) == 0);
    assert(presentation_wait_ms(0, 0, 0) == 0);
    assert(presentation_wait_ms(-1, 0, frame) == 0);
    assert(presentation_wait_ms(0, -1, frame) == 0);
    assert(presentation_wait_ms(NAN, 0, frame) == 0);
    assert(presentation_wait_ms(0, INFINITY, frame) == 0);
    assert(presentation_wait_ms(0, 0, INFINITY) == 0);
    for (unsigned step = 0; step < 10000; step++) {
        double elapsed = step / 100000.0;
        unsigned ms = presentation_wait_ms(0, elapsed, frame);
        assert(ms <= 8);
        if (ms) assert(elapsed + ms / 1000.0 < frame);
    }
    unsigned previous = 0;
    for (unsigned c = 0; c <= 255; c++) {
        uint32_t pixel = 0x71000000u | c << 16 | c << 8 | c;
        assert(presentation_pixel(pixel, false, 50) == pixel);
        assert(presentation_pixel(pixel, true, 160) == pixel);
        assert(presentation_pixel(pixel, true, 239) == pixel);
        assert(presentation_pixel(pixel, true, -1) == pixel);
        uint32_t boosted = presentation_pixel(pixel, true, 159);
        unsigned result = boosted & 255u;
        assert(result >= c && result <= c + 12);
        assert(c == 0 || result >= previous);
        assert((boosted & 0xff000000u) == (pixel & 0xff000000u));
        assert(((boosted >> 8) & 255u) == result);
        assert(((boosted >> 16) & 255u) == result);
        previous = result;
    }
    assert(presentation_pixel(0xff000000u, true, 0) == 0xff000000u);
    assert(presentation_pixel(0xffffffffu, true, 0) == 0xffffffffu);
    /* Quantized channel curves can merge adjacent arbitrary RGB values;
     * prove that no distinct color in this port's actual NES/emphasis LUT
     * becomes indistinguishable from another in the sample. */
    for (unsigned i = 0; i < 512; i++) {
        uint32_t a = nes_rgb((uint16_t)i);
        for (unsigned j = 0; j < i; j++) {
            uint32_t b = nes_rgb((uint16_t)j);
            if (a != b)
                assert(presentation_pixel(a, true, 0) != presentation_pixel(b, true, 0));
        }
    }
    puts("presentation: deadline bounds, disabled identity, monotonic color, alpha and UI preservation PASS");
    return 0;
}
