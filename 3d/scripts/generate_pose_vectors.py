#!/usr/bin/env python3
"""Generate deterministic regression vectors for matrix and object-pose APIs.

The oracle deliberately uses the same integer domains as the assembly:

* matrices are row-major signed Q15 with 32767 representing one;
* angles are Pingo signed-turn words and are reduced to 256 lookup phases;
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
from pathlib import Path
from typing import Iterable, Sequence

from generate_sincos_table import build_table


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
OP_COUNT = 18

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
        matrix, overflow = matrix_multiply(
            matrix, axis_matrix(axis, angles[axis]),
        )
        if overflow:
            return IDENTITY, True
    return matrix, False


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
    return matrix_multiply(matrix, axis_matrix(axis, angle))


def rotate_matrix_xyz(
    matrix: Sequence[int],
    angles: Sequence[int],
) -> tuple[tuple[int, ...], bool]:
    """Apply (((R*Rz)*Ry)*Rx), narrowing after each postmultiply."""

    output = tuple(matrix)
    for axis in (2, 1, 0):
        output, overflow = matrix_multiply(
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


def make_record(
    opcode: int,
    primary: ObjectState,
    auxiliary: bytes,
    expected_primary: ObjectState,
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
            else:
                expected_direct = expected_builder_output(state)

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
