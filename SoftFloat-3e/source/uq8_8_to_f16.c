#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "softfloat.h"

float16_t uq8_8_to_f16(uint16_t a) {
    int_fast8_t shiftDist = softfloat_countLeadingZeros16(a) - 5;
    union ui16_f16 u;
    uint_fast16_t sig;
    if (0 <= shiftDist) {
        u.ui = (a == 0)
               ? 0 
               : packToF16UI(0, (uint_fast16_t)(0x10 - shiftDist),(uint_fast16_t) (a << shiftDist));
        return u.f;
    } else {
        shiftDist += 4;
        if (shiftDist < 0) {
            sig = (uint_fast16_t) (a >> -shiftDist) | ((uint16_t) (a << (shiftDist & 15)) != 0);
        } else {
            sig = (uint_fast16_t) (a << shiftDist);
        }
        return softfloat_roundPackToF16(0, (int_fast16_t)(0x14 - shiftDist), sig);
    }
}