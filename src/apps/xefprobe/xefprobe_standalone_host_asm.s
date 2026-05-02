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
RESULT4  = $C7EC
RESULT5  = $C7ED
RESULT6  = $C7EE
RESULT7  = $C7EF
STOP_SUCCESS = $C7F8
STOP_FAIL = $C7FA

HOST_FLAG = $48
HOST_DATA_FLAG = $56
PAYLOAD_BANK = $01
DATA_BANK = $02

PAYLOAD_LOAD = $1000
DATA_BUF = $C000
VERIFY_BUF = $C100

.segment "LOADADDR"
    .word $0801

.segment "STARTUP"
basic_stub:
    .word nextline
    .word 800
    .byte $9E
    .byte "2061"
    .byte 0
nextline:
    .word 0

.segment "CODE"

start:
    sei
    jsr clear_screen_blue
    jsr draw_title
    jsr build_probe_data
    jsr stash_probe_data
    jsr fetch_probe_data
    jsr verify_probe_data
    bcs :+
    jsr draw_data_fail
    jmp hold
:
    jsr draw_data_ok
    lda #HOST_FLAG
    sta RESULT0
    lda #HOST_DATA_FLAG
    sta RESULT1
    jsr stash_payload
    jsr fetch_payload
    lda payload_image
    cmp PAYLOAD_LOAD
    bne @payload_fail
    lda payload_image+1
    cmp PAYLOAD_LOAD+1
    bne @payload_fail
    lda payload_image+2
    cmp PAYLOAD_LOAD+2
    bne @payload_fail
    jmp PAYLOAD_LOAD
@payload_fail:
    jsr draw_payload_fail
hold:
    jmp hold

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
    lda #$0D
    sta COLOR_RAM+80,x
    inx
    bne @copy1
@done:
    rts

draw_data_ok:
    ldx #$00
@copy:
    lda data_ok_line,x
    beq @done
    sta SCREEN+160,x
    lda #$0D
    sta COLOR_RAM+160,x
    inx
    bne @copy
@done:
    rts

draw_data_fail:
    ldx #$00
@copy:
    lda data_fail_line,x
    beq @done
    sta SCREEN+160,x
    lda #$07
    sta COLOR_RAM+160,x
    inx
    bne @copy
@done:
    rts

draw_payload_fail:
    ldx #$00
@copy:
    lda payload_fail_line,x
    beq @done
    sta SCREEN+240,x
    lda #$07
    sta COLOR_RAM+240,x
    inx
    bne @copy
@done:
    rts

draw_payload_ok:
    ldx #$00
@copy:
    lda payload_ok_line,x
    beq @done
    sta SCREEN+240,x
    lda #$0E
    sta COLOR_RAM+240,x
    inx
    bne @copy
@done:
    rts

build_probe_data:
    ldx #$00
    lda #$33
    sta expected
@fill:
    lda expected
    sta DATA_BUF,x
    lda #$00
    sta VERIFY_BUF,x
    clc
    lda expected
    adc #$05
    sta expected
    inx
    bne @fill
    rts

stash_probe_data:
    lda #<DATA_BUF
    sta REU_C64_LO
    lda #>DATA_BUF
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
    lda #REU_CMD_STASH
    sta REU_COMMAND
    rts

fetch_probe_data:
    lda #<VERIFY_BUF
    sta REU_C64_LO
    lda #>VERIFY_BUF
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
    sta expected
@loop:
    lda VERIFY_BUF,x
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
    lda VERIFY_BUF,x
    sta RESULT2
    lda expected
    sta RESULT3
    clc
    rts

stash_payload:
    lda #<payload_image
    sta REU_C64_LO
    lda #>payload_image
    sta REU_C64_HI
    lda #$00
    sta REU_REU_LO
    sta REU_REU_HI
    lda #PAYLOAD_BANK
    sta REU_REU_BANK
    lda #<payload_len
    sta REU_LEN_LO
    lda #>payload_len
    sta REU_LEN_HI
    lda #REU_CMD_STASH
    sta REU_COMMAND
    rts

fetch_payload:
    lda #<PAYLOAD_LOAD
    sta REU_C64_LO
    lda #>PAYLOAD_LOAD
    sta REU_C64_HI
    lda #$00
    sta REU_REU_LO
    sta REU_REU_HI
    lda #PAYLOAD_BANK
    sta REU_REU_BANK
    lda #<payload_len
    sta REU_LEN_LO
    lda #>payload_len
    sta REU_LEN_HI
    lda #REU_CMD_FETCH
    sta REU_COMMAND
    rts

.segment "RODATA"
title_line0:
    .byte 24,5,6,16,18,15,2,5,32,19,20,1,14,4,1,12,15,14,5,32,8,15,19,20,0
title_line1:
    .byte 19,20,1,19,8,32,20,15,32,18,5,21,32,20,8,5,14,32,10,21,13,16,0
data_ok_line:
    .byte 4,1,20,1,32,15,11,0
data_fail_line:
    .byte 4,1,20,1,32,6,1,9,12,0
payload_ok_line:
    .byte 16,1,25,12,15,1,4,32,15,11,0
payload_fail_line:
    .byte 16,1,25,12,15,1,4,32,6,1,9,12,0

payload_image:
    .incbin "../../../bin/xefprobe_payload.prg", 2
payload_end:

payload_len = payload_end - payload_image

.segment "DATA"
expected:
    .byte 0
