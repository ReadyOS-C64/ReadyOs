; rom_float.s - C64 BASIC ROM float bridge for calcplus
;
; Exposes zero-argument helpers that operate on global buffers in calcplus.c.

        .export _romfp_eval_literal
        .export _romfp_add
        .export _romfp_sub
        .export _romfp_mul
        .export _romfp_div
        .export _romfp_to_str
        .export _romfp_set_pi
        .export _romfp_abs
        .export _romfp_sgn
        .export _romfp_int
        .export _romfp_sqr
        .export _romfp_exp
        .export _romfp_log
        .export _romfp_sin
        .export _romfp_cos
        .export _romfp_tan
        .export _romfp_atn
        .export _romfp_rnd

        .segment "CODE"

ROMFP_IN    = $C580
ROMFP_A     = $C5A0
ROMFP_B     = $C5A5
ROMFP_OUT   = $C5AA
ROMFP_TEXT  = $C5AF

;-----------------------------------------------------------------------------
; Internal helpers: temporarily map BASIC/KERNAL ROM in while using ROM math.
;-----------------------------------------------------------------------------
.macro ROM_ENTER
        lda $01
        pha
        ora #$03            ; ensure LORAM+HIRAM set (BASIC/KERNAL visible)
        sta $01
.endmacro

.macro ROM_EXIT
        pla
        sta $01
.endmacro

; Evaluate numeric literal in romfp_in into FAC1, then pack FAC1 into romfp_out.
_romfp_eval_literal:
        ROM_ENTER

        lda #<ROMFP_IN
        sta $7a
        lda #>ROMFP_IN
        sta $7b

        ; FIN: parse numeric literal at TXTPTR into FAC1
        lda ROMFP_IN       ; first character
        clc
        jsr $bcf3

        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF: FAC1 -> memory
        ROM_EXIT
        rts

; romfp_out = romfp_a + romfp_b
_romfp_add:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; FAC1 = A
        lda #<ROMFP_B
        ldy #>ROMFP_B
        jsr $b867           ; FADD: FAC1 += (AY)
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

; romfp_out = romfp_a - romfp_b
_romfp_sub:
        ROM_ENTER
        lda #<ROMFP_B
        ldy #>ROMFP_B
        jsr $bba2           ; FAC1 = B (subtrahend)
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $b850           ; FSUB: (AY) - FAC1 => A - B
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

; romfp_out = romfp_a * romfp_b
_romfp_mul:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; FAC1 = A
        lda #<ROMFP_B
        ldy #>ROMFP_B
        jsr $ba28           ; FAC1 *= (AY)
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

; romfp_out = romfp_a / romfp_b
_romfp_div:
        ROM_ENTER
        lda #<ROMFP_B
        ldy #>ROMFP_B
        jsr $bba2           ; FAC1 = B (divisor)
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bb0f           ; FDIV: (AY) / FAC1 => A / B
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

; Convert romfp_a to text in romfp_text (null-terminated, trimmed later in C).
_romfp_to_str:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; FAC1 = A
        jsr $bddd           ; AY -> string

        ; self-modifying absolute,Y source pointer to avoid trampling BASIC ZP
        sta @src+1
        sty @src+2

        ldy #$00
@copy:
@src:   lda $ffff,y
        beq @done
        sta ROMFP_TEXT,y
        iny
        cpy #31
        bcc @copy
@done:
        lda #$00
        sta ROMFP_TEXT,y
        ROM_EXIT
        rts

; romfp_out = PI
_romfp_set_pi:
        ROM_ENTER
        lda #$a8
        ldy #$ae
        jsr $bba2           ; MOVFM: PI constant bytes -> FAC1
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

; Unary FAC1 function helper pattern:
; FAC1 = romfp_a; call BASIC routine; romfp_out = FAC1

_romfp_abs:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $bc58           ; ABS
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_sgn:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $bc39           ; SGN
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_int:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $bccc           ; INT
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_sqr:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $bf71           ; SQR
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_exp:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $bfed           ; EXP
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_log:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $b9ea           ; LOG
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_sin:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $e26b           ; SIN
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_cos:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $e264           ; COS
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_tan:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $e2b4           ; TAN
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_atn:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $e30e           ; ATN
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts

_romfp_rnd:
        ROM_ENTER
        lda #<ROMFP_A
        ldy #>ROMFP_A
        jsr $bba2           ; MOVFM
        jsr $e097           ; RND
        ldx #<ROMFP_OUT
        ldy #>ROMFP_OUT
        jsr $bbd4           ; MOVMF
        ROM_EXIT
        rts
