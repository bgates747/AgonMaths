#!/usr/bin/env python3
import struct, os
import numpy as np
from cffi import FFI

# — adjust this path to point at your built .so —
SO_LIB = os.path.abspath(
    os.path.join(os.path.dirname(__file__),
                 "../scratch/libf16print.so")
)

# --- Setup CFFI to talk to your half_to_string ---
ffi = FFI()
ffi.cdef("""
    // match the signature from your half.c
    char* half_to_string(uint16_t half, char* out);
""")
lib = ffi.dlopen(SO_LIB)

def generate_f16_print_tests(num_cases: int, out_path: str):
    EXPECTED_LEN = 28  # 1 for sign, 1 for decimal point, 25 for digits, 1 for null
    RECORD_SIZE = 2 + 2 * EXPECTED_LEN  # 2 bytes for half, 2 strings of length EXPECTED_LEN
    data = bytearray()

    for i in range(num_cases):
        # pick a random half-float bit pattern
        bits = np.random.randint(0, 0x10000, dtype=np.uint16).item()

        # compute the expected string via your C function
        buf = ffi.new(f"char[{EXPECTED_LEN+1}]")   # +1 to be safe
        ptr = lib.half_to_string(bits, buf)
        s = ffi.string(ptr)                        # up to the first '\0'
        # pad or truncate to exactly EXPECTED_LEN
        s_fixed = s.ljust(EXPECTED_LEN, b'\0')[:EXPECTED_LEN]

        # placeholder for your assembly output
        placeholder = b'\0' * EXPECTED_LEN

        # pack: <H = input half, then raw bytes>
        data += struct.pack("<H", bits)
        data += s_fixed
        data += placeholder

        if (i+1) % 10000 == 0:
            print(f"\rGenerated {i+1}/{num_cases}", end="", flush=True)

    print()
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(data)
    total = len(data)
    print(f"Wrote {num_cases} records → {total} bytes ({total//RECORD_SIZE} cases)")

if __name__ == "__main__":
    NUM = 10_000
    OUT = "tests/f16_print.bin"
    generate_f16_print_tests(NUM, OUT)
