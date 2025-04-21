; sub two 16-bit floating point numbers
; inputs: hl = a, de = b
; outputs: hl = a - b
; destroys: af, bc, de, ix
f16_sub:
; flip sign of opB and call f16_add
    ld a,d ; sign opB
    xor %10000000 ; flip sign
    ld d,a ; store sign opB
    jp f16_add ; call f16_add
; end f16_sub