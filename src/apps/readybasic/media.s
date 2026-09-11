; rbm.media: bounded resource and certified PSID loader, lifecycle commands.
; Slot code is disposable. Persistent music state belongs to $9000-$91ff.
.import __DRIVER_LOAD__, __DRIVER_SIZE__, driver_start
.export mustune, musplay, mushalt, musdrop, rscfile
.segment "CODE"
.macpack longbranch
state=$c1ff
num=$c210
str_len=$c250
str_buf=$c260
rf=$c300
hdr=$c500
ptr=$fb

mustune:
        lda #1
        sta kind
        jmp loadfile
rscfile:
        lda #0
        sta kind
loadfile:
        lda state
        beq :+
        jmp badstate             ; Drop old resource before any replacement.
:       lda str_len
        bne :+
        jmp badformat
:       cmp #17
        bcc :+
        jmp badformat
:       tax
@name:  dex
        lda str_buf,x
        cmp #','                ; Never let a filename select DOS write mode.
        jeq badformat
        cmp #'@'
        jeq badformat
        cpx #0
        bne @name
        ldx $98                 ; Do not steal/close a BASIC-owned logical file.
@lfn:   dex
        bmi @available
        lda $0259,x
        cmp #14
        jeq badstate
        jmp @lfn
@available:
        lda str_len
        ldx #<str_buf
        ldy #>str_buf
        jsr $ffbd
        lda #14
        ldx #8
        ldy #2
        jsr $ffba
        lda #0
        sta eof
        jsr $ffc0
        bcs iofail
        ldx #14
        jsr $ffc6
        bcs iofail
        lda kind
        beq rawheader
        lda #124
        bne readheader
rawheader:
        lda #8
readheader:
        sta count
        ldx #0
:       stx index
        jsr readbyte
        bcs iofail
        ldx index
        sta hdr,x
        inx
        cpx count
        bne :-
        lda kind
        bne :+
        jmp resource
:       jsr validate_psid
        bcc :+
        jmp formatfail
:       lda hdr+9
        sta ptr
        lda hdr+8
        sta ptr+1
        ora ptr
        bne haveaddr
        jsr readbyte
        bcs iofail
        sta ptr
        jsr readbyte
        bcs iofail
        sta ptr+1
haveaddr:
        lda ptr+1
        cmp #$92
        bcc formatfail
        cmp #$a0
        bcs formatfail
        lda $38
        cmp #$90
        bcc :+
        bne formatfail
        lda $37
        bne formatfail
:       lda ptr
        sta start
        lda ptr+1
        sta start+1
        jmp stream
iofail: lda #21
        bne fail
formatfail:
        lda #22
fail:   pha
        jsr closefile
        pla
        jmp error

resource:
        ldx #3
:       lda hdr,x
        cmp rawmagic,x
        bne formatfail
        dex
        bpl :-
        lda hdr+4
        sta ptr
        lda hdr+5
        sta ptr+1
        lda hdr+6
        ora hdr+7
        beq formatfail
        lda ptr
        cmp $37
        lda ptr+1
        sbc $38
        bcc formatfail
        clc
        lda ptr
        adc hdr+6
        sta endaddr
        lda ptr+1
        adc hdr+7
        sta endaddr+1
        bcs formatfail
        cmp #$a0
        bcc stream
        bne formatfail
        lda endaddr
        bne formatfail
stream:
        lda ptr+1
        cmp #$a0
        bcs formatfail
        jsr readbyte
        bcs iofail
        ldy #0
        sta (ptr),y
        inc ptr
        bne :+
        inc ptr+1
:       lda kind
        bne sidstream
        lda ptr
        cmp endaddr
        bne rawmore
        lda ptr+1
        cmp endaddr+1
        bne rawmore
        lda eof
        beq formatfail           ; Reject trailing data, not just truncation.
        jsr closefile
        jmp success
rawmore:
        lda eof
        jne formatfail
        jmp stream
sidstream:
        lda eof
        beq stream
        ; Init and play must both land in the actual, fully loaded payload.
        ldx #10
        jsr entrycheck
        jcs formatfail
        ldx #12
        jsr entrycheck
        jcs formatfail
        jsr closefile
        ; Driver fits one page (asserted below); no live code in overlay slots.
        ldx #0
:       lda __DRIVER_LOAD__,x
        sta $9000,x
        inx
        cpx #<__DRIVER_SIZE__
        bne :-
        lda hdr+11
        sta $9008
        lda hdr+10
        sta $9009
        lda hdr+13
        sta $900a
        lda hdr+12
        sta $900b
        lda hdr+15
        sta $900c
        lda #1
        sta state
        jmp success

entrycheck:
        lda hdr+1,x
        cmp start
        lda hdr,x
        sbc start+1
        bcc entrybad
        lda hdr+1,x
        cmp ptr
        lda hdr,x
        sbc ptr+1
        rts                     ; C=1 if entry >= exclusive end.
entrybad:
        sec
        rts

validate_psid:
        ldx #3
:       lda hdr,x
        cmp sidmagic,x
        bne invalid
        dex
        bpl :-
        lda hdr+4
        ora hdr+6
        ora hdr+14
        ora hdr+16
        ora hdr+18
        ora hdr+19
        ora hdr+20
        ora hdr+21
        ora hdr+118
        ora hdr+122
        ora hdr+123
        bne invalid
        lda hdr+5
        cmp #2
        bne invalid
        lda hdr+7
        cmp #124
        bne invalid
        lda hdr+15
        beq invalid
        lda hdr+17
        beq invalid
        cmp hdr+15
        beq :+
        bcs invalid
:       lda hdr+119
        and #$cf                ; MUS, PlaySID-specific, reserved; PAL only.
        cmp #4
        bne invalid
        lda $02a6
        cmp #1
        bne invalid
        clc
        rts
invalid:sec
        rts

musplay:
        lda state
        beq badstate
        lda num+1
        bne badformat
        lda num
        beq badformat
        cmp $900c
        beq :+
        bcs badformat
:       jsr $9003
        lda $d01a
        and #$0f                ; Unimplemented upper VIC mask bits read as 1.
        bne badstate             ; No stealing an existing VIC IRQ owner.
        lda num
        sec
        sbc #1
        jsr driver_start
        jmp success
mushalt:
        lda state
        beq success
        jsr $9003
        jmp success
musdrop:
        jsr mushalt
        lda #0
        sta state
success:
        lda #0
        sta rf
        sta rf+2
        rts
badstate:
        lda #23
        bne error
badformat:
        lda #22
error:  sta rf
        sta rf+1
        rts

; ReadST=$40 marks a valid final byte. A later read is a short-read error.
readbyte:
        lda eof
        bne readbad
        jsr $ffcf
        sta saved
        jsr $ffb7
        beq readok
        cmp #$40
        bne readbad
        lda #1
        sta eof
readok: lda saved
        clc
        rts
readbad:sec
        rts
closefile:
        jsr $ffcc
        lda #14
        jmp $ffc3
sidmagic:.byte "PSID"
rawmagic:.byte "RBR1"
kind:   .byte 0
eof:    .byte 0
saved:  .byte 0
count:  .byte 0
index:  .byte 0
start:  .word 0
endaddr:.word 0
.assert __DRIVER_SIZE__ < 256, lderror, "media driver copy must fit one page"
