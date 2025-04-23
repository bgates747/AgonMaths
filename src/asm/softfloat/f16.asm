    assume adl=1 
    org 0x040000 
    jp start 
    align 64 
    db "MOS" 
    db 00h 
    db 01h 
start: 
    push af
    push bc
    push de
    push ix
    push iy
    call main
exit:
    pop iy
    pop ix
    pop de
    pop bc
    pop af
    ld hl,0
    ret

    include "globals.inc"
    include "helpers.inc"
    include "internals.inc"
    include "primitives.inc"
    include "f16_add.inc"
    include "f16_div.inc"
    include "f16_mul.inc"
    include "f16_sqrt.inc"
    include "f16_sub.inc"
    include "s_addMagsF16.inc"
    include "s_normSubnormalF16Sig.inc"
    include "s_roundPackToF16.inc"
    include "s_shiftRightJam32.inc"
    include "s_subMagsF16.inc"

main:
; your code goes hereish

    ret