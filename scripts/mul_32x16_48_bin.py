import random
import struct
import os

def generate_test_cases(output_file, num_cases, op1_min, op1_max, op2_min, op2_max):
    """Generate 32x16 -> 48-bit multiply test cases with no padding. Just 18-byte records."""
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    with open(output_file, 'wb') as f:
        for _ in range(num_cases):
            op1 = random.randint(op1_min, op1_max)  # 32-bit unsigned
            op2 = random.randint(op2_min, op2_max)  # 16-bit unsigned
            product = op1 * op2                     # 48-bit result

            low32 = product & 0xFFFFFFFF
            high16 = (product >> 32) & 0xFFFF

            # Write: op1 (4B), op2 (2B), low32 (4B), high16 (2B), asm_result placeholder (6B)
            f.write(struct.pack('<I H I H', op1, op2, low32, high16))  # 12 bytes
            f.write(b'\x00' * 6)  # 6 bytes for future ez80 glory
    
    print(f"Generated {num_cases} test cases and wrote them to {output_file}")
    print(f"Total file size: {os.path.getsize(output_file)} bytes")

if __name__ == "__main__":
    output_file = 'tests/mul_32x16_48.bin'
    num_cases = 100000

    op1_min = 0
    op1_max = 0xFFFFFFFF

    op2_min = 0
    op2_max = 0xFFFF

    generate_test_cases(output_file, num_cases, op1_min, op1_max, op2_min, op2_max)
