#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "softfloat.h"

float16_t i16_to_f16(int16_t a) {
    bool sign;
    uint_fast16_t absA;
    int_fast8_t shiftDist;
    union ui16_f16 u;
    uint_fast16_t sig;

    sign = (a < 0);
    absA = sign ? -(uint_fast16_t) a : (uint_fast16_t) a;
    // Count leading zeros in 16-bit value, then offset by 5 (16 - 11) for normalization
    shiftDist = softfloat_countLeadingZeros16(absA) - 5;
    
    if (0 <= shiftDist) {
        // Number can be represented without rounding
        u.ui = a ? packToF16UI(sign, 0x18 - shiftDist, (uint_fast16_t) absA << shiftDist) : 0;
        return u.f;
    } else {
        // Need to round; prepare significand with guard bits
        shiftDist += 4;  // add 4 to include guard, round, and sticky bits for rounding
        if (shiftDist < 0) {
            // Too large, shift right and gather sticky bits
            sig = (uint_fast16_t) (absA >> -shiftDist) 
                | (((uint_fast32_t) absA << ((-shiftDist) & 15)) != 0);
        } else {
            // Still room to shift left (for cases just slightly above 10 bits)
            sig = (uint_fast16_t) absA << shiftDist;
        }
        // Round and pack the result to float16
        return softfloat_roundPackToF16(sign, 0x1C - shiftDist, sig);
    }
}

