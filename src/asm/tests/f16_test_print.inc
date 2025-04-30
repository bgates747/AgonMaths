test_f16_print:
; copy test filename
    ld hl,@test_filename
    ld de,test_filename
    call test_copy_filename

; do the test
    call printInline
    asciz "\r\nf16_print test\r\n"
    ld hl,64
    ld (bytes_per_record),hl
    ld hl,262144
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
    push iy ; file buffer pointer
    call f16_print ; hl points to result string
; ; print result
;     call printString
;     call printNewLine
    pop iy ; restore file buffer pointer
; write results to file buffer
    push iy ; stack file buffer pointer
    push hl ; stack result string pointer
    lea iy,iy+33 ; move file pointer to output
    push iy ; copy output pointer 
    pop de ; to de
    push de ; stack output pointer
    ld bc,31 ; bytes to copy
    ldir ; copy string buffer to file buffer
; check for error
    pop de ; restore output pointer
    pop hl ; restore result string pointer
    ld b,31 ; bytes to compare
@error_loop:
    ld a,(de)
    or (hl)
    jr z,@next_record
    ld a,(de)
    cp (hl)
    jr nz,@count_error
    djnz @error_loop
    jr @next_record
@count_error:
; bump error count
    ld hl,(errors)
    inc hl
    ld (errors),hl
@next_record:
    pop iy ; restore file buffer pointer
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

@test_filename: asciz "../scratch/tests/f16_print_all.bin"
