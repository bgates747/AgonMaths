canonicalNaNF16: EQU 0x7E00
ui16_fromPosOverflow: EQU 0xFFFF
ui16_fromNegOverflow: EQU 0xFFFF
ui16_fromNaN: EQU 0xFFFF

uq16_8_fromPosOverflow: EQU 0xFFFFFF
uq16_8_fromNegOverflow: EQU 0x000000
uq16_8_fromNaN:         EQU 0xFFFFFF

str_inf:  asciz "   Inf"
str_nan:  asciz "   NaN"
str_zero: asciz "     0"

str_numbers: db "zzzzzzzz"
strBuff:      blkb 32,0

; 16-bit floating point globals
; A group
uiA:         EQU 0          ; 2 bytes
sigA:        EQU uiA + 3    ; 2 bytes
signA:       EQU sigA + 3   ; 1 byte
expA:        EQU signA + 1  ; 1 byte

; B group
uiB:         EQU expA + 2   ; 2 bytes
sigB:        EQU uiB + 3    ; 2 bytes
signB:       EQU sigB + 3   ; 1 byte
expB:        EQU signB + 1  ; 1 byte

; X group
sigX:        EQU expB + 2   ; 2 bytes
signX:       EQU sigX + 3   ; 1 byte
expX:        EQU signX + 1  ; 1 byte

; Y group
sigY:        EQU expX + 2   ; 2 bytes
signY:       EQU sigY + 3   ; 1 byte
expY:        EQU signY + 1  ; 1 byte

; Z group
uiZ:         EQU expY + 2   ; 2 bytes
sigZ:        EQU uiZ + 3    ; 2 bytes
signZ:       EQU sigZ + 3   ; 1 byte
expZ:        EQU signZ + 1  ; 1 byte

; Shared / Misc
expDiff:     EQU expZ + 2      ; 1 byte
shiftDist:   EQU expDiff + 1   ; 1 byte
sigDiff:     EQU shiftDist + 1 ; 2 bytes
r0:          EQU sigDiff + 3   ; 2 bytes
rem:         EQU r0 + 3        ; 4 bytes

; 32-bit floating point globals
sig32A:      EQU rem + 5       ; 4 bytes
sig32B:      EQU sig32A + 5    ; 4 bytes
sig32X:      EQU sig32B + 5    ; 4 bytes
sig32Y:      EQU sig32X + 5    ; 4 bytes
sig32Z:      EQU sig32Y + 5    ; 4 bytes
sig48Z:      EQU sig32Z + 5    ; 6 bytes

; Global variable table
s_globals:

    _uiA:          dl 0
    _sigA:         dl 0
    _signA:        db 0
    _expA:         db 0
                   db 0   ; padding

    _uiB:          dl 0
    _sigB:         dl 0
    _signB:        db 0
    _expB:         db 0
                   db 0   ; padding

    ; X group 
    _sigX:         dl 0
    _signX:        db 0
    _expX:         db 0
                   db 0   ; padding

    ; Y group 
    _sigY:         dl 0
    _signY:        db 0
    _expY:         db 0
                   db 0   ; padding

    _uiZ:          dl 0
    _sigZ:         dl 0
    _signZ:        db 0
    _expZ:         db 0
                   db 0   ; padding

    _expDiff:      db 0
    _shiftDist:    db 0
    _sigDiff:      dl 0
    _r0:           dl 0
    _rem:          blkb 5,0

    _sig32A:       blkb 5,0
    _sig32B:       blkb 5,0
    _sig32X:       blkb 5,0
    _sig32Y:       blkb 5,0
    _sig32Z:       blkb 5,0
    _sig48Z:       blkb 7,0