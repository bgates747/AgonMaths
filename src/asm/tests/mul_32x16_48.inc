test_mul_32x16_48:
; copy test filename
    ld hl,@test_filename
    ld de,test_filename
    call test_copy_filename

; do the test
    call printInline
    asciz "\r\nmul_32x16_48 test\r\n"
    ld hl,18
    ld (bytes_per_record),hl
    ld hl,360000
    ld (bytes_read_max),hl
    call test_init ; open files for reading and writing
    ret z ; error opening files
@read_loop:
    call test_read_data
    jr z,@read_end
    ld iy,filedata
@calc_loop:
; perform operation 
    ld de,(iy+0) ; op1 low word
    ld hl,(iy+2) ; op1 high word
    ld bc,(iy+4) ; op2
    call mul_32x16_48
; write results to file buffer
    ld (iy+12),bc ; assembly result low word
    ld (iy+14),de ; assembly result middle word
    ld (iy+16),l ; assembly result high word low byte
    ld (iy+17),h ; assembly result high word high byte
; compare high word
    ld de,(iy+10) ; python high
    or a
    sbc.s hl,de
    jr nz,@error
; compare mid word
    ld hl,(iy+14) ; asm mid
    ld de,(iy+8)  ; python mid
    or a
    sbc.s hl,de
    jr nz,@error
; compare low word
    ld hl,(iy+12) ; asm low
    ld de,(iy+6)  ; python low
    or a
    sbc.s hl,de
    jr z,@next_record
@error:
; bump error count
    ld hl,(errors)
    inc hl
    ld (errors),hl
@next_record:
    ld de,(bytes_per_record)
    add iy,de; bump data pointer
    ld hl,(records)
    inc hl
    ld (records),hl
    ld hl,(counter)
    dec hl
    ld (counter),hl
    SIGN_UHL
    jr nz,@calc_loop
; write to file
    call test_write_data
; read next batch from file
    jp @read_loop
@read_end:
    jp test_complete
@test_filename: asciz "../tests/mul_32x16_48.bin"
; end of test_mul_32x16_48