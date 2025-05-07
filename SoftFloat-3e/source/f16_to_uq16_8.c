#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"

//---------------------------------------------------------------------------
// Convert float16 → unsigned 16.8 fixed.
//   - only nearest-even rounding
//   - subnormals → 0 (raise inexact if nonzero frac)
//   - NaN/Infs/negatives as before
uint_fast32_t f16_to_uq16_8( float16_t a, bool exact )
{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    bool sign;
    int_fast8_t exp;
    uint_fast16_t frac;
    uint_fast32_t sig32;
    int_fast8_t shiftDist;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
   uA.f = a;
   uiA = uA.ui;
   sign = signF16UI( uiA );
   exp  = expF16UI( uiA );
   frac = fracF16UI( uiA );

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // Negative finite → invalid
    if ( sign ) {
        softfloat_raiseFlags( softfloat_flag_invalid );
        return uq16_8_fromNegOverflow;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // NaN or Infinity?
    if ( exp == 0x1F ) {
        softfloat_raiseFlags( softfloat_flag_invalid );
        return frac
            ? uq16_8_fromNaN
            : uq16_8_fromPosOverflow;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // Too small to represent (exp < 7) → flush to zero. 
    // Raise inexact for any non-zero value.
    if (exp < 7) {
        if ((exp != 0 || frac != 0) && exact) {
            softfloat_raiseFlags(softfloat_flag_inexact);
        }
        return 0;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    // Normal number: build 11-bit mantissa
    sig32  = frac | 0x0400;

    // exp bias = 15; we need to scale by 2^(e) and then by 2^8 for the .8 format.
    // net shift = (exp – 15) + 8 = exp – 7.
    // We split into two ranges to keep the “roundTo…” call only when needed.

    // 1) For really big exponents (>18) all fractional bits land above the fixed-point
    //    and sig32 << (exp-17) will have its bottom 4 bits == 0 → no rounding.
    //    We can do it inline.
    if ( exp > 18 ) {
        shiftDist = exp - 17;
        return sig32 << (shiftDist);
    }
    // 2) For moderately large exponents [13..18], we shift left enough to leave
    //    exactly 4 fractional bits and call our simple round-to-even routine.
    if ( exp >= 13 ) {
        shiftDist = exp - 13;
        uint_fast32_t shifted = sig32 << (shiftDist);
        return softfloat_roundToUQ16_8( shifted, exact );
    }
    // exp < 13: shift to get exactly 4 fractional bits, then round-to-nearest-even
    {
        shiftDist = 13 - exp;
        if (shiftDist > 0) {
            // right-shift to drop high bits, leaving 4 frac bits
            sig32 >>= shiftDist;
        } else {
            // left-shift to make room for 4 frac bits
            sig32 <<= ( - shiftDist );
        }
        return softfloat_roundToUQ16_8( sig32, exact );
    }
}