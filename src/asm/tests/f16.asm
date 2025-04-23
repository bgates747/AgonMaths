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

    include "../softfloat/globals.inc"
    include "../softfloat/helpers.inc"
    include "../softfloat/internals.inc"
    include "../softfloat/primitives.inc"
    include "../softfloat/f16_add.inc"
    include "../softfloat/f16_div.inc"
    include "../softfloat/f16_mul.inc"
    include "../softfloat/f16_sqrt.inc"
    include "../softfloat/f16_sub.inc"
    include "../softfloat/s_addMagsF16.inc"
    include "../softfloat/s_normSubnormalF16Sig.inc"
    include "../softfloat/s_roundPackToF16.inc"
    include "../softfloat/s_shiftRightJam32.inc"
    include "../softfloat/s_subMagsF16.inc"

main:
; your code goes hereish

    ret