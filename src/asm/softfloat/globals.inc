; 16-bit floating point globals
    ; A group
    uiA:         EQU 0
    sigA:        EQU uiA + 3
    signA:       EQU sigA + 3
    expA:        EQU signA + 1

    ; B group
    uiB:         EQU expA + 2
    sigB:        EQU uiB + 3
    signB:       EQU sigB + 3
    expB:        EQU signB + 1

    ; X group
    sigX:        EQU expB + 2
    signX:       EQU sigX + 3
    expX:        EQU signX + 1
    
    ; Y group
    sigY:        EQU expX + 2
    signY:       EQU sigY + 3
    expY:        EQU signY + 1

    ; Z group
    uiZ:         EQU expY + 2
    sigZ:        EQU uiZ + 3
    signZ:       EQU sigZ + 3
    expZ:        EQU signZ + 1

    ; Shared / Misc
    expDiff:     EQU expZ + 2
    shiftDist:   EQU expDiff + 1

; 32-bit floating point globals
    sig32A:      EQU shiftDist + 1
    sig32B:      EQU sig32A + 5
    sig32Z:      EQU sig32B + 5


; Global variable table
    s_globals:

    _uiA:          dl 0
    _sigA:         dl 0
    _signA:        db 0
    _expA:         db 0
                   db 0   ; padding

    _uiB:          dl 0
    _sigB:         dl 0
    _signB:        db 0
    _expB:         db 0
                   db 0   ; padding

    ; X group 
    _sigX:         dl 0
    _signX:        db 0
    _expX:         db 0
                   db 0   ; padding

    ; Y group 
    _sigY:         dl 0
    _signY:        db 0
    _expY:         db 0
                   db 0   ; padding

    _uiZ:          dl 0
    _sigZ:         dl 0
    _signZ:        db 0
    _expZ:         db 0
                   db 0   ; padding

    _expDiff:      db 0
    _shiftDist:    db 0

    _sig32A:       blkb 5,0
    _sig32B:       blkb 5,0
    _sig32Z:       blkb 5,0
