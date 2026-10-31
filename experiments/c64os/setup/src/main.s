; Bridge Setup: native C64OS application, Toolkit core controls only.
.setcpu "6502"
.macpack longbranch
.include "toolkit.inc"
.import init_api, getsfref, timeque, layerpush, layerpop, markredraw, pgalloc, pgfree
.import stream_open, stream_close, stream_read, stream_write, stream_eof
.export appinit, msgcmd, appquit, appfreeze, appthaw, transient_page
.export read_file, validate_library, crc32, selected_banks, installed_banks, allowed
.export image, before, fileref, operation, do_work, status_index
.segment "VECTORS"
.word appinit,msgcmd,appquit,appfreeze,appthaw
.segment "DATA"
context: .word $0400,$d800
.byte 40,40,25
.word 0,0
env: .word context
.byte 0,1,0
root: .word 0
.word 0,0,0
.byte 0,0
layer: .word draw,mouse,kcmd,kprnt
layer_index: .byte 0
selected_banks: .byte 32
installed_banks: .byte 0
allowed: .byte 0
operation: .byte 1
status_index: .byte 0
pool: .byte 0
pending: .byte 0
work_timer: .byte 1,0,0,$40
.word do_work
.byte 1,0,0
.segment "CODE"
appinit:
 ldx #<api
 ldy #>api
 jsr $02fc
 jsr init_api
 ldx #<layer
 ldy #>layer
 jsr layerpush
 lda #0
 sta env+4
 lda #$ff
 ldx #4
 jsr pgalloc
 jcs init_failed
 sty env+2
 sty pool
 jsr setenv
 ldx #1
 jsr classptr
 jsr tknew
 stx root
 sty root+1
 tkcall TK_INIT
 tkprop8 TK_OFFTOP,1
 tkprop8 TK_OFFBOT,1
 ldy #TK_DFLAGS
 lda (TK_THIS),y
 ora #4
 sta (TK_THIS),y
 ldx #0
build_controls:
 stx control_index
 lda kinds,x
 tax
 jsr classptr
 jsr tknew
 ldx control_index
 lda TK_THIS
 sta controls_lo,x
 lda TK_THIS+1
 sta controls_hi,x
 tkcall TK_INIT
 ldx control_index
 lda rows,x
 ldy #TK_OFFTOP
 sta (TK_THIS),y
 lda columns,x
 ldy #TK_OFFLEFT
 sta (TK_THIS),y
 lda widths,x
 ldy #TK_WIDTH
 sta (TK_THIS),y
 lda #5
 ldy #TK_RSMASK
 sta (TK_THIS),y
 lda kinds,x
 cmp #7
 bne label_control
 txa
 ldy #10
 sta (TK_THIS),y
 ldy #61
 jsr getmethod
 lda #0
 ldx #<button_action
 ldy #>button_action
 jsr TK_JUMP
 ldy #79
 bne title_control
label_control:
 ldy #61
title_control:
 jsr getmethod
 ldx control_index
 jsr assign_text
 ldx root
 ldy root+1
 jsr appendto
 ldx control_index
 inx
 cpx #CONTROL_COUNT
 jne build_controls
 ; Native cycle selector: supports forward/reverse cycling and focus.
 ldx controls_lo+3
 ldy controls_hi+3
 jsr ptrthis
 tkprop8 54,2
 tkprop8 60,8
 tkprop8 61,32
 tkprop8 49,32
 tkcall TK_SETFIRST
 jsr update_labels
 ldx #<work_timer
 ldy #>work_timer
 jsr timeque
 jmp redraw
init_failed:
 jmp appquit
appquit:
 lda #0
 sta work_timer+3
 ldy pool
 beq :+
 ldx #4
 jsr pgfree
: ldx env+4
 jsr layerpop
 clc
 rts
appfreeze:
 lda operation
 beq :+
 sec
 rts
: clc
 rts
appthaw:
 ; Fast App Switching may resume an old Setup instance; re-read disk state.
 lda #1
 sta operation
 jmp redraw
msgcmd:
 cmp #1
 bne :+
 lda #0
 rts
: cmp #0
 bne :+
 txa
 and #$7f
 cmp #'Q'
 bne :+
 jmp quitapp
: clc
 rts
setenv:
 ldx #<env
 ldy #>env
 jmp settkenv
redraw:
 jsr setenv
 ldx root
 ldy root+1
 jsr ptrthis
 ldy #TK_DFLAGS
 lda (TK_THIS),y
 ora #1
 sta (TK_THIS),y
 lda #1
 sta env+3
 ldx env+4
 jmp markredraw
draw:
 lda env+3
 and #1
 bne @update
 jsr redraw_tree
 lda #1
 sta env+3
@update:
 ldx #<env
 ldy #>env
 jsr tkupdate
 clc
 rts
redraw_tree:
 jsr setenv
 ldx root
 ldy root+1
 jsr ptrthis
 ldy #TK_DFLAGS
 lda (TK_THIS),y
 ora #1
 sta (TK_THIS),y
 ldy #TK_MFLAGS
 lda (TK_THIS),y
 ora #2
 sta (TK_THIS),y
 rts
mouse:
 ldx #<env
 ldy #>env
 jsr tkmouse
 jsr event_done
 clc
 rts
kcmd:
 ldx #<env
 ldy #>env
 jsr tkkcmd
 jmp event_done
kprnt:
 ldx #<env
 ldy #>env
 jsr tkkprnt
event_done:
 php
 lda env+3
 and #1
 beq :+
 ldx env+4
 jsr markredraw
: plp
 rts
button_action:
 lda TK_THIS
 pha
 lda TK_THIS+1
 pha
 jsr handle_button
 pla
 sta TK_THIS+1
 pla
 sta TK_THIS
 rts
handle_button:
 lda operation
 jne @done
 ldy #10
 lda (TK_THIS),y
 cmp #3
 beq @cycle
 cmp #4
 beq @minus
 cmp #5
 beq @plus
 cmp #6
 beq @apply
 cmp #7
 beq @undo
 cmp #8
 beq @confirm
 cmp #9
 beq @cancel
 rts
@cycle:
 ldy #49
 lda (TK_THIS),y
 sta selected_banks
 jmp @change
@minus:
 lda selected_banks
 cmp #8
 beq @done
 dec selected_banks
 jmp @change
@plus:
 lda selected_banks
 cmp #32
 beq @done
 inc selected_banks
@change:
 lda #0
 sta pending
 jsr update_labels
 jmp redraw
@apply:
 lda #2
 bne @arm
@undo:
 lda #3
@arm:
 ldx allowed
 beq @done
 sta pending
 clc
 adc #2
 sta status_index
 jsr update_labels
 jmp redraw
@confirm:
 lda pending
 beq @done
 sta operation
 lda #0
 sta pending
 rts
@cancel:
 lda #0
 sta pending
 sta status_index
 jsr update_labels
 jmp redraw
@done: rts

; Defer all disk work until Toolkit event dispatch has returned.
do_work:
 lda operation
 beq @done
 cmp #1
 beq @inspect
 jsr change_library
 jmp @finish
@inspect:
 jsr inspect
 bcs @finish
 lda installed_banks
 beq @finish
 sta selected_banks
 jsr setenv
 ldx controls_lo+3
 ldy controls_hi+3
 jsr ptrthis
 ldy #49
 lda selected_banks
 sta (TK_THIS),y
@finish:
 lda #0
 sta operation
 jsr update_labels
 jmp redraw
@done: rts

; Build a private reference from the running OS system root. No device/path
; constants are assumed; getsfref supplies the active boot volume and root.
make_ref:
 sta ref_kind
 ; getsfref IN: Y=destination page (zero requests allocation).
 ; OUT: X=that page, Y=end-of-path. Never pass an inherited Y register.
 ldy #>fileref
 jsr getsfref
 stx $fc
 lda #0
 sta $fb
 sty root_end
 ldy #0
@copy:
 lda ($fb),y
 sta fileref,y
 iny
 bne @copy
 ldx ref_kind
 lda paths_lo,x
 sta $fb
 lda paths_hi,x
 sta $fc
 ldx #0
 ldy root_end
@path:
 stx ref_index
 ldy ref_index
 lda ($fb),y
 ldy root_end
 sta fileref,y
 inc root_end
 inx
 cmp #0
 bne @path
 ldx ref_kind
 lda names_lo,x
 sta $fb
 lda names_hi,x
 sta $fc
 ldy #0
@name:
 lda ($fb),y
 sta fileref+5,y
 iny
 cmp #0
 bne @name
 rts

; Bounded file read with exact EOF. The image buffer is 256 bytes; oversized
; or truncated files cannot pass the version/fingerprint checks.
read_file:
 lda #$41
 ldx #0
 ldy #>fileref
 jsr stream_open
 bcs @fail
 lda #0
 sta file_size
@loop:
 jsr stream_read
 bcs @end
 ldx file_size
 sta image,x
 inc file_size
 beq @overflow
 lda stream_eof
 beq @loop
 jsr stream_close
 rts
@end:
 jsr stream_close
@fail:
 sec
 rts
@overflow:
 jsr stream_close
 sec
 rts

inspect:
 lda #0
 sta allowed
 sta status_index
 ; SETTINGS/version.t is SEQ, library is PRG; read mode overridden below.
 lda #0
 jsr make_ref
 lda #$81
 sta read_file+1
 jsr read_file
 jcs version_error
 lda file_size
 cmp #5
 jne version_error
 ldx #4
@version:
 lda image,x
 cmp version_text,x
 jne version_error
 dex
 bpl @version
 lda #1
 jsr make_ref
 lda #$41
 sta read_file+1
 jsr read_file
 jcs library_error
 jsr validate_library
 jcs library_error
 lda #1
 sta allowed
 clc
 rts
version_error:
 lda #1
 bne inspect_error
library_error:
 lda #2
inspect_error:
 sta status_index
 sec
 rts

; Accept only the exact original or our six-byte patch. Normalize a copy,
; then compare CRC32 of all 217 bytes against the known 1.09 image.
validate_library:
 lda file_size
 cmp #217
 bne @bad
 lda #0
 sta installed_banks
 lda image+$6f
 cmp #$20
 bne @patched
 ldx #5
@original:
 lda image+$6f,x
 cmp original_patch,x
 bne @bad
 dex
 bpl @original
 jmp @hash
@patched:
 cmp #$a9
 bne @bad
 lda image+$70
 cmp #8
 bcc @bad
 cmp #33
 bcs @bad
 sta installed_banks
 ldx #3
@tail:
 lda image+$71,x
 cmp patch_tail,x
 bne @bad
 dex
 bpl @tail
@hash:
 ldx #0
@copy_all:
 lda image,x
 sta before,x
 inx
 cpx #217
 bne @copy_all
 ldx #5
@normalize:
 lda original_patch,x
 sta image+$6f,x
 dex
 bpl @normalize
 jsr crc32
 ldx #3
@compare:
 lda crc,x
 cmp expected_crc,x
 bne @bad
 dex
 bpl @compare
 clc
 rts
@bad:
 sec
 rts
crc32:
 lda #$ff
 sta crc
 sta crc+1
 sta crc+2
 sta crc+3
 ldx #0
@byte:
 lda image,x
 eor crc
 sta crc
 ldy #8
@bit:
 lsr crc+3
 ror crc+2
 ror crc+1
 ror crc
 bcc @next
 lda crc
 eor #$20
 sta crc
 lda crc+1
 eor #$83
 sta crc+1
 lda crc+2
 eor #$b8
 sta crc+2
 lda crc+3
 eor #$ed
 sta crc+3
@next:
 dey
 bne @bit
 inx
 cpx #217
 bne @byte
 ldx #3
@invert:
 lda crc,x
 eor #$ff
 sta crc,x
 dex
 bpl @invert
 rts

change_library:
 ; Re-read version and exact bytes at commit time, not just at app startup.
 jsr inspect
 bcs @done
 ; image is now normalized original. Keep a full undo image on disk first.
 lda #2
 jsr make_ref
 jsr write_image
 bcs @io_error
 jsr verify_written
 bcs @io_error
 lda operation
 cmp #3
 beq @save
 lda #$a9
 sta image+$6f
 lda selected_banks
 sta image+$70
 ldx #3
@patch:
 lda patch_tail,x
 sta image+$71,x
 dex
 bpl @patch
@save:
 lda #1
 jsr make_ref
 jsr write_image
 bcs @rollback
 jsr verify_written
 bcs @rollback
 lda selected_banks
 ldx operation
 cpx #3
 bne :+
 lda #0
: sta installed_banks
 lda #6
 sta status_index
 lda #0
 sta allowed
 rts
@rollback:
 ; Roll back the exact pre-change image if a normal write/readback fails.
 ldx #0
@restore:
 lda before,x
 sta image,x
 inx
 cpx #217
 bne @restore
 jsr write_image
 bcs @fatal
 jsr verify_written
 bcs @fatal
@io_error:
 lda #3
 sta status_index
@done: rts
@fatal:
 lda #7
 sta status_index
 lda #0
 sta allowed
 rts
write_image:
 lda #$54
 ldx #0
 ldy #>fileref
 jsr stream_open
 bcs @fail
 lda #0
 sta io_index
@loop:
 ldx io_index
 lda image,x
 jsr stream_write
 bcs @close_fail
 inc io_index
 lda io_index
 cmp #217
 bne @loop
 jmp stream_close
@close_fail:
 jsr stream_close
@fail:
 sec
 rts
verify_written:
 ldx #0
@keep:
 lda image,x
 sta desired,x
 inx
 cpx #217
 bne @keep
 jsr read_file
 bcs @bad
 lda file_size
 cmp #217
 bne @bad
 ldx #0
@cmp:
 lda image,x
 cmp desired,x
 bne @bad
 inx
 cpx #217
 bne @cmp
 clc
 rts
@bad:
 sec
 rts

update_labels:
 lda #<original_status
 sta texts_lo+5
 lda #>original_status
 sta texts_hi+5
 lda installed_banks
 beq @selected
 sec
 sbc #8
 asl
 tax
 lda current_sizes,x
 sta texts_lo+5
 lda current_sizes+1,x
 sta texts_hi+5
@selected:
 lda selected_banks
 sec
 sbc #8
 asl
 tax
 lda sizes,x
 sta texts_lo+3
 lda sizes+1,x
 sta texts_hi+3
 ldx status_index
 lda statuses_lo,x
 sta texts_lo+10
 lda statuses_hi,x
 sta texts_hi+10
 ldx #3
@next:
 stx control_index
 lda controls_hi,x
 tay
 lda controls_lo,x
 tax
 jsr ptrthis
 ldx control_index
 ldy #61
 cpx #3
 bne :+
 ldy #79
: jsr getmethod
 ldx control_index
 jsr assign_text
 ldx control_index
 cpx #10
 beq @done
 cpx #3
 bne :+
 ldx #5
 bne @next
: ldx #10
 bne @next
@done: rts

assign_text:
 lda kinds,x
 cmp #7
 beq @button
 lda texts_hi,x
 tay
 lda texts_lo,x
 pha
 tya
 tax
 pla
 jmp TK_JUMP
@button:
 lda texts_hi,x
 tay
 lda texts_lo,x
 tax
 jmp TK_JUMP

.macro ap name,module,offset
name: .byte module,offset,0
.endmacro
api:
 ap quitapp,$f2,$21
 ap settkenv,$ee,$0f
 ap tkupdate,$ee,$12
 ap tkmouse,$ee,$15
 ap tkkcmd,$ee,$18
 ap tkkprnt,$ee,$1b
 ap classptr,$ee,$21
 ap tknew,$ee,$24
 ap ptrthis,$ee,$2a
 ap getmethod,$ee,$33
 ap appendto,$ee,$45
 .byte $ff
.segment "RODATA"
version_text: .byte "1.09",13
patch_tail: .byte $8d,$81,$02,$60
.include "fingerprint.inc"
paths_lo: .lobytes settings_path,library_path,library_path
paths_hi: .hibytes settings_path,library_path,library_path
names_lo: .lobytes version_name,library_name,backup_name
names_hi: .hibytes version_name,library_name,backup_name
settings_path: .byte "SETTINGS/",0
library_path: .byte "LIBRARY/",0
version_name: .byte "VERSION.T",0
library_name: .byte "REC.LIB.O",0
backup_name: .byte "BRIDGE.REC",0
.include "ui_data.inc"
.segment "BSS"
controls_lo: .res CONTROL_COUNT
controls_hi: .res CONTROL_COUNT
control_index: .res 1
ref_kind: .res 1
root_end: .res 1
ref_index: .res 1
file_size: .res 1
io_index: .res 1
crc: .res 4
image: .res 256
before: .res 217
desired: .res 217
transient_page: .res 256
.segment "FILEREFS"
fileref: .res 256
