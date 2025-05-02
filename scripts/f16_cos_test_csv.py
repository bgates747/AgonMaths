#!/usr/bin/env python3
import struct
import csv
import os
import numpy as np

def process_fp16_cos_testfile(infile, detail_outfile):
    """
    Reads a binary file of <angle8_8, expected_bits, asm_bits> records
    and writes a CSV with:
      angle (Q8.8 → float), angle8_8_hex,
      expected_cos (hex), expected_cos (decimal),
      asm_cos      (hex), asm_cos      (decimal)
    """
    record_size = 6  # three uint16_t per record

    with open(infile, 'rb') as f:
        data = f.read()
    num_records = len(data) // record_size

    os.makedirs(os.path.dirname(detail_outfile), exist_ok=True)
    with open(detail_outfile, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow([
            'angle',           # angle8_8 / 256.0
            'angle8_8_hex',
            'expected_hex',
            'expected_dec',
            'asm_hex',
            'asm_dec',
        ])
        for i in range(num_records):
            angle8_8, expected, actual = struct.unpack_from('<HHH', data, i*record_size)
            # Convert Q8.8 integer to floating-point angle
            angle = angle8_8 / 256.0
            # Interpret half-precision bits as Python float
            expected_dec = float(np.uint16(expected).view(np.float16))
            actual_dec   = float(np.uint16(actual).view(np.float16))
            writer.writerow([
                f"{angle:.6g}",
                f"0x{angle8_8:04X}",
                f"0x{expected:04X}",
                f"{expected_dec:.6g}",
                f"0x{actual:04X}",
                f"{actual_dec:.6g}",
            ])

if __name__ == "__main__":
    process_fp16_cos_testfile(
        infile='tests/f16_cos.bin',
        detail_outfile='tests/f16_cos_test_detail.csv'
    )
