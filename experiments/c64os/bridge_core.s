; Common full-RAM transfer trampoline. Both OS images must contain these exact
; bytes at $0400 while suspended. Native resume code replaces the displaced
; screen/input bytes only after execution has left the trampoline.
;
; Initial executable gate: same-context destructive round trip. The incoming
; wrapper has saved P/A/X/Y/$01/$00 on the CPU stack and copied color RAM and
; displaced $0400-$07ff bytes into its own snapshot-resident storage.
.segment "CODE"
current_bank = $0700
target_bank  = $0701
resume_pc    = $0702
saved_sp     = $0704
proof_mode   = $0705
core_start:

        sei
        jsr setup
        lda current_bank
        sta $df06
        lda #$80                ; delayed C64 -> REU, 64 KB
        sta $df01
        lda #$30                ; RAM beneath BASIC, KERNAL and I/O
        sta $01
        sta $ff00               ; trigger transfer (native wrapper saves FF00)
        lda #$35
        sta $01

        lda proof_mode
        beq restore
        ; Deliberately destroy runtime ZP, CPU stack and the full shim. The
        ; fetch below must restore these before any JSR/RTS or C code executes.
        ldx #0
        lda #$a5
damage: sta $c600,x
        sta $c700,x
        sta $c800,x
        sta $c900,x
        sta $0100,x
        inx
        bne damage
        ldx #2
damage_zp:
        sta $00,x
        inx
        bne damage_zp

restore:
        ; No JSR here: the proof intentionally destroyed the stack.
        lda #0
        sta $df02
        sta $df03
        sta $df04
        sta $df05
        sta $df07
        sta $df08
        sta $df09
        sta $df0a
        lda target_bank
        sta $df06
        lda #$81                ; delayed REU -> C64, 64 KB
        sta $df01
        lda #$30
        sta $01
        sta $ff00
        ; CPU resumes here using the destination's copy of this trampoline.
        lda #$35
        sta $01
        ldx saved_sp
        txs
        jmp (resume_pc)

setup: lda #0
        sta $df02
        sta $df03
        sta $df04
        sta $df05
        sta $df07
        sta $df08
        sta $df09
        sta $df0a
        rts
        .assert *-core_start <= $0300, error, "bridge core overlaps context fields"
        .res $0300-(*-core_start), 0
        .byte 32,32             ; current and target RAM image banks
        .word 0                 ; destination native resume routine
        .byte 0,1               ; SP, destructive proof enabled
