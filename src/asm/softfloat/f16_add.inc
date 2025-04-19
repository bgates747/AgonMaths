; add two 16-bit floating point numbers
; inputs: hl = a, de = b
; outputs: hl = a + b
; destroys: af, bc, de, ix
f16_add:
; set index to global variables
    ld ix,s_globals
; compare signs
    ld a,h ; sign opA
    and %10000000
    xor d ; xor sign opA with sign opB
    and %10000000
; if ( signF16UI( uiA ^ uiB ) ) {
;     return softfloat_subMagsF16( uiA, uiB );
        call nz,softfloat_subMagsF16
; } else {
;     return softfloat_addMagsF16( uiA, uiB );
        jp softfloat_addMagsF16
; end f16_add