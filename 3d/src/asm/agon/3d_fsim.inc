; =============================================================================
; Optional Pingo3D flight-simulator companion.
;
; Include this file after 3d.inc and 3d_sincos_table.inc.  It contains only
; the reusable, caller-owned portion of the later pingo_terrain flight work:
; small F2 register kernels, matrix/camera composition helpers, and packet
; builders for the new Pingo commands used by streamed flight.  It deliberately
; does not import the application-global flight state, keyboard policy, terrain
; manager, backdrop drawing, or unacknowledged RST.LIL transport.
;
; The implementation is a source-faithful API adaptation of pingo_terrain
; commit 58f0b61734f35965752833c9f74b34cdd55ed296.  See docs/provenance.md.
; =============================================================================

; Experimental F2 policy bounds.  These are not universal aircraft limits.
p3d_fsim_f2_rate_limit:                 equ 512
p3d_fsim_f2_rate_limit_high:            equ 2
p3d_fsim_f2_rate_negative_limit_high:   equ 0feh
p3d_fsim_f2_velocity_limit_q8:          equ 8192
p3d_fsim_f2_velocity_limit_high:        equ 32
p3d_fsim_f2_velocity_negative_high:     equ 0e0h

; Experimental Pingo protocol additions used by the streamed-flight fixtures.
p3d_fsim_pingo_set_object_active:               equ 51
p3d_fsim_pingo_set_object_xyz_translation_wide: equ 52
p3d_fsim_pingo_set_projection_far_units:        equ 53
p3d_fsim_object_active_packet_size:              equ 10
p3d_fsim_object_translation24_packet_size:       equ 18
p3d_fsim_projection_far_packet_size:             equ 9

; Apply one F2 angular-control quantum.
;
; inputs:
;   hl = current signed 16-bit angular rate
;   a  = signed demand -1, 0, or +1
;   de = positive 16-bit moment
; outputs:
;   hl = rate after F2 damping, one signed moment, and the +/-512 clamp
; destroys:
;   af, bc
; preserves:
;   de, ix, iy and all shadow registers
; notes:
;   Demand is a sign selector, not a multiplier.  Values outside -1/0/+1 are
;   outside the qualified F2 contract.  The retained F2 caller supplies an
;   already-clamped rate and moments 8, 12, or 24; arbitrary operands that
;   overflow signed 16-bit addition are outside this routine's contract.
;   Condition flags on return are not a status result.
p3d_fsim_f2_apply_axis_moment16:
    push af
    push de
    call p3d_fsim_f2_damp_rate16
    pop de
    pop af
    or a
    jr z,p3d_fsim_f2_clamp_rate16
    jp m,@negative
    add.s hl,de
    jr p3d_fsim_f2_clamp_rate16
@negative:
    or a
    sbc.s hl,de
    jr p3d_fsim_f2_clamp_rate16

; Decay one signed rate by approximately 1/16 magnitude.  Every nonzero rate
; loses at least one count, so a released control converges exactly to zero.
;
; inputs/outputs:
;   hl = signed 16-bit rate
; destroys:
;   af, bc, de
; preserves:
;   ix, iy and all shadow registers
p3d_fsim_f2_damp_rate16:
    ld a,h
    or l
    ret z
    ld b,h
    ld c,l
    bit 7,h
    jr nz,@negative
    ld a,4
@positive_shift:
    srl h
    rr l
    dec a
    jr nz,@positive_shift
    ld a,h
    or l
    jr nz,@positive_ready
    inc hl
@positive_ready:
    ld d,h
    ld e,l
    ld h,b
    ld l,c
    or a
    sbc.s hl,de
    ret
@negative:
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
    ld a,4
@negative_shift:
    srl h
    rr l
    dec a
    jr nz,@negative_shift
    ld a,h
    or l
    jr nz,@negative_ready
    inc hl
@negative_ready:
    ld d,h
    ld e,l
    ld h,b
    ld l,c
    add.s hl,de
    ret

; Clamp one signed angular rate to the F2 policy range -512..+512.
;
; inputs/outputs:
;   hl = signed 16-bit rate
; destroys:
;   af
; preserves:
;   bc, de, ix, iy and all shadow registers
p3d_fsim_f2_clamp_rate16:
    bit 7,h
    jr nz,@negative
    ld a,h
    cp p3d_fsim_f2_rate_limit_high
    ret c
    jr nz,@positive_limit
    ld a,l
    or a
    ret z
@positive_limit:
    ld hl,p3d_fsim_f2_rate_limit
    ret
@negative:
    ld a,h
    cp p3d_fsim_f2_rate_negative_limit_high
    ret nc
    ld hl,-p3d_fsim_f2_rate_limit
    ret

; Convert one signed-Q8 velocity into the opposing whole-count F2 drag.
;
; inputs:
;   de = signed Q8 velocity
; outputs:
;   de = -(velocity >> 8), with an arithmetic signed shift
; destroys:
;   af
; preserves:
;   bc, hl, ix, iy and all shadow registers
p3d_fsim_f2_drag_from_velocity_q8_16:
    ld a,d
    ld e,a
    add a,a
    sbc a,a
    ld d,a
    xor a
    sub e
    ld e,a
    ld a,0
    sbc a,d
    ld d,a
    ret

; Clamp one signed-Q8 velocity to the F2 policy range -8192..+8192.
;
; inputs/outputs:
;   hl = signed-Q8 velocity
; destroys:
;   af
; preserves:
;   bc, de, ix, iy and all shadow registers
p3d_fsim_f2_clamp_velocity_q8_16:
    bit 7,h
    jr nz,@negative
    ld a,h
    cp p3d_fsim_f2_velocity_limit_high
    ret c
    jr nz,@positive_limit
    ld a,l
    or a
    ret z
@positive_limit:
    ld hl,p3d_fsim_f2_velocity_limit_q8
    ret
@negative:
    ld a,h
    cp p3d_fsim_f2_velocity_negative_high
    ret nc
    ld hl,-p3d_fsim_f2_velocity_limit_q8
    ret

; Copy the transpose of one row-major signed-Q15 matrix.
;
; inputs:
;   ix -> distinct 18-byte destination matrix
;   iy -> 18-byte source matrix
; outputs:
;   (ix) = transpose((iy))
; destroys:
;   af
; preserves:
;   bc, de, hl, ix, iy and all shadow registers
; notes:
;   Source and destination must not overlap.  The qualified donor used two
;   distinct matrices; this bytewise write order is deliberately not in-place.
p3d_fsim_mat3_transpose16:
    ld a,(iy+p3d_mat3_r0c0+0)
    ld (ix+p3d_mat3_r0c0+0),a
    ld a,(iy+p3d_mat3_r0c0+1)
    ld (ix+p3d_mat3_r0c0+1),a
    ld a,(iy+p3d_mat3_r1c0+0)
    ld (ix+p3d_mat3_r0c1+0),a
    ld a,(iy+p3d_mat3_r1c0+1)
    ld (ix+p3d_mat3_r0c1+1),a
    ld a,(iy+p3d_mat3_r2c0+0)
    ld (ix+p3d_mat3_r0c2+0),a
    ld a,(iy+p3d_mat3_r2c0+1)
    ld (ix+p3d_mat3_r0c2+1),a

    ld a,(iy+p3d_mat3_r0c1+0)
    ld (ix+p3d_mat3_r1c0+0),a
    ld a,(iy+p3d_mat3_r0c1+1)
    ld (ix+p3d_mat3_r1c0+1),a
    ld a,(iy+p3d_mat3_r1c1+0)
    ld (ix+p3d_mat3_r1c1+0),a
    ld a,(iy+p3d_mat3_r1c1+1)
    ld (ix+p3d_mat3_r1c1+1),a
    ld a,(iy+p3d_mat3_r2c1+0)
    ld (ix+p3d_mat3_r1c2+0),a
    ld a,(iy+p3d_mat3_r2c1+1)
    ld (ix+p3d_mat3_r1c2+1),a

    ld a,(iy+p3d_mat3_r0c2+0)
    ld (ix+p3d_mat3_r2c0+0),a
    ld a,(iy+p3d_mat3_r0c2+1)
    ld (ix+p3d_mat3_r2c0+1),a
    ld a,(iy+p3d_mat3_r1c2+0)
    ld (ix+p3d_mat3_r2c1+0),a
    ld a,(iy+p3d_mat3_r1c2+1)
    ld (ix+p3d_mat3_r2c1+1),a
    ld a,(iy+p3d_mat3_r2c2+0)
    ld (ix+p3d_mat3_r2c2+0),a
    ld a,(iy+p3d_mat3_r2c2+1)
    ld (ix+p3d_mat3_r2c2+1),a
    ret

; Convert a caller-owned local point into world coordinates through one
; retained Pingo3D object pose.
;
; inputs:
;   ix -> writable six-byte vec3 initially containing the local point
;   iy -> p3d_object state
; outputs:
;   (ix) = object world position + object orientation * local point
;   carry clear on success
;   carry set on matrix or translation overflow
; destroys:
;   af, af', bc, de, hl and all shadow GP registers
; preserves:
;   ix, iy
; notes:
;   The writable vec3 at IX must not overlap the retained object at IY.
;   Matrix failure leaves the vec3 unchanged.  If the matrix succeeds but the
;   final translation overflows, the vec3 contains the rotated local offset;
;   callers that need an immutable input must copy it into work storage first.
p3d_fsim_local_point_to_world16:
    push iy
    lea iy,iy+p3d_object_orientation
    call p3d_mat3_mul_vec3_16
    pop iy
    ret c
    push iy
    lea iy,iy+p3d_object_world_position
    call p3d_vec3_translate16
    pop iy
    ret

; Derive the branch-free horizon attitude from one retained local-to-world
; matrix.  The world-up row gives pitch=asin(-r1c2) and
; roll=atan2(r1c0,r1c1), avoiding discontinuous individual Euler branches.
;
; inputs:
;   ix -> 18-byte row-major signed-Q15 matrix
; outputs:
;   hl = signed Pingo pitch word, a multiple of 128
;   de = signed Pingo roll word, a multiple of 128
;   carry clear
; destroys:
;   af, af', bc and all shadow GP registers
; preserves:
;   ix, iy
; notes:
;   Negating -32768 saturates to +32767 before the qualified asin path.
p3d_fsim_mat3_horizon16:
    ld l,(ix+p3d_mat3_r1c2)
    ld h,(ix+p3d_mat3_r1c2+1)
    ld a,h
    cp 080h
    jr nz,@negate_pitch
    ld a,l
    or a
    jr nz,@negate_pitch
    ld hl,32767
    jr @pitch_ready
@negate_pitch:
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
@pitch_ready:
    call p3d_asin16
    push hl

    ld l,(ix+p3d_mat3_r1c0)
    ld h,(ix+p3d_mat3_r1c0+1)
    ld e,(ix+p3d_mat3_r1c1)
    ld d,(ix+p3d_mat3_r1c1+1)
    call p3d_atan2_16
    ex de,hl
    pop hl
    ret

; Build experimental Pingo subcommand 51 without sending it.
;
; inputs:
;   bc = unsigned 16-bit Pingo control/scene ID
;   de = unsigned 16-bit object ID
;   a  = 0 inactive, 1 active
;   iy -> caller-owned p3d_fsim_object_active_packet_size-byte buffer
; outputs:
;   hl = packet start (same address as iy)
;   bc = packet size
;   carry clear
; destroys:
;   af, bc, hl
; preserves:
;   de, ix, iy and all shadow registers
p3d_fsim_build_object_active16:
    push af
    ld a,23
    ld (iy+0),a
    xor a
    ld (iy+1),a
    ld a,0a0h
    ld (iy+2),a
    ld a,c
    ld (iy+3),a
    ld a,b
    ld (iy+4),a
    ld a,p3d_pingo_command
    ld (iy+5),a
    ld a,p3d_fsim_pingo_set_object_active
    ld (iy+6),a
    ld a,e
    ld (iy+7),a
    ld a,d
    ld (iy+8),a
    pop af
    ld (iy+9),a
    push iy
    pop hl
    ld bc,p3d_fsim_object_active_packet_size
    or a
    ret

; Build experimental Pingo subcommand 52 without sending it.
;
; inputs:
;   bc = unsigned 16-bit Pingo control/scene ID
;   de = unsigned 16-bit object ID
;   ix -> nine exact signed-24 little-endian bytes: X, Y, Z
;   iy -> caller-owned p3d_fsim_object_translation24_packet_size-byte buffer
; outputs:
;   hl = packet start (same address as iy)
;   bc = packet size
;   carry clear
; destroys:
;   af, bc, hl
; preserves:
;   de, ix, iy and all shadow registers
; notes:
;   Source and packet storage must not overlap.  Values use the same raw
;   translation-count unit as legacy command 17.
p3d_fsim_build_object_xyz_translation24:
    ld a,23
    ld (iy+0),a
    xor a
    ld (iy+1),a
    ld a,0a0h
    ld (iy+2),a
    ld a,c
    ld (iy+3),a
    ld a,b
    ld (iy+4),a
    ld a,p3d_pingo_command
    ld (iy+5),a
    ld a,p3d_fsim_pingo_set_object_xyz_translation_wide
    ld (iy+6),a
    ld a,e
    ld (iy+7),a
    ld a,d
    ld (iy+8),a
    ld a,(ix+0)
    ld (iy+9),a
    ld a,(ix+1)
    ld (iy+10),a
    ld a,(ix+2)
    ld (iy+11),a
    ld a,(ix+3)
    ld (iy+12),a
    ld a,(ix+4)
    ld (iy+13),a
    ld a,(ix+5)
    ld (iy+14),a
    ld a,(ix+6)
    ld (iy+15),a
    ld a,(ix+7)
    ld (iy+16),a
    ld a,(ix+8)
    ld (iy+17),a
    push iy
    pop hl
    ld bc,p3d_fsim_object_translation24_packet_size
    or a
    ret

; Build experimental Pingo subcommand 53 without sending it.
;
; inputs:
;   bc = unsigned 16-bit Pingo control/scene ID
;   de = unsigned 16-bit far distance in Pingo scene units
;   iy -> caller-owned p3d_fsim_projection_far_packet_size-byte buffer
; outputs:
;   hl = packet start (same address as iy)
;   bc = packet size
;   carry clear
; destroys:
;   af, bc, hl
; preserves:
;   de, ix, iy and all shadow registers
; notes:
;   Values 0 and 1 are built unchanged; the experimental firmware consumes
;   but rejects them, preserving its current setting.  Valid values are
;   2..65535.
p3d_fsim_build_projection_far16:
    ld a,23
    ld (iy+0),a
    xor a
    ld (iy+1),a
    ld a,0a0h
    ld (iy+2),a
    ld a,c
    ld (iy+3),a
    ld a,b
    ld (iy+4),a
    ld a,p3d_pingo_command
    ld (iy+5),a
    ld a,p3d_fsim_pingo_set_projection_far_units
    ld (iy+6),a
    ld a,e
    ld (iy+7),a
    ld a,d
    ld (iy+8),a
    push iy
    pop hl
    ld bc,p3d_fsim_projection_far_packet_size
    or a
    ret
