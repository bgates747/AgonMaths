; eZ80-local integer 3D arithmetic for Pingo-compatible values
;
; Scalar arithmetic, forward/inverse trigonometric lookup, vector and matrix
; transforms, persistent object-pose integration, and dirty Pingo command
; synchronization are implemented below. Include 3d_sincos_table.inc after
; this file so its forward table references resolve.
;
; Pingo wire values are little-endian 16-bit quantities:
;
;   mesh coordinate: signed normalized, value = raw / 32767
;   rotation angle:  signed turns,      radians = raw * 2*pi / 32767
;   translation:     signed distance,   value = raw * 256 / 32767
;   scale:           unsigned 8.8,      value = raw / 256
;
; A local vec3 occupies six bytes. Its component interpretation is supplied
; by the operation; the layout itself does not encode a numeric domain.

p3d_vec3_x:       equ 0
p3d_vec3_y:       equ 2
p3d_vec3_z:       equ 4
p3d_vec3_size:    equ 6
p3d_norm_max:     equ 32767
p3d_norm_half_up: equ 16384

; A row-major signed-Q15 matrix occupies 18 bytes.
p3d_mat3_r0c0:    equ 0
p3d_mat3_r0c1:    equ 2
p3d_mat3_r0c2:    equ 4
p3d_mat3_r1c0:    equ 6
p3d_mat3_r1c1:    equ 8
p3d_mat3_r1c2:    equ 10
p3d_mat3_r2c0:    equ 12
p3d_mat3_r2c1:    equ 14
p3d_mat3_r2c2:    equ 16
p3d_mat3_size:    equ 18

; Packed eZ80-side state for one Pingo object. The eZ80 owns this complete
; state; Pingo receives absolute create/scale/rotation/translation commands.
; All word and aggregate fields start at even offsets.
p3d_object_dirty:                    equ 0
p3d_object_reserved:                 equ 1
p3d_object_sid:                      equ 2
p3d_object_oid:                      equ 4
p3d_object_mid:                      equ 6
p3d_object_bmid:                     equ 8
p3d_object_world_position:           equ 10
p3d_object_pingo_translation:        equ p3d_object_world_position
p3d_object_pingo_rotation:           equ 16
p3d_object_scale:                    equ 22
p3d_object_orientation:              equ 28
p3d_object_local_linear_velocity:    equ 46
p3d_object_local_angular_velocity:   equ 52
p3d_object_angular_residual:         equ 58
p3d_object_size:                     equ 64

; The four-word input tuple accepted by p3d_object_init16 has the same ID
; offsets relative to its own base, minus the two-byte state header.
p3d_object_ids_sid:                  equ 0
p3d_object_ids_oid:                  equ 2
p3d_object_ids_mid:                  equ 4
p3d_object_ids_bmid:                 equ 6
p3d_object_ids_size:                 equ 8

p3d_object_dirty_position:           equ 0x01
p3d_object_dirty_rotation:           equ 0x02
p3d_object_dirty_scale:              equ 0x04
p3d_object_dirty_create:             equ 0x08
p3d_object_dirty_pose:               equ 0x03
p3d_object_dirty_all:                equ 0x0f

; Current buffered-command Pingo selector and absolute object subcommands.
p3d_pingo_command:                   equ 0x49
p3d_pingo_create_object:             equ 5
p3d_pingo_set_object_xyz_scale:      equ 9
p3d_pingo_set_object_xyz_rotation:   equ 13
p3d_pingo_set_object_xyz_translation: equ 17
p3d_pingo_create_packet_size:        equ 13
p3d_pingo_update_packet_size:        equ 15

; ---------------------------------------------------------------------------
; Scalar arithmetic primitives
; ---------------------------------------------------------------------------

; Unsigned 16-bit by unsigned 16-bit multiplication.
;
; inputs:
;   hl = unsigned 16-bit multiplicand (H:L; UHL ignored)
;   de = unsigned 16-bit multiplier (D:E; UDE ignored)
; outputs:
;   hl:de = unsigned 32-bit product in bytes H:L:D:E
; destroys:
;   af, bc
; preserves:
;   bc', de', hl'
p3d_umul16x16_32:
    ld b,e
    ld c,l
    ld l,e
    ld e,c
    mlt bc                  ; bc = low * low
    ld a,b                  ; save product byte 1
    ld b,h                  ; prepare high * high
    mlt hl                  ; hl = high * low
    add a,l
    ld l,a
    adc a,h
    sub a,l
    ld h,a
    ld a,c                  ; save product byte 0
    ld c,d                  ; bc = high:high
    mlt de                  ; de = low * high
    add.s hl,de             ; accumulate middle bytes
    ld e,a
    ld d,l
    ld l,h
    ld h,0
    rl h                    ; carry from middle-byte accumulation
    mlt bc                  ; bc = high * high
    add.s hl,bc
    ret

; Signed 16-bit by signed 16-bit multiplication.
;
; inputs:
;   hl = signed 16-bit multiplicand (H:L; UHL ignored)
;   de = signed 16-bit multiplier (D:E; UDE ignored)
; outputs:
;   hl:de = signed 32-bit product in bytes H:L:D:E
;   carry clear on success
; destroys:
;   af, af', bc
; preserves:
;   bc', de', hl'
p3d_smul16x16_32:
; park the product sign in the shadow flags
    ld a,h
    xor d
    ex af,af'
; absolute value of hl
    bit 7,h
    jr z,@hl_positive
    xor a
    sub l
    ld l,a
    ld a,0                  ; ld preserves carry
    sbc a,h
    ld h,a
@hl_positive:
; absolute value of de
    bit 7,d
    jr z,@de_positive
    xor a
    sub e
    ld e,a
    ld a,0
    sbc a,d
    ld d,a
@de_positive:
    call p3d_umul16x16_32
; restore the sign flags
    ex af,af'
    ret p
; negate logical H:L:D:E
    xor a
    sub e
    ld e,a
    ld a,0
    sbc a,d
    ld d,a
    ld a,0
    sbc a,l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
    or a                    ; success: carry clear
    ret

; Add two signed 32-bit values held in opposite GP register banks.
;
; inputs:
;   active hl:de = right/addend in bytes H:L:D:E
;   shadow hl:de = left/accumulator in bytes H':L':D':E'
; outputs:
;   hl:de = left + right
;   the opposite GP register bank is active on return
;   carry clear on success
;   carry set on signed overflow
; destroys:
;   af and both hl:de banks
; preserves:
;   af' and both bc banks
p3d_sadd32:
; low byte
    ld a,e
    exx
    add a,e
    ld e,a
; middle-low byte
    exx
    ld a,d
    exx
    adc a,d
    ld d,a
; middle-high byte
    exx
    ld a,l
    exx
    adc a,l
    ld l,a
; high byte
    exx
    ld a,h
    exx
    adc a,h
    ld h,a
; the final byte operation owns the signed-overflow flag
    jp pe,@overflow
    or a                    ; success: carry clear
    ret
@overflow:
    scf
    ret

; Subtract two signed 32-bit values held in opposite GP register banks.
;
; inputs:
;   active hl:de = right/subtrahend in bytes H:L:D:E
;   shadow hl:de = left/minuend in bytes H':L':D':E'
; outputs:
;   hl:de = left - right
;   the opposite GP register bank is active on return
;   carry clear on success
;   carry set on signed overflow
; destroys:
;   af and both hl:de banks
; preserves:
;   af' and both bc banks
p3d_ssub32:
; low byte: left + ~right + 1
    ld a,e
    cpl
    scf
    exx
    adc a,e
    ld e,a
; middle-low byte
    exx
    ld a,d
    cpl
    exx
    adc a,d
    ld d,a
; middle-high byte
    exx
    ld a,l
    cpl
    exx
    adc a,l
    ld l,a
; high byte
    exx
    ld a,h
    cpl
    exx
    adc a,h
    ld h,a
; the final byte operation owns the signed-overflow flag
    jp pe,@overflow
    or a                    ; success: carry clear
    ret
@overflow:
    scf
    ret

; Reduce a signed 32-bit product by Pingo's signed-normalized divisor.
;
; This is the narrowing operation needed after multiplying a coordinate by a
; normalized sine, cosine, or matrix coefficient:
;
;   result = round(product / 32767)
;
; inputs:
;   hl:de = signed 32-bit product in bytes H:L:D:E
; outputs:
;   hl = signed 16-bit result in H:L
;   carry clear on success
;   carry set if the rounded result is outside -32768..32767
; destroys:
;   af, af', bc, de
; preserves:
;   bc', de', hl'
p3d_sdiv32767_32_to_16:
; park the input sign in the shadow accumulator
    ld a,h
    and 0x80
    ex af,af'
; form the unsigned magnitude in H:L:D:E
    bit 7,h
    jr z,@magnitude_ready
    xor a
    sub e
    ld e,a
    ld a,0
    sbc a,d
    ld d,a
    ld a,0
    sbc a,l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
@magnitude_ready:
; A magnitude above 0x3FFFBFFF rounds beyond 32768 and cannot
; produce a signed 16-bit result for either input sign.
    ld a,h
    cp 0x3F
    jr c,@range_ok
    jr nz,@range_overflow
    ld a,l
    cp 0xFF
    jr c,@range_ok
    ld a,d
    cp 0xC0
    jr nc,@range_overflow
@range_ok:
; q = magnitude >> 15
    ld a,d
    rla                     ; carry = original product bit 15
    adc.s hl,hl
    ld b,h
    ld c,l
; r = magnitude & 32767
    res 7,d
; magnitude = q*32767 + (q+r)
    add.s hl,de             ; range check guarantees no carry
; The accepted result range limits q+r to 49150, so subtract at most once.
; Incrementing makes the threshold 0x8000. On the subtraction path, clearing
; bit 15 then computes (q+r+1)-32768 = (q+r)-32767.
    inc.s hl
    bit 7,h
    jr z,@below_norm
    res 7,h
    inc.s bc
    jr @remainder
@below_norm:
    dec.s hl                ; restore q+r
@remainder:
; Divisor 32767 has no exact half-integer remainder.
    bit 6,h                 ; remainder >= 16384
    jr z,@rounded
    inc.s bc
@rounded:
; Restore the input sign.
    ex af,af'
    or a
    jr nz,@negative
; Positive 32768 is outside the signed 16-bit result range.
    bit 7,b
    jr nz,@overflow
    ld h,b
    ld l,c
    or a                    ; success: carry clear
    ret
@negative:
; The range check guarantees a magnitude no greater than 32768.
    ld h,b
    ld l,c
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
    or a                    ; success: carry clear
    ret
@range_overflow:
; Balance the earlier AF exchange before returning.
    ex af,af'
@overflow:
    scf
    ret

; Multiply a signed 16-bit value by a signed Pingo-normalized coefficient.
;
; This is the common composition of p3d_smul16x16_32 and
; p3d_sdiv32767_32_to_16.
;
; inputs:
;   hl = signed 16-bit value (low word)
;   de = signed normalized coefficient (low word)
; outputs:
;   hl = round(value * coefficient / 32767)
;   carry clear on success
;   carry set on overflow
; destroys:
;   af, af', bc, de
; preserves:
;   bc', de', hl'
p3d_smul_norm16:
    call p3d_smul16x16_32
    jp p3d_sdiv32767_32_to_16

; Convert a Pingo rotation angle to normalized sine and cosine values.
;
; inputs:
;   hl = signed Pingo rotation angle (low word)
; outputs:
;   bc = signed normalized sine (low word)
;   de = signed normalized cosine (low word)
;   carry clear on success
; destroys:
;   af, hl
; preserves:
;   af', ix, iy, and all shadow GP registers
p3d_sincos16:
    ld b,0                  ; input-negative flag
    bit 7,h
    jr z,@magnitude_ready
    inc b
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
@magnitude_ready:
; index = round(abs(angle) / 128), modulo 256
    ld a,l
    add a,64
    ld l,a
    ld a,h
    adc a,0
    add a,a
    bit 7,l
    jr z,@rounded_index
    inc a
@rounded_index:
    bit 0,b
    jr z,@index_ready
    neg
@index_ready:
; Four bytes per interleaved sine/cosine entry.
    ld e,a
    ld d,4
    mlt de
    inc de
    dec.s de                ; zero-extend UDE
    ld hl,p3d_sincos_q15
    add hl,de               ; full 24-bit table address
    ld c,(hl)
    inc hl
    ld b,(hl)
    inc hl
    ld e,(hl)
    inc hl
    ld d,(hl)
    or a
    ret

; Convert one signed Cartesian direction to the nearest 1/256-turn Pingo
; angle. The chosen phase maximizes x*cos(phase)+y*sin(phase) against the
; generated Q15 table. Exact ties choose the numerically smaller signed phase;
; atan2(0,0) is zero.
;
; inputs:
;   hl = signed Y component (low word)
;   de = signed X component (low word)
; outputs:
;   hl = signed Pingo angle, always a multiple of 128
;   carry clear
; destroys:
;   af, af', bc, de, and all shadow GP registers
; preserves:
;   ix, iy
p3d_atan2_16:
    push ix
    push iy
; Record original X/Y signs in bits 0/1.
    ld b,0
    bit 7,d
    jr z,@x_sign_ready
    set 0,b
@x_sign_ready:
    bit 7,h
    jr z,@y_sign_ready
    set 1,b
@y_sign_ready:
; Unsigned magnitudes; -32768 deliberately becomes 32768.
    bit 7,d
    jr z,@x_magnitude_ready
    xor a
    sub e
    ld e,a
    ld a,0
    sbc a,d
    ld d,a
@x_magnitude_ready:
    bit 7,h
    jr z,@y_magnitude_ready
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
@y_magnitude_ready:
    ld a,h
    or l
    or d
    or e
    jr nz,@nonzero
    ld hl,0
    pop iy
    pop ix
    or a
    ret
@nonzero:
; IX=max magnitude, IY=min magnitude. Bit 2 records folding across 45
; degrees so the first-octant result can be unfolded later.
    push de
    pop ix
    push hl
    pop iy
    or a
    sbc.s hl,de
    jr c,@magnitudes_ordered
    jr z,@magnitudes_ordered
    push ix
    push iy
    pop ix
    pop iy
    set 2,b
@magnitudes_ordered:
; An exact boundary must choose the smaller final signed phase. Within the
; folded octant that means choosing the upper bin when (X sign XOR Y sign
; XOR fold) is true.
    ld c,0
    ld a,b
    and 3
    cp 1
    jr z,@different_signs
    cp 2
    jr nz,@sign_difference_ready
@different_signs:
    inc c
@sign_difference_ready:
    bit 2,b
    jr z,@fold_tie_ready
    ld a,c
    xor 1
    ld c,a
@fold_tie_ready:
    bit 0,c
    jr z,@tie_ready
    set 3,b
@tie_ready:
; Park the mapping code in AF'. Search bins 0..32 using exact dot-product
; boundaries. Thirty-three possible bins require at most six comparisons.
    ld a,b
    ex af,af'
    ld b,0                  ; inclusive lower bin
    ld c,32                 ; inclusive upper bin
@search:
    ld a,b
    cp c
    jr z,@search_complete
    push bc
    add a,c
    srl a                   ; midpoint/boundary index
    ld e,a
    ld d,4
    mlt de
    inc de
    dec.s de                ; zero-extend UDE
    ld hl,p3d_atan2_octant_boundaries_q15
    add hl,de
    ld e,(hl)
    inc hl
    ld d,(hl)               ; DE = dx
    push de
    inc hl
    ld e,(hl)
    inc hl
    ld d,(hl)               ; DE = dy
; left = min*dy
    push iy
    pop hl
    call p3d_umul16x16_32
    exx
; right = max*dx
    pop de
    push ix
    pop hl
    call p3d_umul16x16_32
; Return to the original bank with left-right. Both products are below 2^31,
; so signed subtraction also gives an exact unsigned ordering result.
    call p3d_ssub32
    pop bc
    bit 7,h
    jr nz,@choose_lower
    ld a,h
    or l
    or d
    or e
    jr nz,@choose_upper
; Exact boundary: apply the parked final-phase tie rule.
    ex af,af'
    ld e,a
; The quadrant-II boundary between phases +127 and -128 crosses the signed
; phase wrap. Its numerically smaller choice is -128 (folded bin zero), unlike
; all other quadrant-II boundaries.
    and 3
    cp 1
    jr nz,@ordinary_tie_shadow
    ld a,b
    add a,c
    srl a
    jr nz,@ordinary_tie_shadow
    ld a,e
    ex af,af'
    jr @choose_lower
@ordinary_tie_shadow:
    bit 3,e
    jr z,@tie_lower_shadow
    ld a,e
    ex af,af'
    jr @choose_upper
@tie_lower_shadow:
    ld a,e
    ex af,af'
    jr @choose_lower
@choose_upper:
    ld a,b
    add a,c
    srl a
    inc a
    ld b,a
    jr @search
@choose_lower:
    ld a,b
    add a,c
    srl a
    ld c,a
    jr @search

@search_complete:
; Unfold the octant, then map the positive magnitude through the original
; Cartesian quadrant to signed phase -128..127.
    ex af,af'
    ld c,a                  ; mapping code
    ld a,b                  ; folded phase
    bit 2,c
    jr z,@base_ready
    ld a,64
    sub b
@base_ready:
    ld d,a                  ; positive phase magnitude 0..64
    ld a,c
    and 3
    jr z,@phase_positive
    dec a
    jr z,@phase_quadrant_ii
    dec a
    jr z,@phase_quadrant_iv
; Quadrant III: phase = base-128.
    ld a,d
    sub 128
    jr @phase_ready
@phase_quadrant_ii:
    ld a,128
    sub d
    jr @phase_ready
@phase_quadrant_iv:
    xor a
    sub d
    jr @phase_ready
@phase_positive:
    ld a,d
@phase_ready:
    ld l,a
    ld h,0
    bit 7,l
    jr z,@phase_extended
    dec h
@phase_extended:
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    pop iy
    pop ix
    or a
    ret

; Convert one signed normalized sine coefficient to the nearest canonical
; pitch in -1/4..+1/4 turn. Exact ties choose the smaller magnitude.
;
; inputs:
;   hl = signed normalized sine coefficient
; outputs:
;   hl = signed Pingo angle, always a multiple of 128
;   carry clear
; destroys:
;   af, af', bc, de
; preserves:
;   ix, iy and all shadow GP registers
p3d_asin16:
    push ix
    ld b,0
    bit 7,h
    jr z,@magnitude_ready
    inc b
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
@magnitude_ready:
    push hl
    pop ix
    ld a,b
    ex af,af'               ; park input sign
; The final threshold selects the cardinal pitch directly. This also clamps
; magnitude 32768 to the table's maximum 32767.
    ld de,32763
    or a
    sbc.s hl,de
    jr c,@search_needed
    ld b,64
    jr @search_complete
@search_needed:
    ld b,0
    ld c,63
@search:
    ld a,b
    cp c
    jr z,@search_complete
    add a,c
    srl a
    push bc
    ld e,a
    ld d,2
    mlt de
    inc de
    dec.s de
    ld hl,p3d_asin_thresholds_q15
    add hl,de
    ld e,(hl)
    inc hl
    ld d,(hl)
    push ix
    pop hl
    or a
    sbc.s hl,de
    pop bc
    jr c,@choose_lower
    ld a,b
    add a,c
    srl a
    inc a
    ld b,a
    jr @search
@choose_lower:
    ld a,b
    add a,c
    srl a
    ld c,a
    jr @search
@search_complete:
    ld a,b
    ex af,af'
    or a
    jr z,@positive_shadow
    ex af,af'
    neg
    jr @sign_ready
@positive_shadow:
    ex af,af'
@sign_ready:
    ld l,a
    ld h,0
    bit 7,l
    jr z,@phase_extended
    dec h
@phase_extended:
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    add.s hl,hl
    pop ix
    or a
    ret

; ---------------------------------------------------------------------------
; Single-vector transforms
; ---------------------------------------------------------------------------

; Signed low-16 addition with signed-overflow carry.
;
; inputs:  hl = left, de = right
; outputs: hl = wrapped sum; carry set on signed overflow
; destroys: af
p3d_sadd16:
    ld a,l
    add a,e
    ld l,a
    ld a,h
    adc a,d
    ld h,a
    jp pe,@overflow
    or a
    ret
@overflow:
    scf
    ret

; Translate one signed 16-bit vec3 in place.
;
; This performs component-wise integer addition. Both operands must use the
; same Pingo domain, normally translation-distance units.
;
; inputs:
;   ix -> six-byte source/destination vec3
;   iy -> six-byte translation vec3
; outputs:
;   (ix) = (ix) + (iy)
;   carry clear on success
;   carry set if any signed component overflows
;   vector is unchanged on overflow
; destroys:
;   af, bc, de, hl
; preserves:
;   ix, iy, and all shadow registers
p3d_vec3_translate16:
    ld l,(ix+p3d_vec3_x)
    ld h,(ix+p3d_vec3_x+1)
    ld e,(iy+p3d_vec3_x)
    ld d,(iy+p3d_vec3_x+1)
    call p3d_sadd16
    ret c
    push hl
    ld l,(ix+p3d_vec3_y)
    ld h,(ix+p3d_vec3_y+1)
    ld e,(iy+p3d_vec3_y)
    ld d,(iy+p3d_vec3_y+1)
    call p3d_sadd16
    jr c,@y_failed
    push hl
    ld l,(ix+p3d_vec3_z)
    ld h,(ix+p3d_vec3_z+1)
    ld e,(iy+p3d_vec3_z)
    ld d,(iy+p3d_vec3_z+1)
    call p3d_sadd16
    jr c,@z_failed
    ld c,l
    ld b,h
    pop hl
    ld (ix+p3d_vec3_y),l
    ld (ix+p3d_vec3_y+1),h
    pop hl
    ld (ix+p3d_vec3_x),l
    ld (ix+p3d_vec3_x+1),h
    ld (ix+p3d_vec3_z),c
    ld (ix+p3d_vec3_z+1),b
    or a
    ret
@z_failed:
    pop hl
@y_failed:
    pop hl
    scf
    ret

; Load a*cos and b*sin into opposite GP banks.
; Requires BC=sin, DE=cos, IX=vector.
    MACRO P3D_ROT_LOAD_COS_SIN a_offset, b_offset
    ld a,c
    exx
    ld e,a
    exx
    ld a,b
    exx
    ld d,a
    ld l,(ix+b_offset)
    ld h,(ix+b_offset+1)
    exx
    ld l,(ix+a_offset)
    ld h,(ix+a_offset+1)
    ENDMACRO

; Load a*sin and b*cos into opposite GP banks.
; Requires BC=sin, DE=cos, IX=vector.
    MACRO P3D_ROT_LOAD_SIN_COS a_offset, b_offset
    ld a,e
    exx
    ld e,a
    exx
    ld a,d
    exx
    ld d,a
    ld l,(ix+b_offset)
    ld h,(ix+b_offset+1)
    exx
    ld e,c
    ld d,b
    ld l,(ix+a_offset)
    ld h,(ix+a_offset+1)
    ENDMACRO

; Multiply the two active/shadow operand pairs, add, and normalize.
p3d_rot_muladd_norm:
    call p3d_smul16x16_32
    exx
    call p3d_smul16x16_32
    call p3d_sadd32
    ret c
    jp p3d_sdiv32767_32_to_16

; Multiply the two active/shadow operand pairs, subtract, and normalize.
p3d_rot_mulsub_norm:
    call p3d_smul16x16_32
    exx
    call p3d_smul16x16_32
    call p3d_ssub32
    ret c
    jp p3d_sdiv32767_32_to_16

; Rotate one signed 16-bit vec3 around the X axis in place.
;
; inputs:
;   ix -> six-byte source/destination vec3
;   hl = signed Pingo rotation angle (low word)
; outputs:
;   rotated vec3 at (ix)
;   carry clear on success
;   carry set on arithmetic overflow
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy; vector is unchanged on failure
p3d_vec3_rotate_x16:
    push iy
    push hl
    call p3d_sincos16
    P3D_ROT_LOAD_COS_SIN p3d_vec3_y,p3d_vec3_z
    call p3d_rot_mulsub_norm
    jr c,@first_failed
    push hl
    pop iy
    pop hl
    call p3d_sincos16
    P3D_ROT_LOAD_SIN_COS p3d_vec3_y,p3d_vec3_z
    call p3d_rot_muladd_norm
    jr c,@second_failed
    ld e,l
    ld d,h
    push iy
    pop hl
    ld (ix+p3d_vec3_y),l
    ld (ix+p3d_vec3_y+1),h
    ld (ix+p3d_vec3_z),e
    ld (ix+p3d_vec3_z+1),d
    pop iy
    or a
    ret
@second_failed:
    pop iy
    scf
    ret
@first_failed:
    pop hl
    pop iy
    scf
    ret

; Rotate one signed 16-bit vec3 around the Y axis in place.
;
; inputs:
;   ix -> six-byte source/destination vec3
;   hl = signed Pingo rotation angle (low word)
; outputs:
;   rotated vec3 at (ix)
;   carry clear on success
;   carry set on arithmetic overflow
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy; vector is unchanged on failure
p3d_vec3_rotate_y16:
    push iy
    push hl
    call p3d_sincos16
    P3D_ROT_LOAD_COS_SIN p3d_vec3_x,p3d_vec3_z
    call p3d_rot_muladd_norm
    jr c,@first_failed
    push hl
    pop iy
    pop hl
    call p3d_sincos16
    P3D_ROT_LOAD_COS_SIN p3d_vec3_z,p3d_vec3_x
    call p3d_rot_mulsub_norm
    jr c,@second_failed
    ld e,l
    ld d,h
    push iy
    pop hl
    ld (ix+p3d_vec3_x),l
    ld (ix+p3d_vec3_x+1),h
    ld (ix+p3d_vec3_z),e
    ld (ix+p3d_vec3_z+1),d
    pop iy
    or a
    ret
@second_failed:
    pop iy
    scf
    ret
@first_failed:
    pop hl
    pop iy
    scf
    ret

; Rotate one signed 16-bit vec3 around the Z axis in place.
;
; inputs:
;   ix -> six-byte source/destination vec3
;   hl = signed Pingo rotation angle (low word)
; outputs:
;   rotated vec3 at (ix)
;   carry clear on success
;   carry set on arithmetic overflow
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy; vector is unchanged on failure
p3d_vec3_rotate_z16:
    push iy
    push hl
    call p3d_sincos16
    P3D_ROT_LOAD_COS_SIN p3d_vec3_x,p3d_vec3_y
    call p3d_rot_mulsub_norm
    jr c,@first_failed
    push hl
    pop iy
    pop hl
    call p3d_sincos16
    P3D_ROT_LOAD_SIN_COS p3d_vec3_x,p3d_vec3_y
    call p3d_rot_muladd_norm
    jr c,@second_failed
    ld e,l
    ld d,h
    push iy
    pop hl
    ld (ix+p3d_vec3_x),l
    ld (ix+p3d_vec3_x+1),h
    ld (ix+p3d_vec3_y),e
    ld (ix+p3d_vec3_y+1),d
    pop iy
    or a
    ret
@second_failed:
    pop iy
    scf
    ret
@first_failed:
    pop hl
    pop iy
    scf
    ret

; Apply Pingo-order Euler rotations to one signed 16-bit vec3 in place.
;
; Rotation order is X, then Y, then Z, matching Pingo transform construction.
;
; inputs:
;   ix -> six-byte source/destination vec3
;   iy -> six-byte angle vec3 (X, Y, Z)
; outputs:
;   rotated vec3 at (ix)
;   carry clear on success
;   carry set on arithmetic overflow
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; note:
;   The vector is restored to its original value if any axis fails.
p3d_vec3_rotate_xyz16:
; Snapshot the vector and all angles before any in-place write.
    ld l,(ix+p3d_vec3_x)
    ld h,(ix+p3d_vec3_x+1)
    push hl
    ld l,(ix+p3d_vec3_y)
    ld h,(ix+p3d_vec3_y+1)
    push hl
    ld l,(ix+p3d_vec3_z)
    ld h,(ix+p3d_vec3_z+1)
    push hl
    ld l,(iy+p3d_vec3_z)
    ld h,(iy+p3d_vec3_z+1)
    push hl
    ld l,(iy+p3d_vec3_y)
    ld h,(iy+p3d_vec3_y+1)
    push hl
    ld l,(iy+p3d_vec3_x)
    ld h,(iy+p3d_vec3_x+1)
    push hl
    pop hl
    call p3d_vec3_rotate_x16
    jr c,@x_failed
    pop hl
    call p3d_vec3_rotate_y16
    jr c,@y_failed
    pop hl
    call p3d_vec3_rotate_z16
    jr c,@z_failed
; Discard the saved original Z, Y, X.
    pop hl
    pop hl
    pop hl
    or a
    ret
@x_failed:
    pop hl                  ; unused Y angle
@y_failed:
    pop hl                  ; unused Z angle
@z_failed:
; Restore original Z, Y, X.
    pop hl
    ld (ix+p3d_vec3_z),l
    ld (ix+p3d_vec3_z+1),h
    pop hl
    ld (ix+p3d_vec3_y),l
    ld (ix+p3d_vec3_y+1),h
    pop hl
    ld (ix+p3d_vec3_x),l
    ld (ix+p3d_vec3_x+1),h
    scf
    ret

; ---------------------------------------------------------------------------
; Matrix-vector primitives
; ---------------------------------------------------------------------------

; Multiply a signed normalized 3x3 matrix by one signed 16-bit vec3.
;
; Matrix storage is row-major, matching Pingo. Each element is a signed
; normalized 16-bit coefficient, so the matrix occupies 18 bytes.
;
; inputs:
;   ix -> six-byte source/destination vec3
;   iy -> 18-byte row-major matrix
; outputs:
;   transformed vec3 at (ix)
;   carry clear on success
;   carry set on arithmetic overflow
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy; vector is unchanged on failure
p3d_mat3_mul_vec3_16:
    push iy
    call p3d_mat3_row_mul_vec3_16
    jr c,@row0_failed
    push hl
    lea iy,iy+6
    call p3d_mat3_row_mul_vec3_16
    jr c,@row1_failed
    push hl
    lea iy,iy+6
    call p3d_mat3_row_mul_vec3_16
    jr c,@row2_failed
    ld e,l
    ld d,h
    pop hl
    ld (ix+p3d_vec3_y),l
    ld (ix+p3d_vec3_y+1),h
    pop hl
    ld (ix+p3d_vec3_x),l
    ld (ix+p3d_vec3_x+1),h
    ld (ix+p3d_vec3_z),e
    ld (ix+p3d_vec3_z+1),d
    pop iy
    or a
    ret
@row2_failed:
    pop hl
@row1_failed:
    pop hl
@row0_failed:
    pop iy
    scf
    ret

; Multiply the row at IY by the vector at IX, accumulate all three products
; at signed 32-bit width, then normalize once.
p3d_mat3_row_mul_vec3_16:
; product 0 operands in the active bank
    ld l,(ix+p3d_vec3_x)
    ld h,(ix+p3d_vec3_x+1)
    ld e,(iy+0)
    ld d,(iy+1)
; product 1 operands in the shadow bank
    exx
    ld l,(ix+p3d_vec3_y)
    ld h,(ix+p3d_vec3_y+1)
    ld e,(iy+2)
    ld d,(iy+3)
    exx
    call p3d_smul16x16_32
    exx
    call p3d_smul16x16_32
    call p3d_sadd32
    ret c
; park the two-product sum and form product 2 in the opposite bank
    exx
    ld l,(ix+p3d_vec3_z)
    ld h,(ix+p3d_vec3_z+1)
    ld e,(iy+4)
    ld d,(iy+5)
    call p3d_smul16x16_32
    call p3d_sadd32
    ret c
    jp p3d_sdiv32767_32_to_16

; ---------------------------------------------------------------------------
; Matrix composition and orientation conversion
; ---------------------------------------------------------------------------

; Multiply two signed-Q15 row-major matrices in place.
;
; inputs:
;   ix -> left/source/destination matrix A
;   iy -> right/source matrix B
; outputs:
;   (ix) = A*B
;   carry clear on success
;   carry set on signed accumulator or narrowed-result overflow
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy; A is unchanged on failure
; note:
;   Exact IX=IY aliasing is supported. No output is committed until all nine
;   dot products have succeeded.
p3d_mat3_mul_mat3_16:
    push ix
    push iy
    call p3d_mat3_dot_row_col16
    jp c,@result0_failed
    push hl
    lea iy,iy+2
    call p3d_mat3_dot_row_col16
    jp c,@result1_failed
    push hl
    lea iy,iy+2
    call p3d_mat3_dot_row_col16
    jp c,@result2_failed
    push hl
    lea iy,iy-4
    lea ix,ix+6
    call p3d_mat3_dot_row_col16
    jp c,@result3_failed
    push hl
    lea iy,iy+2
    call p3d_mat3_dot_row_col16
    jp c,@result4_failed
    push hl
    lea iy,iy+2
    call p3d_mat3_dot_row_col16
    jp c,@result5_failed
    push hl
    lea iy,iy-4
    lea ix,ix+6
    call p3d_mat3_dot_row_col16
    jp c,@result6_failed
    push hl
    lea iy,iy+2
    call p3d_mat3_dot_row_col16
    jp c,@result7_failed
    push hl
    lea iy,iy+2
    call p3d_mat3_dot_row_col16
    jp c,@result8_failed
    push hl
; Restore the working pointers to their bases, then commit in reverse order.
    lea ix,ix-12
    lea iy,iy-4
    pop hl
    ld (ix+p3d_mat3_r2c2),l
    ld (ix+p3d_mat3_r2c2+1),h
    pop hl
    ld (ix+p3d_mat3_r2c1),l
    ld (ix+p3d_mat3_r2c1+1),h
    pop hl
    ld (ix+p3d_mat3_r2c0),l
    ld (ix+p3d_mat3_r2c0+1),h
    pop hl
    ld (ix+p3d_mat3_r1c2),l
    ld (ix+p3d_mat3_r1c2+1),h
    pop hl
    ld (ix+p3d_mat3_r1c1),l
    ld (ix+p3d_mat3_r1c1+1),h
    pop hl
    ld (ix+p3d_mat3_r1c0),l
    ld (ix+p3d_mat3_r1c0+1),h
    pop hl
    ld (ix+p3d_mat3_r0c2),l
    ld (ix+p3d_mat3_r0c2+1),h
    pop hl
    ld (ix+p3d_mat3_r0c1),l
    ld (ix+p3d_mat3_r0c1+1),h
    pop hl
    ld (ix+p3d_mat3_r0c0),l
    ld (ix+p3d_mat3_r0c0+1),h
    pop iy
    pop ix
    or a
    ret
@result8_failed:
    pop hl
@result7_failed:
    pop hl
@result6_failed:
    pop hl
@result5_failed:
    pop hl
@result4_failed:
    pop hl
@result3_failed:
    pop hl
@result2_failed:
    pop hl
@result1_failed:
    pop hl
@result0_failed:
    pop iy
    pop ix
    scf
    ret

; Dot the row beginning at IX with the column beginning at IY. B's column
; elements are six bytes apart.
p3d_mat3_dot_row_col16:
    ld l,(ix+0)
    ld h,(ix+1)
    ld e,(iy+0)
    ld d,(iy+1)
    exx
    ld l,(ix+2)
    ld h,(ix+3)
    ld e,(iy+6)
    ld d,(iy+7)
    exx
    call p3d_smul16x16_32
    exx
    call p3d_smul16x16_32
    call p3d_sadd32
    ret c
    exx
    ld l,(ix+4)
    ld h,(ix+5)
    ld e,(iy+12)
    ld d,(iy+13)
    call p3d_smul16x16_32
    call p3d_sadd32
    ret c
    jp p3d_sdiv32767_32_to_16

; Right-multiply a local-to-world matrix by one axis rotation. A row vector
; right-multiplied by Raxis(angle) is the existing column-vector axis rotation
; applied to that row with -angle.
    MACRO P3D_MAT3_LOCAL_AXIS_BODY vector_routine
    push iy
    push ix
; IY retains the negated angle across all three row calls.
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
    push hl
    pop iy
; Snapshot all nine logical words for transactional failure.
    ld l,(ix+0)
    ld h,(ix+1)
    push hl
    ld l,(ix+2)
    ld h,(ix+3)
    push hl
    ld l,(ix+4)
    ld h,(ix+5)
    push hl
    ld l,(ix+6)
    ld h,(ix+7)
    push hl
    ld l,(ix+8)
    ld h,(ix+9)
    push hl
    ld l,(ix+10)
    ld h,(ix+11)
    push hl
    ld l,(ix+12)
    ld h,(ix+13)
    push hl
    ld l,(ix+14)
    ld h,(ix+15)
    push hl
    ld l,(ix+16)
    ld h,(ix+17)
    push hl
    push iy
    pop hl
    call vector_routine
    jr c,@row0_failed
    lea ix,ix+6
    push iy
    pop hl
    call vector_routine
    jr c,@row1_failed
    lea ix,ix+6
    push iy
    pop hl
    call vector_routine
    jr c,@row2_failed
    lea ix,ix-12
; Discard the saved matrix.
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop ix
    pop iy
    or a
    ret
@row2_failed:
    lea ix,ix-12
    jr @restore
@row1_failed:
    lea ix,ix-6
    jr @restore
@row0_failed:
@restore:
; Restore elements 8..0 from the reverse-order stack snapshot.
    pop hl
    ld (ix+16),l
    ld (ix+17),h
    pop hl
    ld (ix+14),l
    ld (ix+15),h
    pop hl
    ld (ix+12),l
    ld (ix+13),h
    pop hl
    ld (ix+10),l
    ld (ix+11),h
    pop hl
    ld (ix+8),l
    ld (ix+9),h
    pop hl
    ld (ix+6),l
    ld (ix+7),h
    pop hl
    ld (ix+4),l
    ld (ix+5),h
    pop hl
    ld (ix+2),l
    ld (ix+3),h
    pop hl
    ld (ix+0),l
    ld (ix+1),h
    pop ix
    pop iy
    scf
    ret
    ENDMACRO

; inputs: IX=matrix, HL=Pingo angle. IX/IY and matrix are preserved on failure.
p3d_mat3_rotate_local_x16:
    P3D_MAT3_LOCAL_AXIS_BODY p3d_vec3_rotate_x16
p3d_mat3_rotate_local_y16:
    P3D_MAT3_LOCAL_AXIS_BODY p3d_vec3_rotate_y16
p3d_mat3_rotate_local_z16:
    P3D_MAT3_LOCAL_AXIS_BODY p3d_vec3_rotate_z16

; Apply one intrinsic local Euler delta to a local-to-world orientation.
; Fixed-point operations are deliberately ordered as:
;
;   ((R * Rz(dz)) * Ry(dy)) * Rx(dx)
;
; This is continuously equivalent to R*(Rz*Ry*Rx), while making the exact
; integer rounding order explicit and avoiding construction of a temporary
; matrix.
;
; inputs:
;   ix -> matrix
;   iy -> Pingo-angle vec3
; outputs:
;   updated matrix, carry as for the axis routines
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy; matrix is unchanged if any axis fails
p3d_mat3_rotate_local_xyz16:
; Whole-operation snapshot, because each individual axis is only locally
; transactional.
    ld l,(ix+0)
    ld h,(ix+1)
    push hl
    ld l,(ix+2)
    ld h,(ix+3)
    push hl
    ld l,(ix+4)
    ld h,(ix+5)
    push hl
    ld l,(ix+6)
    ld h,(ix+7)
    push hl
    ld l,(ix+8)
    ld h,(ix+9)
    push hl
    ld l,(ix+10)
    ld h,(ix+11)
    push hl
    ld l,(ix+12)
    ld h,(ix+13)
    push hl
    ld l,(ix+14)
    ld h,(ix+15)
    push hl
    ld l,(ix+16)
    ld h,(ix+17)
    push hl
    ld l,(iy+p3d_vec3_z)
    ld h,(iy+p3d_vec3_z+1)
    call p3d_mat3_rotate_local_z16
    jr c,@failed
    ld l,(iy+p3d_vec3_y)
    ld h,(iy+p3d_vec3_y+1)
    call p3d_mat3_rotate_local_y16
    jr c,@failed
    ld l,(iy+p3d_vec3_x)
    ld h,(iy+p3d_vec3_x+1)
    call p3d_mat3_rotate_local_x16
    jr c,@failed
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    or a
    ret
@failed:
    pop hl
    ld (ix+16),l
    ld (ix+17),h
    pop hl
    ld (ix+14),l
    ld (ix+15),h
    pop hl
    ld (ix+12),l
    ld (ix+13),h
    pop hl
    ld (ix+10),l
    ld (ix+11),h
    pop hl
    ld (ix+8),l
    ld (ix+9),h
    pop hl
    ld (ix+6),l
    ld (ix+7),h
    pop hl
    ld (ix+4),l
    ld (ix+5),h
    pop hl
    ld (ix+2),l
    ld (ix+3),h
    pop hl
    ld (ix+0),l
    ld (ix+1),h
    scf
    ret

; Build a canonical local-to-world matrix from Pingo XYZ Euler angles.
;
; inputs:
;   ix -> destination matrix
;   iy -> Pingo-angle vec3
; outputs:
;   matrix = Rz(z)*Ry(y)*Rx(x)
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy; matrix is unchanged on failure
p3d_mat3_from_euler16:
    ld l,(ix+0)
    ld h,(ix+1)
    push hl
    ld l,(ix+2)
    ld h,(ix+3)
    push hl
    ld l,(ix+4)
    ld h,(ix+5)
    push hl
    ld l,(ix+6)
    ld h,(ix+7)
    push hl
    ld l,(ix+8)
    ld h,(ix+9)
    push hl
    ld l,(ix+10)
    ld h,(ix+11)
    push hl
    ld l,(ix+12)
    ld h,(ix+13)
    push hl
    ld l,(ix+14)
    ld h,(ix+15)
    push hl
    ld l,(ix+16)
    ld h,(ix+17)
    push hl
    ld hl,32767
    ld (ix+0),l
    ld (ix+1),h
    ld (ix+8),l
    ld (ix+9),h
    ld (ix+16),l
    ld (ix+17),h
    ld hl,0
    ld (ix+2),l
    ld (ix+3),h
    ld (ix+4),l
    ld (ix+5),h
    ld (ix+6),l
    ld (ix+7),h
    ld (ix+10),l
    ld (ix+11),h
    ld (ix+12),l
    ld (ix+13),h
    ld (ix+14),l
    ld (ix+15),h
    call p3d_mat3_rotate_local_xyz16
    jr c,@failed
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    or a
    ret
@failed:
    pop hl
    ld (ix+16),l
    ld (ix+17),h
    pop hl
    ld (ix+14),l
    ld (ix+15),h
    pop hl
    ld (ix+12),l
    ld (ix+13),h
    pop hl
    ld (ix+10),l
    ld (ix+11),h
    pop hl
    ld (ix+8),l
    ld (ix+9),h
    pop hl
    ld (ix+6),l
    ld (ix+7),h
    pop hl
    ld (ix+4),l
    ld (ix+5),h
    pop hl
    ld (ix+2),l
    ld (ix+3),h
    pop hl
    ld (ix+0),l
    ld (ix+1),h
    scf
    ret

; Export a row-major local-to-world matrix as canonical Pingo XYZ Euler words.
; The convention is R=Rz*Ry*Rx, with pitch in -quarter..+quarter turn.
;
; inputs:
;   ix -> matrix
;   iy -> output angle vec3
; outputs:
;   canonical angle vec3
;   carry clear
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy
p3d_mat3_to_euler16:
; y = asin(-r20), saturating the otherwise unrepresentable -(-32768).
    ld l,(ix+p3d_mat3_r2c0)
    ld h,(ix+p3d_mat3_r2c0+1)
    ld a,h
    cp 0x80
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
    push hl                  ; canonical Y
; Gimbal lock is exactly the cardinal pitch bins +/-8192.
    ld a,h
    cp 0x20
    jr nz,@check_negative_gimbal
    ld a,l
    or a
    jr z,@gimbal
@check_negative_gimbal:
    ld a,h
    cp 0xe0
    jr nz,@nonsingular
    ld a,l
    or a
    jr z,@gimbal

@nonsingular:
; x = atan2(r21,r22)
    ld l,(ix+p3d_mat3_r2c1)
    ld h,(ix+p3d_mat3_r2c1+1)
    ld e,(ix+p3d_mat3_r2c2)
    ld d,(ix+p3d_mat3_r2c2+1)
    call p3d_atan2_16
    push hl
; z = atan2(r10,r00)
    ld l,(ix+p3d_mat3_r1c0)
    ld h,(ix+p3d_mat3_r1c0+1)
    ld e,(ix+p3d_mat3_r0c0)
    ld d,(ix+p3d_mat3_r0c0+1)
    call p3d_atan2_16
    ld (iy+p3d_vec3_z),l
    ld (iy+p3d_vec3_z+1),h
    pop hl
    ld (iy+p3d_vec3_x),l
    ld (iy+p3d_vec3_x+1),h
    pop hl
    ld (iy+p3d_vec3_y),l
    ld (iy+p3d_vec3_y+1),h
    or a
    ret

@gimbal:
; Canonical lock: z=0, x=atan2(-r12,r11). This preserves x-z at +90
; degrees and x+z at -90 degrees.
    ld l,(ix+p3d_mat3_r1c2)
    ld h,(ix+p3d_mat3_r1c2+1)
    ld a,h
    cp 0x80
    jr nz,@negate_lock_y
    ld a,l
    or a
    jr nz,@negate_lock_y
    ld hl,32767
    jr @lock_y_ready
@negate_lock_y:
    xor a
    sub l
    ld l,a
    ld a,0
    sbc a,h
    ld h,a
@lock_y_ready:
    ld e,(ix+p3d_mat3_r1c1)
    ld d,(ix+p3d_mat3_r1c1+1)
    call p3d_atan2_16
    ld (iy+p3d_vec3_x),l
    ld (iy+p3d_vec3_x+1),h
    pop hl
    ld (iy+p3d_vec3_y),l
    ld (iy+p3d_vec3_y+1),h
    ld hl,0
    ld (iy+p3d_vec3_z),l
    ld (iy+p3d_vec3_z+1),h
    or a
    ret

; ---------------------------------------------------------------------------
; Persistent eZ80-side Pingo object state
; ---------------------------------------------------------------------------

; Initialize all 64 bytes of one object state.
;
; inputs:
;   ix -> destination p3d object state
;   iy -> four-word tuple: sid, oid, mid, bmid
; outputs:
;   identity pose, unit scale, zero velocities/residuals, copied IDs
;   all four known dirty bits set for initial Pingo synchronization
;   carry clear
; destroys:
;   af, bc, de, hl
; preserves:
;   ix, iy and all shadow registers
; note:
;   The ID tuple may alias the destination; it is captured before the state is
;   overwritten.
p3d_object_init16:
    push ix
    push iy
    ld l,(iy+p3d_object_ids_sid)
    ld h,(iy+p3d_object_ids_sid+1)
    push hl
    ld l,(iy+p3d_object_ids_oid)
    ld h,(iy+p3d_object_ids_oid+1)
    push hl
    ld l,(iy+p3d_object_ids_mid)
    ld h,(iy+p3d_object_ids_mid+1)
    push hl
    ld l,(iy+p3d_object_ids_bmid)
    ld h,(iy+p3d_object_ids_bmid+1)
    push hl
; Seed byte zero, then replicate it over the remaining 63 bytes.
    xor a
    ld (ix+0),a
    push ix
    pop hl
    push ix
    pop de
    inc de
    ld bc,p3d_object_size-1
    ldir
; Restore the captured creation IDs.
    pop hl
    ld (ix+p3d_object_bmid),l
    ld (ix+p3d_object_bmid+1),h
    pop hl
    ld (ix+p3d_object_mid),l
    ld (ix+p3d_object_mid+1),h
    pop hl
    ld (ix+p3d_object_oid),l
    ld (ix+p3d_object_oid+1),h
    pop hl
    ld (ix+p3d_object_sid),l
    ld (ix+p3d_object_sid+1),h
    ld a,p3d_object_dirty_all
    ld (ix+p3d_object_dirty),a
; Pingo unit scale is unsigned 8.8 value 256.
    ld hl,256
    ld (ix+p3d_object_scale+p3d_vec3_x),l
    ld (ix+p3d_object_scale+p3d_vec3_x+1),h
    ld (ix+p3d_object_scale+p3d_vec3_y),l
    ld (ix+p3d_object_scale+p3d_vec3_y+1),h
    ld (ix+p3d_object_scale+p3d_vec3_z),l
    ld (ix+p3d_object_scale+p3d_vec3_z+1),h
; Signed-Q15 identity orientation.
    ld hl,32767
    ld (ix+p3d_object_orientation+p3d_mat3_r0c0),l
    ld (ix+p3d_object_orientation+p3d_mat3_r0c0+1),h
    ld (ix+p3d_object_orientation+p3d_mat3_r1c1),l
    ld (ix+p3d_object_orientation+p3d_mat3_r1c1+1),h
    ld (ix+p3d_object_orientation+p3d_mat3_r2c2),l
    ld (ix+p3d_object_orientation+p3d_mat3_r2c2+1),h
    pop iy
    pop ix
    or a
    ret

; Return Z when an angle maps to the identity lookup-table phase.
; inputs: HL=Pingo angle. Destroys AF,BC,DE,HL; preserves indexes/shadows.
p3d_angle_is_identity16:
    call p3d_sincos16
    ld a,b
    or c
    jr nz,@not_identity
    ld a,d
    cp 0x7f
    jr nz,@not_identity
    ld a,e
    cp 0xff
    jr nz,@not_identity
    xor a
    ret
@not_identity:
    ld a,1
    or a
    ret

; Finish a successful object-orientation update. IX is the state pointer.
; This exports canonical absolute Euler words and then rebuilds the internal
; matrix from those exact words, keeping eZ80 movement and Pingo rendering
; aligned while removing fixed-point orthogonality drift.
p3d_object_finish_rotation16:
    lea iy,ix+p3d_object_pingo_rotation
    lea ix,ix+p3d_object_orientation
    call p3d_mat3_to_euler16
    call p3d_mat3_from_euler16
    lea ix,ix-p3d_object_orientation
    ret c
    or a
    ret

; The object-rotation wrappers have 13 ADL words on the stack in this order:
; dirty, Euler X/Y/Z, matrix 0..8. With matrix 8 on top, the old Euler Z/Y/X
; low bytes are at SP+27/+30/+33. Mark rotation dirty only when the absolute
; Pingo Euler command actually changed.
    MACRO P3D_OBJECT_MARK_ROTATION_IF_CHANGED
    ld iy,0
    add iy,sp
    ld a,(ix+p3d_object_pingo_rotation+0)
    cp (iy+33)
    jr nz,@changed
    ld a,(ix+p3d_object_pingo_rotation+1)
    cp (iy+34)
    jr nz,@changed
    ld a,(ix+p3d_object_pingo_rotation+2)
    cp (iy+30)
    jr nz,@changed
    ld a,(ix+p3d_object_pingo_rotation+3)
    cp (iy+31)
    jr nz,@changed
    ld a,(ix+p3d_object_pingo_rotation+4)
    cp (iy+27)
    jr nz,@changed
    ld a,(ix+p3d_object_pingo_rotation+5)
    cp (iy+28)
    jr z,@complete
@changed:
    ld a,(ix+p3d_object_dirty)
    or p3d_object_dirty_rotation
    ld (ix+p3d_object_dirty),a
@complete:
    ENDMACRO

; Snapshot/finish body for a one-axis object-local rotation.
    MACRO P3D_OBJECT_ROTATE_LOCAL_AXIS_BODY matrix_routine
    push hl
    call p3d_angle_is_identity16
    pop hl
    ret z
; Preserve caller IY and retain the input angle there while HL is used for the
; transactional state snapshot.
    push iy
    push hl
    pop iy
    ld l,(ix+p3d_object_dirty)
    ld h,0
    push hl
    ld l,(ix+p3d_object_pingo_rotation+0)
    ld h,(ix+p3d_object_pingo_rotation+1)
    push hl
    ld l,(ix+p3d_object_pingo_rotation+2)
    ld h,(ix+p3d_object_pingo_rotation+3)
    push hl
    ld l,(ix+p3d_object_pingo_rotation+4)
    ld h,(ix+p3d_object_pingo_rotation+5)
    push hl
    ld l,(ix+p3d_object_orientation+0)
    ld h,(ix+p3d_object_orientation+1)
    push hl
    ld l,(ix+p3d_object_orientation+2)
    ld h,(ix+p3d_object_orientation+3)
    push hl
    ld l,(ix+p3d_object_orientation+4)
    ld h,(ix+p3d_object_orientation+5)
    push hl
    ld l,(ix+p3d_object_orientation+6)
    ld h,(ix+p3d_object_orientation+7)
    push hl
    ld l,(ix+p3d_object_orientation+8)
    ld h,(ix+p3d_object_orientation+9)
    push hl
    ld l,(ix+p3d_object_orientation+10)
    ld h,(ix+p3d_object_orientation+11)
    push hl
    ld l,(ix+p3d_object_orientation+12)
    ld h,(ix+p3d_object_orientation+13)
    push hl
    ld l,(ix+p3d_object_orientation+14)
    ld h,(ix+p3d_object_orientation+15)
    push hl
    ld l,(ix+p3d_object_orientation+16)
    ld h,(ix+p3d_object_orientation+17)
    push hl
    push iy
    pop hl
    lea ix,ix+p3d_object_orientation
    call matrix_routine
    lea ix,ix-p3d_object_orientation
    jr c,@discard_failed
    call p3d_object_finish_rotation16
    jr c,@restore_failed
    P3D_OBJECT_MARK_ROTATION_IF_CHANGED
; Discard 13 saved logical words.
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop iy
    or a
    ret
@discard_failed:
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop iy
    scf
    ret
@restore_failed:
; Matrix 8..0, Euler Z/Y/X, then the prior dirty byte.
    pop hl
    ld (ix+p3d_object_orientation+16),l
    ld (ix+p3d_object_orientation+17),h
    pop hl
    ld (ix+p3d_object_orientation+14),l
    ld (ix+p3d_object_orientation+15),h
    pop hl
    ld (ix+p3d_object_orientation+12),l
    ld (ix+p3d_object_orientation+13),h
    pop hl
    ld (ix+p3d_object_orientation+10),l
    ld (ix+p3d_object_orientation+11),h
    pop hl
    ld (ix+p3d_object_orientation+8),l
    ld (ix+p3d_object_orientation+9),h
    pop hl
    ld (ix+p3d_object_orientation+6),l
    ld (ix+p3d_object_orientation+7),h
    pop hl
    ld (ix+p3d_object_orientation+4),l
    ld (ix+p3d_object_orientation+5),h
    pop hl
    ld (ix+p3d_object_orientation+2),l
    ld (ix+p3d_object_orientation+3),h
    pop hl
    ld (ix+p3d_object_orientation+0),l
    ld (ix+p3d_object_orientation+1),h
    pop hl
    ld (ix+p3d_object_pingo_rotation+4),l
    ld (ix+p3d_object_pingo_rotation+5),h
    pop hl
    ld (ix+p3d_object_pingo_rotation+2),l
    ld (ix+p3d_object_pingo_rotation+3),h
    pop hl
    ld (ix+p3d_object_pingo_rotation+0),l
    ld (ix+p3d_object_pingo_rotation+1),h
    pop hl
    ld (ix+p3d_object_dirty),l
    pop iy
    scf
    ret
    ENDMACRO

; Rotate one object around its own current X/Y/Z axis.
; inputs: IX=object state, HL=Pingo angle. IX/IY are preserved.
p3d_object_rotate_local_x16:
    P3D_OBJECT_ROTATE_LOCAL_AXIS_BODY p3d_mat3_rotate_local_x16
p3d_object_rotate_local_y16:
    P3D_OBJECT_ROTATE_LOCAL_AXIS_BODY p3d_mat3_rotate_local_y16
p3d_object_rotate_local_z16:
    P3D_OBJECT_ROTATE_LOCAL_AXIS_BODY p3d_mat3_rotate_local_z16

; Rotate one object intrinsically by a local XYZ delta. Identity table phases
; leave both state and dirty flags untouched.
;
; inputs:
;   ix -> object state
;   iy -> local Pingo-angle vec3
; outputs:
;   canonicalized orientation/Euler cache and rotation dirty bit
;   carry set leaves the complete orientation/Euler/dirty subset unchanged
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy
p3d_object_rotate_local_xyz16:
; Fast identity test. Preserve IY; p3d_angle_is_identity16 already preserves
; both index registers.
    ld l,(iy+p3d_vec3_x)
    ld h,(iy+p3d_vec3_x+1)
    call p3d_angle_is_identity16
    jr nz,@apply
    ld l,(iy+p3d_vec3_y)
    ld h,(iy+p3d_vec3_y+1)
    call p3d_angle_is_identity16
    jr nz,@apply
    ld l,(iy+p3d_vec3_z)
    ld h,(iy+p3d_vec3_z+1)
    call p3d_angle_is_identity16
    ret z
@apply:
    push iy
    ld l,(ix+p3d_object_dirty)
    ld h,0
    push hl
    ld l,(ix+p3d_object_pingo_rotation+0)
    ld h,(ix+p3d_object_pingo_rotation+1)
    push hl
    ld l,(ix+p3d_object_pingo_rotation+2)
    ld h,(ix+p3d_object_pingo_rotation+3)
    push hl
    ld l,(ix+p3d_object_pingo_rotation+4)
    ld h,(ix+p3d_object_pingo_rotation+5)
    push hl
    ld l,(ix+p3d_object_orientation+0)
    ld h,(ix+p3d_object_orientation+1)
    push hl
    ld l,(ix+p3d_object_orientation+2)
    ld h,(ix+p3d_object_orientation+3)
    push hl
    ld l,(ix+p3d_object_orientation+4)
    ld h,(ix+p3d_object_orientation+5)
    push hl
    ld l,(ix+p3d_object_orientation+6)
    ld h,(ix+p3d_object_orientation+7)
    push hl
    ld l,(ix+p3d_object_orientation+8)
    ld h,(ix+p3d_object_orientation+9)
    push hl
    ld l,(ix+p3d_object_orientation+10)
    ld h,(ix+p3d_object_orientation+11)
    push hl
    ld l,(ix+p3d_object_orientation+12)
    ld h,(ix+p3d_object_orientation+13)
    push hl
    ld l,(ix+p3d_object_orientation+14)
    ld h,(ix+p3d_object_orientation+15)
    push hl
    ld l,(ix+p3d_object_orientation+16)
    ld h,(ix+p3d_object_orientation+17)
    push hl
    lea ix,ix+p3d_object_orientation
    call p3d_mat3_rotate_local_xyz16
    lea ix,ix-p3d_object_orientation
    jr c,@discard_failed
    call p3d_object_finish_rotation16
    jr c,@restore_failed
    P3D_OBJECT_MARK_ROTATION_IF_CHANGED
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop iy
    or a
    ret
@discard_failed:
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop hl
    pop iy
    scf
    ret
@restore_failed:
    pop hl
    ld (ix+p3d_object_orientation+16),l
    ld (ix+p3d_object_orientation+17),h
    pop hl
    ld (ix+p3d_object_orientation+14),l
    ld (ix+p3d_object_orientation+15),h
    pop hl
    ld (ix+p3d_object_orientation+12),l
    ld (ix+p3d_object_orientation+13),h
    pop hl
    ld (ix+p3d_object_orientation+10),l
    ld (ix+p3d_object_orientation+11),h
    pop hl
    ld (ix+p3d_object_orientation+8),l
    ld (ix+p3d_object_orientation+9),h
    pop hl
    ld (ix+p3d_object_orientation+6),l
    ld (ix+p3d_object_orientation+7),h
    pop hl
    ld (ix+p3d_object_orientation+4),l
    ld (ix+p3d_object_orientation+5),h
    pop hl
    ld (ix+p3d_object_orientation+2),l
    ld (ix+p3d_object_orientation+3),h
    pop hl
    ld (ix+p3d_object_orientation+0),l
    ld (ix+p3d_object_orientation+1),h
    pop hl
    ld (ix+p3d_object_pingo_rotation+4),l
    ld (ix+p3d_object_pingo_rotation+5),h
    pop hl
    ld (ix+p3d_object_pingo_rotation+2),l
    ld (ix+p3d_object_pingo_rotation+3),h
    pop hl
    ld (ix+p3d_object_pingo_rotation+0),l
    ld (ix+p3d_object_pingo_rotation+1),h
    pop hl
    ld (ix+p3d_object_dirty),l
    pop iy
    scf
    ret

; Transform a local displacement through the object's current orientation and
; add the resulting world displacement to its absolute position.
;
; inputs:
;   ix -> object state
;   iy -> local displacement vec3 in Pingo translation words
; outputs:
;   updated world position and position dirty bit when the rounded world delta
;   is nonzero
;   carry set leaves the object state unchanged
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy and the caller's displacement
p3d_object_translate_local16:
; Preserve caller pointers, then expose IX=delta and IY=orientation to the
; existing three-product row helper.
    push ix
    push iy
    push ix
    push iy
    pop ix
    pop iy
    lea iy,iy+p3d_object_orientation
    call p3d_mat3_row_mul_vec3_16
    jp c,@row0_failed
    push hl
    lea iy,iy+6
    call p3d_mat3_row_mul_vec3_16
    jp c,@row1_failed
    push hl
    lea iy,iy+6
    call p3d_mat3_row_mul_vec3_16
    jp c,@row2_failed
    push hl
; Park world X/Y/Z in BC'/DE'/HL', then restore the caller pointers.
    exx
    pop hl                  ; Z
    pop de                  ; Y
    pop bc                  ; X
    exx
    pop iy
    pop ix
; Candidate X.
    ld l,(ix+p3d_object_world_position+p3d_vec3_x)
    ld h,(ix+p3d_object_world_position+p3d_vec3_x+1)
    exx
    ld a,c
    exx
    ld e,a
    exx
    ld a,b
    exx
    ld d,a
    call p3d_sadd16
    ret c
    push hl
; Candidate Y.
    ld l,(ix+p3d_object_world_position+p3d_vec3_y)
    ld h,(ix+p3d_object_world_position+p3d_vec3_y+1)
    exx
    ld a,e
    exx
    ld e,a
    exx
    ld a,d
    exx
    ld d,a
    call p3d_sadd16
    jr c,@position_y_failed
    push hl
; Candidate Z.
    ld l,(ix+p3d_object_world_position+p3d_vec3_z)
    ld h,(ix+p3d_object_world_position+p3d_vec3_z+1)
    exx
    ld a,l
    exx
    ld e,a
    exx
    ld a,h
    exx
    ld d,a
    call p3d_sadd16
    jr c,@position_z_failed
    ld c,l
    ld b,h
    pop hl
    ld (ix+p3d_object_world_position+p3d_vec3_y),l
    ld (ix+p3d_object_world_position+p3d_vec3_y+1),h
    pop hl
    ld (ix+p3d_object_world_position+p3d_vec3_x),l
    ld (ix+p3d_object_world_position+p3d_vec3_x+1),h
    ld (ix+p3d_object_world_position+p3d_vec3_z),c
    ld (ix+p3d_object_world_position+p3d_vec3_z+1),b
; Dirty only when the rounded displacement actually changed the renderable
; position.
    exx
    ld a,b
    or c
    or d
    or e
    or h
    or l
    exx
    jr z,@position_complete
    ld a,(ix+p3d_object_dirty)
    or p3d_object_dirty_position
    ld (ix+p3d_object_dirty),a
@position_complete:
    or a
    ret
@position_z_failed:
    pop hl
@position_y_failed:
    pop hl
    scf
    ret
@row2_failed:
    pop hl
@row1_failed:
    pop hl
@row0_failed:
    pop iy
    pop ix
    scf
    ret

; Add one tick of persistent object-local angular and linear velocity.
;
; Angular residuals retain sub-bin rates. Per component:
;
;   total    = velocity + residual                   (16-bit angular wrap)
;   residual = ((total + 64) mod 128) - 64          (-64..63)
;   delta    = total - residual                     (multiple of 128)
;
; The tie rule is intentionally toward the positive phase, making residual
; -64 stable when velocity becomes zero. The tick applies rotation first,
; snaps the absolute Euler/matrix state, then translates through that resulting
; orientation. Any failure restores all 64 state bytes.
;
; inputs:
;   ix -> object state
; outputs:
;   integrated object state; carry set leaves all 64 bytes unchanged
; destroys:
;   af, af', bc, de, hl, bc', de', hl'
; preserves:
;   ix, iy
    MACRO P3D_OBJECT_ACCUMULATE_ANGLE velocity_offset, residual_offset, delta_offset
    ld l,(ix+velocity_offset)
    ld h,(ix+velocity_offset+1)
    ld e,(ix+residual_offset)
    ld d,(ix+residual_offset+1)
    add.s hl,de
    ld a,l
    add a,64
    and 0x7f
    sub 64
    ld e,a
    add a,a
    sbc a,a
    ld d,a
    ld (ix+residual_offset),e
    ld (ix+residual_offset+1),d
    or a
    sbc.s hl,de
    ld (iy+delta_offset),l
    ld (iy+delta_offset+1),h
    ENDMACRO

p3d_object_step16:
    push iy
    push ix
; Reserve a packed 64-byte snapshot followed by a six-byte local angular
; delta. SP arithmetic is full ADL; all public arithmetic remains low-16.
    ld hl,-70
    add hl,sp
    ld sp,hl
    ld iy,0
    add iy,sp
; Snapshot the entire state.
    push ix
    pop hl
    push iy
    pop de
    ld bc,p3d_object_size
    ldir
; Build the quantized angular delta while committing new internal residuals.
    P3D_OBJECT_ACCUMULATE_ANGLE p3d_object_local_angular_velocity+p3d_vec3_x,p3d_object_angular_residual+p3d_vec3_x,64+p3d_vec3_x
    P3D_OBJECT_ACCUMULATE_ANGLE p3d_object_local_angular_velocity+p3d_vec3_y,p3d_object_angular_residual+p3d_vec3_y,64+p3d_vec3_y
    P3D_OBJECT_ACCUMULATE_ANGLE p3d_object_local_angular_velocity+p3d_vec3_z,p3d_object_angular_residual+p3d_vec3_z,64+p3d_vec3_z
; Skip the expensive orientation path for the common zero-delta tick.
    ld a,(iy+64)
    or (iy+65)
    or (iy+66)
    or (iy+67)
    or (iy+68)
    or (iy+69)
    jr z,@rotation_complete
    lea iy,iy+64
    call p3d_object_rotate_local_xyz16
    jr c,@restore
@rotation_complete:
; Skip a static linear velocity as well.
    ld a,(ix+p3d_object_local_linear_velocity+0)
    or (ix+p3d_object_local_linear_velocity+1)
    or (ix+p3d_object_local_linear_velocity+2)
    or (ix+p3d_object_local_linear_velocity+3)
    or (ix+p3d_object_local_linear_velocity+4)
    or (ix+p3d_object_local_linear_velocity+5)
    jr z,@success
    lea iy,ix+p3d_object_local_linear_velocity
    call p3d_object_translate_local16
    jr c,@restore
@success:
    ld hl,70
    add hl,sp
    ld sp,hl
    pop ix
    pop iy
    or a
    ret
@restore:
; Reconstruct the local snapshot pointer because IY may currently address the
; delta or velocity field.
    ld iy,0
    add iy,sp
    push iy
    pop hl
    push ix
    pop de
    ld bc,p3d_object_size
    ldir
    ld hl,70
    add hl,sp
    ld sp,hl
    pop ix
    pop iy
    scf
    ret

; ---------------------------------------------------------------------------
; Pingo command construction and dirty-state synchronization
; ---------------------------------------------------------------------------

; Write the common prefix for a 15-byte absolute object update.
; input: A=subcommand, IX=state, IY=packet. Preserves IX/IY.
p3d_object_build_update_prefix16:
    ld (iy+6),a
    ld a,23
    ld (iy+0),a
    xor a
    ld (iy+1),a
    ld a,0xa0
    ld (iy+2),a
    ld a,(ix+p3d_object_sid)
    ld (iy+3),a
    ld a,(ix+p3d_object_sid+1)
    ld (iy+4),a
    ld a,p3d_pingo_command
    ld (iy+5),a
    ld a,(ix+p3d_object_oid)
    ld (iy+7),a
    ld a,(ix+p3d_object_oid+1)
    ld (iy+8),a
    ret

; Build the Pingo create-object packet from the IDs retained in the state.
;
; inputs:
;   ix -> object state
;   iy -> at least 13 writable bytes
; outputs:
;   packet at IY
;   HL=IY, BC=13, A=create dirty bit, carry clear
; destroys:
;   af, bc, hl
; preserves:
;   ix, iy, de and all shadow registers
p3d_object_build_create16:
    ld a,p3d_pingo_create_object
    call p3d_object_build_update_prefix16
    ld a,(ix+p3d_object_mid)
    ld (iy+9),a
    ld a,(ix+p3d_object_mid+1)
    ld (iy+10),a
    ld a,(ix+p3d_object_bmid)
    ld (iy+11),a
    ld a,(ix+p3d_object_bmid+1)
    ld (iy+12),a
    push iy
    pop hl
    ld bc,p3d_pingo_create_packet_size
    ld a,p3d_object_dirty_create
    or a
    ret

; Build the next pending Pingo command without clearing its dirty bit.
; Priority is create, scale, rotation, then translation, so a newly initialized
; object is established before its absolute state is applied.
;
; inputs:
;   ix -> object state
;   iy -> at least 15 writable bytes
; outputs:
;   A=dirty bit represented by the packet, BC=packet length, HL=IY
;   Z set, A=0, BC=0 when no known dirty bit remains
;   carry clear
; destroys:
;   af, bc, hl
; preserves:
;   ix, iy, de and all shadow registers
p3d_object_build_next_dirty16:
    ld a,(ix+p3d_object_dirty)
    bit 3,a
    jp nz,p3d_object_build_create16
    bit 2,a
    jr nz,@scale
    bit 1,a
    jr nz,@rotation
    bit 0,a
    jr nz,@translation
    push iy
    pop hl
    ld bc,0
    xor a
    ret
@scale:
    ld a,p3d_pingo_set_object_xyz_scale
    call p3d_object_build_update_prefix16
    ld a,(ix+p3d_object_scale+0)
    ld (iy+9),a
    ld a,(ix+p3d_object_scale+1)
    ld (iy+10),a
    ld a,(ix+p3d_object_scale+2)
    ld (iy+11),a
    ld a,(ix+p3d_object_scale+3)
    ld (iy+12),a
    ld a,(ix+p3d_object_scale+4)
    ld (iy+13),a
    ld a,(ix+p3d_object_scale+5)
    ld (iy+14),a
    ld a,p3d_object_dirty_scale
    jr @update_ready
@rotation:
    ld a,p3d_pingo_set_object_xyz_rotation
    call p3d_object_build_update_prefix16
    ld a,(ix+p3d_object_pingo_rotation+0)
    ld (iy+9),a
    ld a,(ix+p3d_object_pingo_rotation+1)
    ld (iy+10),a
    ld a,(ix+p3d_object_pingo_rotation+2)
    ld (iy+11),a
    ld a,(ix+p3d_object_pingo_rotation+3)
    ld (iy+12),a
    ld a,(ix+p3d_object_pingo_rotation+4)
    ld (iy+13),a
    ld a,(ix+p3d_object_pingo_rotation+5)
    ld (iy+14),a
    ld a,p3d_object_dirty_rotation
    jr @update_ready
@translation:
    ld a,p3d_pingo_set_object_xyz_translation
    call p3d_object_build_update_prefix16
    ld a,(ix+p3d_object_world_position+0)
    ld (iy+9),a
    ld a,(ix+p3d_object_world_position+1)
    ld (iy+10),a
    ld a,(ix+p3d_object_world_position+2)
    ld (iy+11),a
    ld a,(ix+p3d_object_world_position+3)
    ld (iy+12),a
    ld a,(ix+p3d_object_world_position+4)
    ld (iy+13),a
    ld a,(ix+p3d_object_world_position+5)
    ld (iy+14),a
    ld a,p3d_object_dirty_position
@update_ready:
    push iy
    pop hl
    ld bc,p3d_pingo_update_packet_size
    or a
    ret

; Send every known dirty object command through the ordinary VDU byte-stream
; entry point. Each bit is cleared after its RST.LIL $18 call returns. The VDU
; stream has no delivery-status result, so transport failure cannot be reported
; by this routine.
;
; inputs:
;   ix -> object state
;   iy -> reusable 15-byte packet scratch
; outputs:
;   known dirty bits cleared, carry clear
; destroys:
;   af, bc, de, hl and any registers destroyed by RST.LIL $18
; preserves:
;   ix, iy
p3d_object_sync16:
@next:
    call p3d_object_build_next_dirty16
    or a
    ret z
    push ix
    push iy
    push af
    rst.lil 18h
    pop af
    pop iy
    pop ix
    cpl
    and (ix+p3d_object_dirty)
    ld (ix+p3d_object_dirty),a
    jr @next
