#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

typedef uint16_t float16_t;  // 16-bit half-precision float

// Function to extract the raw binary value of a float16
void float16_components(float16_t half, int* sign, int* exponent, int* mantissa) {
    *sign = (half >> 15) & 0x1;
    *exponent = (half >> 10) & 0x1F;
    *mantissa = half & 0x3FF;
}

// Convert float16 to its decimal string representation
void float16_to_decimal_string(float16_t half, char* buffer, size_t buffer_size) {
    int sign, exponent, mantissa;
    float16_components(half, &sign, &exponent, &mantissa);
    
    // Initialize buffer
    buffer[0] = '\0';
    
    // Handle special cases
    if (exponent == 0x1F) {  // NaN or Infinity
        if (mantissa != 0) {  // NaN
            snprintf(buffer, buffer_size, "NaN");
            return;
        } else {              // Infinity
            snprintf(buffer, buffer_size, "%sInf", sign ? "-" : "");
            return;
        }
    }
    
    // Handle zero
    if (exponent == 0 && mantissa == 0) {
        snprintf(buffer, buffer_size, "%s0.0", sign ? "-" : "");
        return;
    }
    
    // The algorithm below explicitly converts the floating-point value to decimal
    // without using floating-point operations
    
    // Step 1: Determine the actual value based on IEEE-754 specification
    int actual_exponent;
    int64_t significand;
    
    if (exponent == 0) {  // Subnormal number
        actual_exponent = 1 - 15;  // -14
        significand = mantissa;
    } else {  // Normal number
        actual_exponent = exponent - 15;
        significand = mantissa | 0x400;  // Add implicit leading 1
    }
    
    // Step 2: Create a decimal representation
    // This is the core algorithm to convert binary scientific notation to decimal
    
    // First, we'll normalize to an integer and keep track of the decimal point
    int decimal_point = 10;  // Start with 10 to allow for negative exponents
    
    // Adjust for the binary exponent
    if (actual_exponent > 0) {
        // Multiply significand by 2^actual_exponent
        for (int i = 0; i < actual_exponent; i++) {
            significand *= 2;
        }
    } else if (actual_exponent < 0) {
        // We need to divide by 2^|actual_exponent|, but to maintain precision
        // we'll multiply by 10^|actual_exponent| and adjust decimal_point
        decimal_point += -actual_exponent;
        
        // We'll use a large enough multiplication to ensure precision
        // 2^10 is approximately 10^3, so multiply by 10^3 for each 10 bits
        for (int i = 0; i < -actual_exponent; i++) {
            significand *= 5;  // Multiply by 10/2 for each negative exponent
        }
    }
    
    // Convert significand to decimal string in reverse order
    char decimal_digits[50];  // Temporary buffer for digits
    int digit_count = 0;
    int64_t temp_significand = significand;
    
    // Generate all significant digits
    while (temp_significand > 0 || digit_count < decimal_point + 1) {
        decimal_digits[digit_count++] = '0' + (temp_significand % 10);
        temp_significand /= 10;
    }
    
    // Reverse the digits and add the decimal point
    int buff_pos = 0;
    
    // Add sign if negative
    if (sign) {
        buffer[buff_pos++] = '-';
    }
    
    // Add digits with decimal point
    bool decimal_added = false;
    bool leading_zeros_skipped = false;
    int nonzero_digits = 0;
    int digits_after_decimal = 0;
    const int max_precision = 7;  // Appropriate for float16
    
    // Process digits from most significant to least significant
    for (int i = digit_count - 1; i >= 0; i--) {
        // Place decimal point when needed
        if (i == decimal_point - 1 && !decimal_added) {
            buffer[buff_pos++] = '.';
            decimal_added = true;
            
            // If we're adding a decimal point at the start, prepend with 0
            if (!leading_zeros_skipped) {
                memmove(buffer + 1, buffer, buff_pos - 1);
                buffer[0] = '0';
                buff_pos++;
            }
        }
        
        // Add digit if significant
        if (i >= decimal_point || decimal_digits[i] != '0') {
            leading_zeros_skipped = true;
        }
        
        // Add the digit if it's significant or if we've already started adding digits
        if (leading_zeros_skipped || i == decimal_point - 1) {
            buffer[buff_pos++] = decimal_digits[i];
            
            if (decimal_digits[i] != '0') {
                nonzero_digits++;
            }
            
            if (decimal_added) {
                digits_after_decimal++;
                
                // Limit precision for float16
                if (digits_after_decimal >= max_precision && 
                    (i == 0 || nonzero_digits > 0)) {
                    break;
                }
            }
        }
    }
    
    // If no decimal point was added and no digits after decimal
    if (!decimal_added && digits_after_decimal == 0) {
        buffer[buff_pos++] = '.';
        buffer[buff_pos++] = '0';  // Add .0 to make it clear it's a floating point value
    }
    
    buffer[buff_pos] = '\0';  // Null terminate
    
    // Check if the number would be better represented in scientific notation
    // For float16, values smaller than 0.0001 or larger than 9999 are good
    // candidates for scientific notation
    int exponent_value = 0;
    bool use_scientific = false;
    char* p = buffer;
    
    // Skip sign if present
    if (*p == '-') {
        p++;
    }
    
    // Check if very small number (0.000...x)
    if (*p == '0' && *(p+1) == '.') {
        p += 2;  // Skip "0."
        
        // Count leading zeros
        while (*p == '0') {
            exponent_value--;
            p++;
        }
        
        if (*p != '\0' && exponent_value < -4) {
            use_scientific = true;
        }
    } 
    // Check if very large number
    else {
        // Count digits before decimal
        char* decimal_pos = strchr(buffer, '.');
        if (decimal_pos) {
            exponent_value = decimal_pos - buffer - (buffer[0] == '-' ? 1 : 0) - 1;
            if (exponent_value > 4) {
                use_scientific = true;
            }
        }
    }
    
    // Convert to scientific notation if needed
    if (use_scientific) {
        char temp[50];
        strcpy(temp, buffer);
        
        // Find first significant digit
        p = temp;
        if (*p == '-') p++;
        
        // Skip leading zeros
        while (*p == '0' || *p == '.') {
            if (*p == '.') {
                // Skip the decimal point
                memmove(p, p+1, strlen(p));
                continue;
            }
            p++;
        }
        
        // Format as scientific notation
        if (*p != '\0') {
            char first_digit = *p;
            memmove(p, p+1, strlen(p));
            
            // Move decimal point after first digit
            p = temp;
            if (*p == '-') p++;
            
            memmove(p+2, p, strlen(p)+1);
            *p = first_digit;
            *(p+1) = '.';
            
            // Add exponent
            int len = strlen(temp);
            if (exponent_value > 0) {
                snprintf(temp + len, sizeof(temp) - len, "e+%d", exponent_value);
            } else {
                snprintf(temp + len, sizeof(temp) - len, "e%d", exponent_value);
            }
            
            // Copy back to buffer
            strncpy(buffer, temp, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
        }
    }
}

// Function to convert a decimal string back to float16
float16_t decimal_string_to_float16(const char* str) {
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
    
    // Parse exponent if present
    int exponent = 0;
    bool exponent_negative = false;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '-') {
            exponent_negative = true;
            p++;
        } else if (*p == '+') {
            p++;
        }
        
        while (*p >= '0' && *p <= '9') {
            exponent = exponent * 10 + (*p - '0');
            p++;
        }
        
        if (exponent_negative) {
            exponent = -exponent;
        }
    }
    
    // Combine all parts into a binary floating-point representation
    // Adjust for the decimal point and specified exponent
    double value = (double)integer_part;
    
    // Add fractional part
    if (fractional_digits > 0) {
        double fraction = (double)fractional_part;
        for (int i = 0; i < fractional_digits; i++) {
            fraction /= 10.0;
        }
        value += fraction;
    }
    
    // Adjust for exponent
    if (exponent != 0) {
        double multiplier = 1.0;
        for (int i = 0; i < abs(exponent); i++) {
            multiplier *= 10.0;
        }
        
        if (exponent > 0) {
            value *= multiplier;
        } else {
            value /= multiplier;
        }
    }
    
    // Apply sign
    if (sign) {
        value = -value;
    }
    
    // Now convert to binary representation for float16
    // Check for special cases again
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

// Example usage
int main() {
    float16_t test_values[] = {
        0x0000,  // +0.0
        0x8000,  // -0.0
        0x3C00,  // 1.0
        0xC000,  // -2.0
        0x3800,  // 0.5
        0x7BFF,  // 65504.0 (max normal half-precision value)
        0x0400,  // 6.10352e-5 (minimum normal half-precision value)
        0x0001,  // 5.96046e-8 (minimum subnormal half-precision value)
        0x7C00,  // +Infinity
        0xFC00,  // -Infinity
        0x7E00   // NaN
    };
    
    char buffer[100];
    int num_tests = sizeof(test_values) / sizeof(test_values[0]);
    
    printf("Testing float16_to_decimal_string:\n");
    printf("================================\n");
    
    for (int i = 0; i < num_tests; i++) {
        float16_t value = test_values[i];
        int sign, exponent, mantissa;
        float16_components(value, &sign, &exponent, &mantissa);
        
        float16_to_decimal_string(value, buffer, sizeof(buffer));
        
        printf("Float16: 0x%04X (S:%d, E:%d, M:0x%03X)\n", 
               value, sign, exponent, mantissa);
        printf("Decimal: %s\n", buffer);
        
        // Round-trip test
        float16_t round_trip = decimal_string_to_float16(buffer);
        
        printf("Round-trip: 0x%04X %s\n", 
               round_trip, (round_trip == value) ? "(identical)" : "(changed)");
        printf("--------------------------------\n");
    }
    
    // Test user input
    printf("\nEnter a decimal number (or 'q' to quit): ");
    while (fgets(buffer, sizeof(buffer), stdin)) {
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
        
        if (strcmp(buffer, "q") == 0 || strcmp(buffer, "Q") == 0) {
            break;
        }
        
        float16_t value = decimal_string_to_float16(buffer);
        int sign, exponent, mantissa;
        float16_components(value, &sign, &exponent, &mantissa);
        
        printf("Converted to float16: 0x%04X (S:%d, E:%d, M:0x%03X)\n", 
               value, sign, exponent, mantissa);
        
        char decimal_buffer[100];
        float16_to_decimal_string(value, decimal_buffer, sizeof(decimal_buffer));
        printf("Back to decimal: %s\n", decimal_buffer);
        
        printf("\nEnter a decimal number (or 'q' to quit): ");
    }
    
    return 0;
}