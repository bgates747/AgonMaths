; convert a float16 to an unsigned 16-bit integer
; inputs: hl = float16
; outputs: hl = unsigned 16-bit integer
; destroys: af, bc, de, ix
f16_to_ui16:
; set index to global variables
    ld ix,s_globals
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
    ld (ix+signA),bc ; implicitly stores expA
; frac  = fracF16UI( uiA );
    fracF16UI
    ld (ix+sigA),hl
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( signA ) {
    ld a,c ; signA
    or a ; check signA for zero
    jp z,@sign_positive
;     softfloat_raiseFlags( softfloat_flag_invalid );
;     return ui16_fromNegOverflow;
        ld hl,ui16_fromNegOverflow
        ret
; }
@sign_positive:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( expA == 0x1F ) {
    ld a,b ; expA
    cp 0x1F ; check expA for inf
    jr nz,@exp_not_inf
    ; frac ? ui16_fromNaN
    ZERO_HL
    jr z,@frac_zero
        ld hl,ui16_fromNaN
        ret
    @frac_zero:
    ; : ui16_fromPosOverflow
        ld hl,ui16_fromPosOverflow
        ret
; }
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
@exp_not_inf:
; sig = (uint_fast32_t) frac; 
; if ( expA ) {
    ld a,b ; expA
    or a ; check expA for zero
    jp z,softfloat_roundToUI16
;     sig |= 0x0400;
        set 2,h ; set implicit 1
;     shiftDist = expA - 0x19;
        sub 0x19 ; expA - 0x19
;     if ( 0 <= shiftDist) ) {
        ret z ; no need to shift
        jp m,@shiftDist_neg
        ld b,a ; shiftDist = loop counter
;         return (uint_fast16_t)(sig << shiftDist);
        @shift_loop:
            add hl,hl ; sig <<= 1
            djnz @shift_loop ; loop until all bits shifted
            ret
;     }
    @shiftDist_neg:
;     shiftDist = expA - 0x0D;
        ld a,b ; expA
        sub 0x0D ; expA - 0x0D
;     if ( 0 < shiftDist ) {
        jp m,softfloat_roundToUI16
        ld b,a ; shiftDist = loop counter
        @shift_loop2:
;         sig <<= shiftDist; // This will never overfow 22 bits
            add hl,hl ; sig <<= 1
            djnz @shift_loop2 ; loop until all bits shifted
            jp softfloat_roundToUI16
;     }
; }
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; return softfloat_roundToUI16( (uint_fast16_t) sig );
    ; will have been called above as appropriate
