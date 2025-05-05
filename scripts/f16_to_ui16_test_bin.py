#!/usr/bin/env python3
import struct
import os
from SoftFloat import lib

def generate_test_file(outfile):
    """
    Generate a binary file containing only positive test cases for f16_to_ui16.
    Each record is three little-endian uint16_t values:
      [ a_f16, result_ui16, placeholder(=0) ]
    """
    data = bytearray()
    for a in range(0,0xFFFF+1):
        result = lib.f16_to_ui16(a)
        data += struct.pack('<HHH', a, result, 0x0000)

    os.makedirs(os.path.dirname(outfile), exist_ok=True)
    with open(outfile, 'wb') as f:
        f.write(data)
    print(f"Wrote ({len(data)} bytes) to {outfile}")

if __name__ == "__main__":
    generate_test_file('tests/f16_to_ui16.bin')
