#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

// uiA    : raw 16-bit half-precision float input (bits)
// iy     : output string buffer pointer
// strBuff: saved start of output buffer
// signA  : sign bit (0 = positive, 1 = negative)
// expA   : exponent field (5 bits, biased by 15)
// sigA   : fraction/mantissa field (10 bits, no implicit leading 1)
// expZ   : unbiased exponent minus 10 (adjusted for 10-bit mantissa shift)
// sig32Z : full normalized mantissa (10 or 11 bits as 32-bit value)
// sig32X : integer part after applying exponent
// sig32Y : fractional part numerator (still divided by 2^expY)
// expY   : number of bits shifted (i.e., denominator is 2^expY)
// temp[] : temporary buffer for reversing integer digits
// ti     : counter for integer digit collection

char* half_to_string(uint16_t uiA, char* iy) {
    char* strBuff = iy;              // Save start of output buffer
    uint16_t signA = uiA >> 15;         // 1-bit signA
    uint16_t expA  = (uiA >> 10) & 0x1F; // 5-bit exponent (biased by 15)
    uint16_t sigA = uiA & 0x3FF;       // 10-bit fraction (mantissa)

    // Special cases: Inf/NaN&#8203;:contentReference[oaicite:0]{index=0}
    if (expA == 0x1F) {  
        if (sigA == 0) {               // Infinity
            if (signA) *iy++ = '-';
            *iy++ = 'I'; *iy++ = 'n'; *iy++ = 'f';
        } else {                       // NaN (Not a Number)
            *iy++ = 'N'; *iy++ = 'a'; *iy++ = 'N';
        }
        *iy = '\0';
        return strBuff;
    }

    // Special case: Zero (expA=0, sigA=0)&#8203;:contentReference[oaicite:1]{index=1}
    if (expA == 0 && sigA == 0) {
        if (signA) *iy++ = '-';
        *iy++ = '0';
        *iy = '\0';
        return strBuff;
    }

    // Determine normalized mantissa and exponent
    int expZ;
    uint32_t sig32Z;
    if (expA == 0) {
        // Subnormal number: exponent = 1 - bias (15)&#8203;:contentReference[oaicite:2]{index=2}
        expZ = -14;                 // (1 - 15)
        sig32Z = sigA;                  // no implicit leading 1
    } else {
        expZ = (int)expA - 15;
        sig32Z   = 0x400 | sigA;        // add implicit 1 (1<<10)
    }
    // Now value = (-1)^signA * sig32Z * 2^(expZ - 10)

    // Incorporate the 2^(-10) factor of the mantissa into exponent
    expZ -= 10;
    if (signA) *iy++ = '-';               // write signA if negative

    // Separate integer and fractional parts by base-2 exponent
    uint32_t sig32X;
    uint32_t sig32Y = 0;
    uint32_t expY = 0;
    
    if (expZ >= 0) {
        sig32X = sig32Z << expZ;
    } else {
        expY     = (uint32_t)(-expZ);
        sig32X = sig32Z >> expY;
        sig32Y      = sig32Z & ((1u << expY) - 1);
    }

    // Convert integer part to decimal string
    if (sig32X == 0) {
        *iy++ = '0';
    } else {
        char temp[16];
        int ti = 0;
        while (sig32X) {
            temp[ti++] = '0' + (sig32X % 10);
            sig32X  /= 10;
        }
        while (ti--) {
            *iy++ = temp[ti];
        }
    }

    // Convert fractional part (if any) by multiplying by 10 repeatedly
    if (sig32Y != 0) {
        *iy++ = '.';
        for (uint32_t i = 0; i < expY && sig32Y != 0; ++i) {
            sig32Y *= 10;
            uint32_t digit = sig32Y >> expY;           // divide by 2^expY
            sig32Y &= ((1u << expY) - 1);              // sig32Y mod 2^expY
            *iy++ = (char)('0' + digit);
        }
    }
    *iy = '\0';  // null-terminate the string
    return strBuff;
}
