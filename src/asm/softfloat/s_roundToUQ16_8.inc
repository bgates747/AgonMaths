; //---------------------------------------------------------------------------
; // Round a 4-fraction-bit value into 16.8 fixed, ties-to-even only.
; // We know no finite half-float can overflow 24 bits.
; inputs: uhl = unsigned 12.12 fiixed-point number to be rounded to an unsigned 16.8 integer
; outputs uhl = the rounded unsigned 16.8 integer
; destroys: af, bc
softfloat_roundToUQ16_8:
; uint_fast32_t roundBits = sig & 0xF;  // bottom 4 bits
    ld a,l
    and 0x0F 
; sig += 0x8;                           // add ½ULP (1<<(4−1))
    ld bc,8
    add hl,bc
; sig >>= 4;           // drop those 4 bits
    ld c,a ; save roundBits
    ld b,4 ; loop counter
    HLU_TO_A ; put upper byte of sig into a
    @shift_loop:
        srl a
        rr h
        rr l
    djnz @shift_loop ; loop until all bits shifted
    A_TO_HLU ; put upper byte of sig back into hl
; // ties-to-even: if exactly halfway and LSB=1, clear it
; if ( (roundBits == 0x8) && (sig & 1) ) {
    ld a,c ; roundBits
    cp 0x08 ; check roundBits for 0x08
    ret nz ; no rounding needed
    bit 0,l ; check sig for odd
    ret z ; LSB is even, no rounding needed
;     sig &= ~1u;
        res 0,l ; clear LSB
; }
; // flag inexact if *any* low bits were nonzero
; if ( roundBits && exact ) {
;     softfloat_raiseFlags( softfloat_flag_inexact );
; }
; return sig;
    ret
