; Bank-D resource loaders and palette-preserving multicolor lines in rbm.media.
; No resident/parser changes. Slot-private state survives only within one call.
.export koaload, sprfile, mcline
.segment "CODE"
.macpack longbranch
cf=$c210
rf=$c300
buf=$c260
hdr=$c500
ptr=$fb

koaload:
        lda #0
        beq open_resource
sprfile:
        lda #1
open_resource:
        sta kind
        lda $c1ff
        jne state_error           ; Load all resources before installing music.
        lda $c250
        jeq format_error
        cmp #17
        jcs format_error
        tax
@name:  dex
        lda buf,x
        cmp #','
        jeq format_error
        cmp #'@'
        jeq format_error
        cpx #0
        bne @name
        ldx $98
@lfn:   dex
        bmi @available
        lda $0259,x
        cmp #14
        jeq state_error
        jmp @lfn
@available:
        ; Streamed KERNAL IEC reads cannot tolerate sprite DMA stealing cycles.
        ; Keep the caller's mask hidden until CLOSE, including all error paths.
        lda $d015
        sta saved_sprites
        lda #0
        sta $d015
        ; A Koala file puts palettes after pixels. Hide the display until all
        ; parts and the exact EOF have been checked, rather than exposing an
        ; unfinished bitmap with the previous palette. ReadyBASIC graphics
        ; owns the VIC mode; raster IRQ effects are outside this loader ABI.
        lda $d011
        and #$7f
        sta saved_display
        lda kind
        bne :+
        lda saved_display
        and #$ef
        sta $d011
:
        lda $c250
        ldx #<buf
        ldy #>buf
        jsr $ffbd
        lda #14
        ldx #8
        ldy #2
        jsr $ffba
        lda #0
        sta eof
        sta packed
        sta packleft
        jsr $ffc0
        jcs io_fail
        ldx #14
        jsr $ffc6
        jcs io_fail
        lda #<hdr
        sta ptr
        lda #>hdr
        sta ptr+1
        lda #2
        ldx kind
        beq :+
        lda #8
:       sta left
        lda #0
        sta left+1
        jsr read_range
        jcs io_fail
        lda kind
        jne sprites
        lda hdr
        beq @koala
        cmp #'R'
        jne format_fail
        lda hdr+1
        cmp #'K'
        jne format_fail
        jsr rawbyte
        jcs io_fail
        cmp #'C'
        jne format_fail
        jsr rawbyte
        jcs io_fail
        cmp #'1'
        jne format_fail
        lda #1
        sta packed
        bne @image
@koala:
        lda hdr+1
        cmp #$60
        jne format_fail
@image:
        lda #0
        sta part
@part: ldx part
        lda destlo,x
        sta ptr
        lda desthi,x
        sta ptr+1
        lda lenlo,x
        sta left
        lda lenhi,x
        sta left+1
        jsr read_range
        jcs io_fail
        inc part
        lda part
        cmp #4
        bne @part
        lda $d021
        and #15
        sta $d021
        jmp finished
sprites:
        ldx #3
@magic: lda hdr,x
        cmp magic,x
        jne format_fail
        dex
        bpl @magic
        ; SPRFILE accepts RBR1 only within the eight owned 64-byte sprite slots.
        lda hdr+5
        cmp #$ca
        jcc format_fail
        cmp #$cc
        jcs format_fail
        sta ptr+1
        lda hdr+4
        and #63
        jne format_fail
        lda hdr+4
        sta ptr
        lda hdr+6
        and #63
        jne format_fail
        lda hdr+6
        ora hdr+7
        jeq format_fail
        clc
        lda ptr
        adc hdr+6
        sta lastlo
        lda ptr+1
        adc hdr+7
        jcs format_fail
        cmp #$cc
        bcc @valid
        jne format_fail
        lda lastlo
        jne format_fail
@valid: lda hdr+6
        sta left
        lda hdr+7
        sta left+1
        jsr read_range
        jcs io_fail
finished:
        lda packleft
        jne format_fail           ; Reject packets beyond the fixed image size.
        lda eof
        jeq format_fail            ; No trailing or truncated data accepted.
        jsr close
        jmp ok
io_fail:
        lda #21
        bne close_error
format_fail:
        lda #22
close_error:
        pha
        jsr close
        pla
        bne error
state_error:
        lda #23
        bne error
format_error:
        lda #22
error:  sta rf
        sta rf+1
        rts
ok:     lda #0
        sta rf
        sta rf+2
        rts
close:  jsr $ffcc
        lda #14
        jsr $ffc3
        lda kind
        bne :+
        lda saved_display
        sta $d011
:
        lda saved_sprites
        sta $d015
        rts
read_range:
        jsr imagebyte
        bcs @bad
        ldy #0
        sta (ptr),y              ; Writes under KERNAL land in bitmap RAM.
        inc ptr
        bne :+
        inc ptr+1
:       lda left
        bne :+
        dec left+1
:       dec left
        lda left
        ora left+1
        bne read_range
        clc
        rts
@bad:   sec
        rts

; RKC1 packets may span bitmap/palette boundaries. Only the bounded caller
; chooses destinations. Physical EOF is checked independently of expansion.
imagebyte:
        lda packed
        beq rawbyte
        lda packleft
        bne @emit
        jsr rawbyte
        bcs @bad
        pha
        and #$80
        sta packrepeat
        pla
        and #$7f
        clc
        adc #1
        sta packleft
        lda packrepeat
        beq @emit
        jsr rawbyte
        bcs @bad
        sta packvalue
@emit: lda packrepeat
        beq @literal
        lda packvalue
        jmp @value               ; Zero is also a valid repeated byte.
@literal:
        jsr rawbyte
        bcs @bad
@value: dec packleft
        clc
        rts
@bad:   sec
        rts
rawbyte:
        lda eof
        bne @bad
        jsr $ffcf
        sta byte
        jsr $ffb7
        beq @store
        cmp #$40
        bne @bad
        lda #1
        sta eof
@store: lda byte
        clc
        rts
@bad:   sec
        rts
magic: .byte "RBR1"
destlo: .byte $00,$00,$00,$21
desthi: .byte $e0,$cc,$d8,$d0
lenlo: .byte $40,$e8,$e8,1
lenhi: .byte $1f,3,3,0
kind: .byte 0
eof: .byte 0
byte: .byte 0
part: .byte 0
left: .word 0
lastlo: .byte 0
saved_sprites: .byte 0
saved_display: .byte 0
packed: .byte 0
packleft: .byte 0
packrepeat: .byte 0
packvalue: .byte 0

; MCLINE(x1,y1,x2,y2,slot): bounded all-octant integer line. Slot is the
; two-bit pixel value 0..3, NOT a palette color. Never changes CC00/D800/D021.
; Both endpoints are inclusive; invalid coordinates fail before any write.
mcline:
        lda $c4f4
        cmp #2
        jne format_error
        lda cf+1
        ora cf+3
        ora cf+5
        ora cf+7
        ora cf+9
        jne format_error
        lda cf
        cmp #160
        jcs format_error
        sta px
        lda cf+4
        cmp #160
        jcs format_error
        lda cf+2
        cmp #200
        jcs format_error
        sta py
        lda cf+6
        cmp #200
        jcs format_error
        lda cf+8
        cmp #4
        jcs format_error
        asl
        asl
        sta ink
        lda #1
        sta sx
        sta sy
        lda cf+4
        sec
        sbc px
        bcs :+
        eor #$ff
        clc
        adc #1
        dec sx
        dec sx
:       sta dx
        lda cf+6
        sec
        sbc py
        bcs :+
        eor #$ff
        clc
        adc #1
        dec sy
        dec sy
:       sta dy
        cmp dx
        bcs :+
        lda dx
:       sta major
        sta steps
        lsr
        sta ax
        sta ay
        ; The dispatcher enters with SEI and $01=$36. During a live media
        ; lifetime, deliberately admit the certified IRQ between pixels:
        ; KERNAL stays visible, and the driver preserves our FB-FE scratch.
        ; Restore the dispatcher's original flags before returning.
        php
        lda $c1ff
        cmp #2
        bne @loop
        cli
@loop: jsr pixel
        lda steps
        beq @done
        dec steps
        clc
        lda ax
        adc dx
        bcs @xstep
        cmp major
        bcc @xsave
@xstep: sec
        sbc major
        pha
        clc
        lda px
        adc sx
        sta px
        pla
@xsave: sta ax
        clc
        lda ay
        adc dy
        bcs @ystep
        cmp major
        bcc @ysave
@ystep: sec
        sbc major
        pha
        clc
        lda py
        adc sy
        sta py
        pla
@ysave: sta ay
        jmp @loop
@done: plp
        jmp ok
pixel:
        lda py
        lsr
        lsr
        lsr
        tax
        lda rowlo,x
        sta ptr
        lda rowhi,x
        sta ptr+1
        lda px
        and #$fc
        asl
        bcc :+
        inc ptr+1
:       clc
        adc ptr
        sta ptr
        bcc :+
        inc ptr+1
:       lda py
        and #7
        tay
        lda px
        and #3
        tax
        ; Protect only this pixel's banked read/modify/write, not the line.
        ; Music and CIA IRQs can run between pixels even at 1 MHz. The media
        ; driver preserves FB-FE, and KERNAL is visible again before PLP.
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
        lda (ptr),y
        and masks,x
        sta byte
        txa
        ora ink
        tax
        lda pairs,x
        ora byte
        sta (ptr),y
        pla
        sta $01
        plp
        rts
rowlo:
.repeat 25,I
        .byte <($e000+I*320)
.endrepeat
rowhi:
.repeat 25,I
        .byte >($e000+I*320)
.endrepeat
masks: .byte $3f,$cf,$f3,$fc
pairs: .byte 0,0,0,0,$40,$10,4,1,$80,$20,8,2,$c0,$30,12,3
px: .byte 0
py: .byte 0
dx: .byte 0
dy: .byte 0
sx: .byte 0
sy: .byte 0
major: .byte 0
steps: .byte 0
ax: .byte 0
ay: .byte 0
ink: .byte 0
