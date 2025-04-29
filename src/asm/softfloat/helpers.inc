; Inputs: hl is the 16 bit multiplicand, a is the 8 bit multiplier
; Outputs: ade = 24-bit result
; Destroys: f, hl
mul_16x8_24_ade:
    ld d,l
    ld e,a
    mlt de ; l * a
    ld l,a
    mlt hl ; h * a
; hl<<8 + de -> ade
    ld a,d
    add a,l
    ld d,a
    adc a,h
    sub a,d
    ret
; end mul_16x8_24_ade

; Inputs: hl is the 16 bit multiplicand, a is the 8 bit multiplier
; Outputs: uhl = 24-bit result
; Destroys: f, de
mul_16x8_24_uhl:
    ld d,l
    ld e,a
    mlt de ; l * a
    ld l,a
    mlt hl ; h * a
; hl<<8 + de -> uhl
    add hl,hl ; 1
    add hl,hl ; 2
    add hl,hl ; 3
    add hl,hl ; 4
    add hl,hl ; 5
    add hl,hl ; 6
    add hl,hl ; 7
    add hl,hl ; 8
    add hl,de
    ret
; end mul_16x8_24_uhl

;Inputs: hl,de = operands
;Outputs: hlde = 32-bit product
;Destroys: af,bc
;53 cycles
;32 bytes
mul_16x16_32:
    ld b,e
    ld c,l
    ld l,e
    ld e,c
    mlt bc
    ld a,b
    ld b,h
    mlt hl
    ; Add high part of low product, cannot overflow 16 bits
    add a,l
    ld l,a
    adc a,h
    sub a,l
    ld h,a
    ld a,c
    ld c,d
    mlt de
    add.s hl,de ; .s to force 16-bit addition
    ld e,a
    ld d,l
    ld l,h
    ld h,0
    rl h
    mlt bc
    add hl,bc ; Cannot overflow 16 bits
    ret
; end mul_16x16_32

; multiply 32 x 16 -> 48 bit product
; inputs: hlde = 32-bit multiplicand, bc = 16-bit multiplier
; outputs: hldebc = 48-bit product
mul_32x16_48:
    push bc
    push de
; hl * bc
    ld e,c
    ld d,b
    call mul_16x16_32 ; hlde = hl * bc
; de * bc
    pop bc ; de
    ex (sp),hl ; bc
    push de
    ld e,c
    ld d,b
    call mul_16x16_32 ; hlde = de * bc
; accumulate bc out
    ld c,e
    ld b,d ; bc out
; accumulate de out
    pop de
    add.s hl,de
    ex de,hl ; de out
; accumulate hl out
    pop hl
    ret nc
    inc hl ; Cannot overflow 16 bits
    ret
; end mul_32x16_48_calc84

; https://www.cemetech.net/forum/viewtopic.php?t=12833&start=0
;Inputs:
;  HL:DE  32-bit unsigned integer
;Outputs:
;  A = HLDE mod 10
;  Z flag is set if divisible by 10
;Destroys:
;  HL
;  25 bytes,  109cc
hlde_mod10:
   add.s hl,de                  ;add words, n mod 65535 (+1)
   ld a,h                     ;add bytes
   adc a,l
   adc a,0                    ;n mod 255 (+1)
   ld h,a                     ;add nibbles
   rrca 
   rrca 
   rrca 
   rrca
   add a,h
   adc a,0                    ;n mod 15 (+1) in both nibbles
   daa
   ld h,a
   sub e                      ; Test if quotient is even or odd
   rra
   sbc a,a
   and 5
   add a,h
   daa
   and 0Fh
   ret
; end hlde_mod10

; https://www.cemetech.net/forum/viewtopic.php?t=12833&start=0
;Inputs:
;  HL  16-bit unsigned integer
;Outputs:
;  A = HL mod 10
;  Z flag is set if divisible by 10
;Destroys:
;  HL
;  24 bytes,  98cc
hl_mod10:
   ld a,h                     ;add bytes
   add a,l
   adc a,0                    ;n mod 255 (+1)
   ld h,a                     ;add nibbles
   rrca 
   rrca
   rrca
   rrca
   add a,h
   adc a,0                    ;n mod 15 (+1) in both nibbles
   daa
   ld h,a
   sub l
   rra
   sbc a,a
   and 5
   add a,h
   daa
   and 0Fh
   ret
; end hl_mod10

; https://www.cemetech.net/forum/viewtopic.php?t=12833&start=0
;Inputs:
;  A  8-bit unsigned integer
;Outputs:
;  A = A mod 10
;  Z flag is set if divisible by 10
;Destroys:
;  HL
a_mod10:
   ld h,a                     ;add nibbles
   rrca 
   rrca
   rrca
   rrca
   add a,h
   adc a,0                    ;n mod 15 (+1) in both nibbles
   daa
   ld l,a                     ; -- h -> l for 8-bit
   sub h                      ; -- l -> h for 8-bit. Test if quotient is even or odd
   rra
   sbc a,a
   and 5
   add a,l                    ; -- h -> l for 8-bit
   daa
   and 0Fh
   ret
; end a_mod10
