#!/usr/bin/env python3
import struct
import csv
import os
import numpy as np

def process_f16_to_uq16_8_testfile(infile, detail_outfile, errors_only=False):
    # Each record is now 8 bytes:
    #   2 bytes  f16 input
    #   3 bytes  expected uq16_8 result (little-endian)
    #   3 bytes  actual   uq16_8 result (little-endian placeholder → assembly output)
    record_size = 8

    with open(infile, 'rb') as f:
        data = f.read()
    num_records = len(data) // record_size

    os.makedirs(os.path.dirname(detail_outfile), exist_ok=True)
    with open(detail_outfile, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow([
            'f16_dec',        # original half‐precision value as Python float
            'f16_hex',        # half‐precision bitpattern
            'sign',           # raw sign bit
            'exp_biased',     # raw exponent bits
            'significand',    # raw 10‐bit fraction
            'mantissa',       # 11‐bit mantissa (incl. implied 1)
            'expected_hex',   # as hex (24-bit)
            'expected_dec',   # interpreted as fixed‐point → float
            'actual_hex',     # as hex (24-bit)
            'actual_dec',     # interpreted as fixed‐point → float
            'error'
        ])
        for i in range(num_records):
            base = i * record_size
            # unpack input half
            a = struct.unpack_from('<H', data, base)[0]
            # unpack 3-byte expected and actual
            expected = int.from_bytes(data[base+2:base+5], 'little')
            actual   = int.from_bytes(data[base+5:base+8], 'little')

            # decode half→float
            f16_dec = float(np.uint16(a).view(np.float16))

            sign = (a >> 15) & 0x1
            exp  = (a >> 10) & 0x1F
            frac = a & 0x03FF
            mant = frac | (0x0400 if exp != 0 else 0)

            # decimal float values of the Q8.8 words
            expected_dec = expected / 256.0
            actual_dec   = actual   / 256.0

            error = 1 if expected != actual else 0
            if errors_only and not error:
                continue

            # if exp > 18 or exp <= 12:
            #     continue

            writer.writerow([
                f"{f16_dec}",
                f"0x{a:04X}",
                sign,
                exp,
                frac,
                mant,
                f"0x{expected:06X}",
                f"{expected_dec:.6f}",
                f"0x{actual:06X}",
                f"{actual_dec:.6f}",
                error
            ])

if __name__ == "__main__":
    # Set errors_only=True to output only the records with mismatches
    process_f16_to_uq16_8_testfile(
        infile='tests/f16_to_uq16_8.bin',
        detail_outfile='tests/f16_to_uq16_8_test_detail.csv',
        errors_only=True
    )
