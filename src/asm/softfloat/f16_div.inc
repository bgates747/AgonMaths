; inputs: hl = dividend, de = divisor
; outputs: hl = float16 result
; destroys af, bc, de, ix
f16_div:
; set index to global variables
    ld ix,s_globals
; compute sign of the quotient
    ld a,h ; sign opA
    and %10000000
    xor d ; xor sign opA with sign opB
    and %10000000
    ld (ix+signZ),a

; unpack opB
    ex de,hl
    call softfloat16_unpack
    ld (ix+sigB),hl ; store sigB
    ld a,b ; exponent
    ld (ix+expB),a ; store exponent

; unpack opA
    ex de,hl
    call softfloat16_unpack
    ld a,b ; exponent
    ld (ix+expA),a
    ld (ix+sigA),hl ; store sigA

; === HANDLE SPECIAL CASES ===
; if ( expA == 0x1F ) {
    cp 0x1F ; check expA for inf
    jr nz,@check_expB_inf
;     if ( sigA ) goto propagateNaN;
        SIGN_HL
        jp nz,@return_nan
;     if ( expB == 0x1F ) {
        ld a,(ix+expB)
        cp 0x1F ; check expB for inf
        jp nz,@return_inf
;         if ( sigB ) goto propagateNaN;
            ld hl,(ix+sigB)
            SIGN_HL
            jp nz,@return_nan ; non-signalling NaN
;         goto invalid;
            jp @return_invalid ; signalling NaN
;     }
;     goto infinity; // handled above
; }
@check_expB_inf:
; if ( expB == 0x1F ) {
    ld a,(ix+expB) 
    cp 0x1F ; check expB for inf
    jr nz,@check_expB_zero
;     if ( sigB ) goto propagateNaN;
        ld hl,(ix+sigB)
        SIGN_HL
        jp nz,@return_nan ; non-signalling NaN
;     goto zero;
        jp @return_zero
; }
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
@check_expB_zero:
; if ( ! expB ) {
    or a ; check expB for zero
    jr nz,@check_expA_zero
; if ( ! sigB ) {
    ld hl,(ix+sigB)
    SIGN_HL
    jr nz,@check_expA_zero
; if ( ! (expA | sigA) ) goto invalid;
    ld a,(ix+expA)
    or a ; check expA for zero
    jr nz,@b_zero_a_nonzero
    ld hl,(ix+sigA)
    SIGN_HL
    jr nz,@b_zero_a_nonzero
; 0/0 case
    jp @return_invalid
@b_zero_a_nonzero:
; goto infinity;
    jp @return_inf
; }
@check_expA_zero:
; if ( ! expA ) {
    ld a,(ix+expA)
    or a ; check expA for zero
    jr nz,@end_specials
;     if ( ! sigA ) goto zero;
        ld hl,(ix+sigA)
        SIGN_HL
        jp z,@return_zero
; }
@end_specials:

; expZ = expA - expB + 0xE
    ld a,(ix+expA)
    ld b,a
    ld a,(ix+expB)
    sub b
    neg
    add 0x0E
; if sigA < sigB: expZ = expZ - 1, numShifts = 5
; else: numShifts = 4
; sigA = u16(sigA << numShifts)
    ld bc,(ix+sigB)
    or a ; clear carry
    sbc.s hl,bc ; sigA - sigB (.s to force 16-bit subtraction)
    ld hl,(ix+sigA)
    jr nc,@sigA_gte_sigB ; sigA < sigB
; expZ = expZ - 1
    dec a
; sigA = u16(sigA << 5)
    add hl,hl
@sigA_gte_sigB:
    ld (ix+expZ),a
; else: sigA = u16(sigA << 4)
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    ld (ix+sigA),hl ; store sigA shifted up

; index = sigB>>6 & 0xF;
    ld l,c
    ld h,b
    add hl,hl
    add hl,hl
    ld a,h
    and 0x0F ; index

    add a,a ; 2 bytes per LUT entry
    ld e,a
    ld d,0
    dec de
    inc.s de ; zero deu
    push ix ; preserve
    ld ix,softfloat_approxRecip_1k0s
    add ix,de

; r0 = approx1k0s - (((approx1k1s * (sigB & 0x3F)) >> 10))
    ld hl,(ix+32) ; approx1k1s
    ld a,c ; sigB low byte
    and 0x3F ; sigB & 0x3F
    call mul_16x8_24_ade ; (sigB & 0x3F) -> ade
; >>10
    or a ; clear carry
    rra
    rr d
    or a ; clear carry
    rra
    rr d
    ld e,d
    ld d,a
    dec de
    inc.s de ; zero deu
    ld hl,(ix) ; approx1k0s
    pop ix ; send back how it came in
    or a ; clear carry
    sbc.s hl,de ; hl = r0 = approx1k0s - ((approx1k1s * (sigB & 0x3F)) >> 10)
    ld (ix+r0),hl ; store r0

; sigZ = u32((sigA * r0) >> 16)
    ld de,(ix+sigA)
    call mul_16x16_32 ; hl = sigZ
    ld (ix+sigZ),hl
    call @compute_sigZ_error

; sigZ += (rem * (uint_fast32_t) r0)>>26;
; both multiplicands are 16 bits so we can use 16x16->32 bit multiply
    ld hl,(ix+r0)
    call mul_16x16_32 ; hlde = rem * r0
    ld a,h ; top byte of product
    srl a
    srl a
    ld hl,(ix+sigZ)
    add a,l
    ld l,a
    adc a,h
    sub l
    ld h,a
; ++sigZ;
    inc hl

; if ( ! (sigZ & 7) ) {
    ld a,l
    and 0x07
    jr nz,@return
;     sigZ &= ~1;
        res 0,l
        ld (ix+sigZ),hl
;     rem = (sigA<<10) - sigZ * sigB;
        call @compute_sigZ_error
;     if ( rem & 0x8000 ) {
        ld hl,(ix+sigZ)
        ld a,d
        and 0x80
        jr z,@F
;         sigZ -= 2;
            dec hl
            dec hl
            jr @return
;     } else {
@@:
;         if ( rem ) sigZ |= 1;
            ld a,d
            or a
            jr nz,@F
            ld a,e
            or a
            jr z,@return
@@:
            set 0,l
;     }
; }

@return:
; hl must be set to sigZ at this point
    ld bc,(ix+signZ) ; b=expZ, c=signZ
    jp softfloat_roundPackToF16 ; assumes hl holds sig
@return_invalid: ; fow now, we don't distinguish between signalling and non-signalling NaN
@return_nan:
    ld hl,canonicalNaNF16
    ret
@return_inf:
    ld l,0
    ld a,(ix+signZ)
    or 0x7C
    ld h,a
    ret
@return_zero:
    ld l,0
    ld a,(ix+signZ)
    ld h,a
    ret
; compute the error of sigZ
; inputs: hl = sigZ, (ix+sigB), (ix+sigA), (ix+r0)
; returns: hlde = rem (only de should matter)
@compute_sigZ_error:
; rem = u32((sigA << 10) - sigZ * sigB)
    ld de,(ix+sigB)
    call mul_16x16_32 ; hlde = sigZ * sigB
    push hl ; stack upper word of product
    ld hl,(ix+sigA)
    add hl,hl
    add hl,hl
    ; ld (ix+rem),hl ; don't need since difference will not occupy more than two bytes
    pop bc ; restore upper word of product (was hl)
    xor a ; a is our phantom zero register
; now we're set to do the subtraction:
;   uhl0
; - bcde
;  ------
;   bcde
    sub e
    ld e,a
    ld a,l
    sbc a,d
    ld d,a
; ; difference is contained in two bytes so we only need de
;     ld a,h
;     sbc a,c
;     ld c,a
;     ld a,(ix+rem+2) ; hlu
;     sub b
;     ld b,a ; we assume no carry out
    ret
; end @compute_sigZ_error
; end f16_div
