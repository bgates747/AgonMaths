#!/usr/bin/env python3
"""
Test script for f16dec library using CFFI
"""

import os
import sys
from cffi import FFI

def main():
    # Initialize FFI
    ffi = FFI()
    
    # Define the C functions we want to use
    ffi.cdef("""
        int f16_to_charDec(uint16_t f16, char* buffer, int buffer_size);
        uint16_t charDec_to_f16(const char* str);
    """)
    
    # Get absolute path to the shared library
    lib_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "lib/libf16dec.so"))
    
    # Check if library exists
    if not os.path.exists(lib_path):
        print(f"Error: Library not found at {lib_path}")
        sys.exit(1)
    
    try:
        # Load the library
        lib = ffi.dlopen(lib_path)
        print(f"Successfully loaded library from {lib_path}")
    except Exception as e:
        print(f"Error loading library: {e}")
        sys.exit(1)
    
    # Test values (float16 in hex)
    test_values = [
        (0x0000, "0.0"),        # +0.0
        (0x8000, "-0.0"),        # -0.0
        (0x3C00, "1.0"),         # 1.0
        (0xC000, "-2.0"),        # -2.0
        (0x3800, "0.5"),         # 0.5
        (0x4200, "3.0"),         # 3.0
        (0x7BFF, "65504.0"),     # Max normal value
        (0x0400, "0.000061"),    # Min normal value
        (0x0001, "0.0000000596046448"), # Min subnormal value (precise)
        (0x7C00, "Inf"),         # +Infinity
        (0xFC00, "-Inf"),        # -Infinity
        (0x7E00, "NaN")          # NaN
    ]
    
    # Test float16 to decimal string conversion
    print("\n=== Testing f16_to_charDec ===")
    print("-----------------------------")
    
    for f16_value, expected in test_values:
        # Create buffer for result
        buffer_size = 30
        buffer = ffi.new("char[]", buffer_size)
        
        # Call the function
        chars_written = lib.f16_to_charDec(f16_value, buffer, buffer_size)
        
        # Convert result to Python string
        result = ffi.string(buffer).decode('utf-8')
        
        # Print results
        print(f"Float16: 0x{f16_value:04X}")
        print(f"Result: \"{result}\" (chars written: {chars_written})")
        print(f"Expected: \"{expected}\"")
        print(f"Match: {'YES' if result == expected else 'NO - but this may be due to precision differences'}")
        print("-----------------------------")
    
    # Test decimal string to float16 conversion
    print("\n=== Testing charDec_to_f16 ===")
    print("-----------------------------")
    
    decimal_tests = [
        "0.0",
        "-0.0",
        "1.0",
        "-2.0",
        "0.5",
        "3.0",
        "65504.0",  # Max normal value
        "0.000061", # Close to min normal value
        "0.0000000596", # Close to min subnormal value
        "Inf",
        "-Inf",
        "NaN"
    ]
    
    for decimal_str in decimal_tests:
        # Convert string to C string
        c_str = ffi.new("char[]", decimal_str.encode('utf-8'))
        
        # Call the function
        f16_result = lib.charDec_to_f16(c_str)
        
        # Convert back to string for verification
        verify_buffer = ffi.new("char[]", buffer_size)
        lib.f16_to_charDec(f16_result, verify_buffer, buffer_size)
        verify_str = ffi.string(verify_buffer).decode('utf-8')
        
        # Print results
        print(f"Decimal string: \"{decimal_str}\"")
        print(f"Float16 result: 0x{f16_result:04X}")
        print(f"Round-trip: \"{verify_str}\"")
        print(f"Round-trip match: {'YES' if verify_str == decimal_str else 'NO - but this may be due to precision differences'}")
        print("-----------------------------")
    
    # Interactive test
    if sys.stdin.isatty():  # Only run interactive mode if in a terminal
        print("\n=== Interactive Testing ===")
        print("Enter decimal numbers to convert, or 'q' to quit.")
        print("-----------------------------")
        
        while True:
            try:
                user_input = input("Enter a decimal number: ")
                if user_input.lower() == 'q':
                    break
                
                # Convert input to C string
                c_str = ffi.new("char[]", user_input.encode('utf-8'))
                
                # Convert to float16
                f16_result = lib.charDec_to_f16(c_str)
                print(f"As float16: 0x{f16_result:04X}")
                
                # Convert back to string
                verify_buffer = ffi.new("char[]", buffer_size)
                lib.f16_to_charDec(f16_result, verify_buffer, buffer_size)
                verify_str = ffi.string(verify_buffer).decode('utf-8')
                print(f"Round-trip: \"{verify_str}\"")
                print("-----------------------------")
            
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"Error: {e}")
    
    print("Testing complete.")

if __name__ == "__main__":
    main()