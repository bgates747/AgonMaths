#!/usr/bin/env python3
import struct
import csv
import os
from collections import Counter

def as_text(cell):
    # wrap in ="..." so the spreadsheet sees a formula that yields text
    return f'="{cell}"'

def process_f16_print_testfile(infile, detail_outfile):
    EXPECTED_LEN = 31  # 1 for sign, 1 for decimal point, 28 for digits, 1 for null
    RECORD_SIZE = 2 + 2 * EXPECTED_LEN  # 2 bytes for half, 2 strings of length EXPECTED_LEN
    counts = Counter()

    # read all data
    data = open(infile, "rb").read()
    num_records = len(data) // RECORD_SIZE

    # prepare CSV writer
    os.makedirs(os.path.dirname(detail_outfile), exist_ok=True)
    with open(detail_outfile, "w", newline="") as df:
        det_writer = csv.writer(df)

        # Detail CSV header
        det_writer.writerow([
            "index",
            "half_bits",        # hex
            "expected_str",
            "asm_str",
            "match"             # True/False
        ])

        # Process each record
        for i in range(num_records):
            base = i * RECORD_SIZE
            # unpack input bits
            half_bits = struct.unpack_from("<H", data, base)[0]

            # slice out the two string fields
            start_exp = base + 2
            exp_bytes = data[start_exp:start_exp + EXPECTED_LEN]
            asm_bytes = data[start_exp + EXPECTED_LEN:
                             start_exp + 2 * EXPECTED_LEN]

            # decode up to the first null
            expected = exp_bytes.split(b"\0", 1)[0].decode("ascii")
            actual   = asm_bytes.split(b"\0", 1)[0].decode("ascii")

            match = (expected == actual)
            counts["correct" if match else "incorrect"] += 1

            # always write detail row
            det_writer.writerow([
                i,
                f"0x{half_bits:04X}",
                as_text(expected),
                as_text(actual),
                match
            ])


    print(f"Wrote detail CSV to {detail_outfile}")

if __name__ == "__main__":
    BINFILE     = "scratch/tests/f16_print_all.bin"
    DETAIL_CSV  = "scratch/tests/f16_print_all.csv"

    process_f16_print_testfile(BINFILE, DETAIL_CSV)
