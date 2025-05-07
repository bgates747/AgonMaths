#!/usr/bin/env python3
import struct
import os
from SoftFloat import lib

def generate_test_file(outfile):
    """
    Generate a binary file containing only positive test cases for f16_to_uq16_8.
    Each record is:
      [ a_f16 (2 bytes), result_uq16_8 (3 bytes, little-endian), placeholder (3 zero bytes) ]
    """
    data = bytearray()
    for a in range(0, 0xFFFF + 1):
        # Call the C library, mask to 24 bits
        full_result = int(lib.f16_to_uq16_8(a)) & 0x00FFFFFF
        # Convert to 3 little-endian bytes
        result_bytes = full_result.to_bytes(4, 'little')[:3]
        # 3-byte placeholder for assembly output
        placeholder = b'\x00' * 3
        # Pack the 16-bit input, then append 3-byte result and 3-byte placeholder
        data += struct.pack('<H', a) + result_bytes + placeholder

    os.makedirs(os.path.dirname(outfile), exist_ok=True)
    with open(outfile, 'wb') as f:
        f.write(data)
    print(f"Wrote {len(data)} bytes to {outfile}")

if __name__ == "__main__":
    generate_test_file('tests/f16_to_uq16_8.bin')
