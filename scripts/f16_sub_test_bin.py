#!/usr/bin/env python3
import struct
import numpy as np
import os
from SoftFloat import (float_to_f16_bits, float16_bits_to_float, f16_sub_python)

# SPECIALS = [-np.nan, np.inf, -np.inf, 0.0, -0.0]
# SPECIALS = [-np.nan, np.inf, 0.0, 'subnormal']
# SPECIALS = [-np.nan, -np.inf, -0.0, '-subnormal']
SPECIALS = [-np.nan, np.inf, 0.0, 'subnormal']


def generate_valid_fp16(min_val, max_val):
    if np.random.rand() < FREQ_SPECIALS:
        choice = np.random.choice(SPECIALS)
        if choice == 'subnormal':
            # Smallest positive subnormal in float16 is 0x0001
            # Largest subnormal before normal starts is 0x03FF
            bits = np.random.randint(0x0001, 0x0400)
            return float16_bits_to_float(bits)
        elif choice == '-subnormal':
            # Same magnitude range as positive subnormal, but with sign bit set
            bits = np.random.randint(0x0001, 0x0400)
            return float16_bits_to_float(0x8000 | bits)
        return choice

    candidate = np.random.uniform(min_val, max_val)
    if np.random.rand() < 0.5:
        candidate = -candidate
    candidate_f16 = float16_bits_to_float(float_to_f16_bits(candidate))
    return candidate_f16

def generate_fp16_opposite_signed_pair():
    # --- inner helper unchanged from before ---
    def random_fp16(min_val, max_val, force_sign=None):
        if np.random.rand() < FREQ_SPECIALS:
            choice = np.random.choice(SPECIALS)
            if choice == 'subnormal':
                bits = np.random.randint(0x0001, 0x0400)
                if force_sign == '-':
                    bits |= 0x8000
                elif force_sign is None and np.random.rand() < 0.5:
                    bits |= 0x8000
                return float16_bits_to_float(bits)
            elif isinstance(choice, float):
                # NaNs always come back as -NaN
                if np.isnan(choice):
                    return -np.nan
                if force_sign == '-':
                    return -choice
                if force_sign == '+':
                    return +choice
                return choice
        # normal case
        val = np.random.uniform(min_val, max_val)
        if force_sign == '-':
            val = -abs(val)
        elif force_sign == '+':
            val = abs(val)
        elif np.random.rand() < 0.5:
            val = -val
        return float16_bits_to_float(float_to_f16_bits(val))

    # --- pick op1 with a truly random sign ---
    op1 = random_fp16(OP1_MIN, OP1_MAX)

    # --- pick op2 totally independent ---
    op2 = random_fp16(OP2_MIN, OP2_MAX)

    # --- now enforce opposite sign for any non‑NaN pair ---
    if not (isinstance(op1, float) and np.isnan(op1)) \
    and not (isinstance(op2, float) and np.isnan(op2)):
        # if they ended up with the same sign, flip op2
        if np.signbit(op1) == np.signbit(op2):
            op2 = -op2

    return op1, op2

def generate_fp16_sub_test(N, op1_min, op1_max, op2_min, op2_max, outfile):
    data = bytearray()
    for i in range(N):
        op1 = generate_valid_fp16(op1_min, op1_max)
        op2 = generate_valid_fp16(op2_min, op2_max)

        # op1, op2 = generate_fp16_opposite_signed_pair()

        result = f16_sub_python(op1, op2)

        op1_rep = np.float16(op1).view(np.uint16)
        op2_rep = np.float16(op2).view(np.uint16)
        res_rep = np.float16(result).view(np.uint16)
        asm_placeholder = 0x0000

        data += struct.pack('<HHHH', int(op1_rep), int(op2_rep), int(res_rep), asm_placeholder)
        print(f"\r{i+1}/{N} generated", end='', flush=True)

    print()
    os.makedirs(os.path.dirname(outfile), exist_ok=True)
    with open(outfile, 'wb') as f:
        f.write(data)
    print(f"Wrote {N} test cases ({len(data)} bytes) to {outfile}")

if __name__ == "__main__":
    NUM_TESTS = 1000000
    OP1_MIN = 0.0
    OP1_MAX = 65504.0
    OP2_MIN = 65504.0
    OP2_MAX = 0.0
    FREQ_SPECIALS = 0.2


    GENERATE_OUTFILE = 'tests/f16_sub.bin'

    generate_fp16_sub_test(NUM_TESTS, OP1_MIN, OP1_MAX, OP2_MIN, OP2_MAX, GENERATE_OUTFILE)
