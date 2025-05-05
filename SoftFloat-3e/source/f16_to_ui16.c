#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"

#include "debug_print.h"

uint_fast16_t f16_to_ui16( float16_t a )
{
    union ui16_f16    uA;
    uint_fast16_t     uiA;
    bool              signA;
    int_fast8_t       expA;
    uint_fast16_t     frac;
    uint_fast32_t     sig;
    int_fast8_t       shiftDist;
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    uA.f  = a;
    uiA   = uA.ui;
    signA  = signF16UI( uiA );
    expA   = expF16UI( uiA );
    frac  = fracF16UI( uiA );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    if ( signA ) {
        softfloat_raiseFlags( softfloat_flag_invalid );
        return ui16_fromNegOverflow;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
   if ( expA == 0x1F ) {
    softfloat_raiseFlags( softfloat_flag_invalid );
    return
        frac ? ui16_fromNaN
              : ui16_fromPosOverflow;
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
   sig = (uint_fast32_t) frac; 
   if ( expA ) {
        sig |= 0x0400;
        shiftDist = expA - 0x19;
        if ( 0 <= shiftDist ) {
            return (uint_fast16_t)(sig << shiftDist);
        }
        shiftDist = expA - 0x0D;
        if ( 0 < shiftDist ) {
            sig <<= shiftDist; // This will never overfow 22 bits
        }
    }
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    return softfloat_roundToUI16( sig );
}
