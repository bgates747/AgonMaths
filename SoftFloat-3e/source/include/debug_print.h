// debug_print.h
#pragma once
#include <stdio.h>
#include <stdint.h>
#include <string.h>   // for memcpy()

// Make sure float16_t is known here:
#include "internals.h"   // or wherever float16_t is defined

//— per-width print helpers —//
// u8  : two hex digits
static inline void _dp_u8(uint8_t v, const char *lbl) {
    printf("%#10.2x %s\n", v, lbl);
}
// u16 : four hex digits
static inline void _dp_u16(uint16_t v, const char *lbl) {
    printf("%#10.4x %s\n", v, lbl);
}
// u32 : eight hex digits
static inline void _dp_u32(uint32_t v, const char *lbl) {
    printf("%#10.8x %s\n", v, lbl);
}
// f32 : interpret the float’s bits as a uint32 and print eight hex digits
static inline void _dp_f32(float v, const char *lbl) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof bits);
    printf("%#10.8x %s\n", bits, lbl);
}
// f16 : interpret the float16_t’s bits as a uint16 and print four hex digits
static inline void _dp_f16(float16_t v, const char *lbl) {
    uint16_t bits;
    memcpy(&bits, &v, sizeof bits);
    _dp_u16(bits, lbl);
}

//— public macro chooses helper by type —//
#define debug_print(var, lbl)                                          \
    _Generic((var),                                                    \
        uint8_t:    _dp_u8,  int8_t:    _dp_u8,                        \
       uint16_t:   _dp_u16, int16_t:   _dp_u16,                        \
     /* catch your half-precision type */                              \
       float16_t:  _dp_f16,                                            \
      uint32_t:   _dp_u32,  int32_t:   _dp_u32,                        \
         float:     _dp_f32,                                           \
        default:     _dp_u32                                           \
    )(var, lbl)
