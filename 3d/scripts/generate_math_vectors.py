#!/usr/bin/env python3
"""Generate deterministic regression vectors for the implemented 3D scalars."""

from __future__ import annotations

import argparse
import random
import struct
from pathlib import Path


RECORD = struct.Struct("<BBIIIxx")
SEED = 0x3D168

OP_UMUL = 0
OP_SMUL = 1
OP_SADD = 2
OP_SSUB = 3
OP_SDIV32767 = 4
OP_SMUL_NORM = 5


def u32(value: int) -> int:
    return value & 0xFFFFFFFF


def s16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def s32(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def rounded_norm(value: int) -> tuple[int, int]:
    quotient = (abs(value) + 16383) // 32767
    result = -quotient if value < 0 else quotient
    overflow = not -32768 <= result <= 32767
    return u32(result), int(overflow)


def directed_pairs(bits: int) -> list[tuple[int, int]]:
    if bits == 16:
        values = (0, 1, 2, 0x7FFE, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF)
    else:
        values = (
            0, 1, 2,
            0x3FFFBFFE, 0x3FFFBFFF, 0x3FFFC000,
            0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000001,
            0xBFFFC000, 0xBFFFC001, 0xFFFFFFFF,
        )
    return [(left, right) for left in values for right in values]


def build_vectors(random_cases: int) -> list[bytes]:
    rng = random.Random(SEED)
    records: list[bytes] = []

    def add(op: int, carry: int, left: int, right: int, result: int) -> None:
        records.append(RECORD.pack(op, carry, u32(left), u32(right), u32(result)))

    pairs16 = directed_pairs(16)
    pairs16.extend(
        (rng.randrange(0x10000), rng.randrange(0x10000))
        for _ in range(random_cases)
    )
    for left, right in pairs16:
        add(OP_UMUL, 0, left, right, (left & 0xFFFF) * (right & 0xFFFF))
        add(OP_SMUL, 0, left, right, s16(left) * s16(right))
        result, carry = rounded_norm(s16(left) * s16(right))
        add(OP_SMUL_NORM, carry, left, right, result)

    pairs32 = directed_pairs(32)
    pairs32.extend(
        (rng.randrange(0x100000000), rng.randrange(0x100000000))
        for _ in range(random_cases)
    )
    for left, right in pairs32:
        signed_left = s32(left)
        signed_right = s32(right)
        result = signed_left + signed_right
        add(
            OP_SADD,
            int(not -0x80000000 <= result <= 0x7FFFFFFF),
            left, right, result,
        )
        result = signed_left - signed_right
        add(
            OP_SSUB,
            int(not -0x80000000 <= result <= 0x7FFFFFFF),
            left, right, result,
        )

    dividends = {left for pair in directed_pairs(32) for left in pair}
    dividends.update(
        (
            32767 * 32767 + 16383,
            32767 * 32767 + 16384,
            32768 * 32767 + 16383,
            32768 * 32767 + 16384,
        )
    )
    dividends.update(rng.randrange(0x100000000) for _ in range(random_cases))
    for raw in sorted(u32(value) for value in dividends):
        result, carry = rounded_norm(s32(raw))
        add(OP_SDIV32767, carry, raw, 0, result)

    records.sort(key=lambda record: record[0])
    return records


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("tests/3d_math.vec"))
    parser.add_argument("--random-cases", type=int, default=10000)
    args = parser.parse_args()

    records = build_vectors(args.random_cases)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(struct.pack("<I", len(records)) + b"".join(records))
    print(f"wrote {len(records)} records to {args.output}")


if __name__ == "__main__":
    main()
