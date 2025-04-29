#!/usr/bin/env python3
import struct
import os
from cffi import FFI

# --- Setup CFFI to talk to your half_to_string ---
SO_LIB = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 "../libf16print.so")
)
ffi = FFI()
ffi.cdef("""
    char* half_to_string(uint16_t half, char* out);
""")
lib = ffi.dlopen(SO_LIB)

def generate_f16_print_tests(out_path: str):
    PY_OUTPUT_LEN = 31  # 1 for sign, 5 spaces, number, 1 for null
    ASM_OUTPUT_LEN = 31 # Placeholder, same size
    RECORD_SIZE = 2 + PY_OUTPUT_LEN + ASM_OUTPUT_LEN  # 64 bytes total per record
    data = bytearray()

    for bits in range(0x0000, 0x10000):
        buf = ffi.new(f"char[{PY_OUTPUT_LEN + 1}]")
        ptr = lib.half_to_string(bits, buf)
        s = ffi.string(ptr)

        # Remove leading sign if present in string
        if s.startswith(b'-'):
            rest = s[1:]
            sign = b'-'
        else:
            rest = s
            sign = b' '

        # Split integer and fractional parts
        if b'.' in rest:
            int_part, frac_part = rest.split(b'.', 1)
            if frac_part.rstrip(b'0') == b'':
                # No significant fractional part, treat as integer
                padded_int = int_part.rjust(5, b' ')
                full_number = sign + padded_int
            else:
                # Normal case
                padded_int = int_part.rjust(5, b' ')
                full_number = sign + padded_int + b'.' + frac_part.rstrip(b'0')
        else:
            # No fractional part
            padded_int = rest.rjust(5, b' ')
            full_number = sign + padded_int

        # Pad or truncate to exactly PY_OUTPUT_LEN
        s_fixed = full_number.ljust(PY_OUTPUT_LEN, b'\0')[:PY_OUTPUT_LEN]

        # Placeholder for future assembly output
        placeholder = b'\0' * ASM_OUTPUT_LEN

        # Pack record: bits, python output, asm placeholder
        data += struct.pack("<H", bits)
        data += s_fixed
        data += placeholder

        if (bits+1) % 4096 == 0:
            print(f"\rProcessed {bits+1}/65536", end="", flush=True)

    print()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)
    total = len(data)
    print(f"Wrote 65536 records → {total} bytes ({total//RECORD_SIZE} cases)")

if __name__ == "__main__":
    BINFILE = "scratch/tests/f16_print_all.bin"
    generate_f16_print_tests(BINFILE)
