#!/usr/bin/env python3
import struct
import numpy as np
import os
from SoftFloat import float_to_f16_bits, float16_bits_to_float, f16_sqrt_python

SPECIALS = ['subnormal']

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
    candidate_f16 = float16_bits_to_float(float_to_f16_bits(candidate))
    return candidate_f16


def generate_fp16_sqrt_test(N, op_min, op_max, outfile):
    data = bytearray()

    # Fixed head cases: +NaN, -NaN, +Inf, -Inf, +0, -0, +1, -1
    head_values = [
        float('nan'),     # +NaN
        -float('nan'),    # -NaN
        float('inf'),     # +Inf
        -float('inf'),    # -Inf
        0.0,              # +0
        -0.0,             # -0
        1.0,              # +1
        -1.0              # -1
    ]

    for val in head_values:
        result = f16_sqrt_python(val)
        op_rep = np.float16(val).view(np.uint16)
        res_rep = np.float16(result).view(np.uint16)
        asm_placeholder = 0x0000
        data += struct.pack('<HHH', int(op_rep), int(res_rep), asm_placeholder)

    print(f"Preloaded {len(head_values)} special cases")

    # Generate remaining random values
    for i in range(N - len(head_values)):
        op = generate_valid_fp16(op_min, op_max)
        result = f16_sqrt_python(op)

        op_rep = np.float16(op).view(np.uint16)
        res_rep = np.float16(result).view(np.uint16)
        asm_placeholder = 0x0000

        data += struct.pack('<HHH', int(op_rep), int(res_rep), asm_placeholder)
        if (i + 1) % 5000 == 0:
            print(f"\r{i+1 + len(head_values)}/{N} generated", end='', flush=True)

    print()
    os.makedirs(os.path.dirname(outfile), exist_ok=True)
    with open(outfile, 'wb') as f:
        f.write(data)
    print(f"Wrote {N} test cases ({len(data)} bytes) to {outfile}")


if __name__ == "__main__":
    NUM_TESTS = 1000
    OP_MIN = 0
    OP_MAX = 65504.0
    FREQ_SPECIALS = 0.2
    GENERATE_OUTFILE = 'tests/f16_sqrt.bin'

    generate_fp16_sqrt_test(NUM_TESTS, OP_MIN, OP_MAX, GENERATE_OUTFILE)
