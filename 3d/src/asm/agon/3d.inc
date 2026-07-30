; eZ80-local integer 3D arithmetic for Pingo-compatible values
;
; Scalar arithmetic, trigonometric lookup, vector transforms, and matrix-vector
; multiplication are implemented below. Include 3d_sincos_table.inc after this
; file so the forward table reference resolves.
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
