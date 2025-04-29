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

;     ld de,0x00C1
;     ld hl,0x0000
; ;   12648448 * 10 = 0x078A0000

;             add.s hl,hl
;             ex de,hl
;             adc hl,hl 
;             ex de,hl ; sig32Y * 2
;             push de ; sig32Y * 2 high word
;             push hl ; sig32Y * 2 low word
;             add.s hl,hl
;             ex de,hl
;             adc hl,hl 
;             ex de,hl ; sig32Y * 4
;             add.s hl,hl
;             ex de,hl
;             adc hl,hl 
;             ex de,hl ; sig32Y * 8
;             pop bc ; sig32Y * 2 low word
;             add.s hl,bc
;             ex de,hl
;             pop bc ; sig32Y * 2 high word
;             adc hl,bc
;             ex de,hl ; sig32Y * 10
;             PRINT_DEHL_HEX "DEHL * 10"

;             ret


    ld hl,canonicalNaNF16 ; PASSES
    ld hl,0x7c00 ; infinity PASSES
    ld hl,0xfc00 ; negative infinity PASSES
    ld hl,0 ; zero PASSES
    ld hl,0x8000 ; negative zero PASSES
    ld hl,0x7bff ; max normal ; PASSES
    LD HL,0xFBFF ; min normal ; PASSES
    ld hl,0x5800 ; 128 ; PASSES
    ld hl,0x0001 ; smallest positive subnormal ; PASSES
    ld hl,0x8001 ; largest negative subnormal ; PASSES
    ld hl,0xD800 ; -128 ; PASSES
    ld hl,0xD804 ; -128.5 ; PASSES
    ld hl,0x4248 ; just under pi 3.140625
    ld hl,0x4249 ; just over pi 3.142578125

    call f16_print
    ; ld hl,str_numbers
    call printString
    call printNewLine

    ; ld hl,strBuff
    ; ld a,32
    ; call dumpMemoryHex
    ; call printNewLine
    ret

    ; call test_f16_add
    ; call test_f16_sub
    ; call test_f16_mul
    ; call test_f16_div
    ; call test_f16_sqrt
    ; call test_f16_print
    ; call test_mul_32x16_48

    ret

strBuff0: blkb 31,'x'
    db 0 ; null terminator

; must be final include so filedata doesn't stomp other application code or data
    include "f16_test_all.inc"