#ifndef DRAGON4_H
#define DRAGON4_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef RJ_ASSERT
#include <assert.h>
#define RJ_ASSERT(condition) assert(condition)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Character types
typedef char        tC8;

// Boolean types
typedef bool        tB;

// Unsigned integer types
typedef uint8_t     tU8;
typedef uint16_t    tU16;
typedef uint32_t    tU32;
typedef uint64_t    tU64;

// Signed integer types
typedef int8_t      tS8;
typedef int16_t     tS16;
typedef int32_t     tS32;
typedef int64_t     tS64;

// Floating point types
typedef float       tF32;
typedef double      tF64;

// Size types
typedef size_t      tSize;
typedef ptrdiff_t   tPtrDiff;

// Define enum properly for C
typedef enum tCutoffMode
{
    CutoffMode_Unique,        // as many digits as necessary to identify uniquely
    CutoffMode_TotalLength,   // up to cutoffNumber total significant digits
    CutoffMode_FractionLength // up to cutoffNumber digits past the decimal point
} tCutoffMode;

// Declare Dragon4() API
tU32 Dragon4(
    tU64 mantissa,
    tS32 exponent,
    tU32 mantissaHighBitIdx,
    tB hasUnequalMargins,
    tCutoffMode cutoffMode,
    tU32 cutoffNumber,
    tC8 *pOutBuffer,
    tU32 bufferSize,
    tS32 *pOutExponent
);

#ifdef __cplusplus
}
#endif

#endif // DRAGON4_H
