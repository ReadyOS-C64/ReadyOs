.setcpu "6502"

SCREEN      = $0400
COLOR_RAM   = $D800
BORDER      = $D020
BG          = $D021
CART_BANK_REG = $DE00
CART_CTRL_EX_REG = $DE01

REU_COMMAND  = $DF01
REU_C64_LO   = $DF02
REU_C64_HI   = $DF03
REU_REU_LO   = $DF04
REU_REU_HI   = $DF05
REU_REU_BANK = $DF06
REU_LEN_LO   = $DF07
REU_LEN_HI   = $DF08
REU_STATUS   = $DF00

REU_CMD_STASH = $90
REU_CMD_FETCH = $91

DBG_BASE = $C7A0
DBG_HEAD = $C7DF
RESULT0  = $C7E8
RESULT1  = $C7E9
RESULT2  = $C7EA
RESULT3  = $C7EB
RESULT4  = $C7EC
RESULT5  = $C7ED
RESULT6  = $C7EE
RESULT7  = $C7EF
STOP_SUCCESS = $C7F8
STOP_FAIL = $C7FA

SHIM_TARGET_BANK = $C820
SHIM_CURRENT_BANK = $C834

HOST_FLAG = $48
HOST_DATA_FLAG = $56
HOST_LOCAL_REU_FLAG = $52
PAYLOAD_BANK = $01
DATA_BANK = $02
SCRATCH_BANK = $03
FETCH_BUF = $B000
VERIFY_BUF = $B100

.export _xefprobe_host_start
.export _xefprobe_host_after_clear_checkpoint
.export _xefprobe_host_after_verify_checkpoint
.export _xefprobe_host_fail_loop

.segment "LOADADDR"
    .word $1000

.segment "STARTUP"
_xefprobe_host_start:
    sei
    jsr dbg_put_h
    jsr clear_screen_blue
    jsr _xefprobe_host_after_clear_checkpoint
    jsr draw_title
    lda #HOST_FLAG
    sta RESULT0
    lda #'Q'
    jsr dbg_put
    jsr sample_reu_registers
    lda #'q'
    jsr dbg_put
    lda #'W'
    jsr dbg_put
    jsr local_reu_roundtrip
    bcs :+
    jmp _xefprobe_host_fail_loop
:
    lda #HOST_LOCAL_REU_FLAG
    sta RESULT1
    lda #'R'
    jsr dbg_put
    lda #'D'
    jsr dbg_put
    jsr fetch_probe_data
    lda #'d'
    jsr dbg_put
    jsr snapshot_fetch_bytes
    lda #'V'
    jsr dbg_put
    jsr verify_probe_data
    bcs :+
    jmp _xefprobe_host_fail_loop
:
    lda #HOST_DATA_FLAG
    sta RESULT1
    jsr _xefprobe_host_after_verify_checkpoint
    jsr draw_verified
    jsr short_delay
    lda #'L'
    jsr dbg_put
    lda #PAYLOAD_BANK
    sta SHIM_TARGET_BANK
    lda #$00
    sta SHIM_CURRENT_BANK
    jmp $C80F

dbg_put_h:
    lda #'H'
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

clear_screen_blue:
    lda #$06
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
    beq @line1
    sta SCREEN,x
    lda #$07
    sta COLOR_RAM,x
    inx
    bne @line0
@line1:
    ldx #$00
@copy1:
    lda title_line1,x
    beq @done
    sta SCREEN+80,x
    lda #$01
    sta COLOR_RAM+80,x
    inx
    bne @copy1
@done:
    rts

draw_verified:
    ldx #$00
@copy:
    lda ok_line,x
    beq @done
    sta SCREEN+160,x
    lda #$0D
    sta COLOR_RAM+160,x
    inx
    bne @copy
@done:
    rts

snapshot_fetch_bytes:
    lda FETCH_BUF+0
    sta RESULT2
    lda FETCH_BUF+1
    sta RESULT3
    lda FETCH_BUF+2
    sta RESULT4
    lda FETCH_BUF+3
    sta RESULT5
    lda FETCH_BUF+4
    sta RESULT6
    lda FETCH_BUF+5
    sta RESULT7
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

sample_reu_registers:
    lda #$5A
    sta REU_REU_BANK
    lda REU_REU_BANK
    sta RESULT2
    lda REU_STATUS
    sta RESULT3
    lda CART_BANK_REG
    sta RESULT4
    lda CART_CTRL_EX_REG
    sta RESULT5
    rts

local_reu_roundtrip:
    ldx #$00
    lda #$33
    sta expected
@fill:
    lda expected
    sta FETCH_BUF,x
    lda #$00
    sta VERIFY_BUF,x
    clc
    lda expected
    adc #$05
    sta expected
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
    lda #$33
    sta expected
@verify:
    lda VERIFY_BUF,x
    cmp expected
    bne @fail
    clc
    lda expected
    adc #$05
    sta expected
    inx
    bne @verify
    sec
    rts
@fail:
    clc
    rts

verify_probe_data:
    ldx #$00
    lda #$33
    sta expected
@loop:
    lda FETCH_BUF,x
    cmp expected
    bne @fail
    clc
    lda expected
    adc #$05
    sta expected
    inx
    bne @loop
    sec
    rts
@fail:
    jsr draw_fail
    clc
    rts

draw_fail:
    lda #'F'
    jsr dbg_put
    ldx #$00
@copy:
    lda fail_line,x
    beq @done
    sta SCREEN+160,x
    lda #$07
    sta COLOR_RAM+160,x
    inx
    bne @copy
@done:
    rts

short_delay:
    ldy #$20
@outer:
    ldx #$00
@inner:
    dex
    bne @inner
    dey
    bne @outer
    rts

_xefprobe_host_after_clear_checkpoint:
    lda #'C'
    jmp dbg_put

_xefprobe_host_after_verify_checkpoint:
    lda #'V'
    jmp dbg_put

_xefprobe_host_fail_loop:
    jmp STOP_FAIL

.segment "RODATA"
title_line0:
    .byte $18,$05,$06,$10,$12,$0F,$02,$05,$20,$08,$0F,$13,$14,$00
title_line1:
    .byte $12,$15,$0E,$0E,$09,$0E,$07,$20,$06,$12,$0F,$0D,$20,$12,$05,$15,$20,$02,$01,$0E,$0B,$20,$30,$00
ok_line:
    .byte $03,$12,$14,$2D,$3E,$12,$05,$15,$20,$04,$01,$14,$01,$20,$0F,$0B,$00
fail_line:
    .byte $06,$05,$14,$03,$08,$20,$06,$01,$09,$0C,$05,$04,$00

.segment "BSS"
expected:
    .res 1
