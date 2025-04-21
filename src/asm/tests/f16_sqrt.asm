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
    ld hl,0x6E68 ; sqrt(6561.0) = 81.0
    signF16UI
    expF16UI
    PRINT_BC_HEX "b = expA, c = signA" ; DEBUG
    fracF16UI
    PRINT_HL_HEX "hl = sigA" ; DEBUG

; expZ = ((expA - 0xF)>>1) + 0xE;
    ld a,b
    sub 0xF ; expA - 0xF
    srl a ; expA - 0xF >> 1
    add a,0xE ; expZ = ((expA - 0xF)>>1) + 0xE
; expA &= 1;
    and 1 ; expA &= 1
    ld b,a ; c = expA
; sigA |= 0x0400;
    set 2,h ; set implicit 1
; index = (sigA>>6 & 0xE) + expA;
    ld e,l
    ld d,h ; de = sigA
    add hl,hl ; sigA <<= 1
    add hl,hl ; sigA <<= 1
    ld a,0xE
    and h
    add a,b ; a = index
    PRINT_A_HEX "index = (sigA>>6 & 0xE) + expA;" ; DEBUG
    ret

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
