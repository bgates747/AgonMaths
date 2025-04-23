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
    ; PRINT_A_HEX "expA" ; DEBUG
    sub 0xF ; expA - 0xF
    sra a ; expA - 0xF >> 1
    add a,0xE ; expZ = ((expA - 0xF)>>1) + 0xE
    ld (ix+expZ),a ; store expZ
    ; PRINT_A_HEX "store expZ = ((expA - 0xF)>>1) + 0xE;" ; DEBUG
; expA &= 1;
    ld a,b ; expA
    and 1 ; expA &= 1
    ; PRINT_A_HEX "expA &= 1;" ; DEBUG
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
    ; PRINT_A_HEX "index = (sigA>>6 & 0xE) + expA;" ; DEBUG
    add a,a ; two bytes per LUT entry
    ld de,0
    ld e,a
    push iy ; preserve iy
    ld iy,softfloat_approxRecipSqrt_1k0s
    add iy,de
; r0 = softfloat_approxRecipSqrt_1k0s[index] - (((uint_fast32_t) softfloat_approxRecipSqrt_1k1s[index] * (sigA & 0x7F))>>11);
    ld hl,(iy+32) ; approxRecipSqrt_1k1s
    ; PRINT_HL_HEX "approxRecipSqrt_1k1s[index]" ; DEBUG
    ld a,(ix+sigA) ; sigA low byte
    and 0x7F ; sigA & 0x7F
    ; PRINT_A_HEX "sigA & 0x7F" ; DEBUG
    call mul16x8 ; ade holds the 24-bit product
    ; PRINT_ADE_HEX "softfloat_approxRecipSqrt_1k1s[index] * (sigA & 0x7F)" ; DEBUG
    ld e,d
    ld d,a
    srl d
    rr e
    srl d
    rr e
    srl d
    rr e
    ; PRINT_DE_HEX "softfloat_approxRecipSqrt_1k1s[index] * (sigA & 0x7F) >> 11" ; DEBUG
    ld hl,(iy) ; approxRecipSqrt_1k0s
    pop iy ; restore iy since no longer needed
    or a ; clear carry
    sbc hl,de ; hl = approxRecipSqrt_1k0s - approxRecipSqrt_1k1s
    ld (ix+r0),hl ; store r0
    ; PRINT_HL_HEX "r0" ; DEBUG
; ESqrR0 = ((uint_fast32_t) r0 * r0)>>1;
    ld e,l
    ld d,h
    call mul_16_32 ; hlde = r0 * r0
    srl h
    rr l
    rr d
    rr e ; hlde = (r0 * r0)>>1
    ; PRINT_HLDE_HEX "ESqrR0 = ((uint_fast32_t) r0 * r0)>>1;" ; DEBUG
; if ( expA ) ESqrR0 >>= 1;
    ld a,(ix+expA) ; expA
    ; PRINT_A_HEX "expA" ; DEBUG
    or a ; check expA for zero
    jp z,@F
    srl h
    rr l
    rr d
    rr e
    ; PRINT_HLDE_HEX "if ( expA ) ESqrR0 >>= 1;" ; DEBUG
@@:
; sigma0 = ~(uint_fast16_t) ((ESqrR0 * sigA)>>16);
    ld bc,(ix+sigA) ; sigA
    ; PRINT_BC_HEX "sigA" ; DEBUG
    call mul_32x16_48 ; hlde = (ESqrR0 * sigA)>>16
    ; PRINT_HLDE_HEX "(ESqrR0 * sigA)>>16" ; DEBUG
    ex de,hl
    ld a,l
    cpl
    ld l,a
    ld a,h
    cpl
    ld h,a
    ; PRINT_HL_HEX "~((ESqrR0 * sigA)>>16)" ; DEBUG
; recipSqrt16 = r0 + (((uint_fast32_t) r0 * sigma0)>>25);
    ld de,(ix+r0)
    call mul_16_32 ; hlde = r0 * sigma0
    ; PRINT_HLDE_HEX "r0 * sigma0" ; DEBUG
    ld a,h
    srl a
    ; PRINT_A_HEX "r0 * sigma0 >> 25" ; DEBUG
    ld hl,(ix+r0)
    ; PRINT_HL_HEX "r0" ; DEBUG
    add a,l
    ld l,a
    adc a,h
    sub l
    ld h,a
    ; PRINT_HL_HEX "recipSqrt16 = r0 + ((r0 * sigma0)>>25);" ; DEBUG
; if ( ! (recipSqrt16 & 0x8000) ) recipSqrt16 = 0x8000;
    ld a,h
    and 0x80
    or a ; check MSB
    jp nz,@F
    ld hl,0x8000
@@:
    ; PRINT_HL_HEX "if ( ! (recipSqrt16 & 0x8000) ) recipSqrt16 = 0x8000;" ; DEBUG
; sigZ = ((uint_fast32_t) (sigA<<5) * recipSqrt16)>>16;
    ex de,hl ; de = recipSqrt16
    ld hl,(ix+sigA) ; sigA
    add hl,hl ; sigA <<= 1
    add hl,hl ; sigA <<= 2
    add hl,hl ; sigA <<= 3
    add hl,hl ; sigA <<= 4
    add hl,hl ; sigA <<= 5
    call mul_16_32 ; hlde = (sigA<<5) * recipSqrt16
    ; PRINT_HL_HEX "sigZ = ((uint_fast32_t) (sigA<<5) * recipSqrt16)>>16;" ; DEBUG
; if ( expA ) sigZ >>= 1;
    ld a,(ix+expA) ; expA
    or a ; check expA for zero
    jp z,@F
    srl h
    rr l
    ; PRINT_HL_HEX "if ( expA ) sigZ >>= 1;" ; DEBUG
@@:
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; ++sigZ;
    inc hl
    ; PRINT_HL_HEX "++sigZ;" ; DEBUG
; if ( ! (sigZ & 7) ) {
    ld a,l
    and 0x07
    ; PRINT_A_HEX "if ( ! (sigZ & 7) )" ; DEBUG
    jp nz,@return_roundPack
;     shiftedSigZ = sigZ>>1;
        push hl ; stash sigZ
        srl h
        rr l
;     negRem = shiftedSigZ * shiftedSigZ;
        ld e,l
        ld d,h
        call mul_16_32 ; hlde = shiftedSigZ * shiftedSigZ
        ; PRINT_DE_HEX "negRem = shiftedSigZ * shiftedSigZ;" ; DEBUG
;     sigZ &= ~1;
        pop hl ; restore sigZ
        res 0,l
        ; PRINT_HL_HEX "sigZ &= ~1;" ; DEBUG
;     if ( negRem & 0x8000 ) {
        ld a,d
        and 0x80
        ; PRINT_A_HEX "if ( negRem & 0x8000 )" ; DEBUG
        or a ; check MSB
        jr z,@F
;         sigZ |= 1;
            set 0,l ; set the LSB of sigZ
            ; PRINT_HL_HEX "sigZ |= 1;" ; DEBUG
            jr @return_roundPack
;     } else {
@@:
;         if ( negRem ) --sigZ;
            ; PRINT_DE_HEX "if ( negRem ) --sigZ;" ; DEBUG
            ld a,d
            or e
            ; PRINT_A_HEX "if ( negRem )" ; DEBUG
            jr z,@return_roundPack
            dec hl
            ; PRINT_HL_HEX "--sigZ;" ; DEBUG
;     }
; }
@return_roundPack:
; return softfloat_roundPackToF16( 0, expZ, sigZ );
    ld c,0 ; sign is always positive for sqrt
    ld b,(ix+expZ) ; b = expZ
    ; PRINT_BC_HEX "b = signZ, c = expZ" ; DEBUG
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
    ret
; end f16_sqrt

softfloat_approxRecipSqrt_1k0s:
    dw 0xB4C9, 0xFFAB, 0xAA7D, 0xF11C, 0xA1C5, 0xE4C7, 0x9A43, 0xDA29
    dw 0x93B5, 0xD0E5, 0x8DED, 0xC8B7, 0x88C6, 0xC16D, 0x8424, 0xBAE1

softfloat_approxRecipSqrt_1k1s:
    dw 0xA5A5, 0xEA42, 0x8C21, 0xC62D, 0x788F, 0xAA7F, 0x6928, 0x94B6
    dw 0x5CC7, 0x8335, 0x52A6, 0x74E2, 0x4A3E, 0x68FE, 0x432B, 0x5EFD

; multiply 32 x 16 -> 48 bit product
; inputs: hlde = 32-bit multiplicand, bc = 16-bit multiplier, ix pointing to s_globals
; outputs: hldebc = 48-bit product
mul_32x16_48:
    push bc
    push de
; hl * bc
    ld e,c
    ld d,b
    call mul_16_32 ; hlde = hl * bc
    ld (ix+sig32Z),de
    ld (ix+sig32Z+2),hl
    ; ; PRINT_HLDE_HEX "hl * bc" ; DEBUG
; de * bc
    pop de 
    pop hl ; bc
    call mul_16_32 ; hlde = de * bc
    ; ; PRINT_HLDE_HEX "de * bc" ; DEBUG
; accumulate bc out
    ld c,e
    ld b,d ; bc out
    ; ; PRINT_BC_HEX "bc out" ; DEBUG
; accumulate de out
    ld de,(ix+sig32Z)
    add.s hl,de
    ex de,hl ; de out
    ; ; PRINT_DE_HEX "de out" ; DEBUG
; accumulate hl out
    ld hl,(ix+sig32Z+2)
    ld a,h
    adc a,0
    ld h,a
    ; ; PRINT_HL_HEX "hl out" ; DEBUG
    ret
