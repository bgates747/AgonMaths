#!/usr/bin/env python3
from cffi import FFI
import struct
import csv
from collections import defaultdict
import os

# ----------------------------
# Classification Helpers
# ----------------------------
def signF16UI(a):
    return (a >> 15) & 1

def expF16UI(a):
    return (a >> 10) & 0x1F

def fracF16UI(a):
    return a & 0x03FF

def classify_operand(f16_bits):
    s = signF16UI(f16_bits)
    e = expF16UI(f16_bits)
    f = fracF16UI(f16_bits)

    if e == 0x1F:
        if f == 0:
            return 'Inf' if s == 0 else '-Inf'
        else:
            return 'NaN' if s == 0 else '-NaN'
    elif e == 0:
        if f == 0:
            return 'Zero' if s == 0 else '-Zero'
        else:
            return 'Subnormal' if s == 0 else '-Subnormal'
    else:
        return 'Normal' if s == 0 else '-Normal'

# ----------------------------
# SoftFloat CFFI Setup
# ----------------------------
ffi = FFI()
ffi.cdef("""
    typedef uint16_t float16_t;
    typedef uint32_t float32_t;

    float16_t f32_to_f16(float32_t a);
    float16_t f16_add(float16_t a, float16_t b);
    float32_t f16_to_f32(float16_t a);
    float16_t softfloat_roundPackToF16(bool sign, int_fast16_t exp, uint_fast16_t sig);
""")
lib_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../SoftFloat-3e/build/Linux-x86_64-GCC/softfloat.so"))
lib = ffi.dlopen(lib_path)

def f16_to_f32_softfloat(a_f16):
    bits32 = lib.f16_to_f32(a_f16)
    return struct.unpack('<f', struct.pack('<I', bits32))[0]

# ----------------------------
# Main Combined Processing
# ----------------------------
def process_fp16_testfile(infile, detail_outfile, summary_outfile, errors_only=True):
    record_size = 8
    results_by_category = defaultdict(lambda: {'correct': 0, 'incorrect': 0})

    with open(infile, 'rb') as f:
        data = f.read()
    num_records = len(data) // record_size

    with open(detail_outfile, 'w', newline='') as detail_csv:
        detail_writer = csv.writer(detail_csv)
        detail_writer.writerow([
            'op1_f32', 'op2_f32', 'python_f32', 'asm_f32',
            '0xop1', '0xop2', '0xpython', '0xasm',
            'op1_cat', 'op2_cat', 'python_cat', 'asm_cat'
        ])

        for i in range(num_records):
            rec = data[i * record_size : (i + 1) * record_size]
            op1, op2, py16, asm16 = struct.unpack('<HHHH', rec)

            op1_f32 = f16_to_f32_softfloat(op1)
            op2_f32 = f16_to_f32_softfloat(op2)
            py_f32  = f16_to_f32_softfloat(py16)
            asm_f32 = f16_to_f32_softfloat(asm16)

            op1_cat = classify_operand(op1)
            op2_cat = classify_operand(op2)
            py_cat  = classify_operand(py16)
            asm_cat = classify_operand(asm16)

            is_error = (py16 != asm16)

            if not errors_only or is_error:
                detail_writer.writerow([
                    op1_f32, op2_f32, py_f32, asm_f32,
                    f"0x{op1:04X}", f"0x{op2:04X}",
                    f"0x{py16:04X}", f"0x{asm16:04X}",
                    op1_cat, op2_cat, py_cat, asm_cat
                ])

            key = (op1_cat, op2_cat, py_cat, asm_cat)
            if is_error:
                results_by_category[key]['incorrect'] += 1
            else:
                results_by_category[key]['correct'] += 1

    with open(summary_outfile, 'w', newline='') as summary_csv:
        summary_writer = csv.writer(summary_csv)
        summary_writer.writerow([
            'op1_category', 'op2_category', 'python_category', 'asm_category',
            'correct', 'incorrect'
        ])
        for (op1_cat, op2_cat, py_cat, asm_cat), counts in sorted(results_by_category.items()):
            summary_writer.writerow([
                op1_cat, op2_cat, py_cat, asm_cat,
                counts['correct'], counts['incorrect']
            ])

    print(f"Wrote detail CSV with {num_records} records to {detail_outfile} " +
          f"({'errors only' if errors_only else 'all records'})")
    print(f"Wrote summary CSV to {summary_outfile}")

# ----------------------------
# Example main
# ----------------------------
if __name__ == "__main__":
    BINFILE = 'tests/f16_add.bin'
    DETAILCSV = 'tests/f16_add_test_detail.csv'
    SUMMARYCSV = 'tests/f16_add_test_summary.csv'
    process_fp16_testfile(BINFILE, DETAILCSV, SUMMARYCSV)
