import os
import csv
import struct
from cffi import FFI

# --- helper to convert float to float16 bits ---
def float_to_halfbits(flt: float) -> int:
    packed = struct.pack('<e', flt)
    (bits,) = struct.unpack('<H', packed)
    return bits

# --- helper to convert half bits to float ---
def halfbits_to_float(bits: int) -> float:
    packed = struct.pack('<H', bits)
    (flt,) = struct.unpack('<e', packed)
    return flt

# --- load your libf16print.so ---
ffi = FFI()
ffi.cdef("char* half_to_string(uint16_t half, char* out);")
lib_path = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "libf16print.so")
)
lib = ffi.dlopen(lib_path)

# --- paths ---
out_csv_path = os.path.join("scratch", "tests", "f16_print_all.csv")
os.makedirs(os.path.dirname(out_csv_path), exist_ok=True)

# --- open output file ---
with open(out_csv_path, mode='w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(["bits_hex", "bits_dec", "custom_text", "python_float"])

    buf = ffi.new("char[32]")

    for bits in range(0x0000, 0x8000):  # 0x0000 to 0x7FFF (sign=0 only)
        # call your C function
        cstr = lib.half_to_string(bits, buf)
        c_text = ffi.string(cstr).decode()

        # Python float interpretation
        py_float = halfbits_to_float(bits)

        writer.writerow([
            f"0x{bits:04X}",  # hex representation
            bits,             # decimal bits value
            c_text,
            repr(py_float)     # Python's float string
        ])

print(f"Done. Wrote {out_csv_path}")
