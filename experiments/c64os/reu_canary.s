; Native installation diagnostic, invoked via BASIC SYS49152 before OS boot.
; Fill first/last pages of every bank outside C64 OS's 2MB partition.
; This actually executes DMA on the CPU; monitor register pokes do not.
.segment "CODE"
        php
        sei
        lda #0
        sta $df09
        sta $df0a
        lda #32
        sta bank
nextbank:
        ldx #0
fill:   txa
        eor bank
        sta $c100,x
        inx
        bne fill
        lda #0
        jsr transfer
        lda #$ff
        jsr transfer
        inc bank
        bne nextbank
        ; Remove C64 OS warm-boot signature in workspace bank 0.
        ldx #7
        lda #0
zero:   sta $c100,x
        dex
        bpl zero
        sta bank
        jsr transfer
        lda #$a5
        sta $c0f0
        plp
        rts
transfer:
        sta $df05
        lda #0
        sta $df02
        sta $df04
        sta $df07
        lda #$c1
        sta $df03
        lda bank
        sta $df06
        lda #1
        sta $df08
        lda #$90
        sta $df01
        rts
bank:   .byte 0
