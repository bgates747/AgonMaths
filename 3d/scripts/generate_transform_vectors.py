#!/usr/bin/env python3
"""Generate deterministic vectors for all 3D transform entry points."""

from __future__ import annotations

import argparse
import random
import struct
from pathlib import Path

from generate_sincos_table import build_table


RECORD_SIZE = 32
RANDOM_CASES = 10_000
BASE_SEED = 0x3D7A5F0

OP_SINCOS = 0
OP_TRANSLATE = 1
OP_ROTX = 2
OP_ROTY = 3
OP_ROTZ = 4
OP_ROTXYZ = 5
OP_MAT3VEC = 6

SINE, COSINE = build_table()


def s16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def angle_index(raw: int) -> int:
    angle = s16(raw)
    magnitude = abs(angle)
    rounded = (magnitude + 64) >> 7
    return (-rounded if angle < 0 else rounded) & 255


def sincos(raw: int) -> tuple[int, int]:
    index = angle_index(raw)
    return SINE[index], COSINE[index]


def rounded_norm(value: int) -> int:
    magnitude = (abs(value) + 16383) // 32767
    return -magnitude if value < 0 else magnitude


def axis_rotate(
    vector: tuple[int, int, int],
    angle: int,
    axis: int,
) -> tuple[tuple[int, int, int], bool]:
    x, y, z = vector
    sine, cosine = sincos(angle)
    if axis == OP_ROTX:
        candidate = (
            x,
            rounded_norm(y * cosine - z * sine),
            rounded_norm(y * sine + z * cosine),
        )
    elif axis == OP_ROTY:
        candidate = (
            rounded_norm(x * cosine + z * sine),
            y,
            rounded_norm(z * cosine - x * sine),
        )
    else:
        candidate = (
            rounded_norm(x * cosine - y * sine),
            rounded_norm(x * sine + y * cosine),
            z,
        )
    overflow = any(not -32768 <= value <= 32767 for value in candidate)
    return (vector if overflow else candidate), overflow


def xyz_rotate(
    vector: tuple[int, int, int],
    angles: tuple[int, int, int],
) -> tuple[tuple[int, int, int], bool]:
    result = vector
    for opcode, angle in zip((OP_ROTX, OP_ROTY, OP_ROTZ), angles):
        result, overflow = axis_rotate(result, angle, opcode)
        if overflow:
            return vector, True
    return result, False


def matrix_vector(
    vector: tuple[int, int, int],
    matrix: tuple[int, ...],
) -> tuple[tuple[int, int, int], bool]:
    output = []
    for row in range(3):
        base = row * 3
        partial = vector[0] * matrix[base] + vector[1] * matrix[base + 1]
        total = partial + vector[2] * matrix[base + 2]
        if not -0x80000000 <= total <= 0x7FFFFFFF:
            return vector, True
        value = rounded_norm(total)
        if not -32768 <= value <= 32767:
            return vector, True
        output.append(value)
    return tuple(output), False


def make_record(
    opcode: int,
    vector: tuple[int, int, int] = (0, 0, 0),
    auxiliary: tuple[int, ...] = (),
    expected: tuple[int, int, int] = (0, 0, 0),
    carry: bool = False,
) -> bytes:
    record = bytearray(RECORD_SIZE)
    record[0] = opcode
    record[1] = int(carry)
    struct.pack_into("<hhh", record, 2, *map(s16, vector))
    for index, value in enumerate(auxiliary):
        struct.pack_into("<h", record, 8 + index * 2, s16(value))
    struct.pack_into("<hhh", record, 26, *map(s16, expected))
    return bytes(record)


def directed_angles() -> list[int]:
    values = {
        -32768, -32767, -32704, -32703,
        -24576, -16384, -8192, -65, -64, -63,
        0, 63, 64, 65, 8192, 16384, 24576,
        32703, 32704, 32705, 32767,
    }
    for index in range(1, 257):
        boundary = index * 128 - 64
        for delta in (-1, 0, 1):
            value = boundary + delta
            if -32768 <= value <= 32767:
                values.add(value)
            if -32768 <= -value <= 32767:
                values.add(-value)
    return sorted(values)


def build_records(random_cases: int) -> list[bytes]:
    by_op: list[list[bytes]] = [[] for _ in range(7)]

    for angle in directed_angles():
        sine, cosine = sincos(angle)
        by_op[OP_SINCOS].append(
            make_record(OP_SINCOS, (angle, 0, 0), expected=(sine, cosine, 0))
        )

    translation_values = (-32768, -32767, -1, 0, 1, 32766, 32767)
    for index, left in enumerate(translation_values):
        right = translation_values[-index - 1]
        vector = (left, right, index - 3)
        delta = (right, left, 3 - index)
        sums = tuple(vector[i] + delta[i] for i in range(3))
        overflow = any(not -32768 <= value <= 32767 for value in sums)
        expected = vector if overflow else sums
        by_op[OP_TRANSLATE].append(
            make_record(OP_TRANSLATE, vector, delta, expected, overflow)
        )
    translation_boundaries = (
        ((32767, 0, 0), (1, 0, 0)),
        ((-32768, 0, 0), (-1, 0, 0)),
        ((32766, 0, 0), (1, 0, 0)),
        ((-32767, 0, 0), (-1, 0, 0)),
        ((32767, -32768, 7), (1, -1, -7)),
    )
    for vector, delta in translation_boundaries:
        sums = tuple(vector[i] + delta[i] for i in range(3))
        overflow = any(not -32768 <= value <= 32767 for value in sums)
        by_op[OP_TRANSLATE].append(
            make_record(
                OP_TRANSLATE,
                vector,
                delta,
                vector if overflow else sums,
                overflow,
            )
        )

    basis_vectors = (
        (0, 0, 0), (32767, 0, 0), (0, 32767, 0), (0, 0, 32767),
        (-32768, 0, 0), (0, -32768, 0), (0, 0, -32768),
        (12345, -23456, 30001), (32767, 32767, 32767),
    )
    cardinal_angles = (0, 8192, -8192, 16384, 24576, 32767, -32768)
    for opcode in (OP_ROTX, OP_ROTY, OP_ROTZ):
        for vector in basis_vectors:
            for angle in cardinal_angles:
                expected, carry = axis_rotate(vector, angle, opcode)
                by_op[opcode].append(
                    make_record(opcode, vector, (angle,), expected, carry)
                )

    xyz_cases = (
        ((1234, -5678, 9012), (0, 0, 0)),
        ((1234, -5678, 9012), (8192, 0, 0)),
        ((1234, -5678, 9012), (0, 8192, 0)),
        ((1234, -5678, 9012), (0, 0, 8192)),
        ((12000, -17000, 21000), (4096, -6144, 10240)),
        ((32767, 32767, 32767), (4096, 4096, 4096)),
    )
    for vector, angles in xyz_cases:
        expected, carry = xyz_rotate(vector, angles)
        by_op[OP_ROTXYZ].append(
            make_record(OP_ROTXYZ, vector, angles, expected, carry)
        )

    identity = (32767, 0, 0, 0, 32767, 0, 0, 0, 32767)
    matrices = (
        (0,) * 9,
        identity,
        tuple(-value for value in identity),
        (0, 32767, 0, 32767, 0, 0, 0, 0, 32767),
        (32767, 0, 0, 0, 0, -32767, 0, 32767, 0),
        (0, 0, 32767, 0, 32767, 0, -32767, 0, 0),
        (0, -32767, 0, 32767, 0, 0, 0, 0, 32767),
        (32767,) * 9,
        (-32767,) * 9,
    )
    for matrix in matrices:
        for vector in basis_vectors:
            expected, carry = matrix_vector(vector, matrix)
            by_op[OP_MAT3VEC].append(
                make_record(OP_MAT3VEC, vector, matrix, expected, carry)
            )

    for opcode in range(7):
        rng = random.Random(BASE_SEED ^ (opcode * 0x9E3779B1))
        for case in range(random_cases):
            if opcode == OP_SINCOS:
                angle = rng.randint(-32768, 32767)
                sine, cosine = sincos(angle)
                record = make_record(
                    opcode, (angle, 0, 0), expected=(sine, cosine, 0),
                )
            elif opcode == OP_TRANSLATE:
                vector = tuple(rng.randint(-32768, 32767) for _ in range(3))
                delta = tuple(rng.randint(-32768, 32767) for _ in range(3))
                sums = tuple(vector[i] + delta[i] for i in range(3))
                carry = any(not -32768 <= value <= 32767 for value in sums)
                record = make_record(
                    opcode, vector, delta, vector if carry else sums, carry,
                )
            elif opcode in (OP_ROTX, OP_ROTY, OP_ROTZ):
                vector = tuple(rng.randint(-32768, 32767) for _ in range(3))
                angle = rng.randint(-32768, 32767)
                expected, carry = axis_rotate(vector, angle, opcode)
                record = make_record(
                    opcode, vector, (angle,), expected, carry,
                )
            elif opcode == OP_ROTXYZ:
                vector = tuple(rng.randint(-32768, 32767) for _ in range(3))
                angles = tuple(rng.randint(-32768, 32767) for _ in range(3))
                expected, carry = xyz_rotate(vector, angles)
                record = make_record(
                    opcode, vector, angles, expected, carry,
                )
            else:
                if case & 1:
                    vector = tuple(rng.randint(-10000, 10000) for _ in range(3))
                else:
                    vector = tuple(rng.randint(-32768, 32767) for _ in range(3))
                matrix = tuple(rng.randint(-32767, 32767) for _ in range(9))
                expected, carry = matrix_vector(vector, matrix)
                record = make_record(
                    opcode, vector, matrix, expected, carry,
                )
            by_op[opcode].append(record)

    return [record for records in by_op for record in records]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output", type=Path, default=Path("tests/3d_transform.vec"),
    )
    parser.add_argument("--random-cases", type=int, default=RANDOM_CASES)
    args = parser.parse_args()
    records = build_records(args.random_cases)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(struct.pack("<I", len(records)) + b"".join(records))
    counts = [0] * 7
    for record in records:
        counts[record[0]] += 1
    print(f"wrote {len(records)} records to {args.output}: {counts}")


if __name__ == "__main__":
    main()
