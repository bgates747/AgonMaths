#!/usr/bin/env python3
import struct
import csv
import os
import numpy as np

def process_f16_to_ui16_testfile(infile, detail_outfile, errors_only=False):
    record_size = 6

    with open(infile, 'rb') as f:
        data = f.read()
    num_records = len(data) // record_size

    os.makedirs(os.path.dirname(detail_outfile), exist_ok=True)
    with open(detail_outfile, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow([
            'f16_dec',
            'f16_hex',
            'sign',
            'exp_biased',
            'significand',
            'mantissa',
            'expected_ui16',
            'expected_hex',
            'actual_ui16',
            'actual_hex',
            'error'
        ])
        for i in range(num_records):
            a, expected, actual = struct.unpack_from('<HHH', data, i * record_size)
            f16_dec = float(np.uint16(a).view(np.float16))

            sign = (a >> 15) & 0x1
            exp  = (a >> 10) & 0x1F      # 5-bit biased exponent
            frac = a & 0x03FF            # 10-bit raw significand
            mant = frac | 0x0400 if exp != 0 else frac  # add implied 1 for normal numbers

            error = 1 if expected != actual else 0

            # If errors_only is True, skip rows without an error
            if errors_only and error == 0:
                continue

            writer.writerow([
                f"{f16_dec}",
                f"0x{a:04X}",
                sign,
                exp,
                frac,
                mant,
                expected,
                f"0x{expected:04X}",
                actual,
                f"0x{actual:04X}",
                error
            ])

if __name__ == "__main__":
    # Set errors_only=True to output only the records with mismatches
    process_f16_to_ui16_testfile(
        infile='tests/f16_to_ui16.bin',
        detail_outfile='tests/f16_to_ui16_test_detail.csv',
        errors_only=True
    )
