test_smul_16_32:
; copy test filename
    ld hl,@test_filename
    ld de,test_filename
    call test_copy_filename

; do the test
    call printInline
    asciz "\r\nsmul_16x16_32 test\r\n"
    ld hl,12
    ld (bytes_per_record),hl
    ld hl,240000
    ld (bytes_read_max),hl
    call test_init ; open files for reading and writing
    ret z ; error opening files
@read_loop:
    call test_read_data
    jr z,@read_end
    ld iy,filedata
@calc_loop:
; perform multiplication 
    ld hl,(iy+0) ; op1
    ld de,(iy+2) ; op2
    call smul_16x16_32
    ld (iy+8),e ; assembly result low byte
    ld (iy+9),d ; assembly result middle low byte
    ld (iy+10),l ; assembly result middle high byte
    ld (iy+11),h ; assembly result high byte
; check for error
    ld e,(iy+6) ; python result middle high byte
    ld d,(iy+7) ; python result high byte
    or a ; clear carry
    sbc.s hl,de
    jr nz,@calc_error
    ld l,(iy+8) ; assembly result low byte
    ld h,(iy+9) ; assembly result middle low byte
    ld e,(iy+4) ; python result low byte
    ld d,(iy+5) ; python result middle low byte
    or a ; clear carry
    sbc.s hl,de
    jr z,@next_record
@calc_error:
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

@test_filename: asciz "../tests/smul_16x16_32.bin"

