#!/usr/bin/env python3
import struct
import csv
import os
import numpy as np

def process_fp16_sin_testfile(infile, detail_outfile):
    record_size = 6

    with open(infile, 'rb') as f:
        data = f.read()
    num_records = len(data) // record_size

    os.makedirs(os.path.dirname(detail_outfile), exist_ok=True)
    with open(detail_outfile, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow([
            'uq8_8',
            'uq8_8_hex',
            'expected_hex',
            'expected_dec',
            'asm_hex',
            'asm_dec',
            'error'
        ])
        for i in range(num_records):
            a, expected, actual = struct.unpack_from('<HHH', data, i*record_size)
            expected_dec = float(np.uint16(expected).view(np.float16))
            actual_dec   = float(np.uint16(actual).view(np.float16))
            error = 1 if expected != actual else 0
            writer.writerow([
                f"{a}",
                f"0x{a:04X}",
                f"0x{expected:04X}",
                f"{expected_dec}",
                f"0x{actual:04X}",
                f"{actual_dec}",
                error
            ])

if __name__ == "__main__":
    process_fp16_sin_testfile(
        infile='tests/uq8_8_to_f16.bin',
        detail_outfile='tests/uq8_8_to_f16_test_detail.csv'
    )
