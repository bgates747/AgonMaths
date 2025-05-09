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
    include "../softfloat/f16_cos.inc"
    include "../softfloat/f16_sin.inc"
    include "../softfloat/f16_sqrt.inc"
    include "../softfloat/f16_sub.inc"
    include "../softfloat/f16_to_ui16.inc"
    include "../softfloat/f16_to_uq16_8.inc"
    include "../softfloat/ui16_to_f16.inc"
    include "../softfloat/uq8_8_to_f16.inc"
    include "../softfloat/s_addMagsF16.inc"
    include "../softfloat/s_normSubnormalF16Sig.inc"
    include "../softfloat/s_roundPackToF16.inc"
    include "../softfloat/s_shiftRightJam32.inc"
    include "../softfloat/s_roundToUI16.inc"
    include "../softfloat/s_roundToUQ16_8.inc"
    include "../softfloat/s_subMagsF16.inc"

    include "f16_test_add.inc"
    include "f16_test_div.inc"
    include "f16_test_mul.inc"
    include "f16_test_sub.inc"
    include "f16_test_cos.inc"
    include "f16_test_sin.inc"
    include "f16_test_sqrt.inc"
    include "f16_test_print.inc"
    include "smul_16_32.inc"
    include "f16_test_ui16_to_f16.inc"
    include "f16_test_uq8_8_to_f16.inc"
    include "f16_test_f16_to_ui16.inc"
    include "f16_test_f16_to_uq16_8.inc"

main:
    ; ld a,12
    ; cp 13
    ; PRINT_A_HEX ""
    ; call dumpFlags

    ; ld a,13
    ; cp 13
    ; PRINT_A_HEX ""
    ; call dumpFlags

    ; ld a,14
    ; cp 13
    ; PRINT_A_HEX ""
    ; call dumpFlags
    ; ret

    ; ld hl,0x3801
    ; call f16_to_ui16
    ; PRINT_HL_HEX "ui16 hex"
    ; ret

    ; ld hl,0x4009 ; significand
    ; ld b,0x1d ; exponent
    ; ld c,0 ; sign
    ; call softfloat_roundPackToF16
    ; PRINT_HL_HEX "f16 hex"
    ; PRINT_HL_F16 "f16 dec"
    ; call printNewLine
    ; ret

    ; ld hl,0x8011
    ; call printDec
    ; call printNewLine
    ; call ui16_to_f16
    ; PRINT_HL_HEX "f16 hex"
    ; PRINT_HL_F16 "f16 dec"
    ; call printNewLine
    ; ret

    ; ld hl,8
    ; call ui16_to_f16
    ; call f16_print
    ; call printString
    ; call printNewLine

    ; ld hl,65504
    ; call ui16_to_f16
    ; call f16_print
    ; call printString
    ; call printNewLine

    ; ld hl,65505
    ; call ui16_to_f16
    ; call f16_print
    ; call printString
    ; call printNewLine

    ; ret


    ; call test_f16_add
    ; call test_f16_sub
    ; call test_f16_mul
    ; call test_f16_div
    call test_f16_cos
    call test_f16_sin
    ; call test_f16_sqrt
    ; call test_f16_print
    ; call test_smul_16_32
    ; ; call test_ui16_to_f16 ; bad file
    ; call test_uq8_8_to_f16
    ; ; call test_f16_to_ui16 ; bad file
    ; call test_f16_to_uq16_8

    ret

; must be final include so filedata doesn't stomp other application code or data
    include "f16_test_all.inc"