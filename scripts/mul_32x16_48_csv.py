#!/usr/bin/env python3
"""
Convert binary test results from 32x16->48 multiplication test to CSV format.

Each record is 18 bytes:
- op1 (4 bytes)
- op2 (2 bytes)
- python low32 (4 bytes)
- python high16 (2 bytes)
- asm low16 (2 bytes)
- asm mid16 (2 bytes)
- asm high16 (2 bytes)

CSV output:
op1, op2, python_result, asm_result (all hex)
"""

import struct
import csv

def bin_to_csv_48bit(infile, outfile):
    records = []

    with open(infile, 'rb') as f:
        while True:
            data = f.read(18)
            if not data or len(data) < 18:
                break

            # Unpack full 18-byte structure
            op1, op2 = struct.unpack('<I H', data[0:6])
            py_lo, py_hi = struct.unpack('<I H', data[6:12])
            asm_lo, asm_mid, asm_hi = struct.unpack('<H H H', data[12:18])

            py_full = (py_hi << 32) | py_lo
            asm_full = (asm_hi << 32) | (asm_mid << 16) | asm_lo

            records.append({
                'op1': f'0x{op1:08x}',
                'op2': f'0x{op2:04x}',
                'python_result': f'0x{py_full:012x}',
                'asm_result': f'0x{asm_full:012x}',
            })

    with open(outfile, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=['op1', 'op2', 'python_result', 'asm_result'])
        writer.writeheader()
        writer.writerows(records)

    print(f"Converted {len(records)} records from {infile} to {outfile}")
    print("Each result is shown as full 48-bit hex (0x-prefixed, 12 digits). This is the true form.")

if __name__ == "__main__":
    CONVERT_INFILE = 'tests/mul_32x16_48.bin'
    CONVERT_OUTFILE = 'tests/mul_32x16_48.csv'

    bin_to_csv_48bit(CONVERT_INFILE, CONVERT_OUTFILE)
