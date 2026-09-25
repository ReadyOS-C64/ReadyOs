; C64 OS 1.09 utility. Defer switching until loadutil has closed its files and
; registered the utility. A normal timer callback maps RAM at $E000; direct
; loopbreak callbacks execute with KERNAL ROM visible and cannot live here.
.segment "LOADADDR"
.word $e000
.segment "HEADER"
.word init, message, quit, quit, quit, identity
.segment "CODE"
init:
        lda $0281
        cmp #32                 ; experiment's capped C64 OS environment
        beq :+
        sec
        rts
:
        ; Own a tiny record in ReadyOS's reserved control-bank tail. Only this
        ; experiment's versioned record and a fully committed image permit return.
        php
        sei
        lda $01
        pha
        ; loadutil calls init with $01=$34: RAM at both $E000 and $D000.
        ; Expose I/O explicitly for the REU transaction, keeping utility RAM.
        lda #$35
        sta $01
        ldx #8
save_regs:
        lda $df02,x
        sta init_reu,x
        dex
        bpl save_regs
        lda #<record
        sta $df02
        lda #>record
        sta $df03
        lda #$40
        sta $df04
        lda #$fd
        sta $df05
        lda #39
        sta $df06
        lda #8
        sta $df07
        lda #0
        sta $df08
        sta $df09
        sta $df0a
        lda #$91
        sta $df01
        ldx #8
restore_regs:
        lda init_reu,x
        sta $df02,x
        dex
        bpl restore_regs
        pla
        sta $01
        plp
        ldx #4
check:
        lda record,x
        cmp signature,x
        bne invalid
        dex
        bpl check
        lda record+5
        cmp #1
        bne invalid
        ldx #<externs
        ldy #>externs
        jsr $02fc
        ldx #<timer
        ldy #>timer
        jsr timeque
        rts
invalid:
        sec
        rts
message:
        sec
quit:   rts
handoff:
        lda #32
        sta _bridge_target
        lda #0
        sta _bridge_proof
        sta _bridge_failprobe
        jsr _bridge_enter
        ; Resume in the original timer invocation, then close the invisible
        ; utility through its native lifecycle so it may be reopened normally.
        jsr $ff1b
        rts
externs:
timeque: .byte $ec,0,0
        .byte $ff
.segment "RODATA"
signature: .byte $52,$42,$47,$33,3
identity: .byte $d2,"EADY",$cf,$d3,0
.segment "DATA"
timer:  .byte 1,0,0,0
        .word handoff
        .byte 0,0,0
.segment "BSS"
record: .res 8
init_reu: .res 9
BRIDGE_NATIVE_BANK = 34
.include "experiments/c64os/native_context.inc"
