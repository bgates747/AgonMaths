; multiply two signed fixed16 numbers and get a fixed16 result
; operation: 0hl * 0de -> 0hl
; destroys: af,af', bc, de
f16_mul:
; compute sign of product
    ld a,h ; sign hl
    and %10000000
    xor d ; xor sign hl with sign de
    and %10000000
    ld c,a  ; signZ

; clear sign bits to make specials tests easier
    res 7,h
    res 7,d

; check operands for NaN
    ld a,0x7E ; NaN upper byte is all we need
    cp h
    jp z,@ret_nan
    cp d
    jp z,@ret_nan

; check hl for zero
    SIGN_HL
    jp z,@arg_is_zero

; check hl for inf
    ld a,h ; expA
    and %01111100 ; stored exp of hl
    cp %01111100 ; check for inf
    jp z,@arg_is_inf

; check de for zero
    ex de,hl ; flip operands
    SIGN_HL
    jp z,@arg_is_zero

; check de for inf
    ld a,h ; expB
    and %01111100 ; stored exp of de
    cp %01111100 ; check for inf
    jp z,@arg_is_inf

; get exponent of de
    or a ; clear carry
    rra ; shift exp
    rra ; down twice
    ld b,a ; assume normal
    and a ; test for subnormal
    call z,softfloat_normSubnormalF16Sig

; get mantissa of de
    ld a,h
    and %00000011 ; mask out sign and exponent
    ld h,a
    ex de,hl ; flip operands back

; get exponent of hl
    ld a,h ; expA
    and %01111100 ; stored exp of hl
    rra ; shift exp
    rra ; down twice
    and a ; test for subnormal
    jr nz,@F ; skip if normal
    ld a,b ; a = expB
    call softfloat_normSubnormalF16Sig
@@:
; compute exponent of product
    add a,b ; expA + expB
    sub 0x0F ; expA + expB - 0xF
    ld b,a ; expZ

; get mantissa of hl
    ld a,h
    and %00000011 ; mask out sign and exponent
    ld h,a

; cleared to proceed with the multiplication
    push bc ; stack sign and exponent of product
; sigA = (sigA | 0x0400)<<4;
    set 2,h ; set implicit 1
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
; sigB = (sigB | 0x0400)<<5;
    ex de,hl
    set 2,h ; set implicit 1
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    ex de,hl
; sig32Z = (uint_fast32_t) sigA * sigB;
    call mul_16_32
    ex de,hl
    ex de,hl
; sigZ = sig32Z>>16; (we get this for free since HL contains the relevant bits)
; if ( sig32Z & 0xFFFF ) sigZ |= 1; set sticky bit
    ld a,e
    and a
    jp nz,@need_sticky
    ld a,d
    and a
    jp nz,@need_sticky
    jp @do_sign_exp
@need_sticky:
    set 0,l ; set sticky bit
@do_sign_exp:
    pop bc ; exp and sign of product
; check for subnormal
    ld a,h
    and %11000000 ; sigZ - 0x4000
    jp nz,@end_normalise ; sigZ >= 0x4000
@normalise_loop:
    dec b ; --expZ
    add hl,hl ; sigZ <<= 1
    bit 6,h ; test for carry into assumed 1 place
    jp z,@normalise_loop
@end_normalise: ; we land here if product was normal
; return softfloat_roundPackToF16( signZ, expZ, sigZ );
    jp softfloat_roundPackToF16
@arg_is_zero:
    ld a,d ; check other arg for inf
    and %01111100
    cp %01111100
    jp z,@ret_nan ; return NaN if exponent is inf    
    ld a,c ; otherwise, sign the zero
    or h ; (re)set sign bit
    ld h,a
    ret
@arg_is_inf:
    ex de,hl          ; flip operands to get other arg in hl
    SIGN_HL           ; test other arg for zero
    jr z,@ret_nan     ; zero * inf = NaN
    ld hl,0x7C00      ; Inf result
    ld a,c            ; get product sign
    or h              ; reapply sign
    ld h,a
    ret
@ret_nan:
    ld hl,0xFE00      ; canonical NaN
    ret
; }


;Inputs: hl,de = operands
;Outputs: hlde = 32-bit product
;Destroys: af,bc
;53 cycles
;32 bytes
mul_16_32:
    ld b,e
    ld c,l
    ld l,e
    ld e,c
    mlt bc
    ld a,b
    ld b,h
    mlt hl
    ; Add high part of low product, cannot overflow 16 bits
    add a,l
    ld l,a
    adc a,h
    sub a,l
    ld h,a
    ld a,c
    ld c,d
    mlt de
    add.s hl,de ; .s to force 16-bit addition
    ld e,a
    ld d,l
    ld l,h
    ld h,0
    rl h
    mlt bc
    add hl,bc ; Cannot overflow 16 bits
    ret