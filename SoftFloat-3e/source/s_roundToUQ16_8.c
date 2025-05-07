#include <stdbool.h>
#include <stdint.h>
#include "softfloat.h"

//---------------------------------------------------------------------------
// Round a 4-fraction-bit value into 16.8 fixed, ties-to-even only.
// We know no finite half-float can overflow 24 bits.
uint_fast32_t softfloat_roundToUQ16_8( uint_fast32_t sig, bool exact )
{
    uint_fast32_t roundBits = sig & 0xF;  // bottom 4 bits
    sig += 0x8;                           // add ½ULP (1<<(4−1))
    uint_fast32_t z = sig >> 4;           // drop those 4 bits

    // ties-to-even: if exactly halfway and LSB=1, clear it
    if ( (roundBits == 0x8) && (z & 1) ) {
        z &= ~1u;
    }
    // flag inexact if *any* low bits were nonzero
    if ( roundBits && exact ) {
        softfloat_raiseFlags( softfloat_flag_inexact );
    }
    return z;
}
