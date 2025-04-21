; sqrt of a 16-bit floating point number
; inputs: hl = a, de = b
; outputs: hl = a - b
; destroys: af, bc, de, ix
f16_sqrt:
; set index to global variables
    ld ix,s_globals
; extract the sign, exponent and significand
; signA = signF16UI( uiA );
    signF16UI
; expA  = expF16UI( uiA );
    expF16UI
    ld (ix+signA),bc ; implicitly stores expA
    ; PRINT_BC_HEX "store expA, signA" ; DEBUG
; sigA  = fracF16UI( uiA );
    fracF16UI
    ld (ix+sigA),hl
    ; PRINT_HL_HEX "store sigA\r\n" ; DEBUG
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; if ( expA == 0x1F ) {
    ld a,b ; expA
    cp 0x1F ; check expA for inf
    jp nz,@expA_not_inf
;     if ( sigA ) {
        SIGN_HL ; test sigA for zero
        jr z,@F
;         uiZ = softfloat_propagateNaNF16UI( uiA, 0 );
;         goto uiZ;
            ld hl,canonicalNaNF16
            ret
;     }
@@:
;     if ( ! signA ) return a;
        ld a,c ; signA
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
        SIGN_HL ; test sigA for zero
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
        SIGN_HL ; test sigA for zero
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
    ld a,b
    sub 0xF ; expA - 0xF
    srl a ; expA - 0xF >> 1
    add a,0xE ; expZ = ((expA - 0xF)>>1) + 0xE
; expA &= 1;
    and 1 ; expA &= 1
    ld b,a ; c = expA
; sigA |= 0x0400;
    set 2,h ; set implicit 1
; index = (sigA>>6 & 0xE) + expA;
    ld (ix+sigA),hl ; store sigA
    add hl,hl ; sigA <<= 1
    add hl,hl ; sigA <<= 1
    ld a,0xE
    and h
    add a,b ; a = index
    PRINT_A_HEX "index = (sigA>>6 & 0xE) + expA;" ; DEBUG
    ld de,0
    ld e,a
    push iy ; preserve iy
    ld iy,softfloat_approxRecipSqrt_1k0s
    add iy,de
; r0 = softfloat_approxRecipSqrt_1k0s[index] - (((uint_fast32_t) softfloat_approxRecipSqrt_1k1s[index] * (sigA & 0x7F))>>11);
    ld hl,(iy) ; approxRecipSqrt_1k0s
    ld de,(iy+32) ; approxRecipSqrt_1k1s
    or a ; clear carry
    sbc hl,de ; hl = approxRecipSqrt_1k0s - approxRecipSqrt_1k1s

    pop iy ; restore iy

; ESqrR0 = ((uint_fast32_t) r0 * r0)>>1;
; if ( expA ) ESqrR0 >>= 1;
; sigma0 = ~(uint_fast16_t) ((ESqrR0 * sigA)>>16);
; recipSqrt16 = r0 + (((uint_fast32_t) r0 * sigma0)>>25);
; if ( ! (recipSqrt16 & 0x8000) ) recipSqrt16 = 0x8000;
; sigZ = ((uint_fast32_t) (sigA<<5) * recipSqrt16)>>16;
; if ( expA ) sigZ >>= 1;
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; ++sigZ;
; if ( ! (sigZ & 7) ) {
;     shiftedSigZ = sigZ>>1;
;     negRem = shiftedSigZ * shiftedSigZ;
;     sigZ &= ~1;
;     if ( negRem & 0x8000 ) {
;         sigZ |= 1;
;     } else {
;         if ( negRem ) --sigZ;
;     }
; }
; return softfloat_roundPackToF16( 0, expZ, sigZ );
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; invalid:
; softfloat_raiseFlags( softfloat_flag_invalid );
; uiZ = defaultNaNF16UI;
; uiZ:
; uZ.ui = uiZ;
; return uZ.f;

; }
    ret
; end f16_sqrt

softfloat_approxRecipSqrt_1k0s:
    dl 0xB4C9, 0xFFAB, 0xAA7D, 0xF11C, 0xA1C5, 0xE4C7, 0x9A43, 0xDA29
    dl 0x93B5, 0xD0E5, 0x8DED, 0xC8B7, 0x88C6, 0xC16D, 0x8424, 0xBAE1

softfloat_approxRecipSqrt_1k1s:
    dl 0xA5A5, 0xEA42, 0x8C21, 0xC62D, 0x788F, 0xAA7F, 0x6928, 0x94B6
    dl 0x5CC7, 0x8335, 0x52A6, 0x74E2, 0x4A3E, 0x68FE, 0x432B, 0x5EFD
