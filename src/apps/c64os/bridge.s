; ReadyOS-side native wrapper. This first gate exercises the transfer core
; through launcher+shim, not through a standalone app launch.
.export _bridge_selftest
.segment "CODE"
_bridge_selftest:
        php
        sei
        pha
        txa
        pha
        tya
        pha
        lda $01
        pha
        lda $00
        pha
        lda #$35
        sta $01
        lda $ff00
        sta saved_ff00
        ldx #8
save_reu:
        lda $df02,x
        sta saved_reu,x
        dex
        bpl save_reu
        ldx #0
save_pages:
        lda $0400,x
        sta displaced,x
        lda $0500,x
        sta displaced+$100,x
        lda $0600,x
        sta displaced+$200,x
        lda $0700,x
        sta displaced+$300,x
        lda $d800,x
        sta colors,x
        lda $d900,x
        sta colors+$100,x
        lda $da00,x
        sta colors+$200,x
        lda $db00,x
        sta colors+$300,x
        lda core,x
        sta $0400,x
        lda core+$100,x
        sta $0500,x
        lda core+$200,x
        sta $0600,x
        lda core+$300,x
        sta $0700,x
        inx
        bne save_pages
        lda #<resume
        sta $0702
        lda #>resume
        sta $0703
        tsx
        stx $0704
        jmp $0400

resume:
        ldx #0
restore_pages:
        lda displaced,x
        sta $0400,x
        lda displaced+$100,x
        sta $0500,x
        lda displaced+$200,x
        sta $0600,x
        lda displaced+$300,x
        sta $0700,x
        lda colors,x
        sta $d800,x
        lda colors+$100,x
        sta $d900,x
        lda colors+$200,x
        sta $da00,x
        lda colors+$300,x
        sta $db00,x
        inx
        bne restore_pages
        lda saved_ff00
        sta $ff00
        ldx #8
restore_reu:
        lda saved_reu,x
        sta $df02,x
        dex
        bpl restore_reu
        pla
        sta $00
        pla
        sta $01
        pla
        tay
        pla
        tax
        pla
        plp
        rts
.segment "RODATA"
core:   .incbin "obj/c64os_bridge_core.bin"
.segment "BSS"
displaced: .res $400
colors: .res $400
saved_ff00: .res 1
saved_reu: .res 9
