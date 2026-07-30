; 3d_transform.vec:
;   +0  uint32 record count
;   repeated 32-byte records:
;     +0 opcode, +1 expected carry, +2 vec3 input, +8 auxiliary input,
;     +26 expected vec3 (sincos stores sine/cosine there)
;
; 3d_transform.result (72 bytes):
;   "P3T2", uint24 records, uint24 errors, 7*uint16 errors by opcode,
;   7*uint24 mixed-regression ms, 7*uint24 successful-call net ms,
;   uint24 plain-loop baseline ms, uint24 angle-load baseline ms

    assume adl=1
    org 0x040000
    jp start
    align 64
    db "MOS",0,1

    include "../agon/mos_api.inc"
    include "../agon/3d.inc"
    include "../agon/3d_sincos_table.inc"

record_size: equ 32
benchmark_iterations: equ 20000
benchmark_sincos_angle: equ -12345
benchmark_axis_angle: equ 16384
prt_reload_1ms: equ 72
TMR1_CTL: equ 083h
TMR_REG_CTL: equ 0
TMR_RES_LOW: equ 1
TMR_RES_HIGH: equ 2
PRT_OFF: equ %00001110
PRT_ON: equ %01011111

; All timed-call loops use exactly this counter tail. The two empty baselines
; use it too, allowing their PRT time to be subtracted from the API loops.
    MACRO P3D_BENCHMARK_TAIL loop_target
    ld hl,(benchmark_remaining)
    dec.s hl
    ld (benchmark_remaining),hl
    ld a,h
    or l
    jp nz,loop_target
    ENDMACRO

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
    ld de,0
    or a
    sbc hl,de
    jp z,@complete

    ld a,(iy+0)
    ld b,a
    ld a,(timed_op)
    cp b
    ld a,b
    call nz,timing_switch
    ld a,(iy+0)
    or a
    jp z,@sincos
    dec a
    jp z,@translate
    dec a
    jp z,@rotx
    dec a
    jp z,@roty
    dec a
    jp z,@rotz
    dec a
    jp z,@rotxyz
    dec a
    jp z,@mat3vec
    jp @failed

@sincos:
    di
    xor a
    ld (abi_failed),a
    call prime_preserved_shadow
    ld hl,0xa50000           ; poison UHL; API consumes only H:L
    ld l,(iy+2)
    ld h,(iy+3)
    call p3d_sincos16
    call capture_carry
    call check_preserved_shadow
    call compare_sincos
    ei
    jp @checked

@translate:
    di
    xor a
    ld (abi_failed),a
    call snapshot_aux
    call prime_preserved_shadow
    ld (record_ptr),iy
    lea ix,iy+2
    lea iy,iy+8
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_vec3_translate16
    call capture_carry
    call check_preserved_shadow
    call check_pointers
    ld iy,(record_ptr)
    call check_aux
    call compare_vec3
    ei
    jp @checked

@rotx:
    di
    xor a
    ld (abi_failed),a
    call snapshot_aux
    ld (record_ptr),iy
    lea ix,iy+2
    ld (expected_ix),ix
    ld (expected_iy),iy
    ld l,(iy+8)
    ld h,(iy+9)
    call p3d_vec3_rotate_x16
    call capture_carry
    call check_pointers
    ld iy,(record_ptr)
    call check_aux
    call compare_vec3
    ei
    jp @checked

@roty:
    di
    xor a
    ld (abi_failed),a
    call snapshot_aux
    ld (record_ptr),iy
    lea ix,iy+2
    ld (expected_ix),ix
    ld (expected_iy),iy
    ld l,(iy+8)
    ld h,(iy+9)
    call p3d_vec3_rotate_y16
    call capture_carry
    call check_pointers
    ld iy,(record_ptr)
    call check_aux
    call compare_vec3
    ei
    jp @checked

@rotz:
    di
    xor a
    ld (abi_failed),a
    call snapshot_aux
    ld (record_ptr),iy
    lea ix,iy+2
    ld (expected_ix),ix
    ld (expected_iy),iy
    ld l,(iy+8)
    ld h,(iy+9)
    call p3d_vec3_rotate_z16
    call capture_carry
    call check_pointers
    ld iy,(record_ptr)
    call check_aux
    call compare_vec3
    ei
    jp @checked

@rotxyz:
    di
    xor a
    ld (abi_failed),a
    call snapshot_aux
    ld (record_ptr),iy
    lea ix,iy+2
    lea iy,iy+8
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_vec3_rotate_xyz16
    call capture_carry
    call check_pointers
    ld iy,(record_ptr)
    call check_aux
    call compare_vec3
    ei
    jp @checked

@mat3vec:
    di
    xor a
    ld (abi_failed),a
    call snapshot_aux
    ld (record_ptr),iy
    lea ix,iy+2
    lea iy,iy+8
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_mul_vec3_16
    call capture_carry
    call check_pointers
    ld iy,(record_ptr)
    call check_aux
    call compare_vec3
    ei

@checked:
    jp z,@next
@failed:
    ld hl,(errors)
    inc hl
    ld (errors),hl
    ld a,(iy+0)
    add a,a
    ld e,a
    ld d,0
    inc de
    dec.s de
    ld hl,errors_by_op
    add hl,de
    ld e,(hl)
    inc hl
    ld d,(hl)
    dec hl
    inc.s de
    ld (hl),e
    inc hl
    ld (hl),d
@next:
    ld hl,(records)
    inc hl
    ld (records),hl
    lea iy,iy+record_size
    ld hl,(remaining)
    dec hl
    ld (remaining),hl
    ld hl,(batch_remaining)
    dec.s hl
    ld (batch_remaining),hl
    ld a,h
    or l
    jp nz,@record_loop
    ld hl,(remaining)
    ld de,0
    or a
    sbc hl,de
    jp z,@record_loop
    call read_batch
    jp @record_loop

@complete:
    call timing_finish
    ld a,(input_handle)
    ld c,a
    MOSCALL mos_fclose
    xor a
    ld (input_handle),a
    ld hl,(errors)
    ld de,0
    or a
    sbc hl,de
    call z,run_success_benchmarks
    call prt_irq_restore
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

; Read at most 4,096 complete 32-byte records into the RAM buffer.
read_batch:
    call prt_pause
    ld hl,(remaining)
    ld de,4096
    or a
    sbc hl,de
    jr c,@use_remaining
@use_max:
    ld hl,4096
    jr @count_ready
@use_remaining:
    ld hl,(remaining)
@count_ready:
    ld (batch_remaining),hl
    add hl,hl
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
    ld hl,0xffffff
    ld (errors),hl
    ld hl,0
    ld (remaining),hl
    ld (batch_remaining),hl
    ld iy,filedata
    call prt_resume
    ret

capture_carry:
    ld a,0
    adc a,0
    ld (actual_carry),a
    ret

; Preserve the record's entire 18-byte auxiliary field. This catches writes
; beyond the six-byte in-place destination as well as mutation of an IY input.
snapshot_aux:
    push iy
    pop hl
    ld de,8
    add hl,de
    ld de,aux_snapshot
    ld bc,18
    ldir
    ret

check_aux:
    push ix
    push iy
    pop hl
    ld de,8
    add hl,de
    ld ix,aux_snapshot
    ld b,18
@loop:
    ld a,(hl)
    cp (ix)
    jr nz,@failed
    inc hl
    inc ix
    djnz @loop
    pop ix
    ret
@failed:
    ld a,1
    ld (abi_failed),a
    pop ix
    ret

; Sine/cosine and translation promise to preserve all shadow GP registers.
; Load full 24-bit sentinels, then destructively inspect them after the API
; call while leaving the active result bank intact.
prime_preserved_shadow:
    exx
    ld bc,0x13579b
    ld de,0x2468ac
    ld hl,0x5aa55a
    exx
    ret

check_preserved_shadow:
    exx
    push bc
    push de
    push hl

    pop hl
    ld de,0x5aa55a
    or a
    sbc hl,de
    jr z,@de
    ld a,1
    ld (abi_failed),a
@de:
    pop hl
    ld de,0x2468ac
    or a
    sbc hl,de
    jr z,@bc
    ld a,1
    ld (abi_failed),a
@bc:
    pop hl
    ld de,0x13579b
    or a
    sbc hl,de
    jr z,@done
    ld a,1
    ld (abi_failed),a
@done:
    exx
    ret

check_pointers:
    push ix
    pop hl
    ld de,(expected_ix)
    or a
    sbc hl,de
    jr nz,@failed
    push iy
    pop hl
    ld de,(expected_iy)
    or a
    sbc hl,de
    ret z
@failed:
    ld a,1
    ld (abi_failed),a
    ret

compare_sincos:
    ld a,(abi_failed)
    or a
    ret nz
    ld a,(actual_carry)
    cp (iy+1)
    ret nz
    ld a,c
    cp (iy+26)
    ret nz
    ld a,b
    cp (iy+27)
    ret nz
    ld a,e
    cp (iy+28)
    ret nz
    ld a,d
    cp (iy+29)
    ret

compare_vec3:
    ld a,(abi_failed)
    or a
    ret nz
    ld a,(actual_carry)
    cp (iy+1)
    ret nz
    ld a,(iy+2)
    cp (iy+26)
    ret nz
    ld a,(iy+3)
    cp (iy+27)
    ret nz
    ld a,(iy+4)
    cp (iy+28)
    ret nz
    ld a,(iy+5)
    cp (iy+29)
    ret nz
    ld a,(iy+6)
    cp (iy+30)
    ret nz
    ld a,(iy+7)
    cp (iy+31)
    ret

; ---------------------------------------------------------------------------
; Successful-call timing
; ---------------------------------------------------------------------------
;
; The regression timing above deliberately follows the established fixture
; pattern and therefore includes setup, validation, and early overflow exits.
; These in-memory loops exercise guaranteed-success, invariant inputs. Each
; reported time has an otherwise identical empty loop subtracted, leaving the
; API CALL/RET and full success path. PRT quantization is below one millisecond
; because every measurement is one uninterrupted interval.

run_success_benchmarks:
    call benchmark_plain_baseline_run
    call benchmark_angle_baseline_run
    call benchmark_sincos_run
    call benchmark_translate_run
    call benchmark_rotx_run
    call benchmark_roty_run
    call benchmark_rotz_run
    call benchmark_rotxyz_run
    call benchmark_mat3vec_run
    ret

benchmark_plain_baseline_run:
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_plain_loop:
    di
    ei
    P3D_BENCHMARK_TAIL benchmark_plain_loop
    call benchmark_stop
    ld (benchmark_plain_baseline),hl
    ret

benchmark_angle_baseline_run:
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_angle_loop:
    ld hl,benchmark_sincos_angle
    di
    ei
    P3D_BENCHMARK_TAIL benchmark_angle_loop
    call benchmark_stop
    ld (benchmark_angle_baseline),hl
    ret

benchmark_sincos_run:
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_sincos_loop:
    ld hl,benchmark_sincos_angle
    di
    call p3d_sincos16
    ei
    P3D_BENCHMARK_TAIL benchmark_sincos_loop
    call benchmark_stop
    ld de,(benchmark_angle_baseline)
    or a
    sbc hl,de
    ld (successful_times_by_op+0),hl
    ret

benchmark_translate_run:
    ld ix,benchmark_vector
    ld iy,benchmark_zero_vector
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_translate_loop:
    di
    call p3d_vec3_translate16
    ei
    P3D_BENCHMARK_TAIL benchmark_translate_loop
    call benchmark_stop
    ld de,(benchmark_plain_baseline)
    or a
    sbc hl,de
    ld (successful_times_by_op+3),hl
    ret

benchmark_rotx_run:
    ld ix,benchmark_vector
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_rotx_loop:
    ld hl,benchmark_axis_angle
    di
    call p3d_vec3_rotate_x16
    ei
    P3D_BENCHMARK_TAIL benchmark_rotx_loop
    call benchmark_stop
    ld de,(benchmark_angle_baseline)
    or a
    sbc hl,de
    ld (successful_times_by_op+6),hl
    ret

benchmark_roty_run:
    ld ix,benchmark_vector
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_roty_loop:
    ld hl,benchmark_axis_angle
    di
    call p3d_vec3_rotate_y16
    ei
    P3D_BENCHMARK_TAIL benchmark_roty_loop
    call benchmark_stop
    ld de,(benchmark_angle_baseline)
    or a
    sbc hl,de
    ld (successful_times_by_op+9),hl
    ret

benchmark_rotz_run:
    ld ix,benchmark_vector
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_rotz_loop:
    ld hl,benchmark_axis_angle
    di
    call p3d_vec3_rotate_z16
    ei
    P3D_BENCHMARK_TAIL benchmark_rotz_loop
    call benchmark_stop
    ld de,(benchmark_angle_baseline)
    or a
    sbc hl,de
    ld (successful_times_by_op+12),hl
    ret

benchmark_rotxyz_run:
    ld ix,benchmark_vector
    ld iy,benchmark_half_turns
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_rotxyz_loop:
    di
    call p3d_vec3_rotate_xyz16
    ei
    P3D_BENCHMARK_TAIL benchmark_rotxyz_loop
    call benchmark_stop
    ld de,(benchmark_plain_baseline)
    or a
    sbc hl,de
    ld (successful_times_by_op+15),hl
    ret

benchmark_mat3vec_run:
    ld ix,benchmark_vector
    ld iy,benchmark_negative_identity
    ld hl,benchmark_iterations
    ld (benchmark_remaining),hl
    call prt_start
benchmark_mat3vec_loop:
    di
    call p3d_mat3_mul_vec3_16
    ei
    P3D_BENCHMARK_TAIL benchmark_mat3vec_loop
    call benchmark_stop
    ld de,(benchmark_plain_baseline)
    or a
    sbc hl,de
    ld (successful_times_by_op+18),hl
    ret

; Stop one benchmark interval and return its raw 24-bit milliseconds in HL.
benchmark_stop:
    di
    ld a,PRT_OFF
    out0 (TMR1_CTL+TMR_REG_CTL),a
    ld hl,(prt_irq_counter)
    ei
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
    asciz "\r\n3D transform regression\r\n"
    ld hl,(errors)
    ld de,0
    or a
    sbc hl,de
    jr nz,@fail
    call test_print_inline
    asciz "PASS\r\n"
    jr @counts
@fail:
    call test_print_inline
    asciz "FAIL - errors: "
    call test_print_u24
    call test_print_newline
@counts:
    call test_print_inline
    asciz "Records: "
    ld hl,(records)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "PRT milliseconds (includes validation loop):\r\n"

    call test_print_inline
    asciz "  sincos:     "
    ld hl,(times_by_op+0)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  translate:  "
    ld hl,(times_by_op+3)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate X:   "
    ld hl,(times_by_op+6)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate Y:   "
    ld hl,(times_by_op+9)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate Z:   "
    ld hl,(times_by_op+12)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate XYZ: "
    ld hl,(times_by_op+15)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  mat3*vec3:  "
    ld hl,(times_by_op+18)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "Successful call-only net PRT ms (20,000 calls):\r\n"

    call test_print_inline
    asciz "  sincos:     "
    ld hl,(successful_times_by_op+0)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  translate:  "
    ld hl,(successful_times_by_op+3)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate X:   "
    ld hl,(successful_times_by_op+6)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate Y:   "
    ld hl,(successful_times_by_op+9)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate Z:   "
    ld hl,(successful_times_by_op+12)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  rotate XYZ: "
    ld hl,(successful_times_by_op+15)
    call test_print_u24
    call test_print_newline

    call test_print_inline
    asciz "  mat3*vec3:  "
    ld hl,(successful_times_by_op+18)
    call test_print_u24
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

; Print unsigned 24-bit HL without leading zeroes.
test_print_u24:
    ld b,0
    ld de,10000000
    call test_print_digit
    ld de,1000000
    call test_print_digit
    ld de,100000
    call test_print_digit
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
    sbc hl,de
    jr c,@restore
    inc c
    jr @subtract
@restore:
    add hl,de
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

vector_filename: asciz "3d_transform.vec"
result_filename: asciz "3d_transform.result"
input_handle: db 0
output_handle: db 0
actual_carry: db 0
abi_failed: db 0
record_ptr: dl 0
expected_ix: dl 0
expected_iy: dl 0
expected_bytes: dl 0
remaining: dl 0
batch_remaining: dl 0
timed_op: db 0xff
benchmark_remaining: dl 0
prt_vector: dl 0
prt_saved_vector: blkb 4,0
prt_irq_counter: dl 0
aux_snapshot: blkb 18,0

; Every benchmark input is invariant under its repeated operation:
; half-turn axis rotations and the negative identity alternate exact signs,
; while the three half turns in XYZ compose to the identity.
benchmark_vector: dw 12000,-17000,21000
benchmark_zero_vector: dw 0,0,0
benchmark_half_turns: dw 16384,16384,16384
benchmark_negative_identity:
    dw -32767,0,0
    dw 0,-32767,0
    dw 0,0,-32767

result_data:
    db "P3T2"
records: dl 0
errors: dl 0
errors_by_op: blkb 14,0
times_by_op: blkb 21,0
successful_times_by_op: blkb 21,0
benchmark_plain_baseline: dl 0
benchmark_angle_baseline: dl 0
result_end:

    align 256
filedata: equ $
