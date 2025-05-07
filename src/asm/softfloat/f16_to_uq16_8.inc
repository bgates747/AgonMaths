; convert a float16 to an unsigned 16.8 fixed-point number
; inputs: hl = float16
; outputs: uhl = unsigned 16.8 fixed-point number
; destroys: af, bc, de
f16_to_uq16_8:
; clear hlu
    dec hl
    inc.s hl
; union ui16_f16    uA;
; uint_fast16_t     uiA;
; bool              signA;
; int_fast8_t       expA;
; uint_fast16_t     frac;
; uint_fast32_t     sig;
; int_fast8_t       shiftDist;
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; uA.f  = a;
; uiA   = uA.ui;
; signA  = signF16UI( uiA );
    signF16UI
; expA   = expF16UI( uiA );
    expF16UI
; frac  = fracF16UI( uiA );
    fracF16UI
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( signA ) {
    ld a,c ; signA
    or a ; check signA for zero
    jp z,@sign_positive
;     return uq16_8_fromNegOverflow;
        ld hl,uq16_8_fromNegOverflow
        ret
; }
@sign_positive:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( expA == 0x1F ) {
    ld a,b ; expA
    cp 0x1F ; check expA for inf
    jr nz,@exp_not_inf
    ; frac ? uq16_8_fromNaN
    ZERO_HL
    jr z,@frac_zero
        ld hl,uq16_8_fromNaN
        ret
    @frac_zero:
    ; : uq16_8_fromPosOverflow
        ld hl,uq16_8_fromPosOverflow
        ret
; }
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
@exp_not_inf:
; // Too small to represent (exp < 7) → flush to zero. 
; if (exp < 7) {
    cp 7
    jr nc,@exp_gte_6
;     return 0;
        ld hl,0
        ret
; }
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
@exp_gte_6:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; // Normal number: build 11-bit mantissa
; sig32  = frac | 0x0400;
    set 2,h ; set implicit 1

; // exp bias = 15; we need to scale by 2^(e) and then by 2^8 for the .8 format.
; // net shift = (exp – 15) + 8 = exp – 7.
; // We split into two ranges to keep the “roundTo…” call only when needed.

; // 1) For really big exponents (>18) all fractional bits land above the fixed-point
; //    and sig32 << (exp-17) will have its bottom 4 bits == 0 → no rounding.
; //    We can do it inline.
; if ( exp > 18 ) {
    cp 19
    jr c,@exp_lte_18
;     shiftDist = exp - 17;
        sub 17
        ld b,a ; shiftDist = loop counter
;     return sig32 << (shiftDist);
        @shift_loop18:
            add hl,hl ; shift left
        djnz @shift_loop18 ; loop until all bits shifted
        ret
; }
@exp_lte_18:
; // 2) For moderately large exponents [13..18], we shift left enough to leave
; //    exactly 4 fractional bits and call our simple round-to-even routine.
; if ( exp >= 13 ) {
    cp 13
    jp z,softfloat_roundToUQ16_8 ; no shifting needed so just do rounding
    jr c,@exp_lt_13
;     shiftDist = exp - 13;
        sub 13
;     uint_fast32_t shifted = sig32 << (shiftDist);
        ld b,a ; shiftDist = loop counter
        @shift_loop13:
            add hl,hl ; shift left
        djnz @shift_loop13 ; loop until all bits shifted
;     return softfloat_roundToUQ16_8( shifted, exact );
        jp softfloat_roundToUQ16_8
; }
@exp_lt_13:
; // exp < 13: shift to get exactly 4 fractional bits, then round-to-nearest-even
; {
;     shiftDist = 13 - exp;
        sub 13
        neg ; a = 13 - exp
        jp z,softfloat_roundToUQ16_8 ; no shifting needed so just do rounding
;     if (shiftDist > 0) {
        jp m,@shiftDist_neg
;         // right-shift to drop high bits, leaving 4 frac bits
;         sig32 >>= shiftDist;
            ld b,a ; shiftDist = loop counter
            @shift_loop_pos:
                srl h 
                rr l
            djnz @shift_loop_pos
            jp softfloat_roundToUQ16_8
@shiftDist_neg:
;     } else {
;         // left-shift to make room for 4 frac bits
;         sig32 <<= ( - shiftDist );
            neg ; flip sign of shiftDist
            ld b,a ; shiftDist = loop counter
            @shift_loop_neg:
                add hl,hl ; shift left
            djnz @shift_loop_neg ; loop until all bits shifted
            jp softfloat_roundToUQ16_8
;     }
;     return softfloat_roundToUQ16_8( sig32, exact ); // already did above
; }