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
    // Convert a float16 value to a float32 value.
    float32_t f16_to_f32(float16_t a);
    // Pack sign, exponent and sig to a float16.
    float16_t softfloat_roundPackToF16( bool sign, int_fast16_t exp, uint_fast16_t sig );
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
    # opA = 10.0
    # print(f"float16 of {opA} = 0x{float_to_f16_bits(opA):04X}")
    # opB = 5.0
    # print(f"float16 of {opB} = 0x{float_to_f16_bits(opB):04X}")
    # quotient_f16 = f16_div_softfloat(float_to_f16_bits(opA), float_to_f16_bits(opB))
    # print(f"float16 of {opA} / {opB} = 0x{quotient_f16:04X} ({float16_bits_to_float(quotient_f16)})")

    # sign = False
    # exp = 15
    # sig = 0x7e88
    # result = softfloat_roundPackToF16(sign, exp, sig)
    # print(f"Packed result: 0x{result:04X} ({float16_bits_to_float(result)})")

    # lib.printRoundingModeInfo()

    op1 = 'inf'            # 0x7C00
    op2 = 1.0              # 0x3C00


    # Input as Python float literals
    valA = op1
    valB = op2

    # Convert to float16 bit patterns using NumPy
    opA = parse_float16_input(valA)
    opB = parse_float16_input(valB)

    print('; ----- DEBUG OUTPUT -----')

    # Perform the division using your SoftFloat-emulated f16_div
    result = f16_add_softfloat(opA, opB)

    # Convert result back to Python float using NumPy
    valR = float(np.array([result], dtype=np.uint16).view(np.float16)[0])
    print(f';    0x{opA:04X} + 0x{opB:04X} = 0x{result:04X}')


    # Output
    print(f'\r\n; ----- ASSEMBLY OUTPUT -----')
    print(f'    call printInline')
    print(f'    asciz "{valA} + {valB} = {valR}\\r\\n"')
    print(f'    call printInline')
    print(f'    asciz "0x{opA:04X} + 0x{opB:04X} = 0x{result:04X}\\r\\n"')
    print(f'    ld hl,0x{opA:04X} ; {valA}')
    print(f'    ld de,0x{opB:04X} ; {valB}')
    print(f'    call f16_add')
    print(f'    PRINT_HL_HEX " assembly result"')
    print(f'    call printNewLine')
