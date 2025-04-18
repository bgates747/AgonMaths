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
    include "../softfloat/f16_mul.inc"
    include "../softfloat/f16_div.inc"
    include "../softfloat/internals.inc"
    include "../softfloat/s_normSubnormalF16Sig.inc"
    include "../softfloat/s_roundPackToF16.inc"
    include "../softfloat/s_shiftRightJam32.inc"

main:
    call vdu_cls
    call printInline
    asciz "\r\nmul_16_32 test\r\n"
    ld hl,12
    ld (bytes_per_record),hl
    ld hl,480000
    ld (bytes_read_max),hl
    call test_init ; open files for reading and writing
    ret z ; error opening files
@read_loop:
    call test_read_data
    jr z,@read_end
@calc_loop:
; perform multiplication 
    ld hl,(ix+0) ; op1
    ld de,(ix+2) ; op2
    call mul_16_32
    ld (ix+8),e ; assembly product low byte
    ld (ix+9),d ; assembly product middle low byte
    ld (ix+10),l ; assembly product middle high byte
    ld (ix+11),h ; assembly product high byte
; check for error
    ld e,(ix+6) ; python product middle high byte
    ld d,(ix+7) ; python product high byte
    or a ; clear carry
    sbc.s hl,de
    jr nz,@calc_error
    ld l,(ix+8) ; assembly product low byte
    ld h,(ix+9) ; assembly product middle low byte
    ld e,(ix+4) ; python product low byte
    ld d,(ix+5) ; python product middle low byte
    or a ; clear carry
    sbc.s hl,de
    jr z,@next_record
@calc_error:
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

test_filename: asciz "../tests/mul_16_32.bin"
test_filename_out: asciz "../tests/mul_16_32.bin"

    include "../include/files.inc"
