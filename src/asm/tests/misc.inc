; test the sign of UHL
; inputs: UHL obviously
; outputs: sign flag set if UHL is negative, zero flag set if UHL is zero
    MACRO SIGN_UHL
    add hl,de
    or a ; clear flags
    sbc hl,de
    ENDMACRO

    MACRO HLU_MUL_256
    add hl,hl ; * 2
    add hl,hl ; * 4
    add hl,hl ; * 8
    add hl,hl ; * 16 
    add hl,hl ; * 32
    add hl,hl ; * 64
    add hl,hl ; * 128
    add hl,hl ; * 256
    ENDMACRO

    MACRO SRL_UHL
    dec sp ; 1 cycle
    push hl ; 4 cycles
    inc sp ; 1 cycle
    pop hl ; 4 cycles
    inc hl ; 1 cycle
    dec.s hl ; 2 cycles <-- the .s sets hlu to zero (undocumented)
    ; 13 cycles total
    ENDMACRO

; put the value in HLU into A
    MACRO HLU_TO_A
    dec sp ; 1 cycle
    push hl ; 4 cycles
    inc sp ; 1 cycle
    pop af ; 4 cycles
    ; 10 cycles total
    ENDMACRO

    MACRO PRINT_HL_HEX msg
        push de
        push af
        push hl
        ld a,'0'
        rst.lil 0x10
        ld a,'x'
        rst.lil 0x10
        call printHexHL
        call printInline
        asciz msg,"\r\n"
        pop hl
        pop af
        pop de
    ENDMACRO

; VDU 12: Clear text area (CLS)
vdu_cls:
    ld a,12
	rst.lil $10  
	ret

vdu_flip:       
	ld hl,@cmd         
	ld bc,@end-@cmd    
	rst.lil $18         
	ret
@cmd: db 23,0,0xC3
@end:

; set a stopwatch
; returns: hl = start time 
; destroys: hl,ix
stopwatch_set:
    MOSCALL mos_sysvars     ; ix points to syvars table
    ld hl,(ix+sysvar_time)  ; get current time
    ld (stopwatch_started),hl            ; set start time
    ret

; gets time elapsed on a stopwatch
; returns: hl = time elapsed in 1/120ths of a second
; destroys: af,hl,de,ix
stopwatch_get:
    MOSCALL mos_sysvars     ; ix points to syvars table
    ld hl,(ix+sysvar_time)  ; get current time
    ld de,(stopwatch_started)            ; get start time
    xor a                   ; clear carry
    sbc hl,de               ; hl = time elapsed (will always be zero or positive)
    ret

stopwatch_started: ds 3 ; buffer to hold stopwatch start time

; Print Newline sequence to VDP
; destroys: nothing
printNewLine:
    push af ; for some reason rst.lil 10h sets carry flag
    LD A, '\r'
    RST.LIL 10h
    LD A, '\n'
    RST.LIL 10h
    pop af
    RET

; Print a zero-terminated string inline with code, e.g.:
;
;    call printInline
;    ASCIZ "Hello, world!\r\n"
;
; Destroys: HL,AF
printInline:
    pop hl ; get the return address = pointer to start of string
    call printString ; HL advances to end of string
    push hl ; restore the return address = pointer to end of string
    ret

; Print a zero-terminated string
; HL: Pointer to string
; returns: hl pointed to character after string terminator
; destroys: af, hl
printString:
    PUSH BC
    LD BC,0
    LD A,0
    RST.LIL 18h
    POP BC
    RET

; Prints the right justified decimal value in UHL without leading zeroes
; UHL : Value to print
; preserves all registers and flags
printDec:
; back up all the things
    push af
    push bc
    push de
    push hl
    LD DE, _printDecBuffer
    CALL u24_to_ascii
; replace leading zeroes with spaces
    LD HL, _printDecBuffer
    ld B, 7 ; if HL was 0, we want to keep the final zero 
@loop:
    LD A, (HL)
    CP '0'
    JP NZ, @done
    LD A, ' '
    LD (HL), A
    INC HL
    DJNZ @loop
@done:
    LD HL, _printDecBuffer
    CALL printString
; restore all the things
    pop hl
    pop de
    pop bc
    pop af
    RET
_printDecBuffer: blkb 16,0 ; a few bytes extra just to be sure
_printDec168Buffer: blkb 16,0 ; a few bytes extra just to be sure

; This routine converts the unsigned 24-bit value in HLU into its ASCII representation, 
; starting to memory location pointing by DE, in decimal form and with leading zeroes 
; so it will allways be 8 characters length
; HL : Value to convert to string
; DE : pointer to buffer, at least 8 byte + 0
u24_to_ascii:
    LD BC,-10000000
    CALL one_digit
    LD BC,-1000000
    CALL one_digit
    LD BC,-100000
    CALL one_digit
    LD BC,-10000
    CALL one_digit
u8_to_ascii: ; same arguments but hl <= 255, uhl and h = 0
    LD BC,-1000
    CALL one_digit
    LD BC,-100
    CALL one_digit
    LD C,-10
    CALL one_digit
    LD C,B
one_digit:
    LD A,'0'-1
@divide_me:
    INC A
    ADD HL,BC
    JR C,@divide_me
    SBC HL,BC
    LD (DE),A
    INC DE
    RET
; end u24_to_ascii

print_s168_de:
    push af
    push de
    push hl
    ex de,hl
    call print_s168
    ld a,' '
    rst.lil 10h
    pop hl
    pop de
    pop af
    ret

print_s168:
    push de
    push hl
    ld de,_printDec168Buffer
    call s168_to_ascii
    ld hl,_printDec168Buffer
    call printString
    pop hl
    pop de
    ret

; signed version of u168_to_ascii
s168_to_ascii:
    push de ; save starting address of buffer
    call hlu_abs
    push af ; save sign flag
    call u168_to_ascii
    pop af ; restore sign flag
    pop de ; restore starting address of buffer
    ret p ; hlu was positive so nothing to do
    ld a,'-'
    ld (de),a
    ret

; u168_to_ascii: Converts an unsigned 16.8 fixed-point value in HL to an ASCII string.
; HL : Value to convert to string (integer part in H, fractional part in L)
; DE : Pointer to buffer, requires at least 11 bytes (8 digits, sign, decimal point, and null terminator)
u168_to_ascii:
; add a leading space to make room for sign flag if needed
    ld a,' '
    ld (de),a
    inc de
; Convert integer part
    push hl ; Save HL (we’ll need the fractional part later)
    ; call hlu_udiv256 ; Shift to get integer portion in HL
    SRL_UHL ; Shift to get integer portion in HL
    ld bc, -10000
    call @one_int
    ld bc, -1000
    call @one_int
    ld bc, -100
    call @one_int
    ld c, -10
    call @one_int
    ld c, b
    call @one_int
    jp @frac ; Jump to fractional part conversion
@one_int:
    ld a, '0' - 1 ; Start ASCII character at '0'
@divide_me:
    inc a
    add hl, bc ; Accumulate until overflow
    jr c, @divide_me
    sbc hl, bc ; Remove excess after overflow
    ld (de), a ; Store ASCII digit
    inc de
    ret
; Convert fractional part
@frac:
    ld a, '.' ; Decimal point
    ld (de), a
    inc de
    pop hl ; Restore HL with original fraction
    ld b, 3 ; Loop counter for 3 fractional digits
@frac_loop:
    ld h, 10 ; Load multiplier for fractional part
    mlt hl ; Multiply by 10, result in HL (H holds the integer part)
    ld a, '0'
    add a, h ; Convert integer part to ASCII
    ld (de), a
    inc de
    djnz @frac_loop ; Repeat for each fractional digit
; Add null terminator
    xor a ; Null terminator
    ld (de), a
    ret

; Print a 24-bit HEX number
; HLU: Number to print
printHex24:
    HLU_TO_A
    CALL printHex8
; Print a 16-bit HEX number
; HL: Number to print
printHex16:
    LD A,H
    CALL printHex8
    LD A,L
; Print an 8-bit HEX number
; A: Number to print
printHex8:
    LD C,A
    RRA 
    RRA 
    RRA 
    RRA 
    CALL @F
    LD A,C
@@:
    AND 0Fh
    ADD A,90h
    DAA
    ADC A,40h
    DAA
    RST.LIL 10h
    RET

printHexHL:
    push af
    push bc
    call printHex16
    ld a,' '
    rst.lil 10h
    pop bc
    pop af
    ret

; print registers to screen in hexidecimal format
; inputs: none
; outputs: values of every register printed to screen
;    values of each register in global scratch memory
; destroys: nothing
dumpRegistersHex:
; store everything in scratch
    ld (uhl),hl
    ld (ubc),bc
    ld (ude),de
    ld (uix),ix
    ld (uiy),iy
    push af ; fml
    pop hl ; thanks, zilog
    ld (uaf),hl
    push af ; dammit

; home the cursor
    ; call vdu_home_cursor
    ; call printNewLine

; print each register
    ld hl,str_afu
    call printString
    ld hl,(uaf)
    call printHex24
    ; call printNewLine

    ld hl,str_hlu
    call printString
    ld hl,(uhl)
    call printHex24
    ; call printNewLine

    ld hl,str_bcu
    call printString
    ld hl,(ubc)
    call printHex24
    ; call printNewLine

    ld hl,str_deu
    call printString
    ld hl,(ude)
    call printHex24
    ; call printNewLine

    ld hl,str_ixu
    call printString
    ld hl,(uix)
    call printHex24
    ; call printNewLine

    ld hl,str_iyu
    call printString
    ld hl,(uiy)
    call printHex24
    
    call printNewLine
    ; call printNewLine
; restore everything
    ld hl, (uhl)
    ld bc, (ubc)
    ld de, (ude)
    ld ix, (uix)
    ld iy, (uiy)
    pop af
; all done
    ret

str_afu: db " af=",0
str_hlu: db " hl=",0
str_bcu: db " bc=",0
str_deu: db " de=",0
str_ixu: db " ix=",0
str_iyu: db " iy=",0

; global scratch memory for registers
uaf: dl 0
uhl: dl 0
ubc: dl 0
ude: dl 0
uix: dl 0
uiy: dl 0
usp: dl 0
upc: dl 0

;------------------------------------------------------------------------
; udiv24
; Unsigned 24-bit division
; Divides HLU by DEU. Gives result in DEU (and BC), remainder in HLU.
; 
; Uses AF BC DE HL
; Uses Restoring Division algorithm
;------------------------------------------------------------------------

udiv24:
	push	hl
	pop		bc	;move dividend to BCU
	ld		hl,0	;result
	and		a
	sbc		hl,de	;test for div by 0
	ret		z		;it's zero, carry flag is clear
	add		hl,de	;HL is 0 again
	ld		a,24	;number of loops through.
udiv1:
	push	bc	;complicated way of doing this because of lack of access to top bits
	ex		(sp),hl
	scf
	adc	hl,hl
	ex	(sp),hl
	pop	bc		;we now have bc = (bc * 2) + 1

	adc	hl,hl
	and	a		;is this the bug
	sbc	hl,de
	jr	nc,udiv2
	add	hl,de
;	dec	c
	dec	bc
udiv2:
	dec	a
	jr	nz,udiv1
	scf		;flag used for div0 error
	push	bc
	pop		de	;remainder
	ret

; UH.L / UD.E --> UD.E rem UHL (unsigned)
; perform unsigned division of 16.8 fixed place values
; with an unsigned 16.8 fixed place result and 24-bit remainder
udiv168:
; back up divisor
    ld (@ude),de
; get the 16-bit integer part of the quotient
    call udiv24 ; de = quotient, hl = remainder
; load quotient to upper three bytes of output
    ld (div168_out+1),de
@div256:
; multiply remainder by 256
    HLU_MUL_256
; skip fractional computation if remainder is zero
    SIGN_UHL
    jr nz,@div_frac
    xor a
    jr @write_frac
; now divide the shifted remainder by the divisor
@div_frac:
    ld de,(@ude) ; get back divisor
    call udiv24 ; de = quotient, hl = remainder
; load low byte of quotient to low byte of output
    ld a,e
@write_frac:
    ld (div168_out),a
; load de with return value
    ld de,(div168_out)
; load a with any overflow
    ld a,(div168_out+3)
    ret ; ud.e is the 16.8 result
@ude: ds 6
div168_out: ds 4 ; the extra byte is for overflow

; absolute value of hlu
; returns: abs(hlu), flags set according to the incoming sign of hlu:
;         s1,z0,pv0,n1,c0 if hlu was negative
;         s0,z1,pv0,n1,c0 if hlu was zero
;         s0,z0,pv0,n1,c0 if hlu was positive
; destroys: a
hlu_abs:
    add hl,de
    or a
    sbc hl,de 
    jp m,@is_neg
    ret ; hlu is positive or zero so we're done
@is_neg:
    push af ; otherwise, save current flags for return
    call neg_hlu ; negate hlu
    pop af ; get back flags
    ret

; flip the sign of hlu
; inputs: hlu
; returns: 0-hlu, flags set appropriately for the result:
;         s1,z0,pv0,n1,c1 if result is negative
;         s0,z1,pv0,n1,c0 if result is zero
;         s0,z0,pv0,n1,c1 if result is positive
; destroys a
neg_hlu:
    push de ; save de
    ex de,hl ; put hl into de
    ld hl,0 ; clear hl
    xor a ; clear carry
    sbc hl,de ; 0-hlu = -hlu
    pop de ; get de back
    ret ; easy peasy
