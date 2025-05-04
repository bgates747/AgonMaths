#!/usr/bin/env python3
import struct
import os
from SoftFloat import uq8_8_to_f16_softfloat

def generate_test_file(outfile):
    """
    Generate a binary file containing 65 536 test cases for uq8_8_to_f16.
    Each record is three little-endian uint16_t’s:
      [ a, result_bits, placeholder(=0) ]
    """
    data = bytearray()
    total = 1 << 16
    for a in range(total):
        # call your SoftFloat‐based sine, passing the raw 8.8 angle
        result_bits = uq8_8_to_f16_softfloat(a)
        # pack: input angle, result bits, zero placeholder
        data += struct.pack('<HHH', a, result_bits, 0x0000)
        if (a & 0x1FFF) == 0:  # every 8192 cases
            print(f"\r{a}/{total} generated", end='', flush=True)
    print()

    # write out
    os.makedirs(os.path.dirname(outfile), exist_ok=True)
    with open(outfile, 'wb') as f:
        f.write(data)
    print(f"Wrote {total} test cases ({len(data)} bytes) to {outfile}")

if __name__ == "__main__":
    generate_test_file('tests/uq8_8_to_f16.bin')
