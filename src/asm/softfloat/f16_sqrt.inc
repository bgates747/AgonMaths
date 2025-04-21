; sqrt of a 16-bit floating point number
; inputs: hl = a, de = b
; outputs: hl = a - b
; destroys: af, bc, de, ix
f16_sqrt:
; set index to global variables
    ld ix,s_globals

    ret
; end f16_sqrt