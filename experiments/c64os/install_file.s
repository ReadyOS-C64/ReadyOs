; Installation helper, invoked at native BASIC only in the private environment.
; Host supplies filename at $C200, length $C2F0, payload $2000, byte length C2F1/2.
.segment "CODE"
start:
        lda $c2f0
        ldx #0
        ldy #$c2
        jsr $ffbd
        lda #2
        ldx #12
        ldy #2
        jsr $ffba
        jsr $ffc0
        bcs failed
        ldx #2
        jsr $ffc9
        bcs failed
        lda #0
        sta source+1
        lda #$20
        sta source+2
loop:
        lda $c2f1
        ora $c2f2
        beq done
source: lda $2000
        jsr $ffd2
        jsr $ffb7
        bne failed
        inc source+1
        bne :+
        inc source+2
:       lda $c2f1
        bne :+
        dec $c2f2
:       dec $c2f1
        jmp loop
done:   lda #1
        bne close
failed: lda #2
close:  sta $c2f3
        lda #2
        jsr $ffc3
        jsr $ffcc
        rts
        .res $100-(*-start), 0
load:
        lda $c2f0
        ldx #0
        ldy #$c2
        jsr $ffbd
        lda #2
        ldx #12
        ldy #0
        jsr $ffba
        lda #0
        ldx #0
        ldy #$20
        jsr $ffd5
        lda #1
        adc #0
        sta $c2f3
        rts
        .res $140-(*-start), 0
read_seq:
        lda $c2f0
        ldx #0
        ldy #$c2
        jsr $ffbd
        lda #2
        ldx #12
        ldy #2
        jsr $ffba
        jsr $ffc0
        bcc :+
        jmp failed
:       ldx #2
        jsr $ffc6
        bcc :+
        jmp failed
:       lda #0
        sta destination+1
        lda #$20
        sta destination+2
read_loop:
        lda $c2f1
        ora $c2f2
        bne :+
        jmp done
:       jsr $ffcf
destination:
        sta $2000
        jsr $ffb7
        and #$bf                ; EOF may be reported with the final byte
        beq :+
        jmp failed
:       inc destination+1
        bne :+
        inc destination+2
:       lda $c2f1
        bne :+
        dec $c2f2
:       dec $c2f1
        jmp read_loop
