#include <stdint.h>

uint_fast16_t softfloat_roundToUI16(uint_fast32_t sig) {
    // Separate the frac bits (lower 12) and integer bits (upper bits)
    uint16_t frac = sig & 0x0FFF;       // mask 12 least-significant bits (frac part)
    uint_fast16_t result = sig >> 12;         // shift right to get the integer part
    

    // Apply round-to-nearest-even:
    if (frac > 0x800 ||                    // fraction > 0.5
        (frac == 0x800 && (result & 0x1))) // tie (exact 0.5) and current integer is odd
    {
        result += 1;  // round up
    }
    // (If frac < 0x800, we round down by doing nothing)

    return result;
}
