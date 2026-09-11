; Always-visible driver, installed in MEMCAP-owned RAM, never a module slot.
; ABI: $9000 tick entry, $9003 halt, $9006/7 tick count, $9008/9 init,
; $900a/b play, $900c song count. Only certified FC/FD scratch players.
.segment "DRIVER"
state = $c1ff
        jmp driver_irq
        jmp driver_halt
ticks:  .word 0
init:   .word 0
play:   .word 0
songs:  .byte 0
oldirq: .word 0
zp:     .res 4,0

driver_irq:
        lda $d019
        and #1
        beq chain
        sta $d019
        lda state
        cmp #2
        bne done
        lda $01
        pha
        lda #$36
        sta $01
        cld
        ldx #3
save:   lda $fb,x
        pha
        lda zp,x
        sta $fb,x
        dex
        bpl save
        jsr callplay
        ldx #0
restore:lda $fb,x
        sta zp,x
        pla
        sta $fb,x
        inx
        cpx #4
        bne restore
        inc ticks
        bne :+
        inc ticks+1
:       pla
        sta $01
done:   jmp $ea81               ; ROM IRQ register restore, NOT jiffy handler.
chain:  jmp (oldirq)            ; CIA keyboard/jiffy service retains its cadence.
callplay:jmp (play)

driver_halt:
        php
        sei
        lda state
        cmp #2
        bne mute
        lda #0
        sta $d01a
        lda #1
        sta $d019
        lda oldirq
        sta $0314
        lda oldirq+1
        sta $0315
        lda #1
        sta state
mute:   lda #0
        ldx #24
:       sta $d400,x
        dex
        bpl :-
        plp
        rts

.export driver_start
driver_start:
        ; Caller owns SEI and validated song index in A (zero based).
        pha
        lda $0314
        sta oldirq
        lda $0315
        sta oldirq+1
        ldx #3
:       lda $fb,x
        pha
        lda #0
        sta $fb,x
        dex
        bpl :-
        tsx
        lda $0105,x
        ldx #0
        ldy #0
        jsr callinit
        ldx #0
:       lda $fb,x
        sta zp,x
        pla
        sta $fb,x
        inx
        cpx #4
        bne :-
        pla
        lda #0
        sta ticks
        sta ticks+1
        sta $d012
        lda $d011
        and #$7f
        sta $d011
        lda #<driver_irq
        sta $0314
        lda #>driver_irq
        sta $0315
        lda #1
        sta $d019
        sta $d01a
        lda #2
        sta state
        rts
callinit:jmp (init)
