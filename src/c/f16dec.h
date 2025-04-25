/**
 * f16dec.h - Float16 to decimal string conversion functions
 * 
 * This library provides conversion between IEEE-754 half-precision
 * floating point values and their decimal string representations.
 */

 #ifndef F16DEC_H
 #define F16DEC_H
 
 #include <stdint.h>
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 /**
  * Convert a 16-bit half-precision float to a decimal string representation
  * 
  * @param f16 The half-precision float value (16 bits)
  * @param buffer The output buffer to store the string (must be pre-allocated)
  * @param buffer_size The size of the buffer (recommended min size: 20 bytes)
  * @return The number of characters written (excluding null terminator)
  */
 int f16_to_charDec(uint16_t f16, char* buffer, int buffer_size);
 
 /**
  * Convert a decimal string representation to a 16-bit half-precision float
  * 
  * @param str The null-terminated decimal string to convert
  * @return The half-precision float value (16 bits)
  */
 uint16_t charDec_to_f16(const char* str);
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif /* F16DEC_H */