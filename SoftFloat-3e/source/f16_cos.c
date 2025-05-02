#include "softfloat.h"

/*----------------------------------------------------------------------------
| f16_cos
|   Input  : uint16_t angle8_8  (8.8 fixed-point, 256 units = full circle)
|   Output : float16_t
*----------------------------------------------------------------------------*/
float16_t f16_cos( uint16_t angle8_8 )
{
    angle8_8 += 64 & 0xFFFF; // angle8_8 = angle8_8 + 90 degrees
    return f16_sin( angle8_8 );
}
