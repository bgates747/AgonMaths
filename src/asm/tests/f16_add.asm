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

; API INCLUDES
    include "../include/mos_api.inc"
    include "../include/macros.inc"
    include "../include/functions.inc"
    include "../include/arith24.inc"
    include "../include/fixed168.inc"
    include "../include/maths.inc"
    include "../include/timer.inc"
    include "../include/vdu.inc"
    include "../include/vdu_buffered_api.inc"
    include "../include/vdu_plot.inc"
    include "../include/debug.inc"

; APPLICATION INCLUDES
    include "../softfloat/globals.inc"
    include "../softfloat/internals.inc"
    include "../softfloat/f16_add.inc"
    include "../softfloat/s_addMagsF16.inc"
    include "../softfloat/s_subMagsF16.inc"
    include "../softfloat/s_normSubnormalF16Sig.inc"
    include "../softfloat/s_roundPackToF16.inc"
    include "../softfloat/s_shiftRightJam32.inc"

main:
    ; ld a,0x1D
    ; call printDec8
    ; call printNewLine
    ; cp 0x1E
    ; call dumpFlags
    ; ret

; ; PASSES
;     call printInline
;     asciz "0.0 + 6e-05 = 6.002187728881836e-05\r\n"
;     call printInline
;     asciz "0x0000 + 0x03EF = 0x03EF\r\n"
;     ld hl,0x0000 ; 0.0
;     ld de,0x03EF ; 6e-05
;     call f16_add
;     PRINT_HL_HEX " assembly result"
;     call printNewLine

; ; PASSES
;     call printInline
;     asciz "1.0 + 1.0 = 2.0\r\n"
;     call printInline
;     asciz "0x3C00 + 0x3C00 = 0x4000\r\n"
;     ld hl,0x3C00 ; 1.0
;     ld de,0x3C00 ; 1.0
;     call f16_add
;     PRINT_HL_HEX " assembly result"
;     call printNewLine

; ; FAILS
;     call printInline
;     asciz "inf + 1.0 = inf\r\n"
;     call printInline
;     asciz "0x7C00 + 0x3C00 = 0x7C00\r\n"
;     ld hl,0x7C00 ; inf
;     ld de,0x3C00 ; 1.0
;     call f16_add
;     PRINT_HL_HEX " assembly result"
;     call printNewLine

;     ret

    call vdu_cls
    call printInline
    asciz "\r\nf16_add test\r\n"
    ld hl,8
    ld (bytes_per_record),hl
    ld hl,480000
    ld (bytes_read_max),hl
    call test_init ; open files for reading and writing
    ret z ; error opening files
@read_loop:
    call test_read_data
    jr z,@read_end
@calc_loop:
; perform division 
    ld l,(ix+0) ; op1 low byte
    ld h,(ix+1) ; op1 high byte
    ld e,(ix+2) ; op2 low byte
    ld d,(ix+3) ; op2 high byte
    call f16_add
; write results to file buffer
    ld (ix+6),l ; assembly sum low byte
    ld (ix+7),h ; assembly sum high byte
; check for error
    ld e,(ix+4) ; python sum low byte
    ld d,(ix+5) ; python sum high byte
    or a ; clear carry
    sbc.s hl,de
    jr z,@next_record
; bump error count
    ld hl,(errors)
    inc hl
    ld (errors),hl
@next_record:
; write results to file buffer
    ld de,(bytes_per_record)
    add ix,de; bump data pointer
    ld hl,(records)
    inc hl
    ld (records),hl
    ld hl,(counter)
    dec hl
    ld (counter),hl
    SIGN_HLU
    jr nz,@calc_loop
; write to file
    call test_write_data
; read next batch from file
    jp @read_loop
@read_end:
    jp test_complete

test_filename: asciz "../tests/f16_add.bin"
test_filename_out: asciz "../tests/f16_add.bin"

    include "../include/files.inc"
