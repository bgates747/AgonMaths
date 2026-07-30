; 3d_math.vec:
;   +0 uint32 record count
;   repeated 16-byte records:
;     +0 opcode, +1 expected carry, +2 left uint32, +6 right uint32,
;     +10 expected uint32, +14 padding
;
; 3d_math.result (38 bytes):
;   "P3D1", uint16 records, uint16 errors, 6*uint16 errors by opcode,
;   6*uint24 mixed-regression milliseconds

    assume adl=1
    org 0x040000
    jp start
    align 64
    db "MOS",0,1

    include "../agon/mos_api.inc"
    include "../agon/3d.inc"
    include "../agon/3d_sincos_table.inc"

record_size: equ 16
prt_reload_1ms: equ 72
TMR1_CTL: equ 083h
TMR_REG_CTL: equ 0
TMR_RES_LOW: equ 1
TMR_RES_HIGH: equ 2
PRT_OFF: equ %00001110
PRT_ON: equ %01011111

start:
    push af
    push bc
    push de
    push ix
    push iy
    call run_tests
    pop iy
    pop ix
    pop de
    pop bc
    pop af
    ld hl,0
    ret

run_tests:
    xor a
    ld hl,records
    ld (hl),a
    ld de,records+1
    ld bc,result_end-records-1
    ldir
    xor a
    ld (input_handle),a
    ld a,0xff
    ld (timed_op),a
    call prt_irq_install

    ld hl,vector_filename
    ld c,fa_read
    MOSCALL mos_fopen
    or a
    jp z,@open_failed
    ld (input_handle),a

    ld c,a
    ld hl,filedata
    ld de,4
    MOSCALL mos_fread
    ld hl,4
    or a
    sbc hl,de
    jp nz,@open_failed

    ld hl,(filedata)
    ld (remaining),hl
    call read_batch

@record_loop:
    ld hl,(remaining)
    ld a,h
    or l
    jp z,@complete

    ld a,(iy+0)
    ld b,a
    ld a,(timed_op)
    cp b
    ld a,b
    call nz,timing_switch
    ld a,(iy+0)
    or a
    jp z,@umul
    dec a
    jp z,@smul
    dec a
    jp z,@sadd
    dec a
    jp z,@ssub
    dec a
    jp z,@sdiv
    dec a
    jp z,@smul_norm
    jp @failed

@umul:
    di
    call load_words
    call p3d_umul16x16_32
    call capture_carry
    call compare32
    ei
    jp @checked

@smul:
    di
    call load_words
    call p3d_smul16x16_32
    call capture_carry
    call compare32
    ei
    jp @checked

@sadd:
    di
    call load_b32_active
    exx
    call load_a32_active
    exx
    call p3d_sadd32
    call capture_carry
    call compare32
    exx
    ei
    jp @checked

@ssub:
    di
    call load_b32_active
    exx
    call load_a32_active
    exx
    call p3d_ssub32
    call capture_carry
    call compare32
    exx
    ei
    jp @checked

@sdiv:
    di
    call load_a32_active
    call p3d_sdiv32767_32_to_16
    call capture_carry
    call compare16
    ei
    jp @checked

@smul_norm:
    di
    call load_words
    call p3d_smul_norm16
    call capture_carry
    call compare16
    ei

@checked:
    jp z,@next
@failed:
    ld hl,(errors)
    inc.s hl
    ld (errors),hl
    ld a,(iy+0)
    add a,a
    ld e,a
    ld d,0
    ld hl,errors_by_op
    add.s hl,de
    ld e,(hl)
    inc.s hl
    ld d,(hl)
    dec.s hl
    inc.s de
    ld (hl),e
    inc.s hl
    ld (hl),d
@next:
    ld hl,(records)
    inc.s hl
    ld (records),hl
    lea iy,iy+record_size
    ld hl,(remaining)
    dec.s hl
    ld (remaining),hl
    ld hl,(batch_remaining)
    dec.s hl
    ld (batch_remaining),hl
    ld a,h
    or l
    jp nz,@record_loop
    ld hl,(remaining)
    ld a,h
    or l
    jp z,@record_loop
    call read_batch
    jp @record_loop

@complete:
    call timing_finish
    call prt_irq_restore
    ld a,(input_handle)
    ld c,a
    MOSCALL mos_fclose
    xor a
    ld (input_handle),a
    call write_result
    call print_report
    ret

@open_failed:
    ld a,(input_handle)
    or a
    jr z,@no_input
    ld c,a
    MOSCALL mos_fclose
    xor a
    ld (input_handle),a
@no_input:
    call prt_irq_restore
    ld hl,0xffff
    ld (errors),hl
    call write_result
    call print_report
    ret

; Read at most 10,000 complete records into the RAM buffer.
read_batch:
    call prt_pause
    ld hl,(remaining)
    ld de,10000
    or a
    sbc.s hl,de
    jr c,@use_remaining
    jr z,@use_remaining
    ld hl,10000
    jr @count_ready
@use_remaining:
    ld hl,(remaining)
@count_ready:
    ld (batch_remaining),hl
    add hl,hl
    add hl,hl
    add hl,hl
    add hl,hl
    ex de,hl
    ld (expected_bytes),de
    ld a,(input_handle)
    ld c,a
    ld hl,filedata
    MOSCALL mos_fread
    ld hl,(expected_bytes)
    or a
    sbc hl,de
    jr nz,@short_read
    ld iy,filedata
    call prt_resume
    ret
@short_read:
    ld hl,0xffff
    ld (errors),hl
    ld hl,0
    ld (remaining),hl
    ld (batch_remaining),hl
    ld iy,filedata
    call prt_resume
    ret

load_words:
    ld l,(iy+2)
    ld h,(iy+3)
    ld e,(iy+6)
    ld d,(iy+7)
    ret

load_a32_active:
    ld e,(iy+2)
    ld d,(iy+3)
    ld l,(iy+4)
    ld h,(iy+5)
    ret

load_b32_active:
    ld e,(iy+6)
    ld d,(iy+7)
    ld l,(iy+8)
    ld h,(iy+9)
    ret

capture_carry:
    ld a,0
    adc a,0
    ld (actual_carry),a
    ret

compare32:
    ld a,(actual_carry)
    cp (iy+1)
    ret nz
    ld a,e
    cp (iy+10)
    ret nz
    ld a,d
    cp (iy+11)
    ret nz
    ld a,l
    cp (iy+12)
    ret nz
    ld a,h
    cp (iy+13)
    ret

compare16:
    ld a,(actual_carry)
    cp (iy+1)
    ret nz
    or a
    ret nz
    ld a,l
    cp (iy+10)
    ret nz
    ld a,h
    cp (iy+11)
    ret

; Start a new per-op timing group. A is the new opcode.
timing_switch:
    push af
    call timing_finish
    pop af
    ld (timed_op),a
    call prt_start
    ret

; Stop and save the active group's elapsed millisecond count.
timing_finish:
    ld a,(timed_op)
    cp 0xff
    ret z
    di
    ld a,PRT_OFF
    out0 (TMR1_CTL+TMR_REG_CTL),a
    ld hl,(prt_irq_counter)
    ei
    ld a,(timed_op)
    ld e,a
    add a,a
    add a,e
    ld e,a
    ld d,0
    inc de
    dec.s de
    ld ix,times_by_op
    add ix,de
    ld (ix),hl
    ret

prt_pause:
    ld a,(timed_op)
    cp 0xff
    ret z
    di
    ld a,PRT_OFF
    out0 (TMR1_CTL+TMR_REG_CTL),a
    in0 a,(TMR1_CTL+TMR_REG_CTL)
    ei
    ret

prt_resume:
    ld a,(timed_op)
    cp 0xff
    ret z
    di
    ld hl,prt_reload_1ms
    out0 (TMR1_CTL+TMR_RES_LOW),l
    out0 (TMR1_CTL+TMR_RES_HIGH),h
    ld a,PRT_ON
    out0 (TMR1_CTL+TMR_REG_CTL),a
    ei
    ret

prt_start:
    di
    ld hl,0
    ld (prt_irq_counter),hl
    ld hl,prt_reload_1ms
    out0 (TMR1_CTL+TMR_RES_LOW),l
    out0 (TMR1_CTL+TMR_RES_HIGH),h
    ld a,PRT_OFF
    out0 (TMR1_CTL+TMR_REG_CTL),a
    ld a,PRT_ON
    out0 (TMR1_CTL+TMR_REG_CTL),a
    ei
    ret

; Save and replace the timer interrupt hook in MOS vector table 2.
prt_irq_install:
    di
    ld hl,0
    ld a,($10c)
    ld l,a
    ld a,($10d)
    ld h,a
    inc hl
    ld hl,(hl)
    ld (prt_vector),hl
    ld a,(hl)
    ld (prt_saved_vector+0),a
    inc hl
    ld a,(hl)
    ld (prt_saved_vector+1),a
    inc hl
    ld a,(hl)
    ld (prt_saved_vector+2),a
    inc hl
    ld a,(hl)
    ld (prt_saved_vector+3),a
    ld hl,(prt_vector)
    ld (hl),0xc3
    inc hl
    ld de,prt_irq_handler
    ld (hl),de
    ei
    ret

prt_irq_restore:
    di
    ld a,PRT_OFF
    out0 (TMR1_CTL+TMR_REG_CTL),a
    ld hl,(prt_vector)
    ld a,(prt_saved_vector+0)
    ld (hl),a
    inc hl
    ld a,(prt_saved_vector+1)
    ld (hl),a
    inc hl
    ld a,(prt_saved_vector+2)
    ld (hl),a
    inc hl
    ld a,(prt_saved_vector+3)
    ld (hl),a
    ei
    ret

prt_irq_handler:
    di
    push af
    push hl
    in0 a,(TMR1_CTL+TMR_REG_CTL)
    ld hl,(prt_irq_counter)
    inc hl
    ld (prt_irq_counter),hl
    pop hl
    pop af
    ei
    reti.l

write_result:
    ld hl,result_filename
    ld c,fa_write | fa_create_always
    MOSCALL mos_fopen
    or a
    ret z
    ld (output_handle),a
    ld c,a
    ld hl,result_data
    ld de,result_end-result_data
    MOSCALL mos_fwrite
    ld a,(output_handle)
    ld c,a
    MOSCALL mos_fclose
    ret

print_report:
    call test_print_inline
    asciz "\r\n3D integer maths regression\r\n"
    ld hl,(errors)
    ld a,h
    or l
    jr nz,@fail
    call test_print_inline
    asciz "PASS\r\n"
    jr @counts
@fail:
    call test_print_inline
    asciz "FAIL - errors: "
    call test_print_u16
    call test_print_newline
@counts:
    call test_print_inline
    asciz "Records: "
    ld hl,(records)
    call test_print_u16
    call test_print_newline
    call test_print_inline
    asciz "PRT milliseconds (includes validation loop):\r\n"

    call test_print_inline
    asciz "  umul:       "
    ld hl,(times_by_op+0)
    call test_print_u16
    call test_print_newline

    call test_print_inline
    asciz "  smul:       "
    ld hl,(times_by_op+3)
    call test_print_u16
    call test_print_newline

    call test_print_inline
    asciz "  sadd:       "
    ld hl,(times_by_op+6)
    call test_print_u16
    call test_print_newline

    call test_print_inline
    asciz "  ssub:       "
    ld hl,(times_by_op+9)
    call test_print_u16
    call test_print_newline

    call test_print_inline
    asciz "  sdiv32767:  "
    ld hl,(times_by_op+12)
    call test_print_u16
    call test_print_newline

    call test_print_inline
    asciz "  smul_norm:  "
    ld hl,(times_by_op+15)
    call test_print_u16
    call test_print_newline
    ret

test_print_inline:
    pop hl
    call test_print_string
    push hl
    ret

test_print_string:
    ld a,(hl)
    inc hl
    or a
    ret z
    rst.lil 10h
    jr test_print_string

test_print_newline:
    ld a,13
    rst.lil 10h
    ld a,10
    rst.lil 10h
    ret

; Print unsigned low 16 bits of HL without leading zeroes.
test_print_u16:
    ld b,0
    ld de,10000
    call test_print_digit
    ld de,1000
    call test_print_digit
    ld de,100
    call test_print_digit
    ld de,10
    call test_print_digit
    ld a,l
    add a,'0'
    rst.lil 10h
    ret

test_print_digit:
    ld c,'0'
@subtract:
    or a
    sbc.s hl,de
    jr c,@restore
    inc c
    jr @subtract
@restore:
    add.s hl,de
    ld a,c
    cp '0'
    jr nz,@emit
    ld a,b
    or a
    ret z
    ld a,c
@emit:
    ld b,1
    rst.lil 10h
    ret

vector_filename: asciz "3d_math.vec"
result_filename: asciz "3d_math.result"
input_handle: db 0
output_handle: db 0
actual_carry: db 0
expected_bytes: dl 0
remaining: dl 0
batch_remaining: dl 0
timed_op: db 0xff
prt_vector: dl 0
prt_saved_vector: blkb 4,0
prt_irq_counter: dl 0

result_data:
    db "P3D1"
records: dw 0
errors: dw 0
errors_by_op: blkb 12,0
times_by_op: blkb 18,0
result_end:

    align 256
filedata: equ $
