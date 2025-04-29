#!/usr/bin/env python3
import struct
import csv
import os
from collections import Counter

def process_f16_print_testfile(infile, detail_outfile, summary_outfile, errors_only):
    EXPECTED_LEN = 28  # 1 for sign, 1 for decimal point, 25 for digits, 1 for null
    RECORD_SIZE = 2 + 2 * EXPECTED_LEN  # 2 bytes for half, 2 strings of length EXPECTED_LEN
    counts = Counter()

    # read all data
    data = open(infile, "rb").read()
    num_records = len(data) // RECORD_SIZE

    # prepare CSV writers
    os.makedirs(os.path.dirname(detail_outfile), exist_ok=True)
    with open(detail_outfile, "w", newline="") as df,\
         open(summary_outfile, "w", newline="") as sf:
        det_writer = csv.writer(df)
        sum_writer = csv.writer(sf)

        # Detail CSV header
        det_writer.writerow([
            "index",
            "half_bits",        # hex
            "expected_str",
            "asm_str",
            "match"             # True/False
        ])

        # Summary CSV header
        sum_writer.writerow(["status", "count"])

        # Process each record
        for i in range(num_records):
            base = i * RECORD_SIZE
            # unpack input bits
            half_bits = struct.unpack_from("<H", data, base)[0]

            # slice out the two string fields
            start_exp = base + 2
            exp_bytes = data[start_exp:start_exp + EXPECTED_LEN]
            asm_bytes = data[start_exp + EXPECTED_LEN:
                             start_exp + 2*EXPECTED_LEN]

            # decode up to the first null
            expected = exp_bytes.split(b"\0", 1)[0].decode("ascii")
            actual   = asm_bytes.split(b"\0", 1)[0].decode("ascii")

            match = (expected == actual)
            counts["correct" if match else "incorrect"] += 1

            # write detail row if not filtering or if it was an error
            if not errors_only or not match:
                det_writer.writerow([
                    i,
                    f"0x{half_bits:04X}",
                    expected,
                    actual,
                    match
                ])

        # write summary
        sum_writer.writerow(["correct",   counts["correct"]])
        sum_writer.writerow(["incorrect", counts["incorrect"]])

    print(f"Wrote detail CSV ({'errors only' if errors_only else 'all'}) to {detail_outfile}")
    print(f"Wrote summary CSV to {summary_outfile}")

if __name__ == "__main__":
    BINFILE     = "tests/f16_print.bin"
    DETAIL_CSV  = "tests/f16_print_detail.csv"
    SUMMARY_CSV = "tests/f16_print_summary.csv"
    ERRORS_ONLY = True   # set False if you want every row

    process_f16_print_testfile(BINFILE, DETAIL_CSV, SUMMARY_CSV, ERRORS_ONLY)
