; Native GDOS64 desk accessory. No kernel patch or asynchronous IRQ hook.
.export entry, image_end
BRIDGE_NATIVE_BANK = 66
.segment "CODE"
entry:
        ; Native GD.GEOHELP.DA preserves this same 417-byte caller buffer.
        ; DoDlgBox reuses it; RstrAppl needs the original application state.
        ldx #0
save_caller_page:
        lda $851f,x
        sta caller_state,x
        inx
        bne save_caller_page
        ldx #0
save_caller_tail:
        lda $861f,x
        sta caller_state+256,x
        inx
        cpx #161
        bne save_caller_tail
        lda $c011
        cmp #$88                ; GDOS revision used by 1.03 build260511
        bne reject
        ldx #8
check_os:
        lda $c006,x
        cmp os_name,x
        bne reject
        dex
        bpl check_os
        lda $88c3
        cmp #64
        bne reject
        ldx #3
check_drivers:
        lda $9f8e,x
        beq next_driver
        cmp #3                  ; initial profile: native C1581 only
        bne reject
next_driver:
        dex
        bpl check_drivers
        lda $9fc0               ; no active spooler
        bne reject
        jsr check_timers
        bne reject
        jsr read_contract
        ldx #4
check_contract:
        lda contract,x
        cmp signature,x
        bne reject
        dex
        bpl check_contract
        lda contract+5          ; completed ReadyOS save required
        cmp #1
        bne reject
        lda #<question
        ldx #>question
        jsr dialog
        lda $02                 ; r0L = selected standard dialog icon
        cmp #1
        bne exit
        jsr prepare
        lda #64
        sta _bridge_target
        lda #0
        sta _bridge_proof
        sta _bridge_failprobe
        jsr _bridge_enter
        ; Restored drive bookkeeping was captured after PurgeTurbo.
        ; RstrAppl restores the displaced application and original DA caller.
exit:
        ldx #0
restore_caller_page:
        lda caller_state,x
        sta $851f,x
        inx
        bne restore_caller_page
        ldx #0
restore_caller_tail:
        lda caller_state+256,x
        sta $861f,x
        inx
        cpx #161
        bne restore_caller_tail
        jmp $c23e
reject:
        lda #<unsupported
        ldx #>unsupported
        jsr dialog
        jmp exit

dialog: sta $02
        stx $03
        jmp $c256
prepare:
        ; The native accessory loader already supplies a foreground boundary.
        ; Do NOT call GEOS_InitSystem here: it changes graphics state that
        ; RstrAppl does not restore (geoWrite redraw becomes corrupted).
        ; Wait for the activating mouse button and physical keys to be released.
release_mouse:
        jsr $fe86               ; UpdateMouse = MOUSE_BASE + 6 (1.03 ABI)
        lda $8505
        bpl release_mouse
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
release_keys:
        lda #0
        sta $dc00
        lda $dc01
        cmp #$ff
        bne release_keys
        lda #$ff
        sta $dc00
        pla
        sta $01
        plp
        lda $8489
        sta saved_drive
        ldx #8
purge_loop:
        stx purge_drive
        lda $9f8e-8,x
        beq purge_next
        txa
        jsr $c2b0               ; SetDevice
        jsr $c235               ; PurgeTurbo: ExitTurbo + clear native flags
purge_next:
        ldx purge_drive
        inx
        cpx #12
        bne purge_loop
        lda saved_drive
        jsr $c2b0
        rts

check_timers:
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
        lda $dc0e
        ora $dc0f
        ora $dd0e
        ora $dd0f
        and #1                  ; phase one rejects running CIA timers
        tax
        pla
        sta $01
        plp
        txa
        rts

read_contract:
        php
        sei
        lda $01
        pha
        lda #$35
        sta $01
        ldx #8
save_regs:
        lda $df02,x
        sta dma_regs,x
        dex
        bpl save_regs
        lda #<contract
        sta $df02
        lda #>contract
        sta $df03
        lda #$40
        sta $df04
        lda #$fd
        sta $df05
        lda #71
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
        lda dma_regs,x
        sta $df02,x
        dex
        bpl restore_regs
        pla
        sta $01
        plp
        rts
.segment "RODATA"
os_name: .byte "GDOS64-V3"
signature: .byte $52,$47,$44,$31,1
question:
        .byte $81,11,16,16
        .word title
        .byte 11,16,32
        .word prompt
        .byte 1,1,72,2,17,72,0
title:  .asciiz "ReadyOS Bridge - experimental"
prompt: .asciiz "Suspend GDOS and return to ReadyOS?"
unsupported:
        .byte $81,11,16,16
        .word warning
        .byte 11,16,32
        .word requires
        .byte 1,17,72,0
warning: .asciiz "No compatible ReadyOS session."
requires: .asciiz "Requires GDOS 1.03 / 4 MB / C1581."
.segment "BSS"
caller_state: .res 417
contract: .res 8
dma_regs: .res 9
saved_drive: .res 1
purge_drive: .res 1
.include "experiments/gdos64/native_context.inc"
.segment "BSS"
image_end:
