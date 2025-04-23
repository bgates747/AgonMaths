softfloat_subMagsF16:
    ld (ix+uiA),hl ; store uiA
    ld (ix+uiB),de ; store uiB
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; expA = expF16UI( uiA );
    expF16UI
    signF16UI
    ld (ix+signA),bc ; implicitly stores expA
; sigA = fracF16UI( uiA );
    fracF16UI
    ld (ix+sigA),hl
; expB = expF16UI( uiB );
    ex de,hl ; flip operands
    expF16UI
    signF16UI
    ld (ix+signB),bc ; implicitly stores expB
; sigB = fracF16UI( uiB );
    fracF16UI
    ld (ix+sigB),hl
; /*------------------------------------------------------------------------
; *------------------------------------------------------------------------*/
; expDiff = expA - expB;
    ld a,(ix+expA)
    sub b
    ld (ix+expDiff),a ; store expDiff
; if ( ! expDiff ) {
    jp nz,@expDiff_not_zero
;     /*--------------------------------------------------------------------
;     *--------------------------------------------------------------------*/
;     if ( expA == 0x1F ) {
        add a,b
        cp 0x1F ; check expA for inf
        jp nz,@expA_not_infA
;         if ( sigA | sigB ) goto propagateNaN;
;         softfloat_raiseFlags( softfloat_flag_invalid );
;         uiZ = defaultNaNF16UI;
;         goto uiZ;
            ld hl,canonicalNaNF16
            ret
;     }
@expA_not_infA:
;     sigDiff = sigA - sigB;
        ex de,hl ; de = sigB
        ld hl,(ix+sigA) ; hl = sigA
        or a ; clear carry
        sbc hl,de ; sigDiff = sigA - sigB
;     if ( ! sigDiff ) {
        jp nz,@sigDiff_not_zero
;         uiZ = packToF16UI((softfloat_roundingMode == softfloat_round_min), 0, 0 );
;         goto uiZ;
            ld hl,0
            ret
;     }
@sigDiff_not_zero:
;     if ( expA ) --expA;
        ld a,(ix+expA)
        or a ; check expA for zero
        jp z,@F
        dec (ix+expA)
@@:
;     signZ = signF16UI( uiA );
        ld a,(ix+signA)
;     if ( sigDiff < 0 ) {
        SIGN_HL
        jp p,@F
;         signZ = ! signZ;
            xor %10000000
            ld (ix+signZ),a
;         sigDiff = -sigDiff;
            ex de,hl
            ld hl,0
            sbc hl,de ; sigDiff = -sigDiff
;     }
@@:
        ld (ix+signZ),a
        ld (ix+sigDiff),hl
;     shiftDist = softfloat_countLeadingZeros16( sigDiff ) - 5;
        xor a ; zero shiftDist
@shiftDist_loop:
        bit 2,h ; test for carry into assumed 1 place
        jp nz,@shiftDist_done
        add hl,hl ; shift left
        inc a ; increment shiftDist
        jp @shiftDist_loop
@shiftDist_done:
;     expZ = expA - shiftDist;
        neg
        add a,(ix+expA)
;     if ( expZ < 0 ) {
        jp p,@F
;         shiftDist = expA;
;         expZ = 0;
            xor a ; zero expZ
            ld hl,(ix+sigDiff) ; pre-shifted sigDiff
;     }
@@:
        ld b,a ; b = expZ
        ld c,(ix+signZ) ; c = signZ
;     sigZ = sigDiff<<shiftDist; // already did this when we counted leading zeros
;     goto pack;
        jp packToF16UI 
; } else {
@expDiff_not_zero:
;     /*--------------------------------------------------------------------
;     *--------------------------------------------------------------------*/
;     signZ = signF16UI( uiA );
        ld a,(ix+signA)
        ld (ix+signZ),a ; store signZ
;     if ( expDiff < 0 ) {
        jp p,@expDiff_positive
;         /*----------------------------------------------------------------
;         *----------------------------------------------------------------*/
;         signZ = ! signZ;
            xor %10000000
            ld (ix+signZ),a ; store signZ
            ld c,a ; c = signZ
;         if ( expB == 0x1F ) {
            ld a,(ix+expB)
            cp 0x1F ; check expB for inf
            jp nz,@expB_not_infB
;             if ( sigB ) goto propagateNaN;
                ld hl,(ix+sigB)
                SIGN_HL ; test sigB for zero
                jp z,@F
                ld hl,canonicalNaNF16
                ret
@@:
;             uiZ = packToF16UI( signZ, 0x1F, 0 );
                ld hl,0x7C00 ; +inf
                ld a,c ; signZ
                or h ; reapply sign
                ld h,a
;             goto uiZ;
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
                ld hl,(ix+uiB)
                ld a,c ; signZ
                or h ; reapply sign
                ld h,a
                ret
;         }
@expDiff_gt_neg13:
;         expZ = expA + 19;
            ld a,(ix+expA)
            add a,19
            ld (ix+expZ),a
;         sigX = sigB | 0x0400;
            ld hl,(ix+sigB)
            set 2,h ; set implicit 1
            ld (ix+sigX),hl ; store sigX
;         sigY = sigA + (expA ? 0x0400 : sigA);
            ld hl,(ix+sigA)
            ld a,(ix+expA)
            or a
            jp z,@expA_Zero
            set 2,h ; set implicit 1
            jp @set_sigY_to_sigA
@expA_Zero:
            add hl,hl
@set_sigY_to_sigA:
            ld (ix+sigY),hl ; store sigY
;         expDiff = -expDiff;
            ld a,(ix+expDiff)
            neg
            ld (ix+expDiff),a
            jp @calc_sig32Z
;     } else {
@expDiff_positive:
;         /*----------------------------------------------------------------
;         *----------------------------------------------------------------*/
;         uiZ = uiA; // we do this below
;         if ( expA == 0x1F ) {
            ld a,(ix+expA)
            cp 0x1F ; check expA for inf
            jp nz,@expA_not_infB
;             if ( sigA ) goto propagateNaN;
                ld hl,(ix+sigA)
                SIGN_HL ; test sigA for zero
                jp z,@F
                ld hl,canonicalNaNF16
                ret
@@:
;             goto uiZ;
                ld hl,(ix+uiA) ; uiZ = uiA
                ret
;         }
@expA_not_infB:
;         if ( 13 <= expDiff ) {
            ld a,(ix+expDiff)
            cp 13
            jp c,@expDiff_lt_13
;             if ( expB | sigB ) goto subEpsilon; // no need to do this
;             goto uiZ;
                ld hl,(ix+uiA) ; uiZ = uiA
                ret
;         }
@expDiff_lt_13:
;         expZ = expB + 19;
            ld a,(ix+expB)
            add a,19
            ld (ix+expZ),a
;         sigX = sigA | 0x0400;
            ld hl,(ix+sigA)
            set 2,h ; set implicit 1
            ld (ix+sigX),hl ; store sigX
;         sigY = sigB + (expB ? 0x0400 : sigB);
            ld hl,(ix+sigB)
            ld a,(ix+expB)
            or a
            jp z,@expB_Zero
            set 2,h ; set implicit 1
            jp @set_sigY_to_sigB
@expB_Zero:            
            add hl,hl ; sigB + sigB
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
        ld bc,(ix+sigY)
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
;     sig32Z <<= shiftDist; // already did this when we counted leading zeros
;     expZ -= shiftDist;
        sub (ix+expZ)
        neg
        ld b,a ; b = expZ
        ld c,(ix+signZ) ; c = signZ
;     sigZ = sig32Z>>16; // is already in hl
;     if ( sig32Z & 0xFFFF ) {
        ex de,hl
        SIGN_HL ; test low word of sig32Z for zero
        ex de,hl
        jp z,@F
;         sigZ |= 1;
            set 0,l ; set the LSB of sigZ
            jp softfloat_roundPackToF16
;     } else {
@@:
;         if ( ! (sigZ & 0xF) && ((unsigned int) expZ < 0x1E) ) {
            ld a,l
            and 0x0F ; test round bits for non-zero
            jp nz,softfloat_roundPackToF16
            ld a,b ; expZ
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