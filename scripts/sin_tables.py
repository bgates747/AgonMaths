#!/usr/bin/env python3
import numpy as np
from struct import pack, unpack
from textwrap import wrap

def f16_to_bits(val: np.float16) -> int:
    """Pack a numpy.float16 into a 16-bit integer."""
    return int.from_bytes(pack('<e', val), 'little')

N = 256

# Generate tables
result_bits = [(lambda i: f16_to_bits(np.float16(np.sin((i * np.pi) / 128.0))))(i) for i in range(N)]
diff_bits = []
for i in range(N):
    y0 = np.float16(np.sin((i    ) * np.pi / 128.0))
    y1 = np.float16(np.sin(((i+1) & 0xFF) * np.pi / 128.0))
    diff_bits.append(f16_to_bits(np.float16(y1 - y0)))
frac_bits = [f16_to_bits(np.float16(f / 256.0)) for f in range(N)]

def print_assembly(name, bits_list):
    print(f"; ----------------------------------------")
    print(f"; {name}")
    print(f"; ----------------------------------------")
    print(f"{name}:")
    for i in range(0, N, 8):
        chunk = bits_list[i:i+8]
        line = ", ".join(f"0x{b:04X}" for b in chunk)
        print(f"    dw {line}")

def print_c(name, bits_list):
    print(f"static const uint16_t {name}[{N}] = {{")
    for i in range(0, N, 8):
        chunk = bits_list[i:i+8]
        line = ", ".join(f"0x{b:04X}" for b in chunk)
        comma = "," if i + 8 < N else ""
        print(f"    {line}{comma}")
    print("};\n")

# Output assembly
print_assembly("sin_lut_f16", result_bits)
print()
print_assembly("diff_lut_f16", diff_bits)
print()
print_assembly("frac_lut_f16", frac_bits)
print()

# Output C
print("/* ---------------- C tables ---------------- */")
print_c("sin_lut_f16", result_bits)
print_c("diff_lut_f16", diff_bits)
print_c("frac_lut_f16", frac_bits)


# ----------------------------------------


# import numpy as np
# from struct import pack

# N = 64

# # -------------------------------
# # float16 LUT (f16 binary encoding)
# # -------------------------------
# print("\nsin_lut_f16:")
# for i in range(N + 1):
#     radians = (i / N) * (np.pi / 2)
#     f16 = np.float16(np.sin(radians))
#     bits = pack('<e', f16)
#     value = int.from_bytes(bits, 'little')
#     print(f"    dw 0x{value:04X}")

# # -------------------------------
# # Q8.8 LUT (uint16, 0–255.996)
# # -------------------------------
# print("\nsin_lut_8x8:")
# for i in range(N + 1):
#     radians = (i / N) * (np.pi / 2)
#     fixed = int(round(np.sin(radians) * 256.0)) & 0xFFFF
#     print(f"    dw 0x{fixed:04X}")

# # -------------------------------
# # Q8.16 LUT (uint24, 0–255.999...)
# # -------------------------------
# print("\nsin_lut_8x16:")
# for i in range(N + 1):
#     radians = (i / N) * (np.pi / 2)
#     fixed = int(round(np.sin(radians) * 65536.0)) & 0xFFFFFF
#     print(f"    dl 0x{fixed:06X}")

# # -------------------------------
# # Q8.24 LUT (uint32, 0–255.999...)
# # -------------------------------
# Q824_SCALE = 1 << 24  # 2^24 = 16777216
# print("\nsin_lut_8x24:")
# for i in range(N + 1):
#     radians = (i / N) * (np.pi / 2)
#     fixed = int(round(np.sin(radians) * Q824_SCALE)) & 0xFFFFFFFF
#     print(f"    dw32 0x{fixed:08X}")


# import numpy as np, struct

# # Build 128-entry log-indexed LUT for sin(x) on x ∈ [0, π/2]
# entries = []
# for e in (0,1):               # exponent‐bias = 15 → E = 15+e
#     E = 15 + e
#     for M_hi in range(64):    # top 6 bits of mantissa
#         bits_in  = (E << 10) | (M_hi << 4)
#         # turn bits into float16
#         f16_in    = np.frombuffer(struct.pack('<H', bits_in), dtype=np.float16)[0]
#         # compute sine in float32 then cast back to float16
#         f16_sin   = np.float16(np.sin(np.float32(f16_in)))
#         bits_out  = struct.unpack('<H', struct.pack('<e', f16_sin))[0]
#         entries.append(bits_out)

# # Print as an assembly dw table
# print("sin_lut_log_Q7:")
# for i, v in enumerate(entries):
#     print(f"    dw 0x{v:04X}    ; idx {i:3d}")
