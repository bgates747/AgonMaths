#!/usr/bin/env python3
"""Generate deterministic vectors for the optional Pingo3D F2 companion.

The fixed 64-byte record keeps the target-side reader simple while allowing
the pointer APIs to carry complete matrices, object-position inputs, and exact
packet bytes.  The vector file is::

    +0  "FSV1"
    +4  uint32 record count
    +8  repeated 64-byte records

Each record contains an opcode at +0, expected carry at +1, a carry-check flag
at +2, a directed-case tag at +3, operation-specific input at +4, and expected
output at +40.  Unused bytes are deterministic sentinels.
"""

from __future__ import annotations

import argparse
import random
import struct
from collections.abc import Iterable, Sequence
from pathlib import Path

from generate_pose_vectors import (
    IDENTITY,
    matrix_vector,
    nearest_asin_phase,
    nearest_atan2_phase,
    s16,
)


MAGIC = b"FSV1"
HEADER = struct.Struct("<4sI")
RECORD_SIZE = 64
INPUT_OFFSET = 4
EXPECTED_OFFSET = 40
RANDOM_CASES = 10_000
BASE_SEED = 0xF51A6E80

OP_DAMP_RATE = 0
OP_CLAMP_RATE = 1
OP_APPLY_AXIS_MOMENT = 2
OP_DRAG_Q8 = 3
OP_CLAMP_VELOCITY = 4
OP_TRANSPOSE = 5
OP_LOCAL_POINT_WORLD = 6
OP_HORIZON = 7
OP_BUILD_ACTIVE_51 = 8
OP_BUILD_TRANSLATION_52 = 9
OP_BUILD_PROJECTION_53 = 10
OP_COUNT = 11

OP_NAMES = (
    "damp_rate",
    "clamp_rate",
    "apply_axis_moment",
    "drag_q8",
    "clamp_velocity",
    "transpose",
    "local_point_world",
    "horizon",
    "build_active_51",
    "build_translation_52",
    "build_projection_53",
)

CASE_ORDINARY = 0
CASE_MATRIX_FAILURE = 1
CASE_TRANSLATION_FAILURE = 2
CASE_NEGATIVE_32768 = 3

RATE_LIMIT = 512
VELOCITY_LIMIT = 8192
F2_MOMENTS = (8, 12, 24)
DEMANDS = (-1, 0, 1)


def pack_s16(values: Sequence[int]) -> bytes:
    return struct.pack(f"<{len(values)}h", *map(s16, values))


def make_record(
    opcode: int,
    inputs: bytes,
    expected: bytes,
    *,
    carry: bool = False,
    check_carry: bool = False,
    case_tag: int = CASE_ORDINARY,
) -> bytes:
    if not 0 <= opcode < OP_COUNT:
        raise ValueError(f"invalid opcode {opcode}")
    if len(inputs) > EXPECTED_OFFSET - INPUT_OFFSET:
        raise ValueError("input payload does not fit the record")
    if len(expected) > RECORD_SIZE - EXPECTED_OFFSET:
        raise ValueError("expected payload does not fit the record")
    record = bytearray([0xA5] * RECORD_SIZE)
    record[0] = opcode
    record[1] = int(carry)
    record[2] = int(check_carry)
    record[3] = case_tag & 0xFF
    record[INPUT_OFFSET:INPUT_OFFSET + len(inputs)] = inputs
    record[EXPECTED_OFFSET:EXPECTED_OFFSET + len(expected)] = expected
    record[60:64] = b"\xD1\xE2\xF3\x84"
    return bytes(record)


def damp_rate(value: int) -> int:
    value = s16(value)
    if value == 0:
        return 0
    decay = max(1, abs(value) >> 4)
    return value - decay if value > 0 else value + decay


def clamp_rate(value: int) -> int:
    return max(-RATE_LIMIT, min(RATE_LIMIT, s16(value)))


def apply_axis_moment(rate: int, demand: int, moment: int) -> int:
    if demand not in DEMANDS:
        raise ValueError("F2 demand must be -1, 0, or +1")
    if moment not in F2_MOMENTS:
        raise ValueError("this oracle covers the three qualified F2 moments")
    output = damp_rate(rate)
    if demand < 0:
        output -= moment
    elif demand > 0:
        output += moment
    return clamp_rate(output)


def drag_from_velocity_q8(value: int) -> int:
    return s16(-(s16(value) >> 8))


def clamp_velocity_q8(value: int) -> int:
    return max(-VELOCITY_LIMIT, min(VELOCITY_LIMIT, s16(value)))


def transpose(matrix: Sequence[int]) -> tuple[int, ...]:
    if len(matrix) != 9:
        raise ValueError("matrix must contain nine words")
    return tuple(matrix[column * 3 + row] for row in range(3) for column in range(3))


def add_vec(
    left: Sequence[int], right: Sequence[int],
) -> tuple[tuple[int, int, int], bool]:
    output = tuple(left[index] + right[index] for index in range(3))
    overflow = any(not -32768 <= value <= 32767 for value in output)
    if overflow:
        return (tuple(map(s16, left))), True
    return (output[0], output[1], output[2]), False


def local_point_world(
    matrix: Sequence[int],
    position: Sequence[int],
    local_point: Sequence[int],
) -> tuple[tuple[int, int, int], bool, int]:
    rotated, overflow = matrix_vector(matrix, local_point)
    if overflow:
        return tuple(map(s16, local_point)), True, CASE_MATRIX_FAILURE
    world, overflow = add_vec(position, rotated)
    if overflow:
        return rotated, True, CASE_TRANSLATION_FAILURE
    return world, False, CASE_ORDINARY


def horizon(matrix: Sequence[int]) -> tuple[int, int]:
    if len(matrix) != 9:
        raise ValueError("matrix must contain nine words")
    r1c0, r1c1, r1c2 = map(s16, matrix[3:6])
    pitch_input = 32767 if r1c2 == -32768 else -r1c2
    pitch = s16(nearest_asin_phase(pitch_input) * 128)
    roll = s16(nearest_atan2_phase(r1c0, r1c1) * 128)
    return pitch, roll


def packet_active(sid: int, oid: int, active: int) -> bytes:
    return bytes((
        23, 0, 0xA0,
        sid & 0xFF, (sid >> 8) & 0xFF,
        0x49, 51,
        oid & 0xFF, (oid >> 8) & 0xFF,
        active & 0xFF,
    ))


def packet_translation(sid: int, oid: int, xyz: bytes) -> bytes:
    if len(xyz) != 9:
        raise ValueError("wide translation requires exactly nine bytes")
    return bytes((
        23, 0, 0xA0,
        sid & 0xFF, (sid >> 8) & 0xFF,
        0x49, 52,
        oid & 0xFF, (oid >> 8) & 0xFF,
    )) + xyz


def packet_projection(sid: int, far_units: int) -> bytes:
    return bytes((
        23, 0, 0xA0,
        sid & 0xFF, (sid >> 8) & 0xFF,
        0x49, 53,
        far_units & 0xFF, (far_units >> 8) & 0xFF,
    ))


def f2_records(word_stride: int = 1) -> Iterable[bytes]:
    if not 1 <= word_stride <= 0x10000:
        raise ValueError("word_stride must be in 1..65536")
    for raw in range(0, 0x10000, word_stride):
        value = s16(raw)
        yield make_record(
            OP_DAMP_RATE, pack_s16((value,)), pack_s16((damp_rate(value),)),
        )
    for raw in range(0, 0x10000, word_stride):
        value = s16(raw)
        yield make_record(
            OP_CLAMP_RATE, pack_s16((value,)), pack_s16((clamp_rate(value),)),
        )
    for raw in range(0, 0x10000, word_stride):
        rate = s16(raw)
        for moment in F2_MOMENTS:
            for demand in DEMANDS:
                inputs = pack_s16((rate, moment)) + struct.pack("<b", demand)
                expected = pack_s16((apply_axis_moment(rate, demand, moment),))
                yield make_record(OP_APPLY_AXIS_MOMENT, inputs, expected)
    for raw in range(0, 0x10000, word_stride):
        value = s16(raw)
        yield make_record(
            OP_DRAG_Q8, pack_s16((value,)),
            pack_s16((drag_from_velocity_q8(value),)),
        )
    for raw in range(0, 0x10000, word_stride):
        value = s16(raw)
        yield make_record(
            OP_CLAMP_VELOCITY, pack_s16((value,)),
            pack_s16((clamp_velocity_q8(value),)),
        )


def transpose_records(random_cases: int) -> Iterable[bytes]:
    directed = (
        tuple(range(1, 10)),
        (0,) * 9,
        IDENTITY,
        (-32768, -32767, -1, 0, 1, 2, 32766, 32767, 12345),
    )
    for matrix in directed:
        yield make_record(
            OP_TRANSPOSE, pack_s16(matrix), pack_s16(transpose(matrix)),
        )
    rng = random.Random(BASE_SEED ^ OP_TRANSPOSE)
    for _ in range(random_cases):
        matrix = tuple(rng.randint(-32768, 32767) for _ in range(9))
        yield make_record(
            OP_TRANSPOSE, pack_s16(matrix), pack_s16(transpose(matrix)),
        )


def local_point_records(random_cases: int) -> Iterable[bytes]:
    negative_identity = tuple(-value for value in IDENTITY)
    directed = (
        (IDENTITY, (0, 0, 0), (0, 0, 0)),
        (IDENTITY, (100, -200, 300), (1, 2, 3)),
        (negative_identity, (100, -200, 300), (1, 2, 3)),
        ((32767,) * 9, (0, 0, 0), (32767, 32767, 32767)),
        (IDENTITY, (32767, 0, 0), (1, 0, 0)),
        (IDENTITY, (-32768, 0, 0), (-1, 0, 0)),
    )
    for matrix, position, point in directed:
        expected, carry, case_tag = local_point_world(matrix, position, point)
        inputs = pack_s16(matrix + position + point)
        yield make_record(
            OP_LOCAL_POINT_WORLD,
            inputs,
            pack_s16(expected),
            carry=carry,
            check_carry=True,
            case_tag=case_tag,
        )

    rng = random.Random(BASE_SEED ^ OP_LOCAL_POINT_WORLD)
    signed_permutations = (
        IDENTITY,
        negative_identity,
        (0, 32767, 0, 32767, 0, 0, 0, 0, 32767),
        (32767, 0, 0, 0, 0, -32767, 0, 32767, 0),
        (0, 0, 32767, 0, 32767, 0, -32767, 0, 0),
    )
    for case in range(random_cases):
        category = case & 3
        if category < 2:
            matrix = signed_permutations[rng.randrange(len(signed_permutations))]
            position = tuple(rng.randint(-20000, 20000) for _ in range(3))
            point = tuple(rng.randint(-10000, 10000) for _ in range(3))
        elif category == 2:
            sign = -32768 if case & 4 else 32767
            matrix = (sign,) * 9
            point = (sign, sign, sign)
            position = tuple(rng.randint(-100, 100) for _ in range(3))
        else:
            matrix = IDENTITY
            axis = (case >> 2) % 3
            positive = bool(case & 16)
            point_list = [0, 0, 0]
            position_list = [rng.randint(-100, 100) for _ in range(3)]
            point_list[axis] = rng.randint(1, 32767) * (1 if positive else -1)
            position_list[axis] = 32767 if positive else -32768
            point = tuple(point_list)
            position = tuple(position_list)
        expected, carry, case_tag = local_point_world(matrix, position, point)
        inputs = pack_s16(matrix + position + point)
        yield make_record(
            OP_LOCAL_POINT_WORLD,
            inputs,
            pack_s16(expected),
            carry=carry,
            check_carry=True,
            case_tag=case_tag,
        )


def horizon_records(random_cases: int) -> Iterable[bytes]:
    directed_rows = (
        (0, 32767, 0),
        (32767, 0, 0),
        (-32768, 0, 0),
        (0, -32768, 0),
        (0, 32767, -32768),
        (0, 32767, 32767),
        (0, 0, 0),
    )
    for row in directed_rows:
        matrix = list(IDENTITY)
        matrix[3:6] = row
        pitch, roll = horizon(matrix)
        tag = CASE_NEGATIVE_32768 if row[2] == -32768 else CASE_ORDINARY
        yield make_record(
            OP_HORIZON,
            pack_s16(matrix),
            pack_s16((pitch, roll)),
            carry=False,
            check_carry=True,
            case_tag=tag,
        )
    rng = random.Random(BASE_SEED ^ OP_HORIZON)
    for _ in range(random_cases):
        matrix = tuple(rng.randint(-32768, 32767) for _ in range(9))
        pitch, roll = horizon(matrix)
        yield make_record(
            OP_HORIZON,
            pack_s16(matrix),
            pack_s16((pitch, roll)),
            carry=False,
            check_carry=True,
        )


def packet_records(opcode: int, random_cases: int) -> Iterable[bytes]:
    if opcode not in (
        OP_BUILD_ACTIVE_51, OP_BUILD_TRANSLATION_52, OP_BUILD_PROJECTION_53,
    ):
        raise ValueError("not a packet opcode")
    rng = random.Random(BASE_SEED ^ opcode)
    endpoints = ((0, 0), (0xFFFF, 0xFFFF), (0x1234, 0xABCD))

    if opcode == OP_BUILD_ACTIVE_51:
        directed = (
            (0, 0, 0),
            (0, 0, 1),
            (0xFFFF, 0xFFFF, 0),
            (0xFFFF, 0xFFFF, 1),
            (0x1234, 0xABCD, 0),
            (0x1234, 0xABCD, 1),
            (0, 0xFFFF, 1),
            (0xFFFF, 0, 0),
            (0xAAAA, 0x5555, 1),
        )
        for sid, oid, active in directed:
            inputs = struct.pack("<HHB", sid, oid, active)
            yield make_record(
                opcode, inputs, packet_active(sid, oid, active),
                check_carry=True,
            )
        for _ in range(random_cases):
            sid = rng.randrange(0x10000)
            oid = rng.randrange(0x10000)
            active = rng.randrange(2)
            yield make_record(
                opcode,
                struct.pack("<HHB", sid, oid, active),
                packet_active(sid, oid, active),
                check_carry=True,
            )
        return

    if opcode == OP_BUILD_TRANSLATION_52:
        directed_xyz = (
            bytes(9),
            bytes((0xFF,) * 9),
            bytes.fromhex("000080ffff7f563412"),
        )
        for (sid, oid), xyz in zip(endpoints, directed_xyz):
            inputs = struct.pack("<HH", sid, oid) + xyz
            yield make_record(
                opcode, inputs, packet_translation(sid, oid, xyz),
                check_carry=True,
            )
        for _ in range(random_cases):
            sid = rng.randrange(0x10000)
            oid = rng.randrange(0x10000)
            xyz = bytes(rng.randrange(0x100) for _ in range(9))
            yield make_record(
                opcode,
                struct.pack("<HH", sid, oid) + xyz,
                packet_translation(sid, oid, xyz),
                check_carry=True,
            )
        return

    far_values = (0, 1, 2, 0xFFFF)
    for index, far_units in enumerate(far_values):
        sid = endpoints[index % len(endpoints)][0]
        yield make_record(
            opcode,
            struct.pack("<HH", sid, far_units),
            packet_projection(sid, far_units),
            check_carry=True,
        )
    for _ in range(random_cases):
        sid = rng.randrange(0x10000)
        far_units = rng.randrange(0x10000)
        yield make_record(
            opcode,
            struct.pack("<HH", sid, far_units),
            packet_projection(sid, far_units),
            check_carry=True,
        )


def iter_records(
    random_cases: int = RANDOM_CASES,
    word_stride: int = 1,
) -> Iterable[bytes]:
    if random_cases < 0:
        raise ValueError("random_cases must be non-negative")
    yield from f2_records(word_stride)
    yield from transpose_records(random_cases)
    yield from local_point_records(random_cases)
    yield from horizon_records(random_cases)
    for opcode in (
        OP_BUILD_ACTIVE_51, OP_BUILD_TRANSLATION_52, OP_BUILD_PROJECTION_53,
    ):
        yield from packet_records(opcode, random_cases)


def build_vectors(
    random_cases: int = RANDOM_CASES,
    word_stride: int = 1,
) -> tuple[bytes, tuple[int, ...]]:
    payload = bytearray()
    counts = [0] * OP_COUNT
    previous_opcode = 0
    for record in iter_records(random_cases, word_stride):
        opcode = record[0]
        if opcode < previous_opcode:
            raise AssertionError("records are not grouped by opcode")
        previous_opcode = opcode
        counts[opcode] += 1
        payload.extend(record)
    return HEADER.pack(MAGIC, sum(counts)) + payload, tuple(counts)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("tests/3d_fsim.vec"))
    parser.add_argument("--random-cases", type=int, default=RANDOM_CASES)
    parser.add_argument(
        "--word-stride",
        type=int,
        default=1,
        help="debug-only stride through 16-bit F2 inputs; publication uses 1",
    )
    args = parser.parse_args()
    payload, counts = build_vectors(args.random_cases, args.word_stride)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    details = ", ".join(
        f"{name}={count}" for name, count in zip(OP_NAMES, counts)
    )
    print(f"wrote {sum(counts)} records to {args.output}: {details}")


if __name__ == "__main__":
    main()
