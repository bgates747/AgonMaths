import random
import struct
import os

def generate_test_cases(output_file, num_cases, hl_min, hl_max, de_min, de_max):
    """Generate signed 16x16 test cases and write them to the output file."""
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    with open(output_file, 'wb') as f:
        for _ in range(num_cases):
            hl = random.randint(hl_min, hl_max)
            de = random.randint(de_min, de_max)
            product = hl * de

            # Store as signed 16, signed 16, signed 32, and 4 bytes of reserved padding
            f.write(struct.pack('<hh i 4x', hl, de, product))
            
    print(f"Generated {num_cases} signed test cases to {output_file}")
    print(f"File size: {os.path.getsize(output_file)} bytes")

if __name__ == "__main__":
    output_file = 'tests/smul_16x16_32.bin'
    num_cases = 1000
    
    hl_min = -32768
    hl_max =  32767
    
    de_min = -32768
    de_max =  32767
    
    generate_test_cases(output_file, num_cases, hl_min, hl_max, de_min, de_max)
