#include "dragon4.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Example wrapper: expose Dragon4 with plain C types
unsigned int Dragon4_CFFI(
    uint64_t mantissa,
    int32_t exponent,
    uint32_t mantissaHighBitIdx,
    bool hasUnequalMargins,
    enum tCutoffMode cutoffMode,
    uint32_t cutoffNumber,
    char *pOutBuffer,
    uint32_t bufferSize,
    int32_t *pOutExponent
) {
    return Dragon4(mantissa, exponent, mantissaHighBitIdx, hasUnequalMargins,
                   cutoffMode, cutoffNumber, pOutBuffer, bufferSize, pOutExponent);
}

#ifdef __cplusplus
}
#endif
