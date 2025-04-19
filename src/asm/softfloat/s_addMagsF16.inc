; add two 16-bit floating point numbers of the same sign
; inputs : hl = uiA, de = uiB, ix = s_globals
; outputs: hl = uiA + uiB
; destroys: af, bc, de
softfloat_addMagsF16:
    ld (ix+uiA),hl ; store uiA
; expA = expF16UI( uiA );
    expF16UI
    signF16UI
    ld (ix+signA),bc ; implicitly stores expA
; sigA = fracF16UI( uiA );
    fracF16UI
    ld (ix+sigA),hl ; store sigA
    ; call printNewLine ; DEBUG
    ; PRINT_BC_HEX "store expA, signA" ; DEBUG
    ; PRINT_HL_HEX "store sigA\r\n" ; DEBUG
; expB = expF16UI( uiB );
    ex de,hl ; flip operands
    expF16UI
    signF16UI
    ld (ix+signB),bc ; implicitly stores expB
; sigB = fracF16UI( uiB );
    fracF16UI
    ld (ix+sigB),hl ; store sigB
    ; PRINT_BC_HEX "store expB, signB" ; DEBUG
    ; PRINT_HL_HEX "store sigB\r\n" ; DEBUG
; expDiff = expA - expB;
    ld a,(ix+expA)
    sub b
    ld (ix+expDiff),a ; store expDiff
    ; PRINT_A_HEX "expDiff" ; DEBUG
; if ( ! expDiff ) {
        jp nz,@expDiff_not_zero
;     if ( ! expA ) {
        ld a,(ix+expA)
        or a ; check expA for zero
        jp nz,@expA_not_zero
;         uiZ = uiA + sigB;
            ld de,(ix+uiA)
            ; call dumpRegistersHex ; DEBUG
            add hl,de
            ; PRINT_HL_HEX "uiZ = uiA + sigB" ; DEBUG
;         goto uiZ;
            ret ; jp @uiZ
;     }
@expA_not_zero:
;     if ( expA == 0x1F ) {
        cp 0x1F ; check expA for inf
        jp nz,@expA_not_infA
;         if ( sigA | sigB ) goto propagateNaN;
            SIGN_HL ; test sigB for zero
            jp nz,@propagateNaN
            ld hl,(ix+sigA)
            SIGN_HL ; test sigA for zero
            jp nz,@propagateNaN
;         uiZ = uiA;
;         goto uiZ;
            ld hl,(ix+uiA)
            ret ; jp @uiZ
;     }
@expA_not_infA:
;     signZ = signF16UI( uiA );
;     expZ = expA;
        ld bc,(ix+signA) ; expZ = b, signZ = c
;     sigZ = 0x0800 + sigA + sigB;
        ; hl is sigA
        ld de,(ix+sigB)
        add hl,de ; hl = sigA + sigB
        ld a,0x80
        add a,h
        ld h,a
;     if ( ! (sigZ & 1) && (expZ < 0x1E) ) {
        bit 0,l ; test sigZ for odd
        jp z,@sigZ_even
        ld a,b ; expZ
        cp 0x1E ; check expZ for 0x1E
        jp nc,@sigZ_even
;         sigZ >>= 1;
            srl h
            rr l
;         goto pack;
            jp packToF16UI ; jp @pack
;     }
@sigZ_even:
;     sigZ <<= 3;
        add hl,hl
        add hl,hl
        add hl,hl
        jp softfloat_roundPackToF16
; } else {
@expDiff_not_zero:
;     signZ = signF16UI( uiA );
;     if ( expDiff < 0 ) {
        or a ; check for negative
        jp p,@expDiff_positive
        ld d,a ; keep expDiff handy
;         if ( expB == 0x1F ) {
            ld a,(ix+expB)
            cp 0x1F ; check expB for inf
            jp nz,@expB_not_infB
;             if ( sigB ) goto propagateNaN;
                ld hl,(ix+sigB)
                SIGN_HL ; test sigB for zero
                jp nz,@propagateNaN
;             uiZ = packToF16UI( signZ, 0x1F, 0 );
;             goto uiZ;
                ld c,(ix+signA)
                ld b,0x1F
                ld hl,0
                jp packToF16UI ; jp @uiZ
;         }
@expB_not_infB:
;         if ( expDiff <= -13 ) {
            ld a,-13
            cp d
            jp c,@expDiff_gt_neg13
;             uiZ = packToF16UI( signZ, expB, sigB );
                ld c,(ix+signZ)
                ld b,(ix+expB)
                ld hl,(ix+sigB)
                call packToF16UI
;             if ( expA | sigA ) goto addEpsilon;
                ld a,(ix+expA)
                or a ; check expA for zero
                jp nz,@addEpsilon
                ld hl,(ix+sigA)
                SIGN_HL ; test sigA for zero
                jp nz,@addEpsilon
;             goto uiZ;
                ret ; jp @uiZ
;         }
@expDiff_gt_neg13:
;         expZ = expB;
            ld a,(ix+expB)
            ld (ix+expZ),a
;         sigX = sigB | 0x0400;
            ld hl,(ix+sigB)
            set 2,h ; set implicit 1
            ld (ix+sigX),hl ; store sigX
;         sigY = sigA + (expA ? 0x0400 : sigA);
            ld hl,(ix+sigA)
            ld a,(ix+expA)
            or a
            jp z,@F
            set 2,h ; set implicit 1
@@:
;         shiftDist = 19 + expDiff;
            ld a,(ix+expDiff)
            add a,19
            ; ld (ix+shiftDist),a
            jp @calc_sig32Z
;     } else {
@expDiff_positive:
;         uiZ = uiA;
;         if ( expA == 0x1F ) {
            ld a,(ix+expA)
            cp 0x1F ; check expA for inf
            jp nz,@expA_not_inf
;             if ( sigA ) goto propagateNaN;
                ld hl,(ix+sigA)
                SIGN_HL ; test sigA for zero
                jp nz,@propagateNaN
;             goto uiZ;
                ld hl,(ix+uiA)
                ret ; jp @uiZ
;         }
@expA_not_inf:
;         if ( 13 <= expDiff ) {
            ld a,d
            cp 13
            jp c,@F
;             if ( expB | sigB ) goto addEpsilon;
                ld a,(ix+expB)
                or a ; check expB for zero
                jp nz,@addEpsilon
                ld hl,(ix+sigB)
                SIGN_HL ; test sigB for zero
                jp nz,@addEpsilon
;             goto uiZ;
                ld hl,(ix+uiA)
                ret ; jp @uiZ
;         }
@@:
;         expZ = expA;
            ld a,(ix+expA)
            ld (ix+expZ),a
;         sigX = sigA | 0x0400;
            ld hl,(ix+sigA)
            set 2,h ; set implicit 1
            ld (ix+sigX),hl ; store sigX
;         sigY = sigB + (expB ? 0x0400 : sigB);
            ld hl,(ix+sigB)
            ld a,(ix+expB)
            or a
            jp z,@F
            set 2,h ; set implicit 1
@@:
;         shiftDist = 19 - expDiff;
            ld a,(ix+expDiff)
            sub 19
            ; ld (ix+shiftDist),a
;     }
@calc_sig32Z: ; a needs to be shiftDist, hl needs to be sigY and sigX needs to be populated
;     sig32Z = ((uint_fast32_t) sigX<<19) + ((uint_fast32_t) sigY<<shiftDist);
        ld b,a
        xor a
@sigY_shift: ; sigY<<shiftDist) -> auhl
        add hl,hl ; 1 cycle
        adc a,a ; 1 cycle
        djnz @sigY_shift ; 4 cycles
        ld (ix+sigY),hl ; store low 3 bytes of sigY
        ex de,hl
        ld c,(ix+sigY+2) ; c = upper middle byte of sigY
        ld b,a ; bcde is now sigY
        ld hl,(ix+sigX)
        add hl,hl
        add hl,hl
        add hl,hl ; hl is now the upper word of sigX<<19
        add hl,bc ; hlde = sig32Z
;     if ( sig32Z < 0x40000000 ) {
        ld a,h
        cp 0x40
        jp c,@F
;         --expZ;
            dec (ix+expZ)
;         sig32Z <<= 1;
            ex de,hl
            add.s hl,hl
            ex de,hl
            adc hl,hl
;     }
@@:
;     sigZ = sig32Z>>16;
        ld (ix+sigZ),hl
;     if ( sig32Z & 0xFFFF ) {
        ex de,hl
        SIGN_HL ; test sig32Z for zero
        ex de,hl
        jr z,@F
;         sigZ |= 1;
            set 0,l ; set the LSB of sigZ
;     } else {
@@:
;         if ( ! (sigZ & 0xF) && (expZ < 0x1E) ) {
            ld a,l
            and 0x0F ; test round bits for non-zero
            jr nz,@return_rounded
            ld a,(ix+expZ) ; expZ
            cp 0x1E ; check expZ for 0x1E
            jr nc,@return_rounded
;             sigZ >>= 4;
                srl h
                rr l
                or a ; clear carry
                srl h
                rr l
                or a ; clear carry
                srl h
                rr l
                or a ; clear carry
                srl h
                rr l
                or a ; clear carry
;             goto pack;
                ld bc,(ix+signZ) ; b=expZ, c=signZ
                jp packToF16UI 
;         }
;     }
; }
; return softfloat_roundPackToF16( signZ, expZ, sigZ );
@return_rounded:
    ld hl,(ix+sigZ)
    ld bc,(ix+signZ) ; b=expZ, c=signZ
    jp softfloat_roundPackToF16
@propagateNaN:
; uiZ = softfloat_propagateNaNF16UI( uiA, uiB );
; goto uiZ;
    ld hl,0xFE00 ; NaN
    ret
@addEpsilon: ; since rounding mode is round to even, there's nothing to do here
; softfloat_exceptionFlags |= softfloat_flag_inexact;
; goto uiZ;
    ret ; jp @uiZ
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
@pack:
; uiZ = packToF16UI( signZ, expZ, sigZ );
    ld bc,(ix+signZ) ; b=expZ, c=signZ
    ld hl,(ix+sigZ)
    jp packToF16UI
@uiZ: ; hl must be ready to return at this point
; uZ.ui = uiZ;
; return uZ.f;
    ret
; }
; end f16_add