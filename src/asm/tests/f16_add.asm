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
;     ld hl,0xffff
;     ld (ix+sigX),hl
;     ld hl,0xffff
;     ld (ix+sigY),hl
;     ld a,13
;     ld (ix+expDiff),a ; expDiff = 8

; @calc_sig32Z:
; ;     sig32Z = ((uint_fast32_t) sigX<<expDiff) - sigY;
;         ld b,(ix+expDiff)
;         xor a ; clear a
;         ld hl,(ix+sigX)
; @sigX_shift:
;         add hl,hl
;         adc a,a
;         djnz @sigX_shift

;         ld (ix+sigX),hl ; store low 3 bytes of sigX
;         ld e,(ix+sigX+2) ; e = upper middle byte of sigX
;         ld d,a ; dehl is now sigX
;         ld bc,(ix+sigY)

;     ;   dehl
;     ; - 00bc
;     ; = hlbc 
;         or a ; clear carry
;         sbc.s hl,bc ; .s to force 16-bit subtraction
;         ld c,l
;         ld b,h

;         ex de,hl
;         ld de,0

;         sbc.s hl,de ; hlbc = sigX<<expDiff - sigY

;         push bc
;         pop de

;         PRINT_HLDE_HEX " sigX<<expDiff - sigY"
;         ret


    ; ld hl,0x1f00
    ; PRINT_UNPACK_F16 " unpacked F16"
    ; ret

    ; ld hl,0x7C00 ; 0x7C00
    ; fracF16UI
    ; PRINT_HL_HEX " fracF16UI"
    ; ret

    ; ld hl,0x0601
    ; ld bc,0x1b00
    ; call packToF16UI
    ; PRINT_HL_HEX " packed to F16UI"
    ; RET

    
    ; -14	0xF2
    ; -13	0xF3
    ; -12	0xF4

;     ld d,-12
;     ld b,3
; @loop:
;     ld a,-13
;     cp d
;     ld a,d
;     jp c,@F
;     call printDecS8 
;     call printInline
;     asciz " lte -13\r\n"
;     dec d
;     djnz @loop
;     ret
; @@:
;     call printDecS8 
;     call printInline
;     asciz " gt  -13\r\n"
;     dec d
;     djnz @loop
;     ret

    ; call printInline
    ; asciz "35584.0 + -55072.0 = -19488.0\r\n"
    ; call printInline
    ; asciz "0x7858 + 0xFAB9 = 0xF4C2\r\n"
    ; ld hl,0x7858 ; 0x7858
    ; ld de,0xFAB9 ; 0xFAB9
    ; call f16_add
    ; PRINT_HL_HEX " assembly result"
    ; call printNewLine
    ; RET

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
    ld iy,filedata
@calc_loop:
; perform operation 
    ld l,(iy+0) ; op1 low byte
    ld h,(iy+1) ; op1 high byte
    ld e,(iy+2) ; op2 low byte
    ld d,(iy+3) ; op2 high byte
    call f16_add
; write results to file buffer
    ld (iy+6),l ; assembly sum low byte
    ld (iy+7),h ; assembly sum high byte
; check for error
    ld e,(iy+4) ; python sum low byte
    ld d,(iy+5) ; python sum high byte
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

test_filename: asciz "../tests/f16_add.bin"
test_filename_out: asciz "../tests/f16_add.bin"

    include "../include/files.inc"
