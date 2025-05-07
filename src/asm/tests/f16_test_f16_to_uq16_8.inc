test_f16_to_uq16_8:
; copy test filename
    ld hl,@test_filename
    ld de,test_filename
    call test_copy_filename

; do the test
    call printInline
    asciz "\r\nf16_to_uq16_8 test\r\n"
    ld hl,8
    ld (bytes_per_record),hl
    ld hl,131072
    ld (bytes_read_max),hl
    call test_init ; open files for reading and writing
    ret z ; error opening files
@read_loop:
    call test_read_data
    jr z,@read_end
    ld iy,filedata
@calc_loop:
; perform operation 
    ld hl,(iy+0) ; op1
    call f16_to_uq16_8
; write results to file buffer
    ld (iy+5),hl ; assembly result
; check for error
    ld de,(iy+2) ; python result
    or a ; clear carry
    sbc.s hl,de
    jr z,@next_record
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

@test_filename: asciz "../tests/f16_to_uq16_8.bin"
