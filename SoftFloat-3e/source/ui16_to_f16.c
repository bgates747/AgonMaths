#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "softfloat.h"

#include "debug_print.h"

float16_t ui16_to_f16(uint16_t a)
{
    int_fast8_t shiftDist;
    union ui16_f16 u;
    uint_fast16_t sig;

    shiftDist = softfloat_countLeadingZeros16(a) - 5;
    if (0 <= shiftDist) {
        u.ui = a ? packToF16UI(0, 0x18 - shiftDist, (uint_fast16_t) a << shiftDist )
        : 0;
        return u.f;
    } else {
        shiftDist += 4;
        sig = (shiftDist < 0)
            ? a >> (-shiftDist) | ((uint16_t)(a << (shiftDist & 15)) != 0)
            : (uint_fast16_t) a << shiftDist;
        return softfloat_roundPackToF16(0, 0x1C - shiftDist, sig);
    }
}

