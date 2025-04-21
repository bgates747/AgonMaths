#!/usr/bin/env python3
from cffi import FFI
import struct
import numpy as np
import os

# ----------------------------
# Pure Python Helpers
# ----------------------------
def signF16UI(a):
    """Extract sign bit from float16 (bit 15)."""
    return bool((a >> 15) & 1)

def expF16UI(a):
    """Extract exponent bits [14:10] from float16 and return as signed int."""
    return (a >> 10) & 0x1F  # Returns as int, not necessarily int_fast8_t

def fracF16UI(a):
    """Extract fraction (mantissa) bits [9:0] from float16."""
    return a & 0x03FF

# ----------------------------
# SoftFloat CFFI Setup
# ----------------------------
# Load the SoftFloat shared library using CFFI.

ffi = FFI()
ffi.cdef("""
    typedef uint16_t float16_t;
    typedef uint32_t float32_t;

    // Convert a 32-bit float (given as its bit pattern) to float16.
    float16_t f32_to_f16(float32_t a);

    // Add two float16 values.
    float16_t f16_add(float16_t a, float16_t b);

    // Subtract two float16 values.
    float16_t f16_sub(float16_t a, float16_t b);

    // Multiply two float16 values.
    float16_t f16_mul(float16_t a, float16_t b);

    // Divide two float16 values.
    float16_t f16_div(float16_t a, float16_t b);

    // Square root of a float16 value.
    float16_t f16_sqrt(float16_t a);

    // Convert a float16 value to a float32 value.
    float32_t f16_to_f32(float16_t a);

    // Pack sign, exponent and sig to a float16.
    float16_t softfloat_roundPackToF16(bool sign, int_fast16_t exp, uint_fast16_t sig);

    // DEBUG: print the rounding mode
    void printRoundingModeInfo();
""")

lib_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "../SoftFloat-3e/build/Linux-x86_64-GCC/softfloat.so"))
lib = ffi.dlopen(lib_path)

# ----------------------------
# SoftFloat Functions
# ----------------------------

def float_to_float32_bits(f):
    """Convert a Python float to its 32-bit IEEE 754 representation as an integer."""
    f32 = np.float32(f)
    return struct.unpack('<I', struct.pack('<f', f32))[0]

def float_to_f16_bits(val):
    """
    Uses SoftFloat's f32_to_f16 to convert a Python float (via its float32 bit pattern)
    to a 16-bit float representation.
    Returns a 16-bit integer.
    """
    bits32 = float_to_float32_bits(val)
    return lib.f32_to_f16(bits32)

def f16_add_softfloat(a_f16, b_f16):
    """
    Adds two half-precision numbers (provided as 16-bit integers)
    using SoftFloat's f16_add.
    Returns the 16-bit integer result.
    """
    return lib.f16_add(a_f16, b_f16)

def f16_add_python(a, b):
    """
    Convenience function: add two Python floats interpreted as float16,
    using SoftFloat's f16_add, and return a Python float result.
    """
    a_bits = np.float16(a).view(np.uint16)
    b_bits = np.float16(b).view(np.uint16)
    res_bits = lib.f16_add(a_bits, b_bits)
    return float16_bits_to_float(res_bits)


def f16_div_softfloat(a_f16, b_f16):
    """
    Divides two half-precision numbers (provided as 16-bit integers)
    using SoftFloat's f16_div.
    Returns the 16-bit integer result.
    """
    return lib.f16_div(a_f16, b_f16)

def f16_div_python(a, b):
    """
    Convenience function: divide two Python floats interpreted as float16,
    using SoftFloat's f16_div, and return a Python float result.
    """
    a_bits = np.float16(a).view(np.uint16)
    b_bits = np.float16(b).view(np.uint16)
    res_bits = lib.f16_div(a_bits, b_bits)
    return float16_bits_to_float(res_bits)

def f16_mul_softfloat(a_f16, b_f16):
    """
    Multiplies two half-precision numbers (provided as 16-bit integers)
    using SoftFloat's f16_mul.
    Returns the 16-bit integer result.
    """
    return lib.f16_mul(a_f16, b_f16)

def f16_mul_python(a, b):
    """
    Convenience function: multiply two Python floats interpreted as float16,
    using SoftFloat's f16_mul, and return a Python float result.
    """
    a_bits = np.float16(a).view(np.uint16)
    b_bits = np.float16(b).view(np.uint16)
    res_bits = lib.f16_mul(a_bits, b_bits)
    return float16_bits_to_float(res_bits)

def f16_sqrt_softfloat(a_f16):
    """
    Computes the square root of a half-precision number (provided as a 16-bit integer)
    using SoftFloat's f16_sqrt.
    Returns the 16-bit integer result.
    """
    return lib.f16_sqrt(a_f16)

def f16_sqrt_python(a):
    """
    Convenience function: compute the square root of a Python float interpreted as float16,
    using SoftFloat's f16_sqrt, and return a Python float result.
    """
    a_bits = np.float16(a).view(np.uint16)
    res_bits = lib.f16_sqrt(a_bits)
    return float16_bits_to_float(res_bits)

def f16_sub_softfloat(a_f16, b_f16):
    """
    Subtracts two half-precision numbers (provided as 16-bit integers)
    using SoftFloat's f16_sub.
    Returns the 16-bit integer result.
    """
    return lib.f16_sub(a_f16, b_f16)

def f16_sub_python(a, b):
    """
    Convenience function: subtract two Python floats interpreted as float16,
    using SoftFloat's f16_sub, and return a Python float result.
    """
    a_bits = np.float16(a).view(np.uint16)
    b_bits = np.float16(b).view(np.uint16)
    res_bits = lib.f16_sub(a_bits, b_bits)
    return float16_bits_to_float(res_bits)

def f16_to_f32_softfloat(a_f16):
    """
    Uses SoftFloat's f16_to_f32 to convert a half-precision number (given as a 16-bit integer)
    into a 32-bit float (returned as a Python float).
    """
    bits32 = lib.f16_to_f32(a_f16)
    return struct.unpack('<f', struct.pack('<I', bits32))[0]

def float16_bits_to_float(f16_bits):
    """
    Convert a 16-bit integer (representing a float16) to a Python float using NumPy.
    """
    return np.array([f16_bits], dtype=np.uint16).view(np.float16)[0]

def softfloat_roundPackToF16(sign, exp, sig):
    """
    Pack sign, exponent, and significand into a float16 representation.
    """
    return lib.softfloat_roundPackToF16(sign, exp, sig)

def parse_float16_input(x):
    """
    Converts input `x` (string, float, int, or hex) to a float16 bit pattern (uint16).
    Accepts:
        - Float literals: 65504.0
        - Special strings: 'inf', '-inf', 'nan'
        - Hex strings: '0x7BFF', 'FE00'
        - Integers: 0x7BFF
    Returns:
        uint16 representing the float16 bit pattern
    """
    if isinstance(x, float):
        return int(np.float16(x).view(np.uint16))
    elif isinstance(x, int):
        return x & 0xFFFF
    elif isinstance(x, str):
        x = x.strip().lower()
        if x.startswith("0x"):
            return int(x, 16) & 0xFFFF
        try:
            val = float(x)
            return int(np.float16(val).view(np.uint16))
        except ValueError:
            raise ValueError(f"Invalid input: {x!r}")
    else:
        raise TypeError(f"Unsupported type for input: {type(x)}")


# Example usage
if __name__ == "__main__":
    # Input as string literal (hex float16 bit pattern or decimal string)
    valA_str = 6561.0

    # Convert to float16 bit pattern (uint16)
    opA = parse_float16_input(valA_str)

    print('; ----- DEBUG OUTPUT -----')

    # Perform the square root using SoftFloat
    result = f16_sqrt_softfloat(opA)

    # Convert to Python float
    valA_float = float16_bits_to_float(opA)
    valR_float = float16_bits_to_float(result)

    # Debug output: decimal first, then hex
    print(f';    sqrt({valA_float}) = {valR_float}')
    print(f';    sqrt(0x{opA:04X}) = 0x{result:04X}')

    # Assembly output: decimal first, then hex
    print(f'\n; ----- ASSEMBLY OUTPUT -----')
    print(f'    call printInline')
    print(f'    asciz "sqrt({valA_float}) = {valR_float}\\r\\n"')
    print(f'    call printInline')
    print(f'    asciz "sqrt(0x{opA:04X}) = 0x{result:04X}\\r\\n"')
    print(f'    ld hl,0x{opA:04X} ; 0x{opA:04X}')
    print(f'    call f16_sqrt')
    print(f'    PRINT_HL_HEX " assembly result"')
    print(f'    call printNewLine')



    # # Input as string literals (hex float16 bit patterns or decimal strings)
    # valA_str = '0x7858'
    # valB_str = '0xFAB9'

    # # Convert to float16 bit patterns (uint16)
    # opA = parse_float16_input(valA_str)
    # opB = parse_float16_input(valB_str)

    # print('; ----- DEBUG OUTPUT -----')

    # # Perform the subition using SoftFloat
    # result = f16_sub_softfloat(opA, opB)

    # # Convert result to Python float
    # valA_float = float16_bits_to_float(opA)
    # valB_float = float16_bits_to_float(opB)
    # valR_float = float16_bits_to_float(result)

    # # Debug output: decimal first, then hex
    # print(f';    {valA_float} - {valB_float} = {valR_float}')
    # print(f';    0x{opA:04X} - 0x{opB:04X} = 0x{result:04X}')

    # # Assembly output: decimal first, then hex
    # print(f'\n; ----- ASSEMBLY OUTPUT -----')
    # print(f'    call printInline')
    # print(f'    asciz "{valA_float} - {valB_float} = {valR_float}\\r\\n"')
    # print(f'    call printInline')
    # print(f'    asciz "0x{opA:04X} - 0x{opB:04X} = 0x{result:04X}\\r\\n"')
    # print(f'    ld hl,0x{opA:04X} ; 0x{opA:04X}')
    # print(f'    ld de,0x{opB:04X} ; 0x{opB:04X}')
    # print(f'    call f16_sub')
    # print(f'    PRINT_HL_HEX " assembly result"')
    # print(f'    call printNewLine')

    # # Compute the worst-case sig32Z = (sigX << expDiff) - sigY
    # sigX = 0xffff  # maximum possible value for normalized significand + hidden bit
    # expDiff = 13   # maximum before algorithm shortcuts
    # sigY = 0xffff  # a realistic upper bound for the second operand's adjusted significand

    # sigX_shifted = sigX << expDiff
    # sig32Z = sigX_shifted - sigY

    # print(f"sigX_shifted: {sigX_shifted} (0x{sigX_shifted:08X}), "
    #     f"sigY: {sigY} (0x{sigY:04X}), "
    #     f"sig32Z: {sig32Z} (0x{sig32Z:08X}), "
    #     f"sig32Z.bit_length(): {sig32Z.bit_length()}")