; sub two 16-bit floating point numbers
; inputs: hl = a, de = b
; outputs: hl = a - b
; destroys: af, bc, de, ix
f16_sub:
; float16_t f16_sub( float16_t a, float16_t b )
; {
;     union ui16_f16 uA;
;     uint_fast16_t uiA;
;     union ui16_f16 uB;
;     uint_fast16_t uiB;
; #if ! defined INLINE_LEVEL || (INLINE_LEVEL < 1)
;     float16_t (*magsFuncPtr)( uint_fast16_t, uint_fast16_t );
; #endif

;     uA.f = a;
;     uiA = uA.ui;
;     uB.f = b;
;     uiB = uB.ui;
; #if defined INLINE_LEVEL && (1 <= INLINE_LEVEL)
;     if ( signF16UI( uiA ^ uiB ) ) {
;         return softfloat_addMagsF16( uiA, uiB );
;     } else {
;         return softfloat_subMagsF16( uiA, uiB );
;     }
; #else
;     magsFuncPtr =
;         signF16UI( uiA ^ uiB ) ? softfloat_addMagsF16 : softfloat_subMagsF16;
;     return (*magsFuncPtr)( uiA, uiB );
; #endif

; }
    ret
; end f16_sub