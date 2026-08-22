; 3d_fsim.vec:
;   +0  "FSV1", +4 uint32 record count
;   repeated 64-byte records:
;     +0 opcode, +1 expected carry, +2 carry-check flag, +3 case tag
;     +4 operation-specific input, +40 expected output, +60 tail sentinel
;
; 3d_fsim.result (69 bytes):
;   "P3F1", uint24 records, uint24 errors, 11*uint24 errors by opcode,
;   uint24 first-error record, uint8 first opcode, uint8 reason bitmap,
;   uint8 actual carry, and 20 bytes of first-error actual output.
;
; Failure-reason bits:
;   0 value, 1 carry, 2 active/index register ABI, 3 shadow-register ABI,
;   4 source/canary mutation, 5 packet-builder return registers.

    assume adl=1
    org 0x040000
    jp start
    align 64
    db "MOS",0,1

    include "../agon/mos_api.inc"
    include "../agon/3d.inc"
    include "../agon/3d_sincos_table.inc"
    include "../agon/3d_fsim.inc"

record_size: equ 64
batch_records: equ 1024
op_count: equ 11

reason_value: equ 0x01
reason_carry: equ 0x02
reason_register: equ 0x04
reason_shadow: equ 0x08
reason_source: equ 0x10
reason_return: equ 0x20

op_damp_rate: equ 0
op_clamp_rate: equ 1
op_apply_axis_moment: equ 2
op_drag_q8: equ 3
op_clamp_velocity: equ 4
op_transpose: equ 5
op_local_point_world: equ 6
op_horizon: equ 7
op_build_active_51: equ 8
op_build_translation_52: equ 9
op_build_projection_53: equ 10

active_bc_sentinel: equ 0x13579b
active_de_sentinel: equ 0x2468ac
active_hl_sentinel: equ 0x5aa55a
active_ix_sentinel: equ 0x314159
active_iy_sentinel: equ 0x265358
shadow_bc_sentinel: equ 0x0b1c2d
shadow_de_sentinel: equ 0x3e4f50
shadow_hl_sentinel: equ 0x617283
; PUSH AF in ADL mode stores 00:A:F, so POP HL produces UHL:A:F.
; XOR A followed by SCF establishes F'=Z|P/V|C = 0x45 before A'=0xa7.
shadow_a_sentinel: equ 0xa7
shadow_af_sentinel: equ 0x00a745

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
    ld (input_handle),a

    ld hl,vector_filename
    ld c,fa_read
    MOSCALL mos_fopen
    or a
    jp z,open_failed
    ld (input_handle),a

    ld c,a
    ld hl,filedata
    ld de,8
    MOSCALL mos_fread
    ld hl,8
    or a
    sbc hl,de
    jp nz,open_failed
    ld a,(filedata+0)
    cp 'F'
    jp nz,open_failed
    ld a,(filedata+1)
    cp 'S'
    jp nz,open_failed
    ld a,(filedata+2)
    cp 'V'
    jp nz,open_failed
    ld a,(filedata+3)
    cp '1'
    jp nz,open_failed
    ld a,(filedata+7)
    or a
    jp nz,open_failed
    ld hl,(filedata+4)        ; ADL24 OK: low three bytes of uint32 count
    ld (remaining),hl
    call read_batch

record_loop:
    ld hl,(remaining)
    ld de,0
    or a
    sbc hl,de
    jp z,complete

    ld (record_ptr),iy
    xor a
    ld (failure_reason),a
    ld hl,actual_output
    ld (hl),a
    ld de,actual_output+1
    ld bc,19
    ldir

    ld a,(iy+0)
    or a
    jp z,test_damp_rate
    dec a
    jp z,test_clamp_rate
    dec a
    jp z,test_apply_axis_moment
    dec a
    jp z,test_drag_q8
    dec a
    jp z,test_clamp_velocity
    dec a
    jp z,test_transpose
    dec a
    jp z,test_local_point_world
    dec a
    jp z,test_horizon
    dec a
    jp z,test_build_active_51
    dec a
    jp z,test_build_translation_52
    dec a
    jp z,test_build_projection_53
    ld a,reason_value
    call mark_failure
    jp checked

test_damp_rate:
    call prime_registers
    ld ix,(record_ptr)
    ld l,(ix+4)
    ld h,(ix+5)
    ld ix,active_ix_sentinel
    call p3d_fsim_f2_damp_rate16
    call capture_registers
    call save_actual_hl_word
    call check_index_sentinels
    call check_shadow_sentinels
    ld iy,(record_ptr)
    ld hl,actual_hl
    lea de,iy+40
    ld b,2
    ld c,reason_value
    call compare_bytes
    jp checked

test_clamp_rate:
    call prime_registers
    ld ix,(record_ptr)
    ld l,(ix+4)
    ld h,(ix+5)
    ld ix,active_ix_sentinel
    call p3d_fsim_f2_clamp_rate16
    call capture_registers
    call save_actual_hl_word
    call check_bc_sentinel
    call check_de_sentinel
    call check_index_sentinels
    call check_shadow_sentinels
    ld iy,(record_ptr)
    ld hl,actual_hl
    lea de,iy+40
    ld b,2
    ld c,reason_value
    call compare_bytes
    jp checked

test_apply_axis_moment:
    call prime_registers
    ld ix,(record_ptr)
    ld l,(ix+4)
    ld h,(ix+5)
    ld e,(ix+6)
    ld d,(ix+7)
    ld a,(ix+8)
    ld (expected_de),de
    ld ix,active_ix_sentinel
    call p3d_fsim_f2_apply_axis_moment16
    call capture_registers
    call save_actual_hl_word
    call check_expected_de
    call check_index_sentinels
    call check_shadow_sentinels
    ld iy,(record_ptr)
    ld hl,actual_hl
    lea de,iy+40
    ld b,2
    ld c,reason_value
    call compare_bytes
    jp checked

test_drag_q8:
    call prime_registers
    ld ix,(record_ptr)
    ld e,(ix+4)
    ld d,(ix+5)
    ld ix,active_ix_sentinel
    call p3d_fsim_f2_drag_from_velocity_q8_16
    call capture_registers
    call save_actual_de_word
    call check_bc_sentinel
    call check_hl_sentinel
    call check_index_sentinels
    call check_shadow_sentinels
    ld iy,(record_ptr)
    ld hl,actual_de
    lea de,iy+40
    ld b,2
    ld c,reason_value
    call compare_bytes
    jp checked

test_clamp_velocity:
    call prime_registers
    ld ix,(record_ptr)
    ld l,(ix+4)
    ld h,(ix+5)
    ld ix,active_ix_sentinel
    call p3d_fsim_f2_clamp_velocity_q8_16
    call capture_registers
    call save_actual_hl_word
    call check_bc_sentinel
    call check_de_sentinel
    call check_index_sentinels
    call check_shadow_sentinels
    ld iy,(record_ptr)
    ld hl,actual_hl
    lea de,iy+40
    ld b,2
    ld c,reason_value
    call compare_bytes
    jp checked

test_transpose:
    call clear_work_area
    ld hl,(record_ptr)
    ld de,4
    add hl,de
    ld de,transpose_source
    ld bc,18
    ldir
    call prime_registers
    ld ix,transpose_destination
    ld iy,transpose_source
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_fsim_mat3_transpose16
    call capture_registers
    ld hl,transpose_destination
    call save_actual_block18
    call check_bc_sentinel
    call check_de_sentinel
    call check_hl_sentinel
    call check_expected_indexes
    call check_shadow_sentinels
    call check_transpose_storage
    ld iy,(record_ptr)
    ld hl,transpose_destination
    lea de,iy+40
    ld b,18
    ld c,reason_value
    call compare_bytes
    jp checked

test_local_point_world:
    call clear_work_area
    ld hl,(record_ptr)
    ld de,4
    add hl,de
    ld de,local_object+p3d_object_orientation
    ld bc,18
    ldir
    ld hl,(record_ptr)
    ld de,22
    add hl,de
    ld de,local_object+p3d_object_world_position
    ld bc,6
    ldir
    ld hl,(record_ptr)
    ld de,28
    add hl,de
    ld de,local_vector
    ld bc,6
    ldir
    ld hl,local_object
    ld de,source_snapshot
    ld bc,p3d_object_size
    ldir

    call prime_registers
    ld ix,local_vector
    ld iy,local_object
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_fsim_local_point_to_world16
    call capture_registers
    ld hl,local_vector
    call save_actual_block6
    call check_expected_indexes
    call check_documented_carry
    call check_local_storage
    ld iy,(record_ptr)
    ld hl,local_vector
    lea de,iy+40
    ld b,6
    ld c,reason_value
    call compare_bytes
    jp checked

test_horizon:
    call clear_work_area
    ld hl,(record_ptr)
    ld de,4
    add hl,de
    ld de,horizon_matrix
    ld bc,18
    ldir
    call prime_registers
    ld ix,horizon_matrix
    ld iy,active_iy_sentinel
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_fsim_mat3_horizon16
    call capture_registers
    call save_actual_hl_de_words
    call check_expected_indexes
    call check_documented_carry
    call check_horizon_storage
    ld iy,(record_ptr)
    ld hl,actual_output
    lea de,iy+40
    ld b,4
    ld c,reason_value
    call compare_bytes
    jp checked

test_build_active_51:
    call clear_work_area
    call prime_registers
    ld ix,(record_ptr)
    ld c,(ix+4)
    ld b,(ix+5)
    ld e,(ix+6)
    ld d,(ix+7)
    ld a,(ix+8)
    ld (expected_de),de
    ld ix,active_ix_sentinel
    ld iy,active_packet
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_fsim_build_object_active16
    call capture_registers
    ld hl,active_packet
    ld b,p3d_fsim_object_active_packet_size
    call save_actual_block
    call check_expected_de
    call check_expected_indexes
    call check_shadow_sentinels
    call check_documented_carry
    ld de,p3d_fsim_object_active_packet_size
    call check_builder_returns
    call check_active_storage
    ld iy,(record_ptr)
    ld hl,active_packet
    lea de,iy+40
    ld b,p3d_fsim_object_active_packet_size
    ld c,reason_value
    call compare_bytes
    jp checked

test_build_translation_52:
    call clear_work_area
    ld hl,(record_ptr)
    ld de,8
    add hl,de
    ld de,translation_source
    ld bc,9
    ldir
    call prime_registers
    ld ix,(record_ptr)
    ld c,(ix+4)
    ld b,(ix+5)
    ld e,(ix+6)
    ld d,(ix+7)
    ld (expected_de),de
    ld ix,translation_source
    ld iy,translation_packet
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_fsim_build_object_xyz_translation24
    call capture_registers
    ld hl,translation_packet
    ld b,p3d_fsim_object_translation24_packet_size
    call save_actual_block
    call check_expected_de
    call check_expected_indexes
    call check_shadow_sentinels
    call check_documented_carry
    ld de,p3d_fsim_object_translation24_packet_size
    call check_builder_returns
    call check_translation_storage
    ld iy,(record_ptr)
    ld hl,translation_packet
    lea de,iy+40
    ld b,p3d_fsim_object_translation24_packet_size
    ld c,reason_value
    call compare_bytes
    jp checked

test_build_projection_53:
    call clear_work_area
    call prime_registers
    ld ix,(record_ptr)
    ld c,(ix+4)
    ld b,(ix+5)
    ld e,(ix+6)
    ld d,(ix+7)
    ld (expected_de),de
    ld ix,active_ix_sentinel
    ld iy,projection_packet
    ld (expected_ix),ix
    ld (expected_iy),iy
    call p3d_fsim_build_projection_far16
    call capture_registers
    ld hl,projection_packet
    ld b,p3d_fsim_projection_far_packet_size
    call save_actual_block
    call check_expected_de
    call check_expected_indexes
    call check_shadow_sentinels
    call check_documented_carry
    ld de,p3d_fsim_projection_far_packet_size
    call check_builder_returns
    call check_projection_storage
    ld iy,(record_ptr)
    ld hl,projection_packet
    lea de,iy+40
    ld b,p3d_fsim_projection_far_packet_size
    ld c,reason_value
    call compare_bytes

checked:
    ld a,(failure_reason)
    or a
    jr z,next_record
    ld hl,(errors)
    ld de,0
    or a
    sbc hl,de
    call z,capture_first_error
    ld hl,(errors)
    inc hl
    ld (errors),hl

    ld iy,(record_ptr)
    ld a,(iy+0)
    ld e,a
    add a,a
    add a,e
    ld de,0
    ld e,a
    ld hl,errors_by_op
    add hl,de
    ld de,(hl)               ; ADL24 OK: one three-byte error counter
    inc de
    ld (hl),de               ; ADL24 OK: one three-byte error counter

next_record:
    ld hl,(records)
    inc hl
    ld (records),hl
    ld iy,(record_ptr)
    lea iy,iy+record_size
    ld hl,(remaining)
    dec hl
    ld (remaining),hl
    ld hl,(batch_remaining)
    dec hl
    ld (batch_remaining),hl
    ld de,0
    or a
    sbc hl,de
    jp nz,record_loop
    ld hl,(remaining)
    ld de,0
    or a
    sbc hl,de
    jp z,record_loop
    call read_batch
    jp record_loop

complete:
    ld a,(input_handle)
    ld c,a
    MOSCALL mos_fclose
    xor a
    ld (input_handle),a
    call write_result
    call print_report
    ret

open_failed:
    ld a,(input_handle)
    or a
    jr z,@no_input
    ld c,a
    MOSCALL mos_fclose
    xor a
    ld (input_handle),a
@no_input:
    ld hl,0xffffff
    ld (errors),hl
    call write_result
    call print_report
    ret

read_batch:
    ld hl,(remaining)
    ld de,batch_records
    or a
    sbc hl,de
    jr c,@use_remaining
    ld hl,batch_records
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
    ret
@short_read:
    ld hl,0xffffff
    ld (errors),hl
    ld hl,0
    ld (remaining),hl
    ld (batch_remaining),hl
    ld iy,filedata
    ret

; ---------------------------------------------------------------------------
; ABI snapshots and comparisons
; ---------------------------------------------------------------------------

prime_registers:
    xor a
    scf
    ld a,shadow_a_sentinel
    ex af,af'
    exx
    ld bc,shadow_bc_sentinel
    ld de,shadow_de_sentinel
    ld hl,shadow_hl_sentinel
    exx
    ld bc,active_bc_sentinel
    ld de,active_de_sentinel
    ld hl,active_hl_sentinel
    ld ix,active_ix_sentinel
    ld iy,active_iy_sentinel
    ret

capture_registers:
    ld a,0
    adc a,0
    ld (actual_carry),a
    ld (actual_bc),bc
    ld (actual_de),de
    ld (actual_hl),hl
    ld (actual_ix),ix
    ld (actual_iy),iy
    exx
    ld (actual_shadow_bc),bc
    ld (actual_shadow_de),de
    ld (actual_shadow_hl),hl
    exx
    ex af,af'
    push af
    pop hl
    ld (actual_shadow_af),hl
    ex af,af'
    ret

mark_failure:
    ld c,a
    ld a,(failure_reason)
    or c
    ld (failure_reason),a
    ret

mark_register_failure:
    ld a,reason_register
    jr mark_failure

mark_shadow_failure:
    ld a,reason_shadow
    jr mark_failure

check_bc_sentinel:
    ld hl,(actual_bc)
    ld de,active_bc_sentinel
    or a
    sbc hl,de
    call nz,mark_register_failure
    ret

check_de_sentinel:
    ld hl,(actual_de)
    ld de,active_de_sentinel
    or a
    sbc hl,de
    call nz,mark_register_failure
    ret

check_hl_sentinel:
    ld hl,(actual_hl)
    ld de,active_hl_sentinel
    or a
    sbc hl,de
    call nz,mark_register_failure
    ret

check_expected_de:
    ld hl,(actual_de)
    ld de,(expected_de)
    or a
    sbc hl,de
    call nz,mark_register_failure
    ret

check_index_sentinels:
    ld hl,(actual_ix)
    ld de,active_ix_sentinel
    or a
    sbc hl,de
    call nz,mark_register_failure
    ld hl,(actual_iy)
    ld de,active_iy_sentinel
    or a
    sbc hl,de
    call nz,mark_register_failure
    ret

check_expected_indexes:
    ld hl,(actual_ix)
    ld de,(expected_ix)
    or a
    sbc hl,de
    call nz,mark_register_failure
    ld hl,(actual_iy)
    ld de,(expected_iy)
    or a
    sbc hl,de
    call nz,mark_register_failure
    ret

check_shadow_sentinels:
    ld hl,(actual_shadow_bc)
    ld de,shadow_bc_sentinel
    or a
    sbc hl,de
    call nz,mark_shadow_failure
    ld hl,(actual_shadow_de)
    ld de,shadow_de_sentinel
    or a
    sbc hl,de
    call nz,mark_shadow_failure
    ld hl,(actual_shadow_hl)
    ld de,shadow_hl_sentinel
    or a
    sbc hl,de
    call nz,mark_shadow_failure
    ld hl,(actual_shadow_af)
    ld de,shadow_af_sentinel
    or a
    sbc hl,de
    call nz,mark_shadow_failure
    ret

check_documented_carry:
    ld iy,(record_ptr)
    ld a,(iy+2)
    or a
    ret z
    ld a,(actual_carry)
    cp (iy+1)
    ret z
    ld a,reason_carry
    jp mark_failure

; DE contains the expected packet size. The saved HL must equal the saved IY,
; and saved BC must be the zero-extended exact size.
check_builder_returns:
    push de
    ld hl,(actual_hl)
    ld de,(actual_iy)
    or a
    sbc hl,de
    jr z,@size
    ld a,reason_return
    call mark_failure
@size:
    pop de
    ld hl,(actual_bc)
    or a
    sbc hl,de
    ret z
    ld a,reason_return
    jp mark_failure

; HL=actual, DE=expected, B=count, C=reason bit.
compare_bytes:
@loop:
    ld a,(de)
    cp (hl)
    jr nz,@failed
    inc hl
    inc de
    djnz @loop
    ret
@failed:
    ld a,c
    jp mark_failure

; HL=start, B=count, C=reason. All bytes must retain the 0xcc fill.
check_cc_bytes:
@loop:
    ld a,(hl)
    cp 0xcc
    jr nz,@failed
    inc hl
    djnz @loop
    ret
@failed:
    ld a,c
    jp mark_failure

clear_work_area:
    ld hl,work_area_start
    ld de,work_area_start+1
    ld bc,work_area_end-work_area_start-1
    ld a,0xcc
    ld (hl),a
    ldir
    ret

save_actual_hl_word:
    ld a,(actual_hl+0)
    ld (actual_output+0),a
    ld a,(actual_hl+1)
    ld (actual_output+1),a
    ret

save_actual_de_word:
    ld a,(actual_de+0)
    ld (actual_output+0),a
    ld a,(actual_de+1)
    ld (actual_output+1),a
    ret

save_actual_hl_de_words:
    call save_actual_hl_word
    ld a,(actual_de+0)
    ld (actual_output+2),a
    ld a,(actual_de+1)
    ld (actual_output+3),a
    ret

save_actual_block18:
    ld b,18
    jr save_actual_block

save_actual_block6:
    ld b,6
save_actual_block:
    ld de,actual_output
@loop:
    ld a,(hl)
    ld (de),a
    inc hl
    inc de
    djnz @loop
    ret

check_transpose_storage:
    ld iy,(record_ptr)
    ld hl,transpose_source
    lea de,iy+4
    ld b,18
    ld c,reason_source
    call compare_bytes
    ld hl,transpose_source_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,transpose_source_guard_after
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,transpose_destination_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,transpose_destination_guard_after
    ld b,8
    ld c,reason_source
    jp check_cc_bytes

check_local_storage:
    ld hl,local_object
    ld de,source_snapshot
    ld b,p3d_object_size
    ld c,reason_source
    call compare_bytes
    ld hl,local_object_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,local_object_guard_after
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,local_vector_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,local_vector_guard_after
    ld b,8
    ld c,reason_source
    jp check_cc_bytes

check_horizon_storage:
    ld iy,(record_ptr)
    ld hl,horizon_matrix
    lea de,iy+4
    ld b,18
    ld c,reason_source
    call compare_bytes
    ld hl,horizon_matrix_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,horizon_matrix_guard_after
    ld b,8
    ld c,reason_source
    jp check_cc_bytes

check_active_storage:
    ld hl,active_packet_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,active_packet_guard_after
    ld b,8
    ld c,reason_source
    jp check_cc_bytes

check_translation_storage:
    ld iy,(record_ptr)
    ld hl,translation_source
    lea de,iy+8
    ld b,9
    ld c,reason_source
    call compare_bytes
    ld hl,translation_source_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,translation_source_guard_after
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,translation_packet_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,translation_packet_guard_after
    ld b,8
    ld c,reason_source
    jp check_cc_bytes

check_projection_storage:
    ld hl,projection_packet_guard_before
    ld b,8
    ld c,reason_source
    call check_cc_bytes
    ld hl,projection_packet_guard_after
    ld b,8
    ld c,reason_source
    jp check_cc_bytes

capture_first_error:
    ld hl,(records)
    ld (first_error_record),hl
    ld iy,(record_ptr)
    ld a,(iy+0)
    ld (first_error_opcode),a
    ld a,(failure_reason)
    ld (first_error_reason),a
    ld a,(actual_carry)
    ld (first_error_carry),a
    ld hl,actual_output
    ld de,first_error_output
    ld bc,20
    ldir
    ret

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
    call print_inline
    asciz "\r\nPingo3D F2 regression\r\n"
    ld hl,(errors)
    ld de,0
    or a
    sbc hl,de
    jr nz,@failed
    call print_inline
    asciz "PASS\r\n"
    ret
@failed:
    call print_inline
    asciz "FAIL\r\n"
    ret

print_inline:
    pop hl
@loop:
    ld a,(hl)
    inc hl
    or a
    jr z,@done
    rst.lil 10h
    jr @loop
@done:
    push hl
    ret

vector_filename: asciz "3d_fsim.vec"
result_filename: asciz "3d_fsim.result"
input_handle: db 0
output_handle: db 0
failure_reason: db 0
actual_carry: db 0
record_ptr: dl 0
expected_bytes: dl 0
remaining: dl 0
batch_remaining: dl 0
expected_ix: dl 0
expected_iy: dl 0
expected_de: dl 0
actual_bc: dl 0
actual_de: dl 0
actual_hl: dl 0
actual_ix: dl 0
actual_iy: dl 0
actual_shadow_bc: dl 0
actual_shadow_de: dl 0
actual_shadow_hl: dl 0
actual_shadow_af: dl 0
actual_output: blkb 20,0
source_snapshot: blkb p3d_object_size,0

work_area_start:
transpose_source_guard_before: blkb 8,0xcc
transpose_source: blkb 18,0xcc
transpose_source_guard_after: blkb 8,0xcc
transpose_destination_guard_before: blkb 8,0xcc
transpose_destination: blkb 18,0xcc
transpose_destination_guard_after: blkb 8,0xcc

local_object_guard_before: blkb 8,0xcc
local_object: blkb p3d_object_size,0xcc
local_object_guard_after: blkb 8,0xcc
local_vector_guard_before: blkb 8,0xcc
local_vector: blkb p3d_vec3_size,0xcc
local_vector_guard_after: blkb 8,0xcc

horizon_matrix_guard_before: blkb 8,0xcc
horizon_matrix: blkb p3d_mat3_size,0xcc
horizon_matrix_guard_after: blkb 8,0xcc

active_packet_guard_before: blkb 8,0xcc
active_packet: blkb p3d_fsim_object_active_packet_size,0xcc
active_packet_guard_after: blkb 8,0xcc

translation_source_guard_before: blkb 8,0xcc
translation_source: blkb 9,0xcc
translation_source_guard_after: blkb 8,0xcc
translation_packet_guard_before: blkb 8,0xcc
translation_packet: blkb p3d_fsim_object_translation24_packet_size,0xcc
translation_packet_guard_after: blkb 8,0xcc

projection_packet_guard_before: blkb 8,0xcc
projection_packet: blkb p3d_fsim_projection_far_packet_size,0xcc
projection_packet_guard_after: blkb 8,0xcc
work_area_end:

result_data:
    db "P3F1"
records: dl 0
errors: dl 0
errors_by_op: blkb op_count*3,0
first_error_record: dl 0
first_error_opcode: db 0
first_error_reason: db 0
first_error_carry: db 0
first_error_output: blkb 20,0
result_end:

    align 256
filedata: equ $
