import os
import struct
from cffi import FFI

# --- helper to convert a Python float to IEEE-754 binary16 bit-pattern ---
def float_to_halfbits(flt: float) -> int:
    # '<e' is little-endian half-precision (PEP 3118). 
    # pack to 2 bytes, then unpack as uint16
    packed = struct.pack('<e', flt)
    (bits,) = struct.unpack('<H', packed)
    return bits

# --- load your libf16print.so as before ---
ffi = FFI()
ffi.cdef("char* half_to_string(uint16_t half, char* out);")
lib_path = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "libf16print.so")
)
lib = ffi.dlopen(lib_path)

# --- unified print function ---
def print_half(value):
    """
    value: either
      - a hex string like "0x3C00" (treated as raw half-float bits)
      - a Python float (converted to half bits via struct.pack('<e',…))
    """
    # allocate buffer for C string result
    buf = ffi.new("char[32]")

    # interpret input
    if isinstance(value, str):
        # parse hex string (allow with or without "0x")
        bits = int(value, 16)
    elif isinstance(value, float):
        bits = float_to_halfbits(value)
    else:
        raise TypeError("Expected str or float, got %r" % (value,))

    # call into C
    cstr = lib.half_to_string(bits, buf)
    text = ffi.string(cstr).decode()

    # echo what happened
    print(f"{value!r:>10} → bits = 0x{bits:04X} → {text}")


if __name__ == "__main__":
    # examples
    # print_half(3.141592653589793)         # Python float
    # print_half('0x4248') # just under pi 3.140625
    print_half('0x4249') # just over pi 3.142578125

    # print_half(0.5)
    # print_half(-65504.0)     # max half
    # print_half(-128.5)       # Python float
    # print_half("0x3C00")    # raw bits for 1.0
    # print_half("0xFC00")    # raw bits for -Infinity
    # print_half("0x0001")
