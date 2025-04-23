; sqrt of a 16-bit floating point number
; inputs: hl = a, de = b
; outputs: hl = a - b
; destroys: af, bc, de, ix
f16_sqrt:
; set index to global variables
    ld ix,s_globals
; save original value of a
    ld (ix+uiA),hl ; store uiA
; extract the sign, exponent and significand
; signA = signF16UI( uiA );
    signF16UI
; expA  = expF16UI( uiA );
    expF16UI
    ld (ix+signA),bc ; implicitly stores expA
; sigA  = fracF16UI( uiA );
    fracF16UI
    ld (ix+sigA),hl
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( expA == 0x1F ) {
    ld a,b ; expA
    cp 0x1F ; check expA for inf
    jp nz,@expA_not_inf
;     if ( sigA ) {
        ld a,l
        or h
        jr z,@F
;         uiZ = softfloat_propagateNaNF16UI( uiA, 0 );
;         goto uiZ;
            ld hl,canonicalNaNF16
            ret
;     }
@@:
;     if ( ! signA ) return a;
        ld a,c ; signA
        or a 
        jr nz,@F
        ld hl,(ix+uiA) ; uiZ = uiA
        ret
@@:
;     goto invalid;
        ld hl,canonicalNaNF16
        ret
; }
@expA_not_inf:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( signA ) {
    ld a,c ; signA
    or a ; check signA for zero
    jp z,@signA_zero
;     if ( ! (expA | sigA) ) return a;
        ld a,b ; expA
        or a ; check expA for zero
        jp nz,@F
        ld a,l
        or h ; test sigA for zero
        jp nz,@F
        ld hl,(ix+uiA) ; uiZ = uiA
        ret
@@:
;     goto invalid;
        ld hl,canonicalNaNF16
        ret
; }
@signA_zero:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( ! expA ) {
    ld a,b ; expA
    or a ; check expA for zero
    jp nz,@expA_not_zero
;     if ( ! sigA ) return a;
        ld a,l
        or h ; test sigA for zero
        jp nz,@F
        ld hl,(ix+uiA) ; uiZ = uiA
        ret
@@:
;     normExpSig = softfloat_normSubnormalF16Sig( sigA );
        call softfloat_normSubnormalF16Sig
        ld (ix+sigA),hl ; store normalised sigA
;     expA = normExpSig.exp;
;     sigA = normExpSig.sig;
        ld (ix+signA),bc ; implicitly stores expA
; }
@expA_not_zero:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; expZ = ((expA - 0xF)>>1) + 0xE;
    ld a,b ; expA
    sub 0xF ; expA - 0xF
    sra a ; expA - 0xF >> 1
    add a,0xE ; expZ = ((expA - 0xF)>>1) + 0xE
    ld (ix+expZ),a ; store expZ
; expA &= 1;
    ld a,b ; expA
    and 1 ; expA &= 1
    ld b,a ; b = expA
    ld (ix+expA),a ; store expA
; sigA |= 0x0400;
    set 2,h ; set implicit 1
; index = (sigA>>6 & 0xE) + expA;
    ld (ix+sigA),hl ; store sigA
    add hl,hl ; sigA <<= 1
    add hl,hl ; sigA <<= 1
    ld a,0xE
    and h
    add a,b ; a = index
    add a,a ; two bytes per LUT entry
    ld de,0
    ld e,a
    push iy ; preserve iy
    ld iy,softfloat_approxRecipSqrt_1k0s
    add iy,de
; r0 = softfloat_approxRecipSqrt_1k0s[index] - (((uint_fast32_t) softfloat_approxRecipSqrt_1k1s[index] * (sigA & 0x7F))>>11);
    ld hl,(iy+32) ; approxRecipSqrt_1k1s
    ld a,(ix+sigA) ; sigA low byte
    and 0x7F ; sigA & 0x7F
    call mul_16x8_24_ade ; ade holds the 24-bit product
    ld e,d
    ld d,a
    srl d
    rr e
    srl d
    rr e
    srl d
    rr e
    ld hl,(iy) ; approxRecipSqrt_1k0s
    pop iy ; restore iy since no longer needed
    or a ; clear carry
    sbc hl,de ; hl = approxRecipSqrt_1k0s - approxRecipSqrt_1k1s
    ld (ix+r0),hl ; store r0
; ESqrR0 = ((uint_fast32_t) r0 * r0)>>1;
    ld e,l
    ld d,h
    call mul_16x16_32 ; hlde = r0 * r0
    srl h
    rr l
    rr d
    rr e ; hlde = (r0 * r0)>>1
; if ( expA ) ESqrR0 >>= 1;
    ld a,(ix+expA) ; expA
    or a ; check expA for zero
    jp z,@F
    srl h
    rr l
    rr d
    rr e
@@:
; sigma0 = ~(uint_fast16_t) ((ESqrR0 * sigA)>>16);
    ld bc,(ix+sigA) ; sigA
    call mul_32x16_48 ; hlde = (ESqrR0 * sigA)>>16
    ex de,hl
    ld a,l
    cpl
    ld l,a
    ld a,h
    cpl
    ld h,a
; recipSqrt16 = r0 + (((uint_fast32_t) r0 * sigma0)>>25);
    ld de,(ix+r0)
    call mul_16x16_32 ; hlde = r0 * sigma0
    ld a,h
    srl a
    ld hl,(ix+r0)
    add a,l
    ld l,a
    adc a,h
    sub l
    ld h,a
; if ( ! (recipSqrt16 & 0x8000) ) recipSqrt16 = 0x8000;
    ld a,h
    and 0x80
    or a ; check MSB
    jp nz,@F
    ld hl,0x8000
@@:
; sigZ = ((uint_fast32_t) (sigA<<5) * recipSqrt16)>>16;
    ex de,hl ; de = recipSqrt16
    ld hl,(ix+sigA) ; sigA
    add hl,hl ; sigA <<= 1
    add hl,hl ; sigA <<= 2
    add hl,hl ; sigA <<= 3
    add hl,hl ; sigA <<= 4
    add hl,hl ; sigA <<= 5
    call mul_16x16_32 ; hlde = (sigA<<5) * recipSqrt16
; if ( expA ) sigZ >>= 1;
    ld a,(ix+expA) ; expA
    or a ; check expA for zero
    jp z,@F
    srl h
    rr l
@@:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; ++sigZ;
    inc hl
; if ( ! (sigZ & 7) ) {
    ld a,l
    and 0x07
    jp nz,@return_roundPack
;     shiftedSigZ = sigZ>>1;
        push hl ; stash sigZ
        srl h
        rr l
;     negRem = shiftedSigZ * shiftedSigZ;
        ld e,l
        ld d,h
        call mul_16x16_32 ; hlde = shiftedSigZ * shiftedSigZ
;     sigZ &= ~1;
        pop hl ; restore sigZ
        res 0,l
;     if ( negRem & 0x8000 ) {
        ld a,d
        and 0x80
        or a ; check MSB
        jr z,@F
;         sigZ |= 1;
            set 0,l ; set the LSB of sigZ
            jr @return_roundPack
;     } else {
@@:
;         if ( negRem ) --sigZ;
            ld a,d
            or e
            jr z,@return_roundPack
            dec hl
;     }
; }
@return_roundPack:
; return softfloat_roundPackToF16( 0, expZ, sigZ );
    ld c,0 ; sign is always positive for sqrt
    ld b,(ix+expZ) ; b = expZ
    jp softfloat_roundPackToF16 ; assumes hl holds sigZ
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; invalid:
; softfloat_raiseFlags( softfloat_flag_invalid );
; uiZ = defaultNaNF16UI;
; uiZ:
; uZ.ui = uiZ;
; return uZ.f;

; }
; end f16_sqrt



