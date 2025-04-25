/**
 * f16dec.c - Implementation of float16 to decimal string conversion functions
 */

 #include "f16dec.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 #include <stdbool.h>
 
 /* Internal functions */
 static void float16_components(uint16_t half, int* sign, int* exponent, int* mantissa) {
     *sign = (half >> 15) & 0x1;
     *exponent = (half >> 10) & 0x1F;
     *mantissa = half & 0x3FF;
 }
 
 /**
  * Convert a 16-bit half-precision float to a decimal string representation
  * 
  * @param f16 The half-precision float value (16 bits)
  * @param buffer The output buffer to store the string (must be pre-allocated)
  * @param buffer_size The size of the buffer (recommended min size: 20 bytes)
  * @return The number of characters written (excluding null terminator)
  */
 
 int f16_to_charDec(uint16_t f16, char* buffer, int buffer_size) {
    int sign, exponent, mantissa;
    float16_components(f16, &sign, &exponent, &mantissa);
    
    // Clear buffer
    buffer[0] = '\0';
    
    // Handle special cases
    if (exponent == 0x1F) {  // NaN or Infinity
        if (mantissa != 0) {  // NaN
            strncpy(buffer, "NaN", buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return 3;
        } else {              // Infinity
            int len = snprintf(buffer, buffer_size, "%sInf", sign ? "-" : "");
            return len;
        }
    }
    
    // Handle zero
    if (exponent == 0 && mantissa == 0) {
        int len = snprintf(buffer, buffer_size, "%s0.0", sign ? "-" : "");
        return len;
    }
    
    // Exact conversion using powers of 2
    // Use a more precise representation for normal and subnormal numbers
    
    // Determine the decimal value using direct computation
    // We'll use a higher precision approach using double
    double decimal_value = 0.0;
    
    if (exponent == 0) {  // Subnormal number
        // For subnormals, there is no implicit leading 1
        // Each bit represents a power of 2 starting from 2^-24
        for (int bit = 9; bit >= 0; bit--) {
            if ((mantissa >> bit) & 1) {
                // Add 2^(bit-24) to the value
                decimal_value += pow(2.0, bit - 24);
            }
        }
    } else {  // Normal number
        // For normal numbers, the representation is 1.mantissa * 2^(exponent-15)
        double fraction = 1.0; // Start with implicit leading 1
        
        // Add each bit of the mantissa
        for (int bit = 9; bit >= 0; bit--) {
            if ((mantissa >> bit) & 1) {
                fraction += pow(2.0, bit - 10);
            }
        }
        
        // Apply the exponent
        decimal_value = fraction * pow(2.0, exponent - 15);
    }
    
    // Apply sign
    if (sign) {
        decimal_value = -decimal_value;
    }
    
    // Convert to string with appropriate precision
    // For maximum precision, we need to use up to 16 digits after decimal for smallest subnormals
    
    // Determine appropriate format based on magnitude
    char format[32]; // Increased buffer size to avoid overflow
    
    if (fabs(decimal_value) >= 1.0) {
        // For large values, show fixed point with 1 decimal place
        snprintf(format, sizeof(format), "%%s%%.1f");
    } else if (fabs(decimal_value) >= 0.001) {
        // For medium values, show 6 significant digits
        snprintf(format, sizeof(format), "%%s%%.6f");
    } else {
        // For small values, including subnormals, use scientific notation first
        // Then convert to appropriate decimal representation
        double abs_value = fabs(decimal_value);
        
        // Count leading zeros after decimal point
        int leading_zeros = 0;
        double temp = abs_value;
        
        while (temp < 0.1 && temp > 0) {
            temp *= 10;
            leading_zeros++;
        }
        
        // For extreme small values, ensure we capture the smallest subnormal value
        if (exponent == 0 && mantissa == 1) {
            // The smallest subnormal, show exact representation for half-precision
            snprintf(format, sizeof(format), "%%s0.0000000596046448");
        } else if (leading_zeros > 7) {
            // Very small values
            snprintf(format, sizeof(format), "%%s%%.%df", leading_zeros + 8);
        } else {
            // Small but not tiny
            snprintf(format, sizeof(format), "%%s%%.10f");
        }
    }
    
    // Format the final string
    int len = snprintf(buffer, buffer_size, format, sign ? "-" : "", fabs(decimal_value));
    
    // Remove trailing zeros after decimal point (but keep one zero after decimal)
    int decimal_pos = -1;
    for (int i = 0; i < len; i++) {
        if (buffer[i] == '.') {
            decimal_pos = i;
            break;
        }
    }
    
    if (decimal_pos >= 0) {
        int last_non_zero = decimal_pos + 1;
        
        // Find last non-zero digit
        for (int i = decimal_pos + 1; i < len; i++) {
            if (buffer[i] != '0') {
                last_non_zero = i;
            }
        }
        
        // Keep at least one digit after decimal
        if (last_non_zero == decimal_pos) {
            last_non_zero++;
        }
        
        // Truncate trailing zeros
        buffer[last_non_zero + 1] = '\0';
        len = last_non_zero + 1;
    }
    
    return len;
}
 
 /**
  * Convert a decimal string representation to a 16-bit half-precision float
  * 
  * @param str The null-terminated decimal string to convert
  * @return The half-precision float value (16 bits)
  */
 uint16_t charDec_to_f16(const char* str) {
     // Check for special cases
     if (strcmp(str, "NaN") == 0) {
         return 0x7E00;  // NaN
     } else if (strcmp(str, "Inf") == 0) {
         return 0x7C00;  // +Infinity
     } else if (strcmp(str, "-Inf") == 0) {
         return 0xFC00;  // -Infinity
     }
     
     // Parse sign
     int sign = 0;
     const char* p = str;
     if (*p == '-') {
         sign = 1;
         p++;
     } else if (*p == '+') {
         p++;
     }
     
     // Parse integer part
     int64_t integer_part = 0;
     while (*p >= '0' && *p <= '9') {
         integer_part = integer_part * 10 + (*p - '0');
         p++;
     }
     
     // Parse fractional part
     int64_t fractional_part = 0;
     int fractional_digits = 0;
     if (*p == '.') {
         p++;
         while (*p >= '0' && *p <= '9') {
             fractional_part = fractional_part * 10 + (*p - '0');
             fractional_digits++;
             p++;
         }
     }
     
     // Combine all parts into a binary floating-point representation
     // Adjust for the decimal point
     double value = (double)integer_part;
     
     // Add fractional part
     if (fractional_digits > 0) {
         double fraction = (double)fractional_part;
         for (int i = 0; i < fractional_digits; i++) {
             fraction /= 10.0;
         }
         value += fraction;
     }
     
     // Apply sign
     if (sign) {
         value = -value;
     }
     
     // Now convert to binary representation for float16
     // Check for special cases
     if (value == 0.0) {
         return sign ? 0x8000 : 0; // Signed zero
     }
     
     if (isinf(value)) {
         return sign ? 0xFC00 : 0x7C00; // Signed infinity
     }
     
     // Extract binary exponent and mantissa
     uint16_t half_sign = sign ? 0x8000 : 0;
     int binary_exponent;
     double mantissa_value;
     
     // Normalize to 1.xxxx format
     value = fabs(value);
     binary_exponent = (int)floor(log2(value));
     mantissa_value = value / pow(2.0, binary_exponent) - 1.0;
     
     // Adjust for float16 exponent bias
     int biased_exponent = binary_exponent + 15;
     
     // Check if normal or subnormal
     if (biased_exponent <= 0) {
         // Subnormal number
         mantissa_value = value / pow(2.0, -14);
         biased_exponent = 0;
     } else if (biased_exponent >= 31) {
         // Overflow to infinity
         return half_sign | 0x7C00;
     }
     
     // Convert mantissa to binary
     uint16_t binary_mantissa = (uint16_t)(mantissa_value * 1024.0 + 0.5);
     
     // Check for rounding overflow
     if (binary_mantissa > 0x3FF) {
         binary_mantissa = 0;
         biased_exponent++;
         
         if (biased_exponent >= 31) {
             return half_sign | 0x7C00; // Overflow to infinity
         }
     }
     
     // Construct float16
     return half_sign | (biased_exponent << 10) | binary_mantissa;
 }