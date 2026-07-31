; 3d_pose.vec:
;   +0  uint32 record count
;   repeated 224-byte records:
;     +0 opcode, +1 expected carry, +2 alias mode, +3 header canary
;     +4 primary/input object-state slot (64 bytes)
;     +68 auxiliary slot (64 bytes)
;     +132 expected primary object-state slot (64 bytes)
;     +196 expected direct output (20 bytes)
;     +216 tail canary (8 bytes)
;
; 3d_pose.result (214 bytes):
;   "P3P3", uint24 records, uint24 errors, 23*uint16 errors by opcode,
;   23*uint24 mixed-regression milliseconds, then first-failure diagnostics:
;   uint24 record index, opcode, actual carry, 64-byte primary, 20-byte output.
;
; Public contracts exercised by this fixture:
;   p3d_mat3_mul_mat3_16       IX=A in/out, IY=B, A := A*B
;   p3d_mat3_to_euler16        IX=matrix, IY=Euler output
;   p3d_mat3_rotate_local_*16  IX=matrix, HL=axis angle / IY=XYZ angles
;   p3d_object_translate_local16 IX=state, IY=local delta
;   p3d_object_rotate_local_*16 IX=state, HL=axis angle / IY=XYZ angles
;   p3d_object_step16          IX=state, uses persistent velocities
;   p3d_object_init16          IX=state, IY=sid/oid/mid/bmid words
;   p3d_atan2_16               HL=Y, DE=X, result HL
;   p3d_asin16                 HL=normalized sine, result HL
;   p3d_mat3_from_euler16      IX=matrix output, IY=Euler input
;   p3d_object_build_create16  IX=state, IY=15-byte packet scratch
;   p3d_object_build_next_dirty16 IX=state, IY=15-byte packet scratch
;   p3d_camera_init16          IX=camera, IY=position, HL=scene ID
;   p3d_camera_aim_at_point16  IX=camera, IY=target point, A=roll policy
;   p3d_camera_aim_at_object16 IX=camera, IY=object state, A=roll policy
;   p3d_atan2_fine16           HL=Y, DE=X, result HL
;   p3d_sincos_fine16          HL=angle, results BC=sin and DE=cos
;
; All APIs preserve IX/IY and use carry to report a transactional failure.

    assume adl=1
    org 0x040000
    jp start
    align 64
    db "MOS",0,1

    include "../agon/mos_api.inc"
    include "../agon/3d.inc"
    include "../agon/3d_sincos_table.inc"

record_size: equ 224
batch_max_records: equ 255
op_count: equ 23

record_primary: equ 4
record_aux: equ 68
record_expected_primary: equ 132
record_expected_output: equ 196
record_tail: equ 216
state_size: equ p3d_object_size
state_orientation: equ p3d_object_orientation

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
    ld de,0
    or a
    sbc hl,de
    jp z,@complete

    ld a,(iy+0)
    cp op_count
    jp nc,@failed
    ld b,a
    ld a,(timed_op)
    cp b
    ld a,b
    call nz,timing_switch

    di
    xor a
    ld (abi_failed),a
    ld (check_return_hl),a
    ld (record_ptr),iy
    call snapshot_aux
    call poison_direct_output

    ld a,(iy+0)
    or a
    jp z,@mat3_mul
    dec a
    jp z,@mat3_to_euler
    dec a
    jp z,@mat3_rotx
    dec a
    jp z,@mat3_roty
    dec a
    jp z,@mat3_rotz
    dec a
    jp z,@mat3_rotxyz
    dec a
    jp z,@object_translate
    dec a
    jp z,@object_rotx
    dec a
    jp z,@object_roty
    dec a
    jp z,@object_rotz
    dec a
    jp z,@object_rotxyz
    dec a
    jp z,@object_step
    dec a
    jp z,@object_init
    dec a
    jp z,@atan2
    dec a
    jp z,@asin
    dec a
    jp z,@mat3_from_euler
    dec a
    jp z,@object_build_create
    dec a
    jp z,@object_build_next_dirty
    dec a
    jp z,@camera_init
    dec a
    jp z,@camera_aim_point
    dec a
    jp z,@camera_aim_object
    dec a
    jp z,@atan2_fine
    dec a
    jp z,@sincos_fine
    jp @api_failed

@mat3_mul:
    lea ix,iy+record_primary+state_orientation
    ld a,(iy+2)
    or a
    jr z,@mat3_mul_distinct
    push ix
    pop iy
    jr @mat3_mul_ready
@mat3_mul_distinct:
    lea iy,iy+record_aux+state_orientation
@mat3_mul_ready:
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_mul_mat3_16
    jp @api_done

@mat3_to_euler:
    lea ix,iy+record_primary+state_orientation
    ld iy,direct_output
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_to_euler16
    jp @api_done

@mat3_rotx:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary+state_orientation
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_rotate_local_x16
    jp @api_done

@mat3_roty:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary+state_orientation
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_rotate_local_y16
    jp @api_done

@mat3_rotz:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary+state_orientation
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_rotate_local_z16
    jp @api_done

@mat3_rotxyz:
    lea ix,iy+record_primary+state_orientation
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_rotate_local_xyz16
    jp @api_done

@object_translate:
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_translate_local16
    jp @api_done

@object_rotx:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_rotate_local_x16
    jp @api_done

@object_roty:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_rotate_local_y16
    jp @api_done

@object_rotz:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_rotate_local_z16
    jp @api_done

@object_rotxyz:
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_rotate_local_xyz16
    jp @api_done

@object_step:
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_step16
    jp @api_done

@object_init:
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_init16
    jp @api_done

@atan2:
    ld hl,0xa50000
    ld de,0x5a0000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    ld e,(iy+record_aux+2)
    ld d,(iy+record_aux+3)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_atan2_16
    ld a,l
    ld (direct_output+0),a
    ld a,h
    ld (direct_output+1),a
    jp @api_done

@asin:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_asin16
    ld a,l
    ld (direct_output+0),a
    ld a,h
    ld (direct_output+1),a
    jp @api_done

@mat3_from_euler:
    lea ix,iy+record_primary+state_orientation
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_mat3_from_euler16
    jp @api_done

@object_build_create:
    lea ix,iy+record_primary
    ld iy,direct_output
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_build_create16
    ld (builder_return_hl),hl
    ld (iy+15),a
    ld a,c
    ld (iy+16),a
    ld a,b
    ld (iy+17),a
    ld a,1
    ld (check_return_hl),a
    jp @api_done

@object_build_next_dirty:
    lea ix,iy+record_primary
    ld iy,direct_output
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_object_build_next_dirty16
    ld (builder_return_hl),hl
    ld (iy+15),a
    ld a,c
    ld (iy+16),a
    ld a,b
    ld (iy+17),a
    ld a,1
    ld (check_return_hl),a
    jp @api_done

@camera_init:
    ld a,(iy+2)
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary
    or a
    jr nz,@camera_init_alias
    lea iy,iy+record_aux+2
    jr @camera_init_ready
@camera_init_alias:
    lea iy,ix+p3d_camera_world_position
@camera_init_ready:
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_camera_init16
    jp @api_done

@camera_aim_point:
    ld a,(iy+record_aux+6)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_camera_aim_at_point16
    jp @api_done

@camera_aim_object:
    ld a,(iy+record_aux+p3d_object_reserved)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_camera_aim_at_object16
    jp @api_done

@atan2_fine:
    ld hl,0xa50000
    ld de,0x5a0000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    ld e,(iy+record_aux+2)
    ld d,(iy+record_aux+3)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_atan2_fine16
    ld a,l
    ld (direct_output+0),a
    ld a,h
    ld (direct_output+1),a
    jp @api_done

@sincos_fine:
    ld hl,0xa50000
    ld l,(iy+record_aux)
    ld h,(iy+record_aux+1)
    lea ix,iy+record_primary
    lea iy,iy+record_aux
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_sincos_fine16
    ld a,c
    ld (direct_output+0),a
    ld a,b
    ld (direct_output+1),a
    ld a,e
    ld (direct_output+2),a
    ld a,d
    ld (direct_output+3),a

@api_done:
    call capture_carry
    call check_pointers
    call check_builder_return_hl
    ld iy,(record_ptr)
    call compare_case
    ei
    jp z,@next
    jp @failed

@api_failed:
    scf
    call capture_carry
    ld a,1
    ld (abi_failed),a
    ld iy,(record_ptr)
    ei

@failed:
    ld hl,(errors)
    ld de,0
    or a
    sbc hl,de
    call z,capture_first_failure
    ld hl,(errors)
    inc hl
    ld (errors),hl
    ld a,(iy+0)
    cp op_count
    jr nc,@next
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
    lea iy,iy+112
    lea iy,iy+112
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
    ld hl,0xffffff
    ld (errors),hl
    call write_result
    call print_report
    ret

; Read at most 255 complete records (57,120 bytes) into the RAM buffer.
read_batch:
    call prt_pause
    ld hl,(remaining)
    ld de,batch_max_records
    or a
    sbc hl,de
    jr c,@use_remaining
    ld hl,batch_max_records
    jr @count_ready
@use_remaining:
    ld hl,(remaining)
@count_ready:
    ld (batch_remaining),hl
    ld a,l
    ld d,a
    ld e,record_size
    mlt de
    inc de
    dec.s de
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

snapshot_aux:
    push iy
    pop hl
    ld de,record_aux
    add hl,de
    ld de,aux_snapshot
    ld bc,state_size
    ldir
    ret

poison_direct_output:
    ld hl,direct_output
    ld (hl),0xd7
    ld de,direct_output+1
    ld bc,19
    ldir
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

check_builder_return_hl:
    ld a,(check_return_hl)
    or a
    ret z
    ld hl,(builder_return_hl)
    ld de,(expected_iy)
    or a
    sbc hl,de
    ret z
    ld a,1
    ld (abi_failed),a
    ret

compare_case:
    ld a,(abi_failed)
    or a
    ret nz
    ld a,(actual_carry)
    cp (iy+1)
    ret nz
    call compare_primary
    ret nz
    call compare_aux
    ret nz
    call compare_direct_output
    ret nz
    call compare_canaries
    ret

compare_primary:
    push ix
    push iy
    pop hl
    ld de,record_primary
    add hl,de
    push iy
    pop ix
    ld de,record_expected_primary
    add ix,de
    ld b,state_size
@loop:
    ld a,(hl)
    cp (ix)
    jr nz,@done
    inc hl
    inc ix
    djnz @loop
@done:
    pop ix
    ret

compare_aux:
    push ix
    push iy
    pop hl
    ld de,record_aux
    add hl,de
    ld ix,aux_snapshot
    ld b,state_size
@loop:
    ld a,(hl)
    cp (ix)
    jr nz,@done
    inc hl
    inc ix
    djnz @loop
@done:
    pop ix
    ret

compare_direct_output:
    push ix
    push iy
    pop ix
    ld de,record_expected_output
    add ix,de
    ld hl,direct_output
    ld b,20
@loop:
    ld a,(hl)
    cp (ix)
    jr nz,@done
    inc hl
    inc ix
    djnz @loop
@done:
    pop ix
    ret

compare_canaries:
    ld a,(iy+3)
    cp 0xa5
    ret nz
    push ix
    push iy
    pop hl
    ld de,record_tail
    add hl,de
    ld ix,tail_canary
    ld b,8
@loop:
    ld a,(hl)
    cp (ix)
    jr nz,@done
    inc hl
    inc ix
    djnz @loop
@done:
    pop ix
    ret nz
    ld hl,direct_guard_before
    ld ix,direct_guard_expected
    ld b,8
@direct_before:
    ld a,(hl)
    cp (ix)
    ret nz
    inc hl
    inc ix
    djnz @direct_before
    ld hl,direct_guard_after
    ld ix,direct_guard_expected+8
    ld b,8
@direct_after:
    ld a,(hl)
    cp (ix)
    ret nz
    inc hl
    inc ix
    djnz @direct_after
    ret

capture_first_failure:
    ld hl,(records)
    ld (first_error_record),hl
    ld a,(iy+0)
    ld (first_error_opcode),a
    ld a,(actual_carry)
    ld (first_error_carry),a
    push iy
    pop hl
    ld de,record_primary
    add hl,de
    ld de,first_error_primary
    ld bc,state_size
    ldir
    ld hl,direct_output
    ld de,first_error_output
    ld bc,20
    ldir
    ret

; Start a new per-op timing group. A is the new opcode.
timing_switch:
    push af
    call timing_finish
    pop af
    ld (timed_op),a
    call prt_start
    ret

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
    asciz "\r\n3D pose regression\r\n"
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
    asciz "PRT milliseconds (file I/O excluded):\r\n"

    call test_print_inline
    asciz "  mat3*mat3:       "
    ld hl,(times_by_op+0)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  mat3->Euler:     "
    ld hl,(times_by_op+3)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  mat local X:     "
    ld hl,(times_by_op+6)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  mat local Y:     "
    ld hl,(times_by_op+9)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  mat local Z:     "
    ld hl,(times_by_op+12)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  mat local XYZ:   "
    ld hl,(times_by_op+15)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  object move:     "
    ld hl,(times_by_op+18)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  object local X:  "
    ld hl,(times_by_op+21)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  object local Y:  "
    ld hl,(times_by_op+24)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  object local Z:  "
    ld hl,(times_by_op+27)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  object local XYZ:"
    ld hl,(times_by_op+30)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  object step:     "
    ld hl,(times_by_op+33)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  object init:     "
    ld hl,(times_by_op+36)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  atan2:           "
    ld hl,(times_by_op+39)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  asin:            "
    ld hl,(times_by_op+42)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  Euler->mat3:     "
    ld hl,(times_by_op+45)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  build create:    "
    ld hl,(times_by_op+48)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  build next dirty:"
    ld hl,(times_by_op+51)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  camera init:     "
    ld hl,(times_by_op+54)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  camera aim point:"
    ld hl,(times_by_op+57)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  camera aim obj:  "
    ld hl,(times_by_op+60)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  fine atan2:      "
    ld hl,(times_by_op+63)
    call test_print_u24
    call test_print_newline
    call test_print_inline
    asciz "  fine sin/cos:    "
    ld hl,(times_by_op+66)
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

vector_filename: asciz "3d_pose.vec"
result_filename: asciz "3d_pose.result"
input_handle: db 0
output_handle: db 0
actual_carry: db 0
abi_failed: db 0
check_return_hl: db 0
record_ptr: dl 0
expected_ix: dl 0
expected_iy: dl 0
builder_return_hl: dl 0
expected_bytes: dl 0
remaining: dl 0
batch_remaining: dl 0
timed_op: db 0xff
prt_vector: dl 0
prt_saved_vector: blkb 4,0
prt_irq_counter: dl 0
aux_snapshot: blkb state_size,0
direct_guard_before:
    db 0x91,0x82,0x73,0x64,0x55,0x46,0x37,0x28
direct_output: blkb 20,0xd7
direct_guard_after:
    db 0x19,0x2a,0x3b,0x4c,0x5d,0x6e,0x7f,0x80
direct_guard_expected:
    db 0x91,0x82,0x73,0x64,0x55,0x46,0x37,0x28
    db 0x19,0x2a,0x3b,0x4c,0x5d,0x6e,0x7f,0x80
tail_canary:
    db 0x5a,0x5b,0x58,0x59,0x5e,0x5f,0x5c,0x5d

result_data:
    db "P3P3"
records: dl 0
errors: dl 0
errors_by_op: blkb 46,0
times_by_op: blkb 69,0
first_error_record: dl 0
first_error_opcode: db 0
first_error_carry: db 0
first_error_primary: blkb state_size,0
first_error_output: blkb 20,0
result_end:

    align 256
filedata: equ $
