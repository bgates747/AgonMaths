softfloat_subMagsF16:
    ; call printNewLine ; DEBUG

    ld (ix+uiA),hl ; store uiA
    ld (ix+uiB),de ; store uiB
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; expA = expF16UI( uiA );
    expF16UI
    signF16UI
    ld (ix+signA),bc ; implicitly stores expA
    ; PRINT_BC_HEX "store expA, signA" ; DEBUG
; sigA = fracF16UI( uiA );
    fracF16UI
    ld (ix+sigA),hl
    ; PRINT_HL_HEX "store sigA\r\n" ; DEBUG
; expB = expF16UI( uiB );
    ex de,hl ; flip operands
    expF16UI
    signF16UI
    ld (ix+signB),bc ; implicitly stores expB
    ; PRINT_BC_HEX "store expB, signB" ; DEBUG
; sigB = fracF16UI( uiB );
    fracF16UI
    ld (ix+sigB),hl
    ; PRINT_HL_HEX "store sigB\r\n" ; DEBUG
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; expDiff = expA - expB;
    ld a,(ix+expA)
    sub b
    ld (ix+expDiff),a ; store expDiff
    ; PRINT_A_HEX "store expDiff = expA - expB;" ; DEBUG
; if ( ! expDiff ) {
    jp nz,@expDiff_not_zero
;     /*--------------------------------------------------------------------
;     *--------------------------------------------------------------------*/
;     if ( expA == 0x1F ) {
        add a,b
        ; PRINT_A_HEX "add a,b; check expA == 0x1F" ; DEBUG
        cp 0x1F ; check expA for inf
        jp nz,@expA_not_infA
;         if ( sigA | sigB ) goto propagateNaN;
;         softfloat_raiseFlags( softfloat_flag_invalid );
;         uiZ = defaultNaNF16UI;
;         goto uiZ;
            ld hl,canonicalNaNF16
            ; PRINT_HL_HEX "expA == 0x1F, return uiZ = defaultNaNF16UI" ; DEBUG
            ret
;     }
@expA_not_infA:
;     sigDiff = sigA - sigB;
        ex de,hl ; de = sigB
        ld hl,(ix+sigA) ; hl = sigA
        or a ; clear carry
        sbc hl,de ; sigDiff = sigA - sigB
        ; PRINT_HL_HEX "sigDiff = sigA - sigB" ; DEBUG
        ; ld (ix+sigDiff),hl ; TODO: do we need to store this?
;     if ( ! sigDiff ) {
        jp nz,@sigDiff_not_zero
;         uiZ =
;             packToF16UI(
;                 (softfloat_roundingMode == softfloat_round_min), 0, 0 );
;         goto uiZ;
            ld hl,0
            ; PRINT_HL_HEX "! sigDiff; return uiZ = 0" ; DEBUG
            ret
;     }
@sigDiff_not_zero:
;     if ( expA ) --expA;
        ld a,(ix+expA)
        or a ; check expA for zero
        jp z,@F
        dec (ix+expA)
; ; DEBUG
;         push af
;         ld a,(ix+expA)
;         ; PRINT_A_HEX "expA--" ; DEBUG
;         pop af
; ; END DEBUG
@@:
;     signZ = signF16UI( uiA );
        ld a,(ix+signA)
        ; PRINT_A_HEX "a = signZ = signF16UI( uiA );" ; DEBUG
;     if ( sigDiff < 0 ) {
        ; ld hl,(ix+sigDiff) ; TODO: do we need to load this?
        SIGN_HL
        ; PRINT_HL_HEX "sigDiff < 0" ; DEBUG
        jp p,@F
;         signZ = ! signZ;
            xor %10000000
            ld (ix+signZ),a
            ; PRINT_A_HEX "store signZ = ! signZ;" ; DEBUG
;         sigDiff = -sigDiff;
            ex de,hl
            ld hl,0
            sbc hl,de ; sigDiff = -sigDiff
            ; PRINT_HL_HEX "sigDiff = -sigDiff;" ; DEBUG
;     }
@@:
        ld (ix+signZ),a
        ; PRINT_A_HEX "store signZ" ; DEBUG
        ld (ix+sigDiff),hl
        ; PRINT_HL_HEX "store sigDiff" ; DEBUG
;     shiftDist = softfloat_countLeadingZeros16( sigDiff ) - 5;
        xor a ; zero shiftDist
@shiftDist_loop:
        bit 2,h ; test for carry into assumed 1 place
        jp nz,@shiftDist_done
        add hl,hl ; shift left
        inc a ; increment shiftDist
        jp @shiftDist_loop
@shiftDist_done:
        ; PRINT_A_HEX "shiftDist = softfloat_countLeadingZeros16( sigDiff ) - 5;" ; DEBUG
;     expZ = expA - shiftDist;
        neg
        add a,(ix+expA)
        ; ld (ix+expZ),a
        ; PRINT_A_HEX "expZ = expA - shiftDist;" ; DEBUG
;     if ( expZ < 0 ) {
        jp p,@F
;         shiftDist = expA;
;         expZ = 0;
            xor a ; zero expZ
            ; PRINT_A_HEX "expZ = 0;" ; DEBUG
            ld hl,(ix+sigDiff) ; pre-shifted sigDiff
            ; PRINT_HL_HEX "pre-shifted sigDiff" ; DEBUG
;     }
@@:
        ld b,a ; b = expZ
        ld c,(ix+signZ) ; c = signZ
        ; PRINT_BC_HEX "ld b,a = expZ, ld c,(ix+signZ) = signZ" ; DEBUG
;     sigZ = sigDiff<<shiftDist; // already did this when we counted leading zeros
;     goto pack;
        ; PRINT_HL_HEX "return jp packToF16UI" ; DEBUG
        jp packToF16UI 
; } else {
@expDiff_not_zero:
;     /*--------------------------------------------------------------------
;     *--------------------------------------------------------------------*/
;     signZ = signF16UI( uiA );
        ld a,(ix+signA)
        ld (ix+signZ),a ; store signZ
        ; PRINT_A_HEX "store signZ = signF16UI( uiA );" ; DEBUG
;     if ( expDiff < 0 ) {
        jp p,@expDiff_positive
;         /*----------------------------------------------------------------
;         *----------------------------------------------------------------*/
;         signZ = ! signZ;
            xor %10000000
            ld (ix+signZ),a ; store signZ
            ; PRINT_A_HEX "store signZ = ! signZ;" ; DEBUG
            ld c,a ; c = signZ
;         if ( expB == 0x1F ) {
            ld a,(ix+expB)
            ; PRINT_A_HEX "expB = 0x1F" ; DEBUG
            cp 0x1F ; check expB for inf
            jp nz,@expB_not_infB
;             if ( sigB ) goto propagateNaN;
                ld hl,(ix+sigB)
                ; PRINT_HL_HEX "if ( sigB ) goto propagateNaN;" ; DEBUG
                SIGN_HL ; test sigB for zero
                jp z,@F
                ld hl,canonicalNaNF16
                ; PRINT_HL_HEX "sigB != 0, return NaN" ; DEBUG
                ret
@@:
;             uiZ = packToF16UI( signZ, 0x1F, 0 );
                ld hl,0x7C00 ; +inf
                ld a,c ; signZ
                ; PRINT_A_HEX "signZ = signF16UI( uiA );" ; DEBUG
                or h ; reapply sign
                ld h,a
;             goto uiZ;
                ; PRINT_HL_HEX "return uiZ = +/-inf" ; DEBUG
                ret
;         }
@expB_not_infB:
;         if ( expDiff <= -13 ) {
            ld d,(ix+expDiff)
            ld a,-13
            cp d
            jp c,@expDiff_gt_neg13
;             uiZ = packToF16UI( signZ, expB, sigB );
;             if ( expA | sigA ) goto subEpsilon;
;             goto uiZ;
                ; PRINT_BC_HEX "b = expB, c = signZ" ; DEBUG
                ld hl,(ix+uiB)
                ld a,c ; signZ
                or h ; reapply sign
                ld h,a
                ; PRINT_HL_HEX "return uiZ = packToF16UI( signZ, expB, sigB );" ; DEBUG
                ret
;         }
@expDiff_gt_neg13:
;         expZ = expA + 19;
            ld a,(ix+expA)
            add a,19
            ld (ix+expZ),a
            ; PRINT_A_HEX "expZ = expA + 19;" ; DEBUG
;         sigX = sigB | 0x0400;
            ld hl,(ix+sigB)
            set 2,h ; set implicit 1
            ld (ix+sigX),hl ; store sigX
            ; PRINT_HL_HEX "sigX = sigB | 0x0400;" ; DEBUG
;         sigY = sigA + (expA ? 0x0400 : sigA);
            ld hl,(ix+sigA)
            ld a,(ix+expA)
            or a
            jp z,@expA_Zero
            set 2,h ; set implicit 1
            ; PRINT_HL_HEX "sigY = sigA + (0x0400);" ; DEBUG
            jp @set_sigY_to_sigA
@expA_Zero:
            add hl,hl
            ; PRINT_HL_HEX "sigY = sigA + (sigA);" ; DEBUG
@set_sigY_to_sigA:
            ld (ix+sigY),hl ; store sigY
;         expDiff = -expDiff;
            ld a,(ix+expDiff)
            neg
            ld (ix+expDiff),a
            ; PRINT_A_HEX "store expDiff = -expDiff;" ; DEBUG
            jp @calc_sig32Z
;     } else {
@expDiff_positive:
;         /*----------------------------------------------------------------
;         *----------------------------------------------------------------*/
;         uiZ = uiA; // we do this below
;         if ( expA == 0x1F ) {
            ld a,(ix+expA)
            ; PRINT_A_HEX "expA: if ( expA == 0x1F )" ; DEBUG
            cp 0x1F ; check expA for inf
            jp nz,@expA_not_infB
                ; PRINT_A_HEX "expA == 0x1F" ; DEBUG
;             if ( sigA ) goto propagateNaN;
                ld hl,(ix+sigA)
                SIGN_HL ; test sigA for zero
                jp z,@F
                ld hl,canonicalNaNF16
                ; PRINT_HL_HEX "sigA != 0, return NaN" ; DEBUG
                ret
@@:
;             goto uiZ;
                ld hl,(ix+uiA) ; uiZ = uiA
                ; PRINT_HL_HEX "return uiZ = uiA" ; DEBUG
                ret
;         }
@expA_not_infB:
;         if ( 13 <= expDiff ) {
            ld a,(ix+expDiff)
            cp 13
            jp c,@expDiff_lt_13
                ; PRINT_A_HEX "expDiff > 13" ; DEBUG
;             if ( expB | sigB ) goto subEpsilon; // no need to do this
;             goto uiZ;
                ld hl,(ix+uiA) ; uiZ = uiA
                ; PRINT_HL_HEX "return uiZ = uiA" ; DEBUG
                ret
;         }
@expDiff_lt_13:
;         expZ = expB + 19;
            ld a,(ix+expB)
            add a,19
            ld (ix+expZ),a
            ; PRINT_A_HEX "expZ = expB + 19;" ; DEBUG
;         sigX = sigA | 0x0400;
            ld hl,(ix+sigA)
            set 2,h ; set implicit 1
            ld (ix+sigX),hl ; store sigX
            ; PRINT_HL_HEX "sigX = sigA | 0x0400;" ; DEBUG
;         sigY = sigB + (expB ? 0x0400 : sigB);
            ld hl,(ix+sigB)
            ld a,(ix+expB)
            or a
            jp z,@expB_Zero
            set 2,h ; set implicit 1
            ; PRINT_HL_HEX "sigY = sigB + (0x0400);" ; DEBUG
            jp @set_sigY_to_sigB
@expB_Zero:            
            add hl,hl ; sigB + sigB
            ; PRINT_HL_HEX "sigY = sigB + (sigB);" ; DEBUG
@set_sigY_to_sigB:
            ld (ix+sigY),hl
;     }
@calc_sig32Z:
;     sig32Z = ((uint_fast32_t) sigX<<expDiff) - sigY;
        ld b,(ix+expDiff)
        xor a ; clear a
        ld hl,(ix+sigX)
@sigX_shift:
        add hl,hl
        adc a,a
        djnz @sigX_shift

        ld (ix+sigX),hl ; store low 3 bytes of sigX
        ld e,(ix+sigX+2) ; e = upper middle byte of sigX
        ld d,a ; dehl is now sigX
; ; DEBUG
;         ex de,hl
;         ; PRINT_HLDE_HEX "sigX<<expDiff" ; DEBUG
;         ex de,hl
; ; END DEBUG
        ld bc,(ix+sigY)
        ; PRINT_BC_HEX "sigY" ; DEBUG

    ;   dehl
    ; - 00bc
    ; = hlbc 
        or a ; clear carry
        sbc.s hl,bc ; .s to force 16-bit subtraction
        ld c,l
        ld b,h

        ex de,hl
        ld de,0

        sbc.s hl,de
        ld e,c
        ld d,b ; hlde = sigX<<expDiff - sigY

        ; PRINT_HLDE_HEX "sig32Z = sigX<<expDiff - sigY" ; DEBUG

;     shiftDist = softfloat_countLeadingZeros32( sig32Z ) - 1;
        xor a ; zero shiftDist
@shiftDist_loop32:
        bit 6,h
        jp nz,@shiftDist_done32
        ex de,hl
        add.s hl,hl
        ex de,hl
        adc hl,hl
        inc a ; increment shiftDist
        jp @shiftDist_loop32
@shiftDist_done32:
        ; PRINT_A_HEX "shiftDist = softfloat_countLeadingZeros32( sig32Z ) - 1;" ; DEBUG
;     sig32Z <<= shiftDist; // already did this when we counted leading zeros
        ; PRINT_HLDE_HEX "sig32Z <<= shiftDist" ; DEBUG
;     expZ -= shiftDist;
        sub (ix+expZ)
        neg
        ld b,a ; b = expZ
        ld c,(ix+signZ) ; c = signZ
        ; PRINT_BC_HEX "b = expZ -= shiftDist, c = signZ" ; DEBUG
;     sigZ = sig32Z>>16; // is already in hl
        ; PRINT_HL_HEX "sigZ = sig32Z>>16;" ; DEBUG
;     if ( sig32Z & 0xFFFF ) {
        ex de,hl
        SIGN_HL ; test low word of sig32Z for zero
        ex de,hl
        jp z,@F
;         sigZ |= 1;
            set 0,l ; set the LSB of sigZ
            ; PRINT_HL_HEX "sigZ |= 1; return jp softfloat_roundPackToF16" ; DEBUG
            jp softfloat_roundPackToF16
;     } else {
@@:
;         if ( ! (sigZ & 0xF) && ((unsigned int) expZ < 0x1E) ) {
            ld a,l
            and 0x0F ; test round bits for non-zero
            ; PRINT_A_HEX "check sigZ & 0x0F (round bits)" ; DEBUG
            jp nz,softfloat_roundPackToF16
            ld a,b ; expZ
            ; PRINT_A_HEX "check expZ < 0x1E" ; DEBUG
            cp 0x1E ; check expZ for 0x1E
            jp nc,softfloat_roundPackToF16
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
                ld c,(ix+signZ) ; c = signZ
                ; PRINT_BC_HEX "b = expZ, c = signZ" ; DEBUG
                ; PRINT_HL_HEX "sigZ >>= 4; return jp packToF16UI" ; DEBUG
                jp packToF16UI 
;         }
;     }
;     return softfloat_roundPackToF16( signZ, expZ, sigZ );
; }
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; propagateNaN:
; uiZ = softfloat_propagateNaNF16UI( uiA, uiB );
; goto uiZ;
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; subEpsilon:
; roundingMode = softfloat_roundingMode;
; if ( roundingMode != softfloat_round_near_even ) {
;     if (
;         (roundingMode == softfloat_round_minMag)
;             || (roundingMode
;                     == (signF16UI( uiZ ) ? softfloat_round_max
;                             : softfloat_round_min))
;     ) {
;         --uiZ;
;     }
; #ifdef SOFTFLOAT_ROUND_ODD
;     else if ( roundingMode == softfloat_round_odd ) {
;         uiZ = (uiZ - 1) | 1;
;     }
; #endif
; }
; softfloat_exceptionFlags |= softfloat_flag_inexact;
; goto uiZ;
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; pack:
; uiZ = packToF16UI( signZ, expZ, sigZ );
; uiZ:
; uZ.ui = uiZ;
; return uZ.f;

; }


ret