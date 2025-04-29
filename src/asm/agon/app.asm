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

    include "mos_api.inc"
    include "macros.inc"
    include "misc.inc"
    include "debug.inc"

    include "arith24.inc"
    include "fixed168.inc"
    include "fpp_ext.inc"
    include "fpp.inc"
    include "functions.inc"
    include "maths.inc"
    include "timer_prt_stopwatch.inc"
    include "timer.inc"
    include "vdu_buffered_api.inc"
    include "vdu_fonts.inc"
    include "vdu_plot.inc"
    include "vdu_sound.inc"
    include "vdu.inc"

main:
    ld hl,0xFFE0
    ld de,0x199A
    call mul_16x16_32


    call printDec
    call printNewLine

    ret

;Inputs: hl,de = operands
;Outputs: hlde = 32-bit product
;Destroys: af,bc
;53 cycles
;32 bytes
mul_16x16_32:
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
; end mul_16x16_32

DEHL_Div_10:
; https://github.com/Zeda/Z80-Optimized-Routines/blob/master/math/division/DEHL_Div_10.z80
;Inputs:
;     DEHL
;Outputs:
;     DEHL is the quotient
;     A is the remainder
;     B is the remainder
;     C is 10
;912cc~941cc

    xor a
    ld c,10
    rl d 
    rla
    rl d 
    rla
    rl d 
    rla
    rl d 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl d 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl d 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl d 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl d 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl e 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl h 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    rl l 
    rla 
    sub c 
    jr nc,$+3 
    add a,c
    ld b,a
    ld a,l 
    rra 
    ccf 
    ld l,a
    ld a,h 
    rra 
    ccf 
    ld h,a
    ld a,e 
    rra 
    ccf 
    ld e,a
    ld a,d 
    rra 
    ccf 
    ld d,a
    ld a,b
    ret


; must be final include so filedata doesn't stomp other application code or data
    include "files.inc"
