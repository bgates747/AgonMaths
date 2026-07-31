#!/usr/bin/env python3
"""Generate deterministic regression vectors for matrix, object, and camera APIs.

The oracle deliberately uses the same integer domains as the assembly:

* matrices are row-major signed Q15 with 32767 representing one;
* coarse object angles use 256 phases while camera aim retains every bit;
* matrix dot products accumulate at signed 32-bit width and narrow once;
* object updates are transactional.

The object-step oracle also retains angular quantization error in the
``angular_residual`` vec3.  It uses stable ties-toward-positive phase
quantization, retaining a residual in -64..63 without making a stationary
residual of -64 oscillate.
"""

from __future__ import annotations

import argparse
import random
import struct
from dataclasses import dataclass, replace
from math import isqrt
from pathlib import Path
from typing import Iterable, Sequence

from generate_sincos_table import build_fine_atan_table, build_table


RECORD_SIZE = 224
RANDOM_CASES = 10_000
BASE_SEED = 0x503D5EED

OP_MAT3_MUL = 0
OP_MAT3_TO_EULER = 1
OP_MAT3_ROTX = 2
OP_MAT3_ROTY = 3
OP_MAT3_ROTZ = 4
OP_MAT3_ROTXYZ = 5
OP_OBJECT_TRANSLATE = 6
OP_OBJECT_ROTX = 7
OP_OBJECT_ROTY = 8
OP_OBJECT_ROTZ = 9
OP_OBJECT_ROTXYZ = 10
OP_OBJECT_STEP = 11
OP_OBJECT_INIT = 12
OP_ATAN2 = 13
OP_ASIN = 14
OP_MAT3_FROM_EULER = 15
OP_OBJECT_BUILD_CREATE = 16
OP_OBJECT_BUILD_NEXT_DIRTY = 17
OP_CAMERA_INIT = 18
OP_CAMERA_AIM_POINT = 19
OP_CAMERA_AIM_OBJECT = 20
OP_ATAN2_FINE = 21
OP_SINCOS_FINE = 22
OP_COUNT = 23

OP_NAMES = (
    "mat3_mul",
    "mat3_to_euler",
    "mat3_rotate_x",
    "mat3_rotate_y",
    "mat3_rotate_z",
    "mat3_rotate_xyz",
    "object_translate",
    "object_rotate_x",
    "object_rotate_y",
    "object_rotate_z",
    "object_rotate_xyz",
    "object_step",
    "object_init",
    "atan2",
    "asin",
    "mat3_from_euler",
    "object_build_create",
    "object_build_next_dirty",
    "camera_init",
    "camera_aim_point",
    "camera_aim_object",
    "atan2_fine",
    "sincos_fine",
)

DIRTY_POSITION = 0x01
DIRTY_ROTATION = 0x02
DIRTY_SCALE = 0x04
DIRTY_CREATE = 0x08

STATE_SIZE = 64
STATE_DIRTY = 0
STATE_RESERVED = 1
STATE_SID = 2
STATE_OID = 4
STATE_MID = 6
STATE_BMID = 8
STATE_POSITION = 10
STATE_EULER = 16
STATE_SCALE = 22
STATE_ORIENTATION = 28
STATE_LINEAR_VELOCITY = 46
STATE_ANGULAR_VELOCITY = 52
STATE_ANGULAR_RESIDUAL = 58

CAMERA_SIZE = 36
CAMERA_DIRTY = 0
CAMERA_LOOK_FLAGS = 1
CAMERA_SID = 2
CAMERA_POSITION = 4
CAMERA_EULER = 10
CAMERA_ORIENTATION = 16
CAMERA_POLE_YAW = 34

CAMERA_DIRTY_POSITION = 0x01
CAMERA_DIRTY_ROTATION = 0x02
CAMERA_DIRTY_ALL = 0x03

CAMERA_LOOK_VALID = 0x01
CAMERA_LOOK_IN_POLE = 0x02
CAMERA_LOOK_INVERTED = 0x04

CAMERA_ROLL_UPRIGHT = 0
CAMERA_ROLL_CONTINUOUS = 1
CAMERA_ROLL_POLICY_COUNT = 2

PRIMARY_OFFSET = 4
AUX_OFFSET = 68
EXPECTED_PRIMARY_OFFSET = 132
EXPECTED_OUTPUT_OFFSET = 196
TAIL_OFFSET = 216

NORM = 32767
IDENTITY = (NORM, 0, 0, 0, NORM, 0, 0, 0, NORM)
ZERO_MATRIX = (0,) * 9
ZERO_VEC = (0, 0, 0)
SINE, COSINE = build_table()
FINE_ATAN = tuple(build_fine_atan_table())
ATAN2_BOUNDARIES = tuple(
    (
        COSINE[index] - COSINE[index + 1],
        SINE[index + 1] - SINE[index],
    )
    for index in range(32)
)
ASIN_THRESHOLDS = tuple(
    (SINE[index] + SINE[index + 1]) // 2 + 1
    for index in range(64)
)


def s16(value: int) -> int:
    """Interpret the low word of *value* as signed."""

    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def check_s16(value: int) -> bool:
    return -32768 <= value <= 32767


def phase_index(raw: int) -> int:
    """Mirror p3d_sincos16's signed magnitude rounding."""

    value = s16(raw)
    magnitude = abs(value)
    rounded = (magnitude + 64) >> 7
    return (-rounded if value < 0 else rounded) & 0xFF


def sincos(raw: int) -> tuple[int, int]:
    index = phase_index(raw)
    return SINE[index], COSINE[index]


def round_pow2_signed(value: int, half: int) -> int:
    """Divide by ``2*half``, rounding exact halves away from zero."""

    magnitude = (abs(value) + half) // (2 * half)
    return -magnitude if value < 0 else magnitude


def sincos_fine(raw: int) -> tuple[int, int]:
    """Interpolate the full weighted Q15 coefficient at 1/32768 turn."""

    angle = raw & 0x7FFF
    index = angle >> 7
    fraction = angle & 0x7F
    next_index = (index + 1) & 0xFF

    def interpolate(table: Sequence[int]) -> int:
        numerator = (
            table[index] * (128 - fraction)
            + table[next_index] * fraction
        )
        return round_pow2_signed(numerator, 64)

    return interpolate(SINE), interpolate(COSINE)


def ratio_q15(minimum: int, maximum: int) -> int:
    if minimum == maximum:
        return 0x8000
    return (minimum * 0x8000 + maximum // 2) // maximum


def atan2_fine(y: int, x: int) -> int:
    """Mirror the fine first-octant ratio/table implementation."""

    x_negative = x < 0
    y_negative = y < 0
    x_magnitude = abs(x)
    y_magnitude = abs(y)
    if x_magnitude == 0 and y_magnitude == 0:
        return 0

    steep = y_magnitude > x_magnitude
    maximum = max(x_magnitude, y_magnitude)
    minimum = min(x_magnitude, y_magnitude)
    ratio = ratio_q15(minimum, maximum)
    if ratio == 0x8000:
        octant = 4096
    else:
        index = ratio >> 9
        fraction = ratio & 0x1FF
        delta = FINE_ATAN[index + 1] - FINE_ATAN[index]
        octant = FINE_ATAN[index] + (delta * fraction + 256) // 512
    base = 8192 - octant if steep else octant

    if not x_negative and not y_negative:
        angle = base
    elif x_negative and not y_negative:
        angle = 16384 - base
        if angle == 16384:
            angle = -16384
    elif x_negative and y_negative:
        angle = base - 16384
    else:
        angle = -base
    return s16(angle)


def rounded_norm(value: int) -> int:
    """Round a signed integer divided by 32767, halves away from zero."""

    magnitude = (abs(value) + 16383) // NORM
    return -magnitude if value < 0 else magnitude


def add_s32(left: int, right: int) -> tuple[int, bool]:
    total = left + right
    return total, not -0x80000000 <= total <= 0x7FFFFFFF


def dot3(
    left: Sequence[int],
    right: Sequence[int],
) -> tuple[int, bool]:
    """Q15 dot product with the assembly's intermediate overflow checks."""

    partial, overflow = add_s32(left[0] * right[0], left[1] * right[1])
    if overflow:
        return 0, True
    total, overflow = add_s32(partial, left[2] * right[2])
    if overflow:
        return 0, True
    result = rounded_norm(total)
    return result, not check_s16(result)


def dot3_q15(
    left: Sequence[int],
    right: Sequence[int],
) -> tuple[int, bool]:
    """Matrix-orientation dot product with one-count Q15 saturation."""

    partial, overflow = add_s32(left[0] * right[0], left[1] * right[1])
    if overflow:
        return 0, True
    total, overflow = add_s32(partial, left[2] * right[2])
    if overflow:
        return 0, True
    result = rounded_norm(total)
    if result == 32768:
        return 32767, False
    if result == -32768:
        return -32767, False
    return result, not check_s16(result)


def matrix_multiply(
    left: Sequence[int],
    right: Sequence[int],
) -> tuple[tuple[int, ...], bool]:
    output: list[int] = []
    for row in range(3):
        lhs = left[row * 3:row * 3 + 3]
        for column in range(3):
            rhs = (right[column], right[column + 3], right[column + 6])
            value, overflow = dot3(lhs, rhs)
            if overflow:
                return tuple(left), True
            output.append(value)
    return tuple(output), False


def matrix_multiply_q15(
    left: Sequence[int],
    right: Sequence[int],
) -> tuple[tuple[int, ...], bool]:
    """Matrix composition with Q15 cardinal-boundary saturation."""

    output: list[int] = []
    for row in range(3):
        lhs = left[row * 3:row * 3 + 3]
        for column in range(3):
            rhs = (right[column], right[column + 3], right[column + 6])
            value, overflow = dot3_q15(lhs, rhs)
            if overflow:
                return tuple(left), True
            output.append(value)
    return tuple(output), False


def matrix_vector(
    matrix: Sequence[int],
    vector: Sequence[int],
) -> tuple[tuple[int, int, int], bool]:
    output: list[int] = []
    for row in range(3):
        value, overflow = dot3(matrix[row * 3:row * 3 + 3], vector)
        if overflow:
            return tuple(vector), True
        output.append(value)
    return (output[0], output[1], output[2]), False


def axis_matrix(axis: int, angle: int) -> tuple[int, ...]:
    sine, cosine = sincos(angle)
    if axis == 0:
        return (
            NORM, 0, 0,
            0, cosine, -sine,
            0, sine, cosine,
        )
    if axis == 1:
        return (
            cosine, 0, sine,
            0, NORM, 0,
            -sine, 0, cosine,
        )
    return (
        cosine, -sine, 0,
        sine, cosine, 0,
        0, 0, NORM,
    )


def matrix_from_euler(
    angles: Sequence[int],
) -> tuple[tuple[int, ...], bool]:
    """Build ((I*Rz)*Ry)*Rx, narrowing after every axis multiply."""

    matrix: tuple[int, ...] = IDENTITY
    for axis in (2, 1, 0):
        matrix, overflow = matrix_multiply_q15(
            matrix, axis_matrix(axis, angles[axis]),
        )
        if overflow:
            return IDENTITY, True
    return matrix, False


def camera_matrix_fine(angles: Sequence[int]) -> tuple[int, ...]:
    """Closed-form Ry(y)*Rx(x) used by the zero-roll camera solver."""

    sine_x, cosine_x = sincos_fine(angles[0])
    sine_y, cosine_y = sincos_fine(angles[1])
    return (
        cosine_y,
        rounded_norm(sine_y * sine_x),
        rounded_norm(sine_y * cosine_x),
        0,
        cosine_x,
        -sine_x,
        -sine_y,
        rounded_norm(cosine_y * sine_x),
        rounded_norm(cosine_y * cosine_x),
    )


def required_matrix_from_euler(angles: Sequence[int]) -> tuple[int, ...]:
    matrix, overflow = matrix_from_euler(angles)
    if overflow:
        raise AssertionError(f"directed Euler matrix overflowed: {angles!r}")
    return matrix


def rotate_matrix_axis(
    matrix: Sequence[int],
    angle: int,
    axis: int,
) -> tuple[tuple[int, ...], bool]:
    return matrix_multiply_q15(matrix, axis_matrix(axis, angle))


def rotate_matrix_xyz(
    matrix: Sequence[int],
    angles: Sequence[int],
) -> tuple[tuple[int, ...], bool]:
    """Apply (((R*Rz)*Ry)*Rx), narrowing after each postmultiply."""

    output = tuple(matrix)
    for axis in (2, 1, 0):
        output, overflow = matrix_multiply_q15(
            output, axis_matrix(axis, angles[axis]),
        )
        if overflow:
            return tuple(matrix), True
    return output, False


SIGNED_PHASES = tuple(range(-128, 128))
PITCH_PHASES = tuple(range(-64, 65))


def nearest_atan2_phase(y: int, x: int) -> int:
    if x == 0 and y == 0:
        return 0
    # Maximizing this dot product picks the closest lookup-table direction.
    # Python's min phase tie-break implements the documented canonical result.
    return min(
        SIGNED_PHASES,
        key=lambda phase: (
            -(x * COSINE[phase & 0xFF] + y * SINE[phase & 0xFF]),
            phase,
        ),
    )


def nearest_asin_phase(value: int) -> int:
    value = max(-NORM, min(NORM, value))
    return min(
        PITCH_PHASES,
        key=lambda phase: (
            abs(SINE[phase & 0xFF] - value),
            abs(phase),
            phase,
        ),
    )


def matrix_to_euler(matrix: Sequence[int]) -> tuple[int, int, int]:
    """Return canonical Pingo XYZ words for R = Rz * Ry * Rx."""

    y_phase = nearest_asin_phase(-matrix[6])
    if abs(y_phase) == 64:
        x_phase = nearest_atan2_phase(-matrix[5], matrix[4])
        z_phase = 0
    else:
        x_phase = nearest_atan2_phase(matrix[7], matrix[8])
        z_phase = nearest_atan2_phase(matrix[3], matrix[0])
    return (
        s16(x_phase * 128),
        s16(y_phase * 128),
        s16(z_phase * 128),
    )


def add_vec(
    left: Sequence[int],
    right: Sequence[int],
) -> tuple[tuple[int, int, int], bool]:
    result = tuple(left[index] + right[index] for index in range(3))
    overflow = any(not check_s16(value) for value in result)
    return (tuple(left) if overflow else result), overflow


def quantize_angular_step(
    velocity: Sequence[int],
    residual: Sequence[int],
) -> tuple[tuple[int, int, int], tuple[int, int, int]]:
    deltas: list[int] = []
    remainders: list[int] = []
    for rate, remainder in zip(velocity, residual):
        total = (rate + remainder) & 0xFFFF
        residual_new = ((total + 64) & 0x7F) - 64
        delta = (total - residual_new) & 0xFFFF
        deltas.append(s16(delta))
        remainders.append(residual_new)
    return (
        (deltas[0], deltas[1], deltas[2]),
        (remainders[0], remainders[1], remainders[2]),
    )


@dataclass(frozen=True)
class ObjectState:
    dirty: int
    reserved: int
    sid: int
    oid: int
    mid: int
    bmid: int
    position: tuple[int, int, int]
    euler: tuple[int, int, int]
    scale: tuple[int, int, int]
    orientation: tuple[int, ...]
    linear_velocity: tuple[int, int, int]
    angular_velocity: tuple[int, int, int]
    angular_residual: tuple[int, int, int]

    def pack(self) -> bytes:
        output = bytearray(STATE_SIZE)
        output[STATE_DIRTY] = self.dirty & 0xFF
        output[STATE_RESERVED] = self.reserved & 0xFF
        struct.pack_into(
            "<HHHH",
            output,
            STATE_SID,
            self.sid & 0xFFFF,
            self.oid & 0xFFFF,
            self.mid & 0xFFFF,
            self.bmid & 0xFFFF,
        )
        struct.pack_into("<hhh", output, STATE_POSITION, *map(s16, self.position))
        struct.pack_into("<hhh", output, STATE_EULER, *map(s16, self.euler))
        struct.pack_into("<HHH", output, STATE_SCALE, *(
            value & 0xFFFF for value in self.scale
        ))
        struct.pack_into(
            "<9h", output, STATE_ORIENTATION, *map(s16, self.orientation),
        )
        struct.pack_into(
            "<hhh",
            output,
            STATE_LINEAR_VELOCITY,
            *map(s16, self.linear_velocity),
        )
        struct.pack_into(
            "<hhh",
            output,
            STATE_ANGULAR_VELOCITY,
            *map(s16, self.angular_velocity),
        )
        struct.pack_into(
            "<hhh",
            output,
            STATE_ANGULAR_RESIDUAL,
            *map(s16, self.angular_residual),
        )
        return bytes(output)


@dataclass(frozen=True)
class CameraState:
    """The 36-byte camera state plus the untouched tail of a 64-byte slot."""

    dirty: int
    look_flags: int
    sid: int
    position: tuple[int, int, int]
    euler: tuple[int, int, int]
    orientation: tuple[int, ...]
    pole_yaw: int
    tail: bytes

    def pack(self) -> bytes:
        if len(self.tail) != STATE_SIZE - CAMERA_SIZE:
            raise ValueError("camera slot tail has the wrong size")
        output = bytearray(STATE_SIZE)
        output[CAMERA_DIRTY] = self.dirty & 0xFF
        output[CAMERA_LOOK_FLAGS] = self.look_flags & 0xFF
        struct.pack_into("<H", output, CAMERA_SID, self.sid & 0xFFFF)
        struct.pack_into(
            "<hhh", output, CAMERA_POSITION, *map(s16, self.position),
        )
        struct.pack_into("<hhh", output, CAMERA_EULER, *map(s16, self.euler))
        struct.pack_into(
            "<9h", output, CAMERA_ORIENTATION, *map(s16, self.orientation),
        )
        struct.pack_into("<h", output, CAMERA_POLE_YAW, s16(self.pole_yaw))
        output[CAMERA_SIZE:] = self.tail
        return bytes(output)


def object_translate(
    state: ObjectState,
    local_delta: Sequence[int],
) -> tuple[ObjectState, bool]:
    world_delta, overflow = matrix_vector(state.orientation, local_delta)
    if overflow:
        return state, True
    position, overflow = add_vec(state.position, world_delta)
    if overflow:
        return state, True
    dirty = state.dirty
    if position != state.position:
        dirty |= DIRTY_POSITION
    return replace(state, dirty=dirty, position=position), False


def object_rotate_axis(
    state: ObjectState,
    angle: int,
    axis: int,
) -> tuple[ObjectState, bool]:
    orientation, overflow = rotate_matrix_axis(state.orientation, angle, axis)
    if overflow:
        return state, True
    euler = matrix_to_euler(orientation)
    orientation, overflow = matrix_from_euler(euler)
    if overflow:
        return state, True
    dirty = state.dirty
    if orientation != state.orientation or euler != state.euler:
        dirty |= DIRTY_ROTATION
    return replace(
        state, dirty=dirty, orientation=orientation, euler=euler,
    ), False


def object_rotate_xyz(
    state: ObjectState,
    angles: Sequence[int],
) -> tuple[ObjectState, bool]:
    orientation, overflow = rotate_matrix_xyz(state.orientation, angles)
    if overflow:
        return state, True
    euler = matrix_to_euler(orientation)
    orientation, overflow = matrix_from_euler(euler)
    if overflow:
        return state, True
    dirty = state.dirty
    if orientation != state.orientation or euler != state.euler:
        dirty |= DIRTY_ROTATION
    return replace(
        state, dirty=dirty, orientation=orientation, euler=euler,
    ), False


def object_step(state: ObjectState) -> tuple[ObjectState, bool]:
    angles, residual = quantize_angular_step(
        state.angular_velocity, state.angular_residual,
    )
    candidate = replace(state, angular_residual=residual)
    if any(angles):
        candidate, overflow = object_rotate_xyz(candidate, angles)
        if overflow:
            return state, True
    candidate, overflow = object_translate(
        candidate, candidate.linear_velocity,
    )
    if overflow:
        return state, True
    return candidate, False


def empty_state(
    *,
    orientation: Sequence[int] = IDENTITY,
    euler: Sequence[int] | None = None,
    position: Sequence[int] = ZERO_VEC,
    linear_velocity: Sequence[int] = ZERO_VEC,
    angular_velocity: Sequence[int] = ZERO_VEC,
    angular_residual: Sequence[int] = ZERO_VEC,
    dirty: int = 0,
    reserved: int = 0xA7,
    sid: int = 0x1021,
    oid: int = 0x3243,
    mid: int = 0x5465,
    bmid: int = 0x7687,
    scale: Sequence[int] = (256, 256, 256),
) -> ObjectState:
    orientation_tuple = tuple(orientation)
    return ObjectState(
        dirty=dirty,
        reserved=reserved,
        sid=sid,
        oid=oid,
        mid=mid,
        bmid=bmid,
        position=tuple(position),
        euler=(
            tuple(euler)
            if euler is not None
            else matrix_to_euler(orientation_tuple)
        ),
        scale=tuple(scale),
        orientation=orientation_tuple,
        linear_velocity=tuple(linear_velocity),
        angular_velocity=tuple(angular_velocity),
        angular_residual=tuple(angular_residual),
    )


def initialized_state(ids: Sequence[int]) -> ObjectState:
    if len(ids) != 4:
        raise ValueError("object init requires sid, oid, mid, and bmid")
    return ObjectState(
        dirty=(
            DIRTY_CREATE | DIRTY_SCALE | DIRTY_ROTATION | DIRTY_POSITION
        ),
        reserved=0,
        sid=ids[0],
        oid=ids[1],
        mid=ids[2],
        bmid=ids[3],
        position=ZERO_VEC,
        euler=ZERO_VEC,
        scale=(256, 256, 256),
        orientation=IDENTITY,
        linear_velocity=ZERO_VEC,
        angular_velocity=ZERO_VEC,
        angular_residual=ZERO_VEC,
    )


def empty_camera(
    *,
    dirty: int = 0,
    look_flags: int = CAMERA_LOOK_VALID,
    sid: int = 0x1021,
    position: Sequence[int] = ZERO_VEC,
    euler: Sequence[int] = ZERO_VEC,
    orientation: Sequence[int] | None = None,
    pole_yaw: int = 0,
    tail: bytes | None = None,
) -> CameraState:
    euler_tuple = tuple(map(s16, euler))
    if orientation is None:
        orientation = required_matrix_from_euler(euler_tuple)
    if tail is None:
        tail = bytes((0x80 + index) & 0xFF for index in range(
            STATE_SIZE - CAMERA_SIZE
        ))
    return CameraState(
        dirty=dirty,
        look_flags=look_flags,
        sid=sid,
        position=tuple(map(s16, position)),
        euler=euler_tuple,
        orientation=tuple(map(s16, orientation)),
        pole_yaw=s16(pole_yaw),
        tail=tail,
    )


def camera_init(
    state: CameraState,
    sid: int,
    position: Sequence[int],
) -> CameraState:
    """Mirror p3d_camera_init16, including preservation past byte 35."""

    return CameraState(
        dirty=CAMERA_DIRTY_ALL,
        look_flags=CAMERA_LOOK_VALID,
        sid=sid & 0xFFFF,
        position=tuple(map(s16, position)),
        euler=ZERO_VEC,
        orientation=IDENTITY,
        pole_yaw=0,
        tail=state.tail,
    )


def add_half_turn(value: int) -> int:
    """Mirror p3d_lookat_add_half_turn16 byte for byte."""

    result = (s16(value) + 16384) & 0xFFFF
    if result & 0x4000:
        result |= 0x8000
    return s16(result)


def select_inverted_look(
    canonical_pitch: int,
    canonical_yaw: int,
    prior_pitch: int,
) -> tuple[int, int]:
    """Select the assembly's exact continuous-over-pole Euler branch."""

    pitch = s16(canonical_pitch)
    if pitch == 0:
        pitch = -16384 if s16(prior_pitch) < 0 else 16384
    elif pitch > 0:
        pitch = s16(16384 - pitch)
    else:
        pitch = s16(-16384 - pitch)
    return pitch, add_half_turn(canonical_yaw)


def look_angle_distance(left: int, right: int) -> int:
    """Shortest raw-word distance in the library's 32768-word turn."""

    difference = (s16(left) - s16(right)) & 0x7FFF
    return min(difference, 0x8000 - difference)


def select_closest_look(
    state: CameraState,
    canonical_pitch: int,
    canonical_yaw: int,
) -> tuple[int, int, bool]:
    """Choose the smaller wrapped pitch/yaw path; retain branch on a tie."""

    inverted_pitch, inverted_yaw = select_inverted_look(
        canonical_pitch, canonical_yaw, state.euler[0],
    )
    upright_distance = (
        look_angle_distance(canonical_pitch, state.euler[0])
        + look_angle_distance(canonical_yaw, state.euler[1])
    )
    inverted_distance = (
        look_angle_distance(inverted_pitch, state.euler[0])
        + look_angle_distance(inverted_yaw, state.euler[1])
    )
    choose_inverted = (
        inverted_distance < upright_distance
        or (
            inverted_distance == upright_distance
            and bool(state.look_flags & CAMERA_LOOK_INVERTED)
        )
    )
    if choose_inverted:
        return inverted_pitch, inverted_yaw, True
    return canonical_pitch, canonical_yaw, False


def normalized_look_direction(
    camera: Sequence[int],
    target: Sequence[int],
) -> tuple[
    tuple[int, int, int],
    tuple[int, int, int],
    tuple[bool, bool, bool],
] | None:
    """Return common-scaled signed direction and unsigned magnitudes."""

    differences = tuple(
        int(s16(target[index])) - int(s16(camera[index]))
        for index in range(3)
    )
    negative = tuple(value < 0 for value in differences)
    magnitudes = [abs(value) for value in differences]
    maximum = max(magnitudes)
    if maximum == 0:
        return None
    while maximum >= 2048:
        magnitudes = [value >> 1 for value in magnitudes]
        maximum >>= 1
    while maximum < 1024:
        magnitudes = [value << 1 for value in magnitudes]
        maximum <<= 1
    signed = tuple(
        -magnitude if difference < 0 else magnitude
        for difference, magnitude in zip(differences, magnitudes)
    )
    return (
        (signed[0], signed[1], signed[2]),
        (magnitudes[0], magnitudes[1], magnitudes[2]),
        (negative[0], negative[1], negative[2]),
    )


def camera_aim_at_point(
    state: CameraState,
    target: Sequence[int],
    policy: int,
) -> tuple[CameraState, bool]:
    """Integer oracle for p3d_camera_aim_at_point16."""

    if not 0 <= policy < CAMERA_ROLL_POLICY_COUNT:
        return state, True

    direction = normalized_look_direction(state.position, target)
    if direction is None:
        return state, True
    signed, magnitudes, negative = direction
    dx, dy, dz = signed
    _, abs_dy, _ = magnitudes

    horizontal_squared = magnitudes[0] ** 2 + magnitudes[2] ** 2
    horizontal = isqrt(horizontal_squared)
    if horizontal_squared - horizontal * horizontal > horizontal:
        horizontal += 1

    canonical_pitch = atan2_fine(dy, horizontal)
    exact = tuple(
        int(s16(target[index])) - int(s16(state.position[index]))
        for index in range(3)
    )
    yaw_x_magnitude = abs(exact[0])
    yaw_z_magnitude = abs(exact[2])
    while yaw_x_magnitude >= 32768 or yaw_z_magnitude >= 32768:
        yaw_x_magnitude >>= 1
        yaw_z_magnitude >>= 1
    yaw_x = -yaw_x_magnitude if exact[0] < 0 else yaw_x_magnitude
    yaw_z = -yaw_z_magnitude if exact[2] < 0 else yaw_z_magnitude
    canonical_yaw = atan2_fine(-yaw_x, -yaw_z)
    pitch = canonical_pitch
    yaw = canonical_yaw
    flags = state.look_flags & 0xFF
    pole_yaw = state.pole_yaw

    was_in_pole = bool(
        flags & CAMERA_LOOK_VALID and flags & CAMERA_LOOK_IN_POLE
    )
    threshold = abs_dy >> (4 if was_in_pole else 5)
    in_pole = horizontal <= threshold

    if in_pole and not was_in_pole:
        pitch = -8192 if negative[1] else 8192
        if flags & CAMERA_LOOK_VALID:
            pole_yaw = state.euler[1]
            if flags & CAMERA_LOOK_INVERTED:
                pole_yaw = add_half_turn(pole_yaw)
        else:
            pole_yaw = canonical_yaw
            flags &= ~CAMERA_LOOK_INVERTED
        flags |= CAMERA_LOOK_VALID | CAMERA_LOOK_IN_POLE
        if policy == CAMERA_ROLL_UPRIGHT:
            flags &= ~CAMERA_LOOK_INVERTED
        yaw = (
            add_half_turn(pole_yaw)
            if flags & CAMERA_LOOK_INVERTED
            else pole_yaw
        )
    elif in_pole:
        pitch = -8192 if negative[1] else 8192
        if not flags & CAMERA_LOOK_VALID:
            flags &= ~CAMERA_LOOK_INVERTED
        flags |= CAMERA_LOOK_VALID | CAMERA_LOOK_IN_POLE
        if policy == CAMERA_ROLL_UPRIGHT:
            flags &= ~CAMERA_LOOK_INVERTED
        yaw = (
            add_half_turn(pole_yaw)
            if flags & CAMERA_LOOK_INVERTED
            else pole_yaw
        )
    elif was_in_pole:
        flags |= CAMERA_LOOK_VALID
        flags &= ~CAMERA_LOOK_IN_POLE
        if policy == CAMERA_ROLL_UPRIGHT:
            flags &= ~CAMERA_LOOK_INVERTED
        else:
            delta_yaw = s16(canonical_yaw - pole_yaw)
            _, cosine = sincos_fine(delta_yaw)
            if cosine < 0:
                flags ^= CAMERA_LOOK_INVERTED
        if flags & CAMERA_LOOK_INVERTED:
            pitch, yaw = select_inverted_look(
                canonical_pitch, canonical_yaw, state.euler[0],
            )
    else:
        flags |= CAMERA_LOOK_VALID
        flags &= ~CAMERA_LOOK_IN_POLE
        if policy == CAMERA_ROLL_UPRIGHT:
            flags &= ~CAMERA_LOOK_INVERTED
        elif state.look_flags & CAMERA_LOOK_VALID:
            pitch, yaw, choose_inverted = select_closest_look(
                state, canonical_pitch, canonical_yaw,
            )
            if choose_inverted:
                flags |= CAMERA_LOOK_INVERTED
            else:
                flags &= ~CAMERA_LOOK_INVERTED
        if not state.look_flags & CAMERA_LOOK_VALID:
            flags &= ~CAMERA_LOOK_INVERTED
        elif (
            flags & CAMERA_LOOK_INVERTED
            and (pitch, yaw) == (canonical_pitch, canonical_yaw)
        ):
            pitch, yaw = select_inverted_look(
                canonical_pitch, canonical_yaw, state.euler[0],
            )

    euler = (s16(pitch), s16(yaw), 0)
    orientation = camera_matrix_fine(euler)
    dirty = state.dirty
    if euler != state.euler:
        dirty |= CAMERA_DIRTY_ROTATION
    return (
        replace(
            state,
            dirty=dirty,
            look_flags=flags,
            euler=euler,
            orientation=orientation,
            pole_yaw=s16(pole_yaw),
        ),
        False,
    )


def pingo_packet_prefix(
    state: ObjectState,
    subcommand: int,
) -> bytes:
    return bytes((
        23,
        0,
        0xA0,
        state.sid & 0xFF,
        (state.sid >> 8) & 0xFF,
        0x49,
        subcommand,
        state.oid & 0xFF,
        (state.oid >> 8) & 0xFF,
    ))


def expected_builder_output(
    state: ObjectState,
    *,
    force_create: bool = False,
) -> bytes:
    """Return packet[15], A, BC and two poison bytes for a builder call."""

    output = bytearray([0xD7] * 20)
    dirty = state.dirty & 0x0F
    if force_create or dirty & DIRTY_CREATE:
        packet = (
            pingo_packet_prefix(state, 5)
            + struct.pack("<HH", state.mid & 0xFFFF, state.bmid & 0xFFFF)
        )
        bit = DIRTY_CREATE
    elif dirty & DIRTY_SCALE:
        packet = pingo_packet_prefix(state, 9) + struct.pack(
            "<HHH", *(value & 0xFFFF for value in state.scale),
        )
        bit = DIRTY_SCALE
    elif dirty & DIRTY_ROTATION:
        packet = pingo_packet_prefix(state, 13) + struct.pack(
            "<HHH", *(value & 0xFFFF for value in state.euler),
        )
        bit = DIRTY_ROTATION
    elif dirty & DIRTY_POSITION:
        packet = pingo_packet_prefix(state, 17) + struct.pack(
            "<HHH", *(value & 0xFFFF for value in state.position),
        )
        bit = DIRTY_POSITION
    else:
        packet = b""
        bit = 0
    output[:len(packet)] = packet
    output[15] = bit
    length = len(packet)
    output[16] = length & 0xFF
    output[17] = (length >> 8) & 0xFF
    return bytes(output)


def random_rotation(rng: random.Random) -> tuple[tuple[int, ...], tuple[int, ...]]:
    while True:
        angles = (
            rng.randrange(-128, 128) * 128,
            rng.randrange(-64, 65) * 128,
            rng.randrange(-128, 128) * 128,
        )
        matrix, overflow = matrix_from_euler(angles)
        if not overflow:
            return matrix, angles


def random_state(
    rng: random.Random,
    *,
    stress_position: bool = False,
    stress_velocity: bool = False,
    dense_matrix: bool = False,
) -> ObjectState:
    if dense_matrix:
        orientation = tuple(rng.randint(-32767, 32767) for _ in range(9))
        euler = matrix_to_euler(orientation)
    else:
        while True:
            orientation, _ = random_rotation(rng)
            euler = matrix_to_euler(orientation)
            orientation, overflow = matrix_from_euler(euler)
            if not overflow:
                break
    position_limit = 32768 if stress_position else 24000
    velocity_limit = 32768 if stress_velocity else 6000
    return ObjectState(
        dirty=rng.randrange(256),
        reserved=rng.randrange(256),
        sid=rng.randrange(65536),
        oid=rng.randrange(65536),
        mid=rng.randrange(65536),
        bmid=rng.randrange(65536),
        position=tuple(
            rng.randrange(-position_limit, position_limit) for _ in range(3)
        ),
        euler=euler,
        scale=tuple(rng.randrange(65536) for _ in range(3)),
        orientation=orientation,
        linear_velocity=tuple(
            rng.randrange(-velocity_limit, velocity_limit) for _ in range(3)
        ),
        angular_velocity=tuple(
            rng.randrange(-32768, 32768) for _ in range(3)
        ),
        angular_residual=tuple(rng.randrange(-64, 65) for _ in range(3)),
    )


def random_camera_state(rng: random.Random, case: int) -> CameraState:
    """Build varied but matrix-consistent camera inputs with arbitrary canaries."""

    euler = (
        rng.randrange(-128, 128) * 128,
        rng.randrange(-128, 128) * 128,
        0,
    )
    orientation = required_matrix_from_euler(euler)
    known_flags = rng.randrange(8)
    if case % 11 == 0:
        known_flags &= ~CAMERA_LOOK_VALID
    look_flags = known_flags | (rng.randrange(32) << 3)
    position_values = (-32768, -32767, -1, 0, 1, 32766, 32767)
    if case % 9 == 0:
        position = tuple(rng.choice(position_values) for _ in range(3))
    else:
        position = tuple(rng.randrange(-32768, 32768) for _ in range(3))
    tail = bytes(rng.randrange(256) for _ in range(STATE_SIZE - CAMERA_SIZE))
    return empty_camera(
        dirty=rng.randrange(256),
        look_flags=look_flags,
        sid=rng.randrange(65536),
        position=position,
        euler=euler,
        orientation=orientation,
        pole_yaw=rng.randrange(-128, 128) * 128,
        tail=tail,
    )


def make_record(
    opcode: int,
    primary: ObjectState | CameraState,
    auxiliary: bytes,
    expected_primary: ObjectState | CameraState,
    *,
    expected_output: bytes = b"",
    carry: bool = False,
    mode: int = 0,
) -> bytes:
    if len(auxiliary) > STATE_SIZE:
        raise ValueError("auxiliary payload exceeds slot")
    if len(expected_output) > 20:
        raise ValueError("direct output exceeds slot")
    record = bytearray(RECORD_SIZE)
    record[0] = opcode
    record[1] = int(carry)
    record[2] = mode
    record[3] = 0xA5
    record[PRIMARY_OFFSET:PRIMARY_OFFSET + STATE_SIZE] = primary.pack()
    record[AUX_OFFSET:AUX_OFFSET + STATE_SIZE] = (
        auxiliary + bytes([0xC3]) * (STATE_SIZE - len(auxiliary))
    )
    record[
        EXPECTED_PRIMARY_OFFSET:EXPECTED_PRIMARY_OFFSET + STATE_SIZE
    ] = expected_primary.pack()
    record[
        EXPECTED_OUTPUT_OFFSET:EXPECTED_OUTPUT_OFFSET + len(expected_output)
    ] = expected_output
    record[
        EXPECTED_OUTPUT_OFFSET + len(expected_output):TAIL_OFFSET
    ] = bytes([0xD7]) * (TAIL_OFFSET - EXPECTED_OUTPUT_OFFSET - len(expected_output))
    record[TAIL_OFFSET:] = bytes(
        (0x5A ^ index) & 0xFF for index in range(RECORD_SIZE - TAIL_OFFSET)
    )
    return bytes(record)


def vec_payload(vector: Sequence[int]) -> bytes:
    return struct.pack("<hhh", *map(s16, vector))


def matrix_aux(matrix: Sequence[int]) -> bytes:
    payload = bytearray(STATE_SIZE)
    payload[:] = bytes([0xC3]) * STATE_SIZE
    struct.pack_into("<9h", payload, STATE_ORIENTATION, *map(s16, matrix))
    return bytes(payload)


def directed_records() -> list[list[bytes]]:
    by_op: list[list[bytes]] = [[] for _ in range(OP_COUNT)]
    cardinal = (
        (0, 0, 0),
        (8192, 0, 0),
        (-8192, 0, 0),
        (0, 8192, 0),
        (0, -8192, 0),
        (0, 0, 8192),
        (0, 0, -8192),
        (4096, -6144, 10240),
        (16384, 16384, 16384),
    )
    matrices = [
        IDENTITY,
        ZERO_MATRIX,
        tuple(-value for value in IDENTITY),
        required_matrix_from_euler((4096, -6144, 10240)),
        required_matrix_from_euler((-12032, 8064, 15616)),
        (32767,) * 9,
        (-32767,) * 9,
        (
            32767, 32767, 32767,
            32767, -32767, 32767,
            -32767, 32767, 32767,
        ),
    ]

    for left in matrices:
        for right in matrices:
            state = empty_state(orientation=left)
            expected_matrix, carry = matrix_multiply(left, right)
            expected = (
                state
                if carry
                else replace(state, orientation=expected_matrix)
            )
            by_op[OP_MAT3_MUL].append(
                make_record(
                    OP_MAT3_MUL,
                    state,
                    matrix_aux(right),
                    expected,
                    carry=carry,
                )
            )
        squared, carry = matrix_multiply(left, left)
        state = empty_state(orientation=left)
        expected = state if carry else replace(state, orientation=squared)
        by_op[OP_MAT3_MUL].append(
            make_record(
                OP_MAT3_MUL,
                state,
                matrix_aux(tuple(reversed(left))),
                expected,
                carry=carry,
                mode=1,
            )
        )

    euler_matrices = list(matrices[:5])
    for angles in cardinal:
        euler_matrices.append(required_matrix_from_euler(angles))
    # Values straddling the generated pitch singularity boundary.
    for r20 in (32761, 32762, 32763, 32764, -32761, -32762, -32763, -32764):
        euler_matrices.append((
            23170, 0, 23170,
            0, 32767, 0,
            r20, 0, 23170,
        ))
    for threshold in ASIN_THRESHOLDS:
        for magnitude in (threshold - 1, threshold):
            for sign in (-1, 1):
                # y extraction consumes -r20.  Other elements merely make
                # atan2 deterministic; arbitrary matrices are valid input to
                # the canonical-export primitive.
                euler_matrices.append((
                    32767, 0, 0,
                    0, 32767, 0,
                    -sign * magnitude, 0, 32767,
                ))
    for dx, dy in ATAN2_BOUNDARIES:
        # x=dy,y=dx lies exactly on the lookup-table j/j+1 tie line.
        for x_sign, y_sign in ((1, 1), (-1, 1), (-1, -1), (1, -1)):
            euler_matrices.append((
                32767, 0, 0,
                0, 32767, x_sign * y_sign * dx,
                0, y_sign * dx, x_sign * dy,
            ))
            euler_matrices.append((
                x_sign * dy, 0, 0,
                y_sign * dx, 32767, 0,
                0, 0, 32767,
            ))
    for matrix in euler_matrices:
        state = empty_state(orientation=matrix)
        output = vec_payload(matrix_to_euler(matrix))
        by_op[OP_MAT3_TO_EULER].append(
            make_record(
                OP_MAT3_TO_EULER,
                state,
                b"",
                state,
                expected_output=output,
            )
        )

    for op, axis in (
        (OP_MAT3_ROTX, 0),
        (OP_MAT3_ROTY, 1),
        (OP_MAT3_ROTZ, 2),
    ):
        for matrix in matrices:
            for angle in (-32768, -16384, -8192, -64, 0, 64, 8192, 16384, 32767):
                state = empty_state(orientation=matrix)
                output, carry = rotate_matrix_axis(matrix, angle, axis)
                expected = state if carry else replace(state, orientation=output)
                by_op[op].append(
                    make_record(
                        op,
                        state,
                        vec_payload((angle, 0, 0)),
                        expected,
                        carry=carry,
                    )
                )

    for matrix in matrices:
        for angles in cardinal:
            state = empty_state(orientation=matrix)
            output, carry = rotate_matrix_xyz(matrix, angles)
            expected = state if carry else replace(state, orientation=output)
            by_op[OP_MAT3_ROTXYZ].append(
                make_record(
                    OP_MAT3_ROTXYZ,
                    state,
                    vec_payload(angles),
                    expected,
                    carry=carry,
                )
            )

    oriented_states = [
        empty_state(orientation=required_matrix_from_euler(angles))
        for angles in cardinal
    ]
    local_deltas = (
        (0, 0, 0),
        (0, 0, -1),
        (0, 0, -1000),
        (1000, 0, 0),
        (0, 1000, 0),
        (12345, -23456, 30001),
        (32767, 32767, 32767),
    )
    for state in oriented_states:
        for local_delta in local_deltas:
            output, carry = object_translate(state, local_delta)
            by_op[OP_OBJECT_TRANSLATE].append(
                make_record(
                    OP_OBJECT_TRANSLATE,
                    state,
                    vec_payload(local_delta),
                    output,
                    carry=carry,
                )
            )
    overflow_state = empty_state(position=(32767, 0, 0))
    output, carry = object_translate(overflow_state, (1, 0, 0))
    by_op[OP_OBJECT_TRANSLATE].append(
        make_record(
            OP_OBJECT_TRANSLATE,
            overflow_state,
            vec_payload((1, 0, 0)),
            output,
            carry=carry,
        )
    )

    for op, axis in (
        (OP_OBJECT_ROTX, 0),
        (OP_OBJECT_ROTY, 1),
        (OP_OBJECT_ROTZ, 2),
    ):
        for state in oriented_states:
            for angle in (-16384, -8192, -64, 0, 64, 8192, 16384):
                output, carry = object_rotate_axis(state, angle, axis)
                by_op[op].append(
                    make_record(
                        op,
                        state,
                        vec_payload((angle, 0, 0)),
                        output,
                        carry=carry,
                    )
                )

    for state in oriented_states:
        for angles in cardinal:
            output, carry = object_rotate_xyz(state, angles)
            by_op[OP_OBJECT_ROTXYZ].append(
                make_record(
                    OP_OBJECT_ROTXYZ,
                    state,
                    vec_payload(angles),
                    output,
                    carry=carry,
                )
            )

    step_cases = (
        empty_state(),
        empty_state(linear_velocity=(0, 0, -1000)),
        empty_state(angular_velocity=(0, 8192, 0)),
        empty_state(
            linear_velocity=(0, 0, -1000),
            angular_velocity=(0, 8192, 0),
        ),
        empty_state(angular_velocity=(1, -1, 1), angular_residual=(63, -63, 0)),
        empty_state(angular_velocity=(1, -1, 1), angular_residual=(-1, 1, 63)),
        empty_state(angular_velocity=(0, 0, 0), angular_residual=(-64, 63, 0)),
        empty_state(angular_velocity=(-1, 1, -64), angular_residual=(-63, 62, 0)),
        empty_state(
            dirty=0xF0,
            scale=(128, 512, 1024),
            linear_velocity=(500, -750, 1000),
            angular_velocity=(4096, -2048, 8192),
            angular_residual=(17, -31, 63),
        ),
        empty_state(
            position=(32767, 0, 0),
            linear_velocity=(0, 0, 1),
            angular_velocity=(0, 8192, 0),
        ),
    )
    for state in step_cases:
        output, carry = object_step(state)
        by_op[OP_OBJECT_STEP].append(
            make_record(OP_OBJECT_STEP, state, b"", output, carry=carry)
        )

    # Long held-control trajectories cross every cardinal boundary repeatedly.
    # The original strict matrix narrowing froze pure +X pitch at tick 807
    # when a mathematically unit coefficient rounded to +32768.
    held_angular_velocities = (
        (152, 0, 0),
        (-152, 0, 0),
        (0, 152, 0),
        (0, -152, 0),
        (0, 0, 152),
        (0, 0, -152),
        (152, -152, 304),
        (-152, 152, -304),
    )
    for velocity in held_angular_velocities:
        state = empty_state(angular_velocity=velocity)
        for tick in range(2048):
            output, carry = object_step(state)
            if carry:
                raise AssertionError(
                    f"held rotation {velocity} failed at tick {tick}"
                )
            if output.orientation == state.orientation:
                raise AssertionError(
                    f"held rotation {velocity} froze at tick {tick}"
                )
            by_op[OP_OBJECT_STEP].append(
                make_record(
                    OP_OBJECT_STEP,
                    state,
                    b"",
                    output,
                )
            )
            state = output

    for ids in (
        (0, 0, 0, 0),
        (1, 2, 3, 4),
        (0xFFFF, 0x8000, 0x7FFF, 0x0100),
        (0x1021, 0x3243, 0x5465, 0x7687),
    ):
        state = empty_state(
            dirty=0xF0,
            reserved=0xFF,
            position=(32767, -32768, 12345),
            linear_velocity=(-1, 2, -3),
            angular_velocity=(4, -5, 6),
            angular_residual=(-64, 63, 17),
            scale=(0, 0xFFFF, 1234),
        )
        by_op[OP_OBJECT_INIT].append(
            make_record(
                OP_OBJECT_INIT,
                state,
                struct.pack("<HHHH", *ids),
                initialized_state(ids),
            )
        )

    atan_values = (-32768, -32767, -1, 0, 1, 32767)
    for y in atan_values:
        for x in atan_values:
            expected = vec_payload((nearest_atan2_phase(y, x) * 128, 0, 0))
            by_op[OP_ATAN2].append(
                make_record(
                    OP_ATAN2,
                    empty_state(),
                    struct.pack("<hh", y, x),
                    empty_state(),
                    expected_output=expected[:2],
                )
            )
    for dx, dy in ATAN2_BOUNDARIES:
        for y, x in (
            (dx, dy),
            (dx - 1, dy),
            (dx + 1, dy),
            (dx, -dy),
            (-dx, -dy),
            (-dx, dy),
        ):
            expected = struct.pack(
                "<h", s16(nearest_atan2_phase(y, x) * 128),
            )
            by_op[OP_ATAN2].append(
                make_record(
                    OP_ATAN2,
                    empty_state(),
                    struct.pack("<hh", y, x),
                    empty_state(),
                    expected_output=expected,
                )
            )

    fine_atan_values = (
        -32768, -32767, -16384, -4097, -1, 0, 1, 4095, 16384,
        32767,
    )
    for y in fine_atan_values:
        for x in fine_atan_values:
            by_op[OP_ATAN2_FINE].append(
                make_record(
                    OP_ATAN2_FINE,
                    empty_state(),
                    struct.pack("<hh", y, x),
                    empty_state(),
                    expected_output=struct.pack("<h", atan2_fine(y, x)),
                )
            )
    for maximum in (64, 511, 1024, 16384, 32767, 32768):
        for index in range(65):
            minimum = min(maximum, (maximum * index + 32) // 64)
            for y, x in (
                (minimum, maximum),
                (maximum, minimum),
                (-minimum, maximum),
                (minimum, -maximum),
            ):
                if check_s16(y) and check_s16(x):
                    by_op[OP_ATAN2_FINE].append(
                        make_record(
                            OP_ATAN2_FINE,
                            empty_state(),
                            struct.pack("<hh", y, x),
                            empty_state(),
                            expected_output=struct.pack(
                                "<h", atan2_fine(y, x),
                            ),
                        )
                    )

    fine_angles = {
        -32768, -32767, -16384, -8192, -1, 0, 1, 8192, 16384,
        32767,
    }
    for phase in range(256):
        boundary = phase * 128
        for offset in (-1, 0, 1, 63, 64, 65, 127):
            fine_angles.add(s16(boundary + offset))
    for angle in sorted(fine_angles):
        sine, cosine = sincos_fine(angle)
        by_op[OP_SINCOS_FINE].append(
            make_record(
                OP_SINCOS_FINE,
                empty_state(),
                struct.pack("<h", angle),
                empty_state(),
                expected_output=struct.pack("<hh", sine, cosine),
            )
        )

    asin_values = {-32768, -32767, -1, 0, 1, 32767}
    for threshold in ASIN_THRESHOLDS:
        for value in (threshold - 1, threshold):
            asin_values.add(value)
            asin_values.add(-value)
    for value in sorted(asin_values):
        expected = struct.pack(
            "<h", s16(nearest_asin_phase(value) * 128),
        )
        by_op[OP_ASIN].append(
            make_record(
                OP_ASIN,
                empty_state(),
                struct.pack("<h", value),
                empty_state(),
                expected_output=expected,
            )
        )

    for angles in cardinal:
        state = empty_state(
            orientation=(
                123, -456, 789,
                -1011, 1213, -1415,
                1617, -1819, 2021,
            ),
            euler=(1234, -5678, 9012),
        )
        matrix, carry = matrix_from_euler(angles)
        expected = (
            state if carry else replace(state, orientation=matrix)
        )
        by_op[OP_MAT3_FROM_EULER].append(
            make_record(
                OP_MAT3_FROM_EULER,
                state,
                vec_payload(angles),
                expected,
                carry=carry,
            )
        )

    packet_state = empty_state(
        dirty=0xFF,
        sid=0x1234,
        oid=0x5678,
        mid=0x9ABC,
        bmid=0xDEF0,
        position=(-32768, 12345, 32767),
        euler=(-16384, 8192, 16256),
        scale=(0, 256, 0xFFFF),
    )
    by_op[OP_OBJECT_BUILD_CREATE].append(
        make_record(
            OP_OBJECT_BUILD_CREATE,
            packet_state,
            b"",
            packet_state,
            expected_output=expected_builder_output(
                packet_state, force_create=True,
            ),
        )
    )
    for dirty in (
        0x00, 0x10, 0x80, 0xF0,
        0x01, 0x02, 0x04, 0x08,
        0x03, 0x05, 0x06, 0x07,
        0x09, 0x0A, 0x0C, 0x0F, 0xFF,
    ):
        state = replace(packet_state, dirty=dirty)
        by_op[OP_OBJECT_BUILD_NEXT_DIRTY].append(
            make_record(
                OP_OBJECT_BUILD_NEXT_DIRTY,
                state,
                b"",
                state,
                expected_output=expected_builder_output(state),
            )
        )

    camera_seed = empty_camera(
        dirty=0xF4,
        look_flags=0xED,
        sid=0xBEEF,
        position=(32767, -32768, 12345),
        euler=(4096, -6144, 0),
        pole_yaw=10240,
        tail=bytes(range(STATE_SIZE - CAMERA_SIZE)),
    )
    for sid, position in (
        (0, (0, 0, 0)),
        (1, (1, -1, 32767)),
        (0x8000, (-32768, 32767, -32768)),
        (0xFFFF, (12345, -23456, 30001)),
    ):
        by_op[OP_CAMERA_INIT].append(
            make_record(
                OP_CAMERA_INIT,
                camera_seed,
                struct.pack("<Hhhh", sid, *position),
                camera_init(camera_seed, sid, position),
            )
        )
    alias_sid = 0x2468
    by_op[OP_CAMERA_INIT].append(
        make_record(
            OP_CAMERA_INIT,
            camera_seed,
            struct.pack("<Hhhh", alias_sid, 111, 222, 333),
            camera_init(camera_seed, alias_sid, camera_seed.position),
            mode=1,
        )
    )

    def append_camera_aim(
        opcode: int,
        camera: CameraState,
        target: Sequence[int],
        policy: int,
    ) -> CameraState:
        expected, carry = camera_aim_at_point(camera, target, policy)
        if opcode == OP_CAMERA_AIM_POINT:
            auxiliary = vec_payload(target) + bytes((policy & 0xFF,))
        else:
            auxiliary = empty_state(
                dirty=0xF3,
                reserved=policy,
                sid=0x4567,
                oid=0x89AB,
                mid=0xCDEF,
                bmid=0x0123,
                position=target,
                linear_velocity=(111, -222, 333),
                angular_velocity=(-444, 555, -666),
            ).pack()
        by_op[opcode].append(
            make_record(
                opcode,
                camera,
                auxiliary,
                expected,
                carry=carry,
            )
        )
        return expected

    for opcode in (OP_CAMERA_AIM_POINT, OP_CAMERA_AIM_OBJECT):
        base = empty_camera(
            dirty=0xA0,
            look_flags=0xE0,
            sid=0x1234,
            position=(0, 0, 0),
            euler=(2048, -4096, 0),
            pole_yaw=6144,
            tail=bytes((0xD0 + index) & 0xFF for index in range(
                STATE_SIZE - CAMERA_SIZE
            )),
        )
        for target in (
            (1, 0, 0),
            (-1, 0, 0),
            (0, 1, 0),
            (0, -1, 0),
            (0, 0, 1),
            (0, 0, -1),
            (1, 1, 1),
            (-1, 1, -1),
            (32767, -32768, 12345),
        ):
            for policy in (CAMERA_ROLL_UPRIGHT, CAMERA_ROLL_CONTINUOUS):
                append_camera_aim(opcode, base, target, policy)

        endpoint_cases = (
            ((-32768, -32768, -32768), (32767, 32767, 32767)),
            ((32767, 32767, 32767), (-32768, -32768, -32768)),
            ((-32768, 32767, -32768), (32767, -32768, 32767)),
            ((32767, -32768, 32767), (-32768, 32767, -32768)),
        )
        for position, target in endpoint_cases:
            camera = replace(base, position=position)
            append_camera_aim(
                opcode, camera, target, CAMERA_ROLL_UPRIGHT,
            )
            append_camera_aim(
                opcode, camera, target, CAMERA_ROLL_CONTINUOUS,
            )

        for policy in (CAMERA_ROLL_UPRIGHT, CAMERA_ROLL_CONTINUOUS, 2, 255):
            append_camera_aim(opcode, base, base.position, policy)
        for invalid_policy in (2, 127, 255):
            append_camera_aim(opcode, base, (1, 2, 3), invalid_policy)

        # Exact pole-cone entry boundary: h <= |dy|/32.
        outside = replace(
            base,
            look_flags=0xA0 | CAMERA_LOOK_VALID,
            euler=(0, 0, 0),
            orientation=IDENTITY,
            pole_yaw=0,
        )
        for horizontal in (31, 32, 33):
            append_camera_aim(
                opcode,
                outside,
                (horizontal, 1024, 0),
                CAMERA_ROLL_UPRIGHT,
            )

        # Exact hysteretic retention boundary: h <= |dy|/16.
        inside = replace(
            outside,
            look_flags=(
                0xA0 | CAMERA_LOOK_VALID | CAMERA_LOOK_IN_POLE
            ),
            euler=(8192, -4096, 0),
            orientation=required_matrix_from_euler((8192, -4096, 0)),
            pole_yaw=-4096,
        )
        for horizontal in (63, 64, 65):
            append_camera_aim(
                opcode,
                inside,
                (horizontal, 1024, 0),
                CAMERA_ROLL_CONTINUOUS,
            )

        # Stateful north-then-south traversal. Each expected result is the
        # next input, exercising pole memory and the inverted branch.
        trajectory = (
            (0, 8192, -1024),
            (0, 8192, -128),
            (0, 8192, 0),
            (0, 8192, 128),
            (0, 8192, 1024),
            (0, 0, 1024),
            (0, -8192, 1024),
            (0, -8192, 128),
            (0, -8192, 0),
            (0, -8192, -128),
            (0, -8192, -1024),
            (0, 0, -1024),
        )
        for policy in (CAMERA_ROLL_UPRIGHT, CAMERA_ROLL_CONTINUOUS):
            camera = replace(
                outside,
                dirty=0,
                look_flags=0xA0 | CAMERA_LOOK_VALID,
            )
            for target in trajectory:
                camera = append_camera_aim(
                    opcode, camera, target, policy,
                )

        # One simulation sample may leap completely across the narrow hold
        # cone. Closest-branch selection remains continuous even well outside
        # the superseded 1:4 catch-up heuristic and near the horizon.
        for horizontal in (100, 300, 999):
            for vertical in (1000, -1000):
                for first_z, second_z in (
                    (-horizontal, horizontal),
                    (horizontal, -horizontal),
                ):
                    camera = replace(
                        outside,
                        dirty=0,
                        look_flags=0xA0 | CAMERA_LOOK_VALID,
                        euler=(0, 0, 0),
                        orientation=IDENTITY,
                    )
                    camera = append_camera_aim(
                        opcode,
                        camera,
                        (0, vertical, first_z),
                        CAMERA_ROLL_CONTINUOUS,
                    )
                    append_camera_aim(
                        opcode,
                        camera,
                        (0, vertical, second_z),
                        CAMERA_ROLL_CONTINUOUS,
                    )

        # Identity to exact +Z makes upright-yaw and inverted-pitch paths
        # equally long. The prior branch bit is the deterministic tie-break.
        for prior_inverted in (False, True):
            tie_camera = replace(
                outside,
                dirty=0,
                look_flags=(
                    0xA0
                    | CAMERA_LOOK_VALID
                    | (
                        CAMERA_LOOK_INVERTED
                        if prior_inverted
                        else 0
                    )
                ),
                euler=(0, 0, 0),
                orientation=IDENTITY,
            )
            append_camera_aim(
                opcode,
                tie_camera,
                (0, 0, 1000),
                CAMERA_ROLL_CONTINUOUS,
            )

        # Repeating one solved pose must not invent a new dirty bit.
        solved, carry = camera_aim_at_point(
            outside, (1234, -2345, 3000), CAMERA_ROLL_UPRIGHT,
        )
        if carry:
            raise AssertionError("directed camera aim unexpectedly failed")
        solved = replace(solved, dirty=solved.dirty & ~CAMERA_DIRTY_ROTATION)
        append_camera_aim(
            opcode,
            solved,
            (1234, -2345, 3000),
            CAMERA_ROLL_UPRIGHT,
        )

        # The inverted horizontal half-turn chooses its sign from prior pitch.
        for prior_pitch in (-128, 128):
            inverted = replace(
                outside,
                look_flags=(
                    0xA0 | CAMERA_LOOK_VALID | CAMERA_LOOK_INVERTED
                ),
                euler=(prior_pitch, 0, 0),
                orientation=required_matrix_from_euler(
                    (prior_pitch, 0, 0),
                ),
            )
            append_camera_aim(
                opcode,
                inverted,
                (0, 0, 1024),
                CAMERA_ROLL_CONTINUOUS,
            )

    return by_op


def random_records(
    random_cases: int,
    only_op: int | None = None,
) -> list[list[bytes]]:
    by_op: list[list[bytes]] = [[] for _ in range(OP_COUNT)]
    for opcode in range(OP_COUNT):
        if only_op is not None and opcode != only_op:
            continue
        rng = random.Random(BASE_SEED ^ (opcode * 0x9E3779B1))
        for case in range(random_cases):
            dense = opcode <= OP_MAT3_ROTXYZ and case % 7 == 0
            state = random_state(
                rng,
                stress_position=case % 5 == 0,
                stress_velocity=case % 6 == 0,
                dense_matrix=dense,
            )
            auxiliary = b""
            output = state
            expected_direct = b""
            carry = False
            mode = 0

            if opcode == OP_MAT3_MUL:
                if case % 9 == 0:
                    right, _ = random_rotation(rng)
                else:
                    right = tuple(
                        rng.randint(-32767, 32767) for _ in range(9)
                    )
                mode = int(case % 13 == 0)
                actual_right = state.orientation if mode else right
                matrix, carry = matrix_multiply(
                    state.orientation, actual_right,
                )
                output = (
                    state if carry else replace(state, orientation=matrix)
                )
                auxiliary = matrix_aux(right)
            elif opcode == OP_MAT3_TO_EULER:
                expected_direct = vec_payload(
                    matrix_to_euler(state.orientation),
                )
            elif opcode in (OP_MAT3_ROTX, OP_MAT3_ROTY, OP_MAT3_ROTZ):
                axis = opcode - OP_MAT3_ROTX
                angle = rng.randrange(-32768, 32768)
                matrix, carry = rotate_matrix_axis(
                    state.orientation, angle, axis,
                )
                output = (
                    state if carry else replace(state, orientation=matrix)
                )
                auxiliary = vec_payload((angle, 0, 0))
            elif opcode == OP_MAT3_ROTXYZ:
                angles = tuple(
                    rng.randrange(-32768, 32768) for _ in range(3)
                )
                matrix, carry = rotate_matrix_xyz(
                    state.orientation, angles,
                )
                output = (
                    state if carry else replace(state, orientation=matrix)
                )
                auxiliary = vec_payload(angles)
            elif opcode == OP_OBJECT_TRANSLATE:
                local_delta = tuple(
                    rng.randrange(-32768, 32768) for _ in range(3)
                )
                output, carry = object_translate(state, local_delta)
                auxiliary = vec_payload(local_delta)
            elif opcode in (
                OP_OBJECT_ROTX, OP_OBJECT_ROTY, OP_OBJECT_ROTZ,
            ):
                axis = opcode - OP_OBJECT_ROTX
                angle = rng.randrange(-32768, 32768)
                output, carry = object_rotate_axis(state, angle, axis)
                auxiliary = vec_payload((angle, 0, 0))
            elif opcode == OP_OBJECT_ROTXYZ:
                angles = tuple(
                    rng.randrange(-32768, 32768) for _ in range(3)
                )
                output, carry = object_rotate_xyz(state, angles)
                auxiliary = vec_payload(angles)
            elif opcode == OP_OBJECT_STEP:
                output, carry = object_step(state)
            elif opcode == OP_OBJECT_INIT:
                ids = tuple(rng.randrange(65536) for _ in range(4))
                output = initialized_state(ids)
                auxiliary = struct.pack("<HHHH", *ids)
            elif opcode == OP_ATAN2:
                y = rng.randrange(-32768, 32768)
                x = rng.randrange(-32768, 32768)
                auxiliary = struct.pack("<hh", y, x)
                expected_direct = struct.pack(
                    "<h", s16(nearest_atan2_phase(y, x) * 128),
                )
            elif opcode == OP_ASIN:
                value = rng.randrange(-32768, 32768)
                auxiliary = struct.pack("<h", value)
                expected_direct = struct.pack(
                    "<h", s16(nearest_asin_phase(value) * 128),
                )
            elif opcode == OP_MAT3_FROM_EULER:
                angles = tuple(
                    rng.randrange(-32768, 32768) for _ in range(3)
                )
                matrix, carry = matrix_from_euler(angles)
                output = (
                    state if carry else replace(state, orientation=matrix)
                )
                auxiliary = vec_payload(angles)
            elif opcode == OP_OBJECT_BUILD_CREATE:
                expected_direct = expected_builder_output(
                    state, force_create=True,
                )
            elif opcode == OP_OBJECT_BUILD_NEXT_DIRTY:
                expected_direct = expected_builder_output(state)
            elif opcode == OP_CAMERA_INIT:
                camera = random_camera_state(rng, case)
                sid = rng.randrange(65536)
                position = tuple(
                    rng.randrange(-32768, 32768) for _ in range(3)
                )
                state = camera
                output = camera_init(camera, sid, position)
                auxiliary = struct.pack("<Hhhh", sid, *position)
            elif opcode in (OP_CAMERA_AIM_POINT, OP_CAMERA_AIM_OBJECT):
                target_object = state
                camera = random_camera_state(rng, case)
                if case % 31 == 0:
                    target = camera.position
                elif case % 13 == 0:
                    endpoint_values = (-32768, -32767, 32766, 32767)
                    target = tuple(
                        rng.choice(endpoint_values) for _ in range(3)
                    )
                else:
                    target = tuple(
                        rng.randrange(-32768, 32768) for _ in range(3)
                    )
                if case % 29 == 0:
                    policy = rng.randrange(
                        CAMERA_ROLL_POLICY_COUNT, 256,
                    )
                else:
                    policy = rng.randrange(CAMERA_ROLL_POLICY_COUNT)
                state = camera
                output, carry = camera_aim_at_point(
                    camera, target, policy,
                )
                if opcode == OP_CAMERA_AIM_POINT:
                    auxiliary = (
                        vec_payload(target) + bytes((policy & 0xFF,))
                    )
                else:
                    auxiliary = replace(
                        target_object,
                        reserved=policy,
                        position=target,
                    ).pack()
            elif opcode == OP_ATAN2_FINE:
                y = rng.randrange(-32768, 32768)
                x = rng.randrange(-32768, 32768)
                auxiliary = struct.pack("<hh", y, x)
                expected_direct = struct.pack("<h", atan2_fine(y, x))
            elif opcode == OP_SINCOS_FINE:
                angle = rng.randrange(-32768, 32768)
                auxiliary = struct.pack("<h", angle)
                expected_direct = struct.pack(
                    "<hh", *sincos_fine(angle),
                )
            else:
                raise AssertionError(f"unhandled opcode {opcode}")

            by_op[opcode].append(
                make_record(
                    opcode,
                    state,
                    auxiliary,
                    output,
                    expected_output=expected_direct,
                    carry=carry,
                    mode=mode,
                )
            )
    return by_op


def flatten(groups: Iterable[Iterable[bytes]]) -> Iterable[bytes]:
    for group in groups:
        yield from group


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output", type=Path, default=Path("tests/3d_pose.vec"),
    )
    parser.add_argument("--random-cases", type=int, default=RANDOM_CASES)
    parser.add_argument(
        "--only-op",
        type=int,
        choices=range(OP_COUNT),
        help="emit only one opcode (diagnostic use)",
    )
    args = parser.parse_args()

    directed = directed_records()
    randoms = random_records(args.random_cases, args.only_op)
    groups = [
        directed[opcode] + randoms[opcode] for opcode in range(OP_COUNT)
    ]
    if args.only_op is not None:
        groups = [
            group if opcode == args.only_op else []
            for opcode, group in enumerate(groups)
        ]
    count = sum(map(len, groups))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("wb") as output:
        output.write(struct.pack("<I", count))
        for record in flatten(groups):
            output.write(record)
    counts = [len(group) for group in groups]
    print(f"wrote {count} records to {args.output}: {counts}")


if __name__ == "__main__":
    main()
