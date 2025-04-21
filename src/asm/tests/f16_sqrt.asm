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
    include "../softfloat/f16_sqrt.inc"
    include "../softfloat/s_normSubnormalF16Sig.inc"
    include "../softfloat/s_roundPackToF16.inc"
    include "../softfloat/s_shiftRightJam32.inc"

main:
    ; call printInline
    ; asciz "sqrt(35584.0) = 188.625\r\n"
    ; call printInline
    ; asciz "sqrt(0x7858) = 0x59E5\r\n"
    ; ld hl,0x7858 ; 0x7858
    ; call f16_sqrt
    ; PRINT_HL_HEX " assembly result"
    ; call printNewLine
    ; ret

    call vdu_cls
    call printInline
    asciz "\r\nf16_sqrt test\r\n"
    ld hl,6
    ld (bytes_per_record),hl
    ld hl,480000
    ld (bytes_read_max),hl
    call test_init ; open files for reading and writing
    ret z ; error opening files
@read_loop:
    call test_read_data
    jr z,@read_end
    ld iy,filedata
@calc_loop:
; perform operation 
    ld l,(iy+0) ; op1 low byte
    ld h,(iy+1) ; op1 high byte
    call f16_sqrt
    ld hl,0x8000 ; -0 ; DEBUG
; write results to file buffer
    ld (iy+4),l ; assembly sqrt low byte
    ld (iy+5),h ; assembly sqrt high byte
; check for error
    ld e,(iy+2) ; python sqrt low byte
    ld d,(iy+3) ; python sqrt high byte
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
    add iy,de; bump data pointer
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

test_filename: asciz "../tests/f16_sqrt.bin"
test_filename_out: asciz "../tests/f16_sqrt.bin"

    include "../include/files.inc"
