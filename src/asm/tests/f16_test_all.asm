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

    include "../agon/mos_api.inc"
    include "../agon/macros.inc"
    include "../agon/debug.inc"

    include "../agon/arith24.inc"
    include "../agon/fixed168.inc"
    include "../agon/fpp_ext.inc"
    include "../agon/fpp.inc"
    include "../agon/functions.inc"
    include "../agon/maths.inc"
    include "../agon/timer_prt_stopwatch.inc"
    include "../agon/timer.inc"
    include "../agon/vdu_buffered_api.inc"
    include "../agon/vdu_fonts.inc"
    include "../agon/vdu_plot.inc"
    include "../agon/vdu_sound.inc"
    include "../agon/vdu.inc"

    include "../softfloat/globals.inc"
    include "../softfloat/helpers.inc"
    include "../softfloat/internals.inc"
    include "../softfloat/primitives.inc"
    include "../softfloat/f16_add.inc"
    include "../softfloat/f16_div.inc"
    include "../softfloat/f16_mul.inc"
    include "../softfloat/f16_print.inc"
    include "../softfloat/f16_sqrt.inc"
    include "../softfloat/f16_sub.inc"
    include "../softfloat/s_addMagsF16.inc"
    include "../softfloat/s_normSubnormalF16Sig.inc"
    include "../softfloat/s_roundPackToF16.inc"
    include "../softfloat/s_shiftRightJam32.inc"
    include "../softfloat/s_subMagsF16.inc"

    include "f16_test_add.inc"
    include "f16_test_div.inc"
    include "f16_test_mul.inc"
    include "f16_test_sub.inc"
    include "f16_test_sqrt.inc"
    include "f16_test_print.inc"
    include "mul_32x16_48.inc"
main:

    ; call test_f16_add
    ; call test_f16_sub
    ; call test_f16_mul
    ; call test_f16_div
    ; call test_f16_sqrt
    call test_f16_print
    ; call test_mul_32x16_48

    ret

; must be final include so filedata doesn't stomp other application code or data
    include "f16_test_all.inc"