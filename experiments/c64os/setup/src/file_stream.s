; Native C64 OS transport for codec.s. File references remain caller-owned.
; Byte reads retain exact EOF semantics; output is buffered in one RAM page.
        .setcpu "6502"
        .macpack longbranch
        .import os_fopen, os_fread, os_fwrite, os_fclose, os_ferror
        .import os_killmouse, os_initmouse
        .import transient_page
        .export stream_open, stream_close
        .export stream_eof
        .export stream_read, stream_write, stream_error, stream_ref

STATUS = $90
BACKREGS = $03BC
TRY = $03CA
EXCINDEX = $C7

        .segment "BSS"
stream_ref: .res 2
stream_error: .res 1
stream_eof: .res 1
stream_mode: .res 1
stream_device: .res 1
stream_mouse: .res 1
stream_opened: .res 1
stream_value: .res 1
stream_map: .res 1
stream_stack: .res 1
stream_count: .res 1
stream_buffer = transient_page

        .segment "CODE"
; X/Y = page-aligned reference, A = native fopen flags.
stream_open:
        stx stream_ref
        sty stream_ref+1
        sta stream_mode
        lda #0
        sta stream_error
        sta stream_eof
        sta stream_opened
        sta stream_mouse
        sta stream_count
        lda stream_mode
        jsr os_fopen
        bcc @opened
        sta stream_error
        jsr os_fclose
        sec
        rts
@opened:
        lda $BA
        sta stream_device
        inc stream_opened
        jsr os_killmouse
        lda #0
        rol a
        sta stream_mouse
        ; ROL of zero already clears carry, independent of mouse state.
        rts

; All callbacks preserve X/Y and the editor's $F7..$FF pointers.
stream_read:
        txa
        pha
        tya
        pha
        lda stream_error
        ora stream_eof
        jne stream_fail
        lda $01
        sta stream_map
        tsx
        stx stream_stack
        jsr BACKREGS
        lda #<stream_exception
        ldx #>stream_exception
        jsr TRY
        ldx stream_ref
        ldy stream_ref+1
        jsr os_fread
        .word stream_value, 1
        dec EXCINDEX
        lda STATUS
        and #$BF              ; EOI accompanies the last valid byte.
        bne stream_io_error
        lda STATUS
        and #$40
        sta stream_eof
        jmp stream_ok

stream_write:
        sta stream_value
        txa
        pha
        tya
        pha
        lda stream_error
        jne stream_fail
        ldx stream_count
        lda stream_value
        sta stream_buffer,x
        inc stream_count
        bne stream_ok
        lda #1
        sta write_length+1
        lda #0
        sta write_length
        jsr stream_flush
        bcs stream_fail
stream_ok:
        pla
        tay
        pla
        tax
        lda stream_value
        clc
        rts

stream_io_error:
        sta stream_error
stream_fail:
        pla
        tay
        pla
        tax
        sec
        rts
stream_exception:
        ; Restore our exact callback frame, including its saved X/Y. The
        ; installed exception module's unwind point retains extra stack bytes.
        ldx stream_stack
        txs
        lda stream_map        ; fread's normal mapping epilogue was skipped.
        sta $01
        lda #$80
        bne stream_io_error

stream_flush:
        lda $01
        sta stream_map
        tsx
        stx stream_stack
        jsr BACKREGS
        lda #<stream_write_exception
        ldx #>stream_write_exception
        jsr TRY
        ldx stream_ref
        ldy stream_ref+1
        jsr os_fwrite
        .word stream_buffer
write_length:
        .word 0
        dec EXCINDEX
        lda STATUS
        bne @error
        lda #0
        sta stream_count
        clc
        rts
@error:
        sta stream_error
        sec
        rts
stream_write_exception:
        ldx stream_stack
        txs
        lda stream_map
        sta $01
        lda #$80
        sta stream_error
        sec
        rts

stream_close:
        lda stream_opened
        beq @result
        lda stream_error
        bne @close
        lda stream_mode
        and #4
        beq @close
        lda stream_count
        beq @close
        sta write_length
        lda #0
        sta write_length+1
        jsr stream_flush
@close:
        ldx stream_ref
        ldy stream_ref+1
        jsr os_fclose
        ; Closing a SEQ commits its final sector. Check the drive afterward,
        ; not merely the transport status from the last byte written.
        lda $01
        pha
        and #$FE              ; BASIC must stay out: OS file.o lives at $Bxxx.
        ora #2
        sta $01
        lda stream_device
        sta $BA
        jsr os_ferror
        cmp #20
        bcc @restore
        ldx stream_error
        bne @restore
        sta stream_error
@restore:
        pla
        sta $01
        lda #0
        sta stream_opened
        lda stream_mouse
        beq @result
        sec
        jsr os_initmouse
@result:
        lda stream_error
        cmp #1                ; Clear iff no error, including clean EOF.
        rts
