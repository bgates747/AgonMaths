#!/usr/bin/env python3
import struct
import numpy as np
import os
from SoftFloat import float_to_f16_bits, float16_bits_to_float, f16_sqrt_python

SPECIALS = [-np.nan, np.inf, 0.0, 'subnormal']
FREQ_SPECIALS = 0.2


def generate_valid_fp16(min_val, max_val):
    if np.random.rand() < FREQ_SPECIALS:
        choice = np.random.choice(SPECIALS)
        if choice == 'subnormal':
            bits = np.random.randint(0x0001, 0x0400)
            return float16_bits_to_float(bits)
        elif choice == '-subnormal':
            bits = np.random.randint(0x0001, 0x0400)
            return float16_bits_to_float(0x8000 | bits)
        return choice

    candidate = np.random.uniform(min_val, max_val)
    if np.random.rand() < 0.5:
        candidate = -candidate
    candidate_f16 = float16_bits_to_float(float_to_f16_bits(candidate))
    return candidate_f16


def generate_fp16_sqrt_test(N, op_min, op_max, outfile):
    data = bytearray()
    for i in range(N):
        op = generate_valid_fp16(op_min, op_max)
        result = f16_sqrt_python(op)

        op_rep = np.float16(op).view(np.uint16)
        res_rep = np.float16(result).view(np.uint16)
        asm_placeholder = 0x0000

        data += struct.pack('<HHH', int(op_rep), int(res_rep), asm_placeholder)
        print(f"\r{i+1}/{N} generated", end='', flush=True)

    print()
    os.makedirs(os.path.dirname(outfile), exist_ok=True)
    with open(outfile, 'wb') as f:
        f.write(data)
    print(f"Wrote {N} test cases ({len(data)} bytes) to {outfile}")


if __name__ == "__main__":
    NUM_TESTS = 1000
    OP_MIN = -65504.0
    OP_MAX = 65504.0
    GENERATE_OUTFILE = 'tests/f16_sqrt.bin'

    generate_fp16_sqrt_test(NUM_TESTS, OP_MIN, OP_MAX, GENERATE_OUTFILE)
