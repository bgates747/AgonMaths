#!/usr/bin/env python3
import struct
import os
from SoftFloat import ui16_to_f16_softfloat

def generate_test_file(outfile):
    """
    Generate a binary file containing 65 536 test cases for ui16_to_f16.
    Each record is three little-endian uint16_t’s:
      [ a, result_bits, placeholder(=0) ]
    """
    data = bytearray()
    total = 1 << 16
    for a in range(total):
        result_bits = ui16_to_f16_softfloat(a)
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
    generate_test_file('tests/ui16_to_f16.bin')
