/**
 * test_f16dec.c - Test program for f16dec library
 */

 #include "f16dec.h"
 #include <stdio.h>
 #include <string.h>
 
 // Function to display float16 components for debugging
 void print_float16_components(uint16_t value) {
     int sign = (value >> 15) & 0x1;
     int exponent = (value >> 10) & 0x1F;
     int mantissa = value & 0x3FF;
     
     printf("Float16: 0x%04X (S:%d, E:%d, M:0x%03X)\n", 
            value, sign, exponent, mantissa);
 }
 
 int main() {
     uint16_t test_values[] = {
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
     
     char buffer[50];
     int num_tests = sizeof(test_values) / sizeof(test_values[0]);
     
     printf("Testing f16_to_charDec and charDec_to_f16:\n");
     printf("==========================================\n");
     
     for (int i = 0; i < num_tests; i++) {
         uint16_t value = test_values[i];
         
         // Convert float16 to decimal string
         int len = f16_to_charDec(value, buffer, sizeof(buffer));
         
         // Print original value components
         print_float16_components(value);
         printf("Decimal: \"%s\" (length: %d)\n", buffer, len);
         
         // Round-trip test
         uint16_t round_trip = charDec_to_f16(buffer);
         
         // Print round-trip components
         printf("Round-trip: ");
         print_float16_components(round_trip);
         printf("Match: %s\n", (round_trip == value) ? "YES" : "NO");
         
         printf("------------------------------------------\n");
     }
     
     // Interactive test
     printf("\nInteractive test (Enter 'q' to quit):\n");
     printf("------------------------------------------\n");
     
     // Test decimal string to float16
     printf("Enter a decimal number: ");
     while (fgets(buffer, sizeof(buffer), stdin)) {
         // Remove newline
         buffer[strcspn(buffer, "\n")] = 0;
         
         if (strcmp(buffer, "q") == 0) {
             break;
         }
         
         // Convert decimal string to float16
         uint16_t value = charDec_to_f16(buffer);
         
         // Print converted value components
         printf("Converted to float16: ");
         print_float16_components(value);
         
         // Convert back to decimal string for round-trip check
         char result[50];
         f16_to_charDec(value, result, sizeof(result));
         
         printf("Back to decimal: \"%s\"\n", result);
         printf("------------------------------------------\n");
         printf("Enter a decimal number: ");
     }
     
     return 0;
 }