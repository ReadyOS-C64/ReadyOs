; Both sides share the same boundary: normal foreground callback, no disk I/O
; outstanding, standard text raster, no SID playback. CIA masks are write-only:
; ReadyOS uses KERNAL CIA1 timer A; C64 OS 1.09 uses VIC IRQ and masks both CIAs.
; These are explicit supported-context contracts, not inferred register reads.
.include "experiments/c64os/bridge_abi.inc"
.segment "CODE"
_bridge_enter:
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
        ; Snapshot readable CIA state before stopping timers. ICR is deliberately
        ; excluded: reading it acknowledges pending interrupts, not the mask.
        ldx #15
save_cia:
        cpx #13
        beq skip_icr
        lda $dc00,x
        sta cia1,x
        lda $dd00,x
        sta cia2,x
skip_icr:
        dex
        bpl save_cia
        lda #$7f
        sta $dc0d
        sta $dd0d
        lda $dc0d
        lda $dd0d
        lda #0
        sta $dc0e
        sta $dc0f
        sta $dd0e
        sta $dd0f
        lda $0318
        sta saved_nmi
        lda $0319
        sta saved_nmi+1
        lda $fffa
        sta saved_nmi+2
        lda $fffb
        sta saved_nmi+3
        ldx #$2e
save_vic:
        lda $d000,x
        sta vic,x
        dex
        bpl save_vic
        lda vic+$11
        and #$7f
        sta vic+$11
        lda #0
        sta $d01a
        sta $d011
        lda #$0f
        sta $d019
        ; Silence write-only SID state at this deliberately silent boundary.
        lda #0
        sta $d404
        sta $d40b
        sta $d412
        sta $d418
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
        lda $0800,x
        sta displaced+$400,x
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
        lda #6
        sta $0318
        sta $fffa
        lda #4
        sta $0319
        sta $fffb
        lda #BRIDGE_NATIVE_BANK
        sta BRIDGE_CURRENT
        lda _bridge_target
        sta BRIDGE_TARGET
        lda _bridge_proof
        sta BRIDGE_PROOF
        lda _bridge_failprobe
        sta BRIDGE_FAILPROBE
        lda $de32
        sta BRIDGE_IDE_ROM
        lda #<resume
        sta BRIDGE_RESUME
        lda #>resume
        sta BRIDGE_RESUME+1
        tsx
        stx BRIDGE_SP
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
        lda displaced+$400,x
        sta $0800,x
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
        lda saved_nmi
        sta $0318
        lda saved_nmi+1
        sta $0319
        lda saved_nmi+2
        sta $fffa
        lda saved_nmi+3
        sta $fffb
        ; Restore ports/DDRs and timer latches, without writing TOD or ICR.
        ldx #7
restore_cia:
        lda cia1,x
        sta $dc00,x
        lda cia2,x
        sta $dd00,x
        dex
        bpl restore_cia
.if BRIDGE_NATIVE_BANK = BRIDGE_READY_RAM
        ; A running KERNAL timer has a reload latch different from its counter.
        lda $02a6
        beq ntsc
        lda #$25
        ldx #$40
        bne timer_latch
ntsc:   lda #$95
        ldx #$42
 timer_latch:
        sta $dc04
        stx $dc05
.endif
        lda cia1+14
        ora #$10
        sta $dc0e
        lda cia1+15
        ora #$10
        sta $dc0f
        lda cia2+14
        ora #$10
        sta $dd0e
        lda cia2+15
        ora #$10
        sta $dd0f
        ; D012 is the current raster when read, not the compare latch. C64 OS
        ; returns to standard text mode before invoking utilities: raster IRQ 0.
        ldx #$2e
restore_vic:
        cpx #$11
        beq skip_vic
        cpx #$12
        beq skip_vic
        cpx #$19
        beq skip_vic
        cpx #$1a
        beq skip_vic
        cpx #$1e
        beq skip_vic
        cpx #$1f
        beq skip_vic
        lda vic,x
        sta $d000,x
skip_vic:
        dex
        bpl restore_vic
        lda #0
        sta $d012
        lda #$0f
        sta $d019
        lda vic+$11
        sta $d011
        lda vic+$1a
        sta $d01a
.if BRIDGE_NATIVE_BANK = BRIDGE_READY_RAM
        lda #$81
        sta $dc0d
.endif
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
displaced: .res $500
colors: .res $400
saved_ff00: .res 1
saved_reu: .res 9
saved_nmi: .res 4
vic: .res $2f
cia1: .res 16
cia2: .res 16
_bridge_target: .res 1
_bridge_proof: .res 1
_bridge_failprobe: .res 1
