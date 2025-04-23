errors: dl 0
records: dl 0
counter: dl 0
time: dl 0
bytes_read: dl 0
bytes_read_max: dl 0
bytes_per_record: dl 0

test_init:
; set up counters
    ld hl,0 ; error counter
    ld (errors),hl ; error counter
    ld (records),hl ; record counter
    ld (time),hl ; time counter
; open file for reading
    call open_testfile_read
    or a
    jr nz,@open_writefile
    call printInline
    asciz "Error opening file for reading\r\n"
    ld hl,test_filename
    call printString
    call printNewLine
    jr @error
@open_writefile:
; open file for writing
    call open_testfile_write
    or a
    ret nz
    call printInline
    asciz "Error opening file for writing\r\n"
    ld hl,test_filename
    call printString
    call printNewLine
@error:
    xor a ; return zero indicating error
    ret

test_read_data:
; read data from file
    ld a,(testfile_read_handle)
    ld c,a
    ld hl,filedata
    ld de,(bytes_read_max) ; max bytes to read
    MOSCALL mos_fread
    ld (bytes_read),de
    ex de,hl
    SIGN_UHL
    ret z
; compute number of records in batch
    push hl
    ld de,(bytes_per_record) ; bytes per record
    call udiv24
    ld (counter),de ; record counter
    ex de,hl
    call printDec
    call printInline
    asciz " records in batch"
    pop hl
; output bytes read
    call printDec
    call printInline
    asciz " bytes read"
; start stopwatch
    call vdu_flip
    call stopwatch_set
; reset data pointer
    ld ix,filedata
    ret

test_write_data:
; get elapsed time
    call stopwatch_get
    ld de,(time)
    add hl,de
    ld (time),hl
; write data to file
    ld a,(testfile_write_handle)
    ld c,a
    ld hl,filedata
    ld de,(bytes_read) ; bytes to write
    MOSCALL mos_fwrite
    ex de,hl
    call printDec
    call printInline
    asciz " bytes written\r\n"
    ret

test_complete:
; close files
    call close_testfile_read
    call close_testfile_write
; report elapsed time
    ld hl,(time)
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    ld de,120*256 ; ticks per second in 16.8 fixed point
    call udiv168
    call print_s168_de
    call printInline
    asciz " seconds elapsed\r\n"
; display error count
    ld hl,(errors)
    call printDec
    call printInline
    asciz " errors\r\n"
; display record count
    ld hl,(records)
    call printDec
    call printInline
    asciz " records\r\n"
    ret

; copy a zero-terminated filname to the target address
; inputs: hl = pointer to filename, de = target address
; returns: hl = pointer to end of filename, de = pointer to end of target address, bc=0, a=0
; destroys: af, hl, de, bc
test_copy_filename:
    push hl ; save filename pointer
    ld bc,0 ; init byte counter
@loop:
    inc hl ; assume filename is at least one character
    inc bc ; ditto
    ld a,(hl)
    or a
    jr nz,@loop
    pop hl ; get back filename pointer
    ldir ; copy the filename to target address

open_testfile_read:
; open file for reading
    ld hl,test_filename
    ld c,fa_read
    MOSCALL mos_fopen
    ld (testfile_read_handle),a
    ret

open_testfile_write:
; open file for writing
    ld hl,test_filename
    ld c,fa_write | fa_open_existing
    MOSCALL mos_fopen
    ld (testfile_write_handle),a
    ret

close_testfile_read:
    ld a,(testfile_read_handle)
    ld c,a
    MOSCALL mos_fclose
    ret

close_testfile_write:
    ld a,(testfile_write_handle)
    ld c,a
    MOSCALL mos_fclose
    ret

testfile_read_handle: db 0
testfile_write_handle: db 0

test_fil: equ $
test_filinfo: equ test_fil + fil_struct_size

test_fil_out: equ test_filinfo + filinfo_struct_size
test_filinfo_out: equ test_fil_out + fil_struct_size

    align 256

test_filename: blkb 256,0

; this must be the final label included in the application 
; so that data written here doesn't stomp on other code or data
filedata: equ $