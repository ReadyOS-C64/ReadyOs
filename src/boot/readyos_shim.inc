;-----------------------------------------------------------------------------
; Shared ReadyOS shim image ($C800-$C9FF, 512 bytes)
; This file is the canonical shim byte layout used by both the disk boot path
; and the EasyFlash cartridge flavor. Keep it identical across all variants.
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; Page 1: $C800-$C8FF - Jump table, data, and helper routines
;-----------------------------------------------------------------------------

; $C800-$C817: Jump Table (24 bytes, 8 entries)
jt_load_disk    = $C800     ; Load from disk, run
jt_load_reu     = $C803     ; Fetch from REU, run
jt_run_app      = $C806     ; Just run app
jt_preload      = $C809     ; Preload to REU, return
jt_return       = $C80C     ; Return to launcher
jt_switch       = $C80F     ; Switch to another app
jt_stash_cur    = $C812     ; Helper: stash current app
jt_fetch_bank   = $C815     ; Helper: fetch from bank in A

.byte $4C, $40, $C8         ; $C800: JMP load_disk ($C840)
.byte $4C, $60, $C8         ; $C803: JMP load_reu ($C860)
.byte $4C, $00, $10         ; $C806: JMP $1000 (run app)
.byte $4C, $80, $C8         ; $C809: JMP preload ($C880)
.byte $4C, $00, $C9         ; $C80C: JMP return_to_launcher ($C900)
.byte $4C, $40, $C9         ; $C80F: JMP switch_app ($C940)
.byte $4C, $C0, $C8         ; $C812: JMP stash_current ($C8C0) - placeholder
.byte $4C, $F0, $C8         ; $C815: JMP fetch_bank ($C8F0)

; $C818: JMP log_byte ($C9E0)
.byte $4C, $E0, $C9         ; $C818: JMP log_byte
.byte $00,$00,$00,$00,$00   ; $C81B-$C81F: Padding

; $C820-$C83F: Data Area (32 bytes)
; $C820: target_bank - bank to load/switch to
; $C821: filename_len
; $C822-$C823: unused
; $C824-$C82F: filename (12 bytes)
; $C830: load_end_lo (KERNAL LOAD end addr low byte, saved by preload)
; $C831: load_end_hi (KERNAL LOAD end addr high byte, saved by preload)
; $C832-$C833: unused
; $C834: current_bank - currently running app
; $C835: last_saved
; $C836: reu_bitmap_lo (banks 0-7)
; $C837: reu_bitmap_hi (banks 8-15)
; $C838: reu_bitmap_xhi (banks 16-23)
; $C839: storage_drive - shim-global default storage drive for D8/D9 app dialogs
;        This persists across app switches and is shared by apps that use the
;        common file-dialog default-drive contract.
; $C83A: log_index - debug byte ring head
; $C83B-$C83F: reserved

.byte $00                   ; $C820: target_bank
.byte $08                   ; $C821: filename_len
.byte $00, $00              ; $C822-$C823: unused
.byte "LAUNCHER    "        ; $C824-$C82F: filename (12 bytes)
.byte $00,$00               ; $C830-$C831: load_end_lo, load_end_hi
.byte $00,$00               ; $C832-$C833: unused
.byte $00                   ; $C834: current_bank
.byte $FF                   ; $C835: last_saved
.byte $00                   ; $C836: reu_bitmap_lo
.byte $00                   ; $C837: reu_bitmap_hi
.byte $00                   ; $C838: reu_bitmap_xhi
.byte $08                   ; $C839: storage_drive (default drive 8)
.byte $00                   ; $C83A: log_index (for debug buffer)
.byte $00,$00,$00,$00,$00   ; $C83B-$C83F: reserved

;-----------------------------------------------------------------------------
; $C840: load_disk - Load app from disk and run (32 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $21, $C8         ; LDA $C821 (filename len)
.byte $A2, $24              ; LDX #$24
.byte $A0, $C8              ; LDY #$C8 (filename at $C824)
.byte $20, $BD, $FF         ; JSR $FFBD (SETNAM)
.byte $A9, $00              ; LDA #0
.byte $A2, $08              ; LDX #8
.byte $A0, $01              ; LDY #1
.byte $20, $BA, $FF         ; JSR $FFBA (SETLFS)
.byte $A9, $00              ; LDA #0
.byte $20, $D5, $FF         ; JSR $FFD5 (LOAD)
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00   ; Padding to $C860

;-----------------------------------------------------------------------------
; $C860: load_reu - Fetch app from REU and run (32 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C880: preload - Load to REU, return to launcher
; Called via JSR $C809
; DEBUG: writes markers to $C007-$C00C
;-----------------------------------------------------------------------------
.byte $A9, $AA              ; LDA #$AA (170 = "preload entered")
.byte $8D, $07, $C0         ; STA $C007

.byte $A9, $00              ; LDA #0 (bank 0)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)

.byte $A9, $BB              ; LDA #$BB (187 = "launcher stashed")
.byte $8D, $08, $C0         ; STA $C008

.byte $AD, $21, $C8         ; LDA $C821 (len)
.byte $A2, $24              ; LDX #$24
.byte $A0, $C8              ; LDY #$C8 (filename at $C824)
.byte $20, $BD, $FF         ; JSR SETNAM

.byte $A9, $00              ; LDA #0
.byte $A2, $08              ; LDX #8
.byte $A0, $01              ; LDY #1
.byte $20, $BA, $FF         ; JSR SETLFS

.byte $A9, $CC              ; LDA #$CC (204 = "about to LOAD")
.byte $8D, $09, $C0         ; STA $C009

.byte $A9, $00              ; LDA #0
.byte $20, $D5, $FF         ; JSR LOAD

.byte $8E, $30, $C8         ; STX $C830 (end addr lo)
.byte $8C, $31, $C8         ; STY $C831 (end addr hi)

.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)

.byte $A9, $EE              ; LDA #$EE (238 = "app stashed")
.byte $8D, $0B, $C0         ; STA $C00B

.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $C0, $C9         ; JSR set_bitmap ($C9C0)

.byte $A9, $00              ; LDA #0
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)

.byte $A9, $FF              ; LDA #$FF (255 = "launcher restored")
.byte $8D, $0C, $C0         ; STA $C00C

.byte $60                   ; RTS

.byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C8E0: stash_to_bank - Stash $1000-$C5FF to REU bank in A (16 bytes)
;-----------------------------------------------------------------------------
.byte $20, $A0, $C9         ; JSR reu_setup ($C9A0)
.byte $A9, $90              ; LDA #$90 (STASH command)
.byte $8D, $01, $DF         ; STA $DF01 - execute transfer
.byte $60                   ; RTS
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C8F0: fetch_bank - Fetch from REU bank in A to $1000-$C5FF (16 bytes)
;-----------------------------------------------------------------------------
.byte $20, $A0, $C9         ; JSR reu_setup ($C9A0)
.byte $A9, $91              ; LDA #$91 (FETCH command)
.byte $8D, $01, $DF         ; STA $DF01 - execute transfer
.byte $60                   ; RTS
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; Page 2: $C900-$C9FF - Main routines
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; $C900: return_to_launcher (64 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $C0, $C9         ; JSR set_bitmap ($C9C0)
.byte $AD, $34, $C8         ; LDA $C834
.byte $8D, $35, $C8         ; STA $C835
.byte $A9, $00              ; LDA #0
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)
.byte $A9, $00              ; LDA #0
.byte $8D, $34, $C8         ; STA $C834
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00

;-----------------------------------------------------------------------------
; $C940: switch_app (64 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $C0, $C9         ; JSR set_bitmap ($C9C0)
.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)
.byte $AD, $20, $C8         ; LDA $C820
.byte $8D, $34, $C8         ; STA $C834
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C960: debug_log_step - crash-surviving debug marker
;-----------------------------------------------------------------------------
.byte $8D, $20, $D0         ; STA $D020 - border color shows current step
.byte $8D, $F0, $01         ; STA $01F0 - store in stack page
.byte $8D, $00, $04         ; STA $0400 - store at top-left of screen
.byte $18                   ; CLC
.byte $69, $30              ; ADC #$30 - convert to ASCII digit
.byte $8D, $00, $04         ; STA $0400 - show as digit on screen
.byte $60                   ; RTS

.byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C9A0: reu_setup - Set up REU registers for $B600 transfer at $1000
;-----------------------------------------------------------------------------
.byte $8D, $06, $DF         ; STA $DF06 (bank)
.byte $A9, $00              ; LDA #$00
.byte $8D, $02, $DF         ; STA $DF02 (C64 addr lo = $00)
.byte $A9, $10              ; LDA #$10
.byte $8D, $03, $DF         ; STA $DF03 (C64 addr hi = $10, so $1000)
.byte $A9, $00              ; LDA #$00
.byte $8D, $04, $DF         ; STA $DF04 (REU addr lo)
.byte $8D, $05, $DF         ; STA $DF05 (REU addr hi)
.byte $8D, $07, $DF         ; STA $DF07 (len lo = $00)
.byte $A9, $B6              ; LDA #$B6
.byte $8D, $08, $DF         ; STA $DF08 (len hi = $B6, so $B600 bytes)
.byte $60                   ; RTS
.byte $00,$00

;-----------------------------------------------------------------------------
; $C9C0: set_bitmap - Set bit for bank in A in reu_bitmap ($C836-$C838)
;-----------------------------------------------------------------------------
.byte $C9, $18              ; CMP #$18 (only banks 0-23 are tracked)
.byte $B0, $17              ; BCS done
.byte $AA                   ; TAX - preserve full bank
.byte $29, $07              ; AND #$07 - bit index within byte
.byte $A8                   ; TAY - Y = bit index
.byte $8A                   ; TXA - restore full bank
.byte $4A                   ; LSR A
.byte $4A                   ; LSR A
.byte $4A                   ; LSR A
.byte $AA                   ; TAX - X = byte offset 0..2
.byte $A9, $01              ; LDA #$01
.byte $88                   ; DEY
.byte $30, $03              ; BMI store_bit
.byte $0A                   ; ASL A
.byte $10, $FA              ; BPL shift_loop
.byte $1D, $36, $C8         ; ORA $C836,X
.byte $9D, $36, $C8         ; STA $C836,X
.byte $60                   ; RTS
.byte $00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C9E0: log_byte - Write debug marker to buffer at $C980
;-----------------------------------------------------------------------------
.byte $48                   ; PHA - save the byte to log
.byte $8E, $FC, $00         ; STX $00FC - save X to ZP temp
.byte $AE, $3A, $C8         ; LDX $C83A - get log_index
.byte $68                   ; PLA - get byte back
.byte $9D, $80, $C9         ; STA $C980,X - write to buffer
.byte $E8                   ; INX - increment index
.byte $E0, $20              ; CPX #$20 - wrap at 32
.byte $D0, $02              ; BNE +2 (skip reset)
.byte $A2, $00              ; LDX #$00 - reset to 0
.byte $8E, $3A, $C8         ; STX $C83A - store new index
.byte $AE, $FC, $00         ; LDX $00FC - restore X
.byte $60                   ; RTS
.byte $00,$00,$00,$00,$00,$00,$00
