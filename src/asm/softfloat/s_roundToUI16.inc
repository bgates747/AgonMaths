; inputs: uhl is an unsigned 12.12 fiixed-point number to be rounded to an unsigned 16-bit integer
; outputs: hl is the rounded unsigned 16-bit integer
; destroys: af, bc, de
softfloat_roundToUI16:
; // Separate the frac bits (lower 12) and integer bits (upper bits)
; uint16_t frac = sig & 0x0FFF;       // mask 12 least-significant bits (frac part)
    ld a,h
    and 0x0F ; mask out the top 4 bits
    ld d,a
    ld e,l ; de = frac
; uint_fast16_t result = sig >> 12;         // shift right to get the integer part
    HLU_TO_A ; put upper byte of sig into a
    ld b,12 ; loop counter
    @shift_loop:
        srl a
        rr h
        rr l
        djnz @shift_loop ; loop until all bits shifted
; // Apply round-to-nearest-even:
; if (frac > 0x0800 ||                    // fraction > 0.5
;     (frac == 0x0800 && (result & 0x1))) // tie (exact 0.5) and current integer is odd
    ld bc,0x0800
    ex de,hl ; hl = frac, de = result
    or a ; clear carry
    sbc hl,bc ; hl = frac - 0x0800
    ex de,hl ; hl = result
    ret m ; frac <= 0x0800, no rounding needed
    jr nz,@round_up ; frac > 0x0800, round up
    bit 0,l ; check result for odd
    ret z ; result is even, no rounding needed

; {
@round_up:
;     result += 1;  // round up
        inc hl
; }
; return result;
    ret