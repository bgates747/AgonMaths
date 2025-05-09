; inputs: hl = float16 to print
; outputs: hl points to zero-terminated string
; destroys: af, bc, de, ix, iy

; // uiA    : raw 16-bit half-precision float input (bits)
; // iy     : output string buffer pointer
; // strBuff: saved start of output buffer
; // signA  : sign bit (0 = positive, 1 = negative)
; // expA   : exponent field (5 bits, biased by 15)
; // sigA   : fraction/mantissa field (10 bits, no implicit leading 1)
; // expZ   : unbiased exponent minus 10 (adjusted for 10-bit mantissa shift)
; // sig32Z : full normalized mantissa (10 or 11 bits as 32-bit value)
; // sig32X : integer part after applying exponent
; // sig32Y : fractional part numerator (still divided by 2^expY)
; // sigY   ; mask for sig32Y
; // expY   : number of bits shifted (i.e., denominator is 2^expY)
; // temp[] : temporary buffer for reversing integer digits
; // ti     : counter for integer digit collection

f16_print:
; set index to global variables
    ld ix,s_globals
; char* strBuff = iy;              // Save start of output buffer
; uint16_t signA = uiA >> 15;         // 1-bit signA
; uint16_t expA  = (uiA >> 10) & 0x1F; // 5-bit exponent (biased by 15)
; uint16_t sigA = uiA & 0x3FF;       // 10-bit fraction (mantissa)
; signA = signF16UI( uiA );
    signF16UI
; expA  = expF16UI( uiA );
    expF16UI
    ld (ix+signA),bc ; implicitly stores expA
; sigA  = fracF16UI( uiA );
    fracF16UI
    ld (ix+sigA),hl
; // Special cases: Inf/NaN
; if (expA == 0x1F) {  
    ld a,b ; expA
    cp 0x1F ; check expA for inf
    jr nz,@check_zero
;     if (sigA == 0) {               // Infinity
        ZERO_HL
        jr nz,@return_nan
;         *iy++ = 'I'; *iy++ = 'n'; *iy++ = 'f';
            ld hl,str_inf
;         if (signA) *iy++ = '-';
            jp @set_sign
;     } else {                       // NaN (Not a Number)
    @return_nan:
        ld hl,str_nan
        ret
; }
@check_zero:
; // Special case: Zero (expA=0, sigA=0)
; if (expA == 0 && sigA == 0) {
    or a ; check expA for zero
    jr nz,@end_specials
    SIGN_HL ; test sigA for zero (don't use ZERO_HL because it destroys a)
    jr nz,@end_specials
        ld hl,str_zero
;     if (signA) *iy++ = '-';
        jp @set_sign
; }
@end_specials:
; // Determine normalized mantissa and exponent
; int expZ;
; uint32_t sig32Z;
; if (expA == 0) {
    or a ; check expA for zero
    jr nz,@is_normal
;     // Subnormal number: exponent = 1 - bias (15)
;     expZ = -14;                 // (1 - 15)
        ld a,-14
;     sig32Z = sigA;                  // no implicit leading 1
        jr @store_sig32Z
; } else {
@is_normal:
;     expZ = (int)expA - 15;
        sub 15 ; expA - 15
;     sig32Z   = 0x400 | sigA;        // add implicit 1 (1<<10)
        set 2,h ; set implicit 1
; }
@store_sig32Z:
; // Now value = (-1)^signA * sig32Z * 2^(expZ - 10)
    ld (ix+sig32Z),hl ; store sig32Z low word
    ld de,0 ; clear high word of sig32Z
    ld (ix+sig32Z+2),de ; store sig32Z high word
; // Incorporate the 2^(-10) factor of the mantissa into exponent
; expZ -= 10;
    sub 10 ; expZ - 10
; // Separate integer and fractional parts by base-2 exponent
; uint32_t sig32X;
; uint32_t sig32Y = 0;
    ld bc,0
    ld (ix+sig32Y),bc ; clear sig32Y low word
    ld (ix+sig32Y+2),bc ; clear sig32Y high word
; uint32_t expY = 0;
; if (expZ >= 0) {
    jr z,@expZ_eq_zero
    jp m,@expZ_lt0
        ld b,a ; loop counter = expZ
        xor a
        ld (ix+expY),a ; store expY
        @expZ_gte0_loop:
        ; sig32X = sig32Z << expZ;
            ex de,hl ; de = low word
            add.s hl,hl ; low word <<= 1, .s to force 16-bit add
            ex de,hl
            adc.s hl,hl ; high word <<= 1, .s to force 16-bit adc
        djnz @expZ_gte0_loop ; loop until all bits shifted
    @expZ_eq_zero:
        ld (ix+sig32X),hl ; store sig32X low word
        ld (ix+sig32X+2),de ; store sig32X high word
        jr @convert_int_to_string
; } else {
@expZ_lt0:
;     expY = (uint32_t)(-expZ);
        neg ; a = -expZ
        ld (ix+expY),a ; store 
        ld b,a ; loop counter = expY
    @expZ_lt0_loop_sig32X:
    ; sig32X = sig32Z >> expY;
        srl h ; low word high byte >>= 1
        rr l ; low word low byte >>= 1
        djnz @expZ_lt0_loop_sig32X ; loop until all bits shifted
        ld (ix+sig32X),hl ; store sig32X low word
        ld (ix+sig32X+2),de ; store sig32X high word
    ; sig32Y      = sig32Z & ((1u << expY) - 1);
        ld hl,1
        ld b,a ; loop counter = expY (max 25)
        @expZ_lt0_loop_sig32Y:
            add hl,hl
            djnz @expZ_lt0_loop_sig32Y ; loop until all bits shifted
        dec hl
        ld (ix+sigY),hl ; mask for sig32Y
        ld de,(ix+sig32Z) ; get sig32Z low word
        ld a,l
        and e
        ld e,a
        ld a,h
        and d
        ld d,a
        ld a,(ix+sigY+2) ; mask middle low byte
        ld hl,(ix+sig32Z+2) ; get sig32Z high word
        and l
        ld l,a
        ld (ix+sig32Y),de ; store sig32Y low word
        ld (ix+sig32Y+2),hl ; store sig32Y high word
; }
@convert_int_to_string:
; // Convert integer part to decimal string
    ld iy,strBuff+5; one's place
; if (sig32X == 0) {
    ld hl,(ix+sig32X) ; get sig32X low word
    ZERO_HL
    jr nz,@sig32X_not_zero
;     *iy++ = '0';
        ld a,'0'
        ld (iy),a
        dec iy
        ld a,4 ; 4-spaces
        jr @write_spaces
; } else {
    @sig32X_not_zero:
;     char temp[16];
;     int ti = 0;
        di ; we'll be using af'
        ld a,5 ; ti = max integer digits
;     while (sig32X) {
        @write_sig32X_loop:
            SIGN_HL ; check sig32X for zero (don't use ZERO_HL because that destroys a)
            jr z,@write_spaces
            ex af,af' ; save ti
;         sig32X  /= 10;
;         temp[ti++] = '0' + (sig32X % 10);
            call HL_Div_10
            add a,'0'
            ld (iy),a ; store digit
            dec iy ; move to next position
            ex af,af' ; restore ti
            dec a ; decrement ti
            jr @write_sig32X_loop ; loop until all digits written
;     }
    @write_spaces:
        ei
        or a
        jr z,@do_frac
        ld b,a ; loop counter = ti
        ld a,' '
;     while (ti--) {
        @write_spaces_loop:
;         *iy++ = temp[ti];
            ld (iy),a ; store space
            dec iy ; move to next position
        djnz @write_spaces_loop ; loop until all spaces written
;     }
; }
; // Convert fractional part (if any) by multiplying by 10 repeatedly
@do_frac:
    ld iy,strBuff+5 ; one's place
; if (sig32Y != 0) {
    ld hl,(ix+sig32Y) ; get sig32Y low word
    ZERO_HL
    jp z,@terminate_string
;     *iy++ = '.';
        inc iy ; move to decimal point position
        ld a,'.'
        ld (iy),a ; store decimal point
        ld hl,(ix+sig32Y) ; sig32Y low word
        ld de,(ix+sig32Y+2) ; sig32Y high word
    @write_frac_loop:
;    while (sig32Y != 0) {
        ZERO_HL ; check sig32Y low word for zero
        jr nz,@sig32Y_not_zero
        ex de,hl ; hl = high word, de = low word
        ZERO_HL ; check sig32Y high word for zero
        ex de,hl ; hl = low word, de = high word
        jr z,@terminate_string
;         sig32Y *= 10;
        @sig32Y_not_zero:
            add.s hl,hl
            ex de,hl
            adc hl,hl 
            ex de,hl ; sig32Y * 2
            push de ; sig32Y * 2 high word
            push hl ; sig32Y * 2 low word
            add.s hl,hl
            ex de,hl
            adc hl,hl 
            ex de,hl ; sig32Y * 4
            add.s hl,hl
            ex de,hl
            adc hl,hl 
            ex de,hl ; sig32Y * 8
            pop bc ; sig32Y * 2 low word
            add.s hl,bc
            ex de,hl
            pop bc ; sig32Y * 2 high word
            adc hl,bc
            ex de,hl ; sig32Y * 10
            push de ; sig32Y high word
            push hl ; sig32Y low word
;         uint32_t digit = sig32Y >> expY;           // divide by 2^expY
            ld b,(ix+expY) ; expY = loop counter
            @digit_loop:
                srl d
                rr e
                rr h
                rr l
                djnz @digit_loop ; l is our digit at the end of the loop
;         *iy++ = (char)('0' + digit);
            inc iy ; move to next position
            ld a,'0'
            add a,l ; convert digit to ASCII
            ld (iy),a ; store digit
;         sig32Y &= ((1u << expY) - 1);              // sig32Y mod 2^expY
            ld bc,(ix+sigY) ; get mask low word
            pop hl ; sig32Y low word
            ld a,c
            and l
            ld l,a
            ld a,b
            and h
            ld h,a
            ld a,(ix+sigY+2) ; get mask middle high byte
            pop de ; sig32Y high word
            and e
            ld e,a
            ld d,0 ; high byte will always be masked out
            jr @write_frac_loop
;     }
; }
; *iy = '\0';  // null-terminate the string
@terminate_string:
    xor a ; zero = null terminator
    inc iy
    ld (iy),a
; return strBuff;
    ld hl,strBuff
@set_sign:
; set sign
    ld a,(ix+signA) ; signA
    or a ; check signA for zero
    jr nz,@sign_neg
    ld a,' ' ; positive sign
    jr @write_sign
@sign_neg:
    ld a,'-' ; negative sign
@write_sign:
    ld (hl),a ; store sign
    ret