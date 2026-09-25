; Copied above the BOOTER load range after both ReadyOS images are committed.
; Keep the $0400 trampoline intact so a failed LOAD can restore ReadyOS.
.segment "CODE"
        lda #$37
        sta $01
        ldx #$ff
        txs
        ; Recreate the cold KERNAL workspace; native C64 OS boot expects its
        ; low-RAM globals to start clean. Retain IDEDOS's patched I/O vectors.
        ldx #25
save_vectors:
        lda $031a,x
        sta vectors,x
        dex
        bpl save_vectors
        jsr $ff87
        jsr $ff8a
        ldx #25
restore_vectors:
        lda vectors,x
        sta $031a,x
        dex
        bpl restore_vectors
        jsr $e453
        jsr $e3bf
        jsr $a644
        lda #$31
        sta $0314
        lda #$ea
        sta $0315
        lda #1
        sta $cc
        lda #0
        sta $c6
        sta $9d
        sta $02
        lda #$47
        sta $0318
        lda #$fe
        sta $0319
        lda #$25
        sta $dc04
        lda #$40
        sta $dc05
        lda #$11
        sta $dc0e
        lda #$1b
        sta $d011
        lda #$81
        sta $dc0d
        cli
        lda #3
        ldx #<partition
        ldy #>partition
        jsr command
        bcs failed
        lda $070a
        beq :+
        lda #$21                ; failure probe asks for nonexistent !OOTER
        sta booter
:
        lda #6
        ldx #<directory
        ldy #>directory
        jsr command
        bcs failed
        lda #6
        ldx #<booter
        ldy #>booter
        jsr $ffbd
        lda #0
        ldx #12
        ldy #1
        jsr $ffba
        lda #0
        jsr $ffd5
        bcs failed
        lda $0805
        cmp #$9e
        bne failed
        jmp $080d
failed:
        sei
        sta $070b
        lda #32
        sta $0701
        jmp $0403
command:
        jsr $ffbd
        lda #15
        ldx #12
        ldy #15
        jsr $ffba
        jsr $ffc0
        php
        pha
        lda #15
        jsr $ffc3
        jsr $ffcc
        pla
        plp
        rts
partition: .byte "CP1"
directory: .byte "CD//OS"
booter: .byte "BOOTER"
vectors: .res 26,0
