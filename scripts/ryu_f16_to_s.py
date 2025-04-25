#!/usr/bin/env python3
"""
Python CFFI wrapper and test harness for the Ryu half-precision printer.
Uses the f16_to_s_buffered API to avoid malloc, and compares by round-trip.
"""

import os
import sys
import numpy as np
from cffi import FFI

class RyuWrapper:
    """Wrapper for the Ryu half-precision C library"""

    def __init__(self, lib_path=None):
        if lib_path is None:
            lib_path = os.path.abspath(
                os.path.join(os.path.dirname(__file__),
                             "../ryu/src/libhalfryu.so")
            )
        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"Library not found: {lib_path}")
        self.ffi = FFI()
        # Use the buffered API (no malloc inside C)
        self.ffi.cdef("""
            void f16_to_s_buffered(uint16_t value, char* buffer);
        """)
        try:
            self.lib = self.ffi.dlopen(lib_path)
            print(f"Loaded Ryu library from {lib_path}")
        except Exception as e:
            raise RuntimeError(f"Failed to load Ryu library: {e}")

    def to_string(self, bit_pattern: int) -> str:
        # Allocate a 32-byte buffer on the Python side
        buf = self.ffi.new("char[32]")
        # Call the buffered function
        self.lib.f16_to_s_buffered(bit_pattern, buf)
        return self.ffi.string(buf).decode('utf-8')


def normalize(s: str) -> str:
    """Normalize formats so Inf/Infinity, NaN/nan, -0/-0.0 compare equal."""
    s = s.strip()
    if s.lower() in ("inf", "infinity"):
        return "inf"
    if s.lower() == "-inf":
        return "-inf"
    if s.lower() in ("nan",):
        return "nan"
    if s in ("0", "-0"):
        # Distinguish signed zero
        return "-0.0" if s.startswith("-") else "0.0"
    # Ensure a decimal point for whole numbers
    if s.replace("-", "").isdigit():
        return s + ".0"
    return s


def parse_float16_input(x):
    """Convert various inputs to a uint16 bit-pattern for float16."""
    if isinstance(x, float):
        return int(np.float16(x).view(np.uint16))
    if isinstance(x, int):
        return x & 0xFFFF
    if isinstance(x, str):
        s = x.strip().lower()
        if s.startswith("0x"):
            return int(s, 16) & 0xFFFF
        v = float(s)
        return int(np.float16(v).view(np.uint16))
    raise TypeError(f"Unsupported input type: {type(x)}")


def numpy_half_to_string(bit_pattern: int) -> str:
    """Get NumPy’s default string for a float16 bit-pattern."""
    f16 = np.array([bit_pattern], dtype=np.uint16).view(np.float16)[0]
    if np.isnan(f16):
        return "nan"
    if np.isposinf(f16):
        return "inf"
    if np.isneginf(f16):
        return "-inf"
    s = str(f16)
    # Ensure consistent decimal form
    if "." not in s and not any(c in s for c in ("n", "i")):
        s += ".0"
    return s


def print_comparison(bit_pattern, ryu_str, numpy_str, round_trip_ok):
    """Print comparison and round-trip result."""
    hexval = f"{bit_pattern:04X}"
    binfmt = format(bit_pattern, "016b")
    match = "✓" if round_trip_ok else "✗"
    print(f"Value: 0x{hexval} (bin {binfmt[:1]} {binfmt[1:6]} {binfmt[6:]})")
    print(f"  Ryu:    {ryu_str}")
    print(f"  NumPy:  {numpy_str}")
    print(f"  Round-trip == input? {match}")
    if match == "✗":
        print("  ** DISCREPANCY **")
    print()


if __name__ == "__main__":
    try:
        ryu = RyuWrapper()
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)

    test_values = [
        '0x0000', '0x8000', '0x7C00', '0xFC00',
        '0x7E00', '0x3C00', '0xBC00', '0x4200',
        '0x3800', '0x3400', '0x74DA', '0x7BFF',
        '65504.0', '0.0000001', '9.8765', '0x3D00', '0xC180'
    ]

    print("Ryu vs NumPy float16 round-trip validation")
    print("===========================================")
    for token in test_values:
        bp        = parse_float16_input(token)
        ryu_raw   = ryu.to_string(bp)
        np_raw    = numpy_half_to_string(bp)

        # Normalize both representations for clarity
        ryu_norm = normalize(ryu_raw)
        np_norm  = normalize(np_raw)

        # Round-trip: parse Ryu's string back to float16 and compare bit patterns
        try:
            f16         = np.float16(ryu_norm)
            rt_bp       = int(f16.view(np.uint16))
            round_trip  = (rt_bp == bp)
        except Exception:
            round_trip = False

        print_comparison(bp, ryu_norm, np_norm, round_trip)
