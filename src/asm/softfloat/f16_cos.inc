; inputs: uhl = angle256 in 16.8 format (top byte is ignored to get mod 256 for free)
; output: hl = cos(angle256) in float16 format
; destroys: af, hl, bc, de, ix
f16_cos:
    ld a,64
    add a,h ; convert to cos
    ld h,a
    jp f16_sin ; call f16_sin
; }
; end f16_cos