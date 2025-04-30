#include <stdbool.h>
#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "specialize.h"
#include "softfloat.h"

/*-------------------------------------------------------------------------
 * π constants in Q2.14 fixed-point
 *-------------------------------------------------------------------------*/
#define Q14(x)  ((int32_t)((x)*16384.0 + 0.5))
static const int32_t PI_OVER_2_Q14   = Q14(1.57079632679489661923);
static const int32_t TWO_OVER_PI_Q14 = Q14(0.63661977236758134308);

/*-------------------------------------------------------------------------
 * 5th-order minimax for sin(y) on |y| ≤ π/4, coefficients in Q2.14
 *   sin y ≈ y + C3·y³ + C5·y⁵
 *-------------------------------------------------------------------------*/
static const int16_t C3_Q14 = (int16_t)Q14(-0.1666665668f);
static const int16_t C5_Q14 = (int16_t)Q14(+0.0083330251f);

static inline int_fast32_t
poly_sin_q14( int_fast32_t y_q14 )
{
    /* y_q14 is Q2.14 */
    int_fast32_t y2 = (int_fast32_t)((int64_t)y_q14 * y_q14 >> 14);     /* y² Q2.14 */
    int_fast32_t y3 = (int_fast32_t)((int64_t)y2     * y_q14 >> 14);    /* y³ Q2.14 */
    int_fast32_t t3 = (int_fast32_t)((int64_t)C3_Q14 * y3     >> 14);    /* C3·y³ */
    int_fast32_t y5 = (int_fast32_t)((int64_t)y3     * y2     >> 14);    /* y⁵ Q2.14 */
    int_fast32_t t5 = (int_fast32_t)((int64_t)C5_Q14 * y5     >> 14);    /* C5·y⁵ */
    return y_q14 + t3 + t5;                                             /* Q2.14 */
}

//----------------------------------------------------------------------
// Convert float16 bits → Q2.14 fixed‐point (32‐bit signed)
//    raw 16‐bit → sign, expA (unbiased), sigA
//    x_Q14 = (1.frac)*2^14 * 2^(expA−15)
//----------------------------------------------------------------------
static inline int_fast32_t
f16_to_q14( uint_fast16_t uiA )
{
    bool          neg  = signF16UI(uiA);
    int_fast8_t   expA = (int_fast8_t)expF16UI(uiA) - 15;   // unbiased
    uint_fast16_t sigA = fracF16UI(uiA);
    // build 1.frac as integer, then shift left 4 to Q2.14
    uint32_t mant = expF16UI(uiA) ? (0x400u | sigA) : sigA;
    int_fast32_t val  = (int_fast32_t)(mant << 4);
    // apply true exponent (expA − 15 already baked into expA)
    if (expA > 0)    val <<= expA;
    else if (expA < 0) val >>= -expA;
    return neg ? -val : val;
}

//----------------------------------------------------------------------
// Convert Q2.14 fixed‐point → float16 bits
//    x_q14 = val * 2^14  (int‐scaled), want f16 ≈ x_q14/2^14
//----------------------------------------------------------------------
static inline uint_fast16_t
q14_to_f16( int_fast32_t x_q14 )
{
    bool neg = x_q14 < 0;
    if (neg) x_q14 = -x_q14;

    // zero special‐case
    if (x_q14 == 0) {
        return packToF16UI( neg, 0, 0 );
    }

    // normalize so that 1.0 ≤ (x_norm/2^14) < 2.0
    // i.e. x_norm ∈ [2^14, 2^15)
    int shift = 0;
    while (x_q14 < (1 << 14)) {
        x_q14 <<= 1;
        --shift;
    }
    while (x_q14 >= (1 << 15)) {
        x_q14 >>= 1;
        ++shift;
    }

    // now x_q14 = (1.frac) in Q2.14 form,
    // fraction bits = (x_norm - 2^14) >> (14−10) = >>4
    uint_fast16_t frac = (uint_fast16_t)((x_q14 - (1 << 14)) >> 4) & 0x03FFu;

    // exponent = unbiased E + bias(15),
    // where E = shift
    uint_fast8_t exp = (uint_fast8_t)(15 + shift);

    return packToF16UI( neg, exp, frac );
}


/*-------------------------------------------------------------------------
 * f16_sin
 *
 * Variables (from your scratch list):
 *
 *  uiA    : raw 16-bit half-precision input (bits)
 *  signA  : sign bit (0 = +, 1 = −)
 *  expA   : exponent field (5 bits, biased by 15)
 *  sigA   : fraction field (10 bits, no implicit leading 1)
 *
 *  expDiff: quadrant (0–3) from range reduction
 *  rem    : remainder in Q2.14 (after subtracting k·π/2)
 *
 *  sig32A : input x in Q2.14 (32-bit signed)
 *  sig32Y : reduced argument y in Q2.14
 *  sig32Z : polynomial result sin(y) in Q2.14
 *
 *  uiZ    : raw 16-bit half-precision output (bits)
 *-------------------------------------------------------------------------*/
float16_t f16_sin( float16_t a )
{
    union ui16_f16 uA, uZ;
    uint_fast16_t uiA, uiZ;
    bool          signA;
    int_fast8_t   expA, expDiff;
    uint_fast16_t sigA;
    int_fast32_t  sig32A, sig32Y, sig32Z;
    int_fast32_t  rem;

    /*------------------------------------------------------------------------
     * unpack input
     *------------------------------------------------------------------------*/
    uA.f   = a;
    uiA    = uA.ui;
    signA  = signF16UI(uiA);
    expA   = (int_fast8_t)expF16UI(uiA);
    sigA   = fracF16UI(uiA);

    /*------------------------------------------------------------------------
     * handle zeros
     *------------------------------------------------------------------------*/
    if (expA == 0 && sigA == 0) {
        /* preserve signed zero */
        return a;
    }
    /*------------------------------------------------------------------------
     * handle NaN/Inf
     *------------------------------------------------------------------------*/
    if (expA == 0x1F) {
        if (sigA) {
            uiZ = softfloat_propagateNaNF16UI(uiA, 0);
        } else {
            softfloat_raiseFlags( softfloat_flag_invalid );
            uiZ = defaultNaNF16UI;
        }
        goto uiZ_label;
    }

    /*------------------------------------------------------------------------
     * 1) Convert input → Q2.14
     *------------------------------------------------------------------------*/
    sig32A = f16_to_q14(uiA);

    /*------------------------------------------------------------------------
     * 2) Range-reduce x → y in [−π/4,π/4]
     *    k = round(x * 2/π)
     *    expDiff = k & 3
     *    rem      = x_q14 − k*(π/2)_Q14
     *------------------------------------------------------------------------*/
    {
        int_fast32_t prod = (int_fast32_t)((int64_t)sig32A * TWO_OVER_PI_Q14 >> 14);
        int_fast8_t  k    = (prod + (prod>=0 ? 0x2000 : -0x2000)) >> 14;
        expDiff        = k & 3;
        rem            = sig32A - (int_fast32_t)(k * PI_OVER_2_Q14);
        sig32Y         = rem;
    }

    /*------------------------------------------------------------------------
     * 3) Evaluate sin or cos via the same poly
     *------------------------------------------------------------------------*/
    if ((expDiff & 1) == 0) {
        /* quadrants 0 or 2: direct sine */
        sig32Z = poly_sin_q14(sig32Y);
    } else {
        /* quadrants 1 or 3: cosine = sin(π/2 − |y|) */
        int_fast32_t yAbs = sig32Y >= 0 ? sig32Y : -sig32Y;
        sig32Z = poly_sin_q14(PI_OVER_2_Q14 - yAbs);
    }

    /*------------------------------------------------------------------------
     * 4) Flip sign for quadrants 2 and 3
     *------------------------------------------------------------------------*/
    if (expDiff >= 2) sig32Z = -sig32Z;

    /*------------------------------------------------------------------------
     * 5) Pack back → half-precision bits
     *------------------------------------------------------------------------*/
    uiZ = q14_to_f16(sig32Z);

 uiZ_label:
    uZ.ui = uiZ;
    return uZ.f;
}


/*-------------------------------------------------------------------------
 * CORDIC constants in Q2.14 fixed-point
 *-------------------------------------------------------------------------*/
static const int16_t cordic_atan_q14[16] = {
    (int16_t)Q14(0.7853981633974483f),  /* atan(1)   = π/4 */
    (int16_t)Q14(0.4636476090008061f),  /* atan(0.5) */
    (int16_t)Q14(0.24497866312686414f), /* 2⁻²      */
    (int16_t)Q14(0.12435499454676144f), /* 2⁻³      */
    (int16_t)Q14(0.06241880999595735f), /* 2⁻⁴      */
    (int16_t)Q14(0.031239833430268277f),/* 2⁻⁵      */
    (int16_t)Q14(0.015623728620476831f),/* 2⁻⁶      */
    (int16_t)Q14(0.007812341060101111f),/* 2⁻⁷      */
    (int16_t)Q14(0.0039062301319669718f),/*2⁻⁸     */
    (int16_t)Q14(0.0019531225164788188f),/*2⁻⁹     */
    (int16_t)Q14(0.0009765621895593195f),/*2⁻¹⁰    */
    (int16_t)Q14(0.0004882812111948983f),/*2⁻¹¹    */
    (int16_t)Q14(0.00024414062014936177f),/*2⁻¹²   */
    (int16_t)Q14(0.00012207031189367021f),/*2⁻¹³  */
    (int16_t)Q14(0.00006103515617420877f),/*2⁻¹⁴  */
    (int16_t)Q14(0.000030517578115526096f)/*2⁻¹⁵  */
};

//----------------------------------------------------------------------
// rempio2_q14:  Range‐reduce x_q14 into [–π/4, +π/4] (Q2.14 fixed‐point)
//   Returns k&3 in [0..3] and writes the remainder into *rem_q14.
//----------------------------------------------------------------------
static inline uint_fast8_t rempio2_q14(int_fast32_t x_q14, int_fast32_t *rem_q14)
{
    // prod = x * (2/π) in Q2.14
    int_fast32_t prod = (int_fast32_t)((int64_t)x_q14 * TWO_OVER_PI_Q14 >> 14);
    // k = round(prod) to nearest integer
    int_fast8_t  k    = (prod + (prod >= 0 ? 0x2000 : -0x2000)) >> 14;
    // remainder = x - k*(π/2)
    *rem_q14 = x_q14 - (int_fast32_t)(k * PI_OVER_2_Q14);
    return (uint_fast8_t)(k & 3);
}

/* 1/K ≈ 0.607252935; in Q2.14 that’s ~9956 decimal */
#define CORDIC_KINV_Q14  ((int_fast32_t)Q14(0.6072529350088814f))

float16_t f16_sin_cordic( float16_t a )
{
    union ui16_f16 uA, uZ;
    uint_fast16_t uiA, uiZ;
    bool          signA;
    int_fast8_t   expA, expDiff;
    uint_fast16_t sigA;
    int_fast32_t  rem, sig32X, sig32Y;
    int           i;

    /*— 1) unpack input —*/
    uA.f   = a;
    uiA    = uA.ui;
    signA  = signF16UI(uiA);
    expA   = (int_fast8_t)expF16UI(uiA);
    sigA   = fracF16UI(uiA);

    /*— 2) handle ±0 —*/
    if (expA == 0 && sigA == 0) {
        return a;
    }
    /*— 3) handle NaN/Inf —*/
    if (expA == 0x1F) {
        if (sigA) {
            uiZ = softfloat_propagateNaNF16UI(uiA, 0);
        } else {
            softfloat_raiseFlags( softfloat_flag_invalid );
            uiZ = defaultNaNF16UI;
        }
        goto uiZ_label;
    }

    /*— 4) range-reduce: float16 → Q2.14 → rem + quadrant (expDiff) —*/
    {
        int_fast32_t x_q14 = f16_to_q14(uiA);
        expDiff = rempio2_q14(x_q14, &rem);  /* expDiff ∈ 0…3, rem ∈ [−π/4,π/4] */
    }

    /*— 5) init CORDIC vector at (1/K, 0) —*/
    sig32X = CORDIC_KINV_Q14;
    sig32Y = 0;

    /*— 6) perform 16 CORDIC rotations on rem —*/
    for (i = 0; i < 16; i++) {
        int_fast32_t dx = sig32X >> i;
        int_fast32_t dy = sig32Y >> i;
        if (rem >= 0) {
            sig32X -= dy;
            sig32Y += dx;
            rem      -= cordic_atan_q14[i];
        } else {
            sig32X += dy;
            sig32Y -= dx;
            rem      += cordic_atan_q14[i];
        }
    }

    /*— 7) dispatch sine vs. cosine by quadrant, fold sign —*/
    switch (expDiff & 3) {
      case 0: uiZ = q14_to_f16( +sig32Y ); break;  /* sin(rem)       */
      case 1: uiZ = q14_to_f16( +sig32X ); break;  /* sin(π/2+rem)=cos(rem) */
      case 2: uiZ = q14_to_f16( -sig32Y ); break;  /* sin(π+rem)=−sin(rem)   */
      default:uiZ = q14_to_f16( -sig32X ); break;  /* sin(3π/2+rem)=−cos(rem)*/
    }

 uiZ_label:
    uZ.ui = uiZ;
    return uZ.f;
}
