#!/usr/bin/env python3
import struct
import numpy as np
from cffi import FFI
from pathlib import Path

# -----------------------------------------------------------------------------
# Load the shared library
# -----------------------------------------------------------------------------
ffi = FFI()
# Resolve absolute path to libdragon4.so
lib_path = Path(__file__).parent / "libdragon4.so"
lib = ffi.dlopen(str(lib_path.resolve()))


ffi.cdef("""
    unsigned int Dragon4_CFFI(
        unsigned long long mantissa,
        int exponent,
        unsigned int mantissaHighBitIdx,
        int hasUnequalMargins,
        unsigned int cutoffMode,
        unsigned int cutoffNumber,
        char *outBuffer,
        unsigned int bufferSize,
        int *outExponent
    );
""")

# -----------------------------------------------------------------------------
# Helpers to call Dragon4
# -----------------------------------------------------------------------------

def float_to_components(f32):
    """Convert float32 to mantissa, exponent, mantissaHighBitIdx, hasUnequalMargins"""
    bits = struct.unpack(">I", struct.pack(">f", f32))[0]
    sign = (bits >> 31) & 0x1
    exponent = (bits >> 23) & 0xFF
    mantissa = bits & 0x7FFFFF

    if exponent == 0:
        if mantissa == 0:
            mantissa_high_bit_idx = 0
            exp = -126
            has_unequal_margins = False
        else:
            leading_zeros = 23 - mantissa.bit_length()
            mantissa <<= leading_zeros + 1
            mantissa_high_bit_idx = 23
            exp = -126 - leading_zeros
            has_unequal_margins = False
    elif exponent == 0xFF:
        mantissa_high_bit_idx = 23
        exp = 128
        has_unequal_margins = False
    else:
        mantissa |= (1 << 23)
        mantissa_high_bit_idx = 23
        exp = exponent - 127
        has_unequal_margins = (mantissa == (1 << 23))

    return mantissa, exp, mantissa_high_bit_idx, has_unequal_margins

def call_dragon4(bits):
    """Call Dragon4 using float16 input converted to float32"""
    f16 = np.frombuffer(struct.pack(">H", bits), dtype=np.float16)[0]
    f32 = np.float32(f16)

    mantissa, exponent, mantissa_high_bit_idx, has_unequal_margins = float_to_components(f32)

    out_buffer = ffi.new("char[32]")
    out_exponent = ffi.new("int[1]")

    ndigits = lib.Dragon4_CFFI(
        mantissa,
        exponent,
        mantissa_high_bit_idx,
        int(has_unequal_margins),
        0,  # CutoffMode_Unique
        0,  # CutoffNumber unused for Unique
        out_buffer,
        32,
        out_exponent
    )

    dragon4_str = ffi.string(out_buffer, ndigits).decode()
    return dragon4_str, out_exponent[0]

def dragon4_to_float(digits, exponent):
    """Convert Dragon4 output back to float for comparison"""
    if digits == "0":
        return 0.0
    return float(f"{digits}e{exponent}")

# -----------------------------------------------------------------------------
# Main test loop
# -----------------------------------------------------------------------------

def test_cases():
    test_bits = [
        0x0000, 0x0001, 0x03FF,
        0x0400, 0x3555, 0x3BFF,
        0x3C00, 0x3C01, 0x7BFF
    ]

    for bits in test_bits:
        numpy_val = np.float32(np.frombuffer(struct.pack(">H", bits), dtype=np.float16)[0])
        dragon4_str, dragon4_exp = call_dragon4(bits)
        dragon4_val = dragon4_to_float(dragon4_str, dragon4_exp)

        print(f"Bits: 0x{bits:04X}")
        print(f"Dragon4: {dragon4_val}")
        print(f"Numpy:   {numpy_val}")
        
        if not np.isclose(numpy_val, dragon4_val, rtol=1e-5, atol=1e-8):
            print("❌ Mismatch!")
        print("----------------------------------------")

# -----------------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------------

if __name__ == "__main__":
    test_cases()
