; convert an unsigned 8.8 fixed point number to a float16
; inputs: hl = unsigned 8.8 fixed point number
; outputs: hl = float16
; destroys af, hl, bc, de, ix
uq8_8_to_f16:
;     int_fast8_t shiftDist;
;     union ui16_f16 u;
;     uint_fast16_t sig;

; set index to global variables
    ld ix,s_globals
; store input
    ld (ix+uiA),hl
; {
;     int_fast8_t shiftDist;
;     union ui16_f16 u;
;     uint_fast16_t sig;

; clear hlu
    dec hl
    inc.s hl

; check for zero
    ZERO_HL
    ret z ; alredy zero so nothing to do

    ld c,0 ; sign
;     shiftDist = softfloat_countLeadingZeros16(a) - 5;
        ld a,-5 ; initialize shiftDist counter
        @shiftDist_loop:
            bit 7,h
            jr nz,@shiftDist_done
            add hl,hl
            inc a ; increment shiftDist
            jr @shiftDist_loop
        @shiftDist_done:
        ld d,a ; store shiftDist
        ld hl,(ix+uiA) ; get input
;     if (0 <= shiftDist) {
        or a ; get sign of shiftDist
        jp m,@shift_dist_neg
;         u.ui = packToF16UI(0, (0x10 - shiftDist), (a << shiftDist));
;         return u.f;
            sub 0x10
            jr z,@no_shift
            neg
            ld b,a ; exponent
            ld a,d ; shiftDist
            or a ; check for zero
            jr z,@no_shift
            @shift_loop:
                add hl,hl ; shift left
            dec a
            jr nz,@shift_loop ; loop until all bits shifted
        @no_shift:
            jp packToF16UI
;     } else {
    @shift_dist_neg:
;         shiftDist += 4;
            add a,4
            ld d,a ; store shiftDist
            ld a,0x14
            sub d ; exponent = 0x14 - shiftDist
            ld e,a ; exponent
;         sig = (shiftDist < 0)
            ld a,d ; shiftDist
            or a ; check sign / zero
            jp z,@return_roundPackToF16
            jp p,@shift_dist_pos
;             ? a >> (-shiftDist) | ((uint16_t)(a << (shiftDist & 15)) != 0)
                neg ; negate shiftDist
                ld d,a ; store shiftDist
                ld b,d ; shiftDist = loop counter
                xor a ; clear sticky bits
                @shift_right_loop:
                    srl h ; shift right
                    rr l ; shift right
                    jr nc,@no_sticky
                    ld a,1 ; set sticky bit
                @no_sticky:
                djnz @shift_right_loop ; loop until all bits shifted
                jr @return_roundPackToF16
            @shift_dist_pos:
;             : (uint_fast16_t) a << shiftDist;
                ld b,d ; shiftDist = loop counter
                @shift_left_loop:
                    add hl,hl ; shift left
                djnz @shift_left_loop ; loop until all bits shifted
                xor a ; clear sticky bits
;         return softfloat_roundPackToF16(0, 0x14 - shiftDist, sig);
        @return_roundPackToF16:
            ld b,e ; b = exponent
            or a,l ; set sticky bit
            ld l,a ; update sticky bit
            jp softfloat_roundPackToF16
;     }
; }
