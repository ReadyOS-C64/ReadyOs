.setcpu "6502"

SCREEN      = $0400
COLOR_RAM   = $D800
BORDER      = $D020
BG          = $D021

REU_COMMAND  = $DF01
REU_C64_LO   = $DF02
REU_C64_HI   = $DF03
REU_REU_LO   = $DF04
REU_REU_HI   = $DF05
REU_REU_BANK = $DF06
REU_LEN_LO   = $DF07
REU_LEN_HI   = $DF08

REU_CMD_STASH = $90
REU_CMD_FETCH = $91

DBG_BASE = $C7A0
DBG_HEAD = $C7DF
RESULT0  = $C7E8
RESULT1  = $C7E9
RESULT2  = $C7EA
RESULT3  = $C7EB
STOP_SUCCESS = $C7F8
STOP_FAIL = $C7FA

HOST_FLAG = $48
HOST_DATA_FLAG = $56
PAYLOAD_FLAG = $50
STASH_FLAG = $53
DATA_BANK = $02
SCRATCH_BANK = $03
FETCH_BUF = $C000
VERIFY_BUF = $C100
EXPECTED = $C200

.export _xefprobe_payload_start
.export _xefprobe_payload_after_fetch_checkpoint
.export _xefprobe_payload_done
.export _xefprobe_payload_fail_loop

.segment "LOADADDR"
    .word $1000

.segment "STARTUP"
_xefprobe_payload_start:
    sei
    jsr clear_screen_green
    jsr draw_title
    lda RESULT0
    cmp #HOST_FLAG
    bne payload_flags_fail_hold
    lda RESULT1
    cmp #HOST_DATA_FLAG
    bne payload_flags_fail_hold
    jsr fetch_probe_data
    jsr verify_probe_data
    bcs :+
    jmp payload_data_fail_hold
:
    jsr draw_fetch_ok
    jsr stash_roundtrip
    bcs :+
    jmp payload_stash_fail_hold
:
    jsr draw_stash_ok
hold:
    jmp hold

payload_flags_fail_hold:
    jsr draw_flags_fail
    jmp hold

payload_data_fail_hold:
    jsr draw_data_fail
    jmp hold

payload_stash_fail_hold:
    jsr draw_stash_fail
    jmp hold

dbg_put_p:
    lda #'P'
    jmp dbg_put

dbg_put:
    ldx DBG_HEAD
    sta DBG_BASE,x
    inx
    cpx #$3F
    bcc :+
    ldx #$00
:
    stx DBG_HEAD
    rts

clear_screen_green:
    lda #$05
    sta BORDER
    sta BG
    ldx #$00
    lda #$20
@screen:
    sta SCREEN,x
    sta SCREEN+$0100,x
    sta SCREEN+$0200,x
    sta SCREEN+$02E8,x
    inx
    bne @screen
    ldx #$00
    lda #$01
@color:
    sta COLOR_RAM,x
    sta COLOR_RAM+$0100,x
    sta COLOR_RAM+$0200,x
    sta COLOR_RAM+$02E8,x
    inx
    bne @color
    rts

draw_title:
    ldx #$00
@line0:
    lda title_line0,x
    beq @done
    sta SCREEN,x
    lda #$07
    sta COLOR_RAM,x
    inx
    bne @line0
@done:
    rts

draw_fetch_ok:
    lda #'D'
    jsr dbg_put
    ldx #$00
@copy:
    lda fetch_ok_line,x
    beq @done
    sta SCREEN+80,x
    lda #$0D
    sta COLOR_RAM+80,x
    inx
    bne @copy
@done:
    rts

draw_stash_ok:
    lda #'S'
    jsr dbg_put
    ldx #$00
@copy:
    lda stash_ok_line,x
    beq @done
    sta SCREEN+160,x
    lda #$0E
    sta COLOR_RAM+160,x
    inx
    bne @copy
@done:
    rts

draw_fail:
    lda #'F'
    jsr dbg_put
    ldx #$00
@copy:
    lda fail_line,x
    beq @done
    sta SCREEN+80,x
    lda #$07
    sta COLOR_RAM+80,x
    inx
    bne @copy
@done:
    rts

draw_flags_fail:
    ldx #$00
@copy:
    lda flags_fail_line,x
    beq @done
    sta SCREEN+80,x
    lda #$07
    sta COLOR_RAM+80,x
    inx
    bne @copy
@done:
    rts

draw_data_fail:
    ldx #$00
@copy:
    lda data_fail_line,x
    beq @done
    sta SCREEN+80,x
    lda #$07
    sta COLOR_RAM+80,x
    inx
    bne @copy
@done:
    rts

draw_stash_fail:
    ldx #$00
@copy:
    lda stash_fail_line,x
    beq @done
    sta SCREEN+160,x
    lda #$07
    sta COLOR_RAM+160,x
    inx
    bne @copy
@done:
    rts

fetch_probe_data:
    lda #<FETCH_BUF
    sta REU_C64_LO
    lda #>FETCH_BUF
    sta REU_C64_HI
    lda #$00
    sta REU_REU_LO
    sta REU_REU_HI
    lda #DATA_BANK
    sta REU_REU_BANK
    lda #$00
    sta REU_LEN_LO
    lda #$01
    sta REU_LEN_HI
    lda #REU_CMD_FETCH
    sta REU_COMMAND
    rts

verify_probe_data:
    ldx #$00
    lda #$33
    sta EXPECTED
@loop:
    lda FETCH_BUF,x
    cmp EXPECTED
    bne @fail
    clc
    lda EXPECTED
    adc #$05
    sta EXPECTED
    inx
    bne @loop
    sec
    rts
@fail:
    clc
    rts

stash_roundtrip:
    ldx #$00
    lda #$69
    sta EXPECTED
@fill:
    lda EXPECTED
    sta FETCH_BUF,x
    lda #$00
    sta VERIFY_BUF,x
    clc
    lda EXPECTED
    adc #$05
    sta EXPECTED
    inx
    bne @fill

    lda #<FETCH_BUF
    sta REU_C64_LO
    lda #>FETCH_BUF
    sta REU_C64_HI
    lda #$00
    sta REU_REU_LO
    sta REU_REU_HI
    lda #SCRATCH_BANK
    sta REU_REU_BANK
    lda #$00
    sta REU_LEN_LO
    lda #$01
    sta REU_LEN_HI
    lda #REU_CMD_STASH
    sta REU_COMMAND

    lda #<VERIFY_BUF
    sta REU_C64_LO
    lda #>VERIFY_BUF
    sta REU_C64_HI
    lda #$00
    sta REU_REU_LO
    sta REU_REU_HI
    lda #SCRATCH_BANK
    sta REU_REU_BANK
    lda #$00
    sta REU_LEN_LO
    lda #$01
    sta REU_LEN_HI
    lda #REU_CMD_FETCH
    sta REU_COMMAND

    ldx #$00
    lda #$69
    sta EXPECTED
@verify:
    lda VERIFY_BUF,x
    cmp EXPECTED
    bne @fail
    clc
    lda EXPECTED
    adc #$05
    sta EXPECTED
    inx
    bne @verify
    sec
    rts
@fail:
    clc
    rts

payload_fail:
    jsr draw_fail
    jmp _xefprobe_payload_fail_loop

_xefprobe_payload_after_fetch_checkpoint:
    rts

_xefprobe_payload_done:
    jmp STOP_SUCCESS

_xefprobe_payload_fail_loop:
    jmp STOP_FAIL

.segment "RODATA"
title_line0:
    .byte $18,$05,$06,$10,$12,$0F,$02,$05,$20,$10,$01,$19,$0C,$0F,$01,$04,$00
fetch_ok_line:
    .byte $03,$12,$14,$2D,$3E,$12,$05,$15,$20,$06,$05,$14,$03,$08,$20,$10,$01,$13,$13,$00
stash_ok_line:
    .byte $12,$05,$15,$20,$13,$14,$01,$13,$08,$2F,$06,$05,$14,$03,$08,$20,$10,$01,$13,$13,$00
fail_line:
    .byte $10,$01,$19,$0C,$0F,$01,$04,$20,$06,$01,$09,$0C,$00
flags_fail_line:
    .byte $06,$0C,$01,$07,$13,$20,$06,$01,$09,$0C,$00
data_fail_line:
    .byte $04,$01,$14,$01,$20,$06,$01,$09,$0C,$00
stash_fail_line:
    .byte $13,$14,$01,$13,$08,$20,$06,$01,$09,$0C,$00
