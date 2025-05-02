#!/usr/bin/env python3
import struct
import os
from SoftFloat import f16_cos_softfloat

def generate_fp16_cos_test(outfile):
    """
    Generate a binary file containing 65 536 test cases for f16_cos.
    Each record is three little-endian uint16_t’s:
      [ angle8_8, cos_bits, placeholder(=0) ]
    """
    data = bytearray()
    total = 1 << 16
    for angle8_8 in range(total):
        # call your SoftFloat‐based cose, pascosg the raw 8.8 angle
        cos_bits = f16_cos_softfloat(angle8_8)
        # pack: input angle, result bits, zero placeholder
        data += struct.pack('<HHH', angle8_8, cos_bits, 0x0000)
        if (angle8_8 & 0x1FFF) == 0:  # every 8192 cases
            print(f"\r{angle8_8}/{total} generated", end='', flush=True)
    print()

    # write out
    os.makedirs(os.path.dirname(outfile), exist_ok=True)
    with open(outfile, 'wb') as f:
        f.write(data)
    print(f"Wrote {total} test cases ({len(data)} bytes) to {outfile}")

if __name__ == "__main__":
    generate_fp16_cos_test('tests/f16_cos.bin')
