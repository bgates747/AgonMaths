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
; ; DEBUG
;     call printNewLine
;     push iy
;     pop hl
;     ld hl,(hl)
;     call printHexHL
;     ld a,' '
;     rst.lil 10h
; ; END DEBUG

; ; DEBUG
;     push iy
;     pop hl
;     inc hl
;     inc hl
;     call printString
;     ld a,' '
;     rst.lil 10h
; ; END DEBUG

; perform operation 
    ld hl,(iy+0) ; op1
    push iy
    call f16_print
    pop iy

; ; DEBUG
;     push hl
;     call printString
;     pop hl
; ; END DEBUG

; ; DEBUG
;     call waitKeypress
; ; END DEBUG

    push iy
    push hl
; write results to file buffer
    lea iy,iy+33
    push iy
    pop de
    push de
    ld bc,31
    ldir
; check for error
    pop de
    pop hl
    ld b,31
@error_loop:
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
; write results to file buffer
    pop iy
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
