;-----------------------------------------------------------------------------
; Shared ReadyOS shim image ($C600-$C9FF, 1024 bytes)
; This file is the canonical shim byte layout used by both the disk boot path
; and the EasyFlash cartridge flavor. Keep it identical across all variants.
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; Pages 1-2: $C600-$C7FF - reserved resident expansion space
;-----------------------------------------------------------------------------
; This space used to contain the C64-RAM REU allocation/metadata mirror.  The
; authoritative state now lives in the ReadyOS REU bank, but the RAM remains
; resident shim-owned capacity rather than becoming app snapshot memory.
.ifndef READYOS_SHIM_ABI_ONLY
.res $0200, $00
.endif

;-----------------------------------------------------------------------------
; Page 3: $C800-$C8FF - Jump table, data, and helper routines
;-----------------------------------------------------------------------------

; $C800-$C817: Jump Table (24 bytes, 8 entries)
jt_load_disk    = $C800     ; Load from disk, run
jt_load_reu     = $C803     ; Fetch from REU, run
jt_run_app      = $C806     ; Just run app
jt_preload      = $C809     ; Preload to REU, return
jt_return       = $C80C     ; Return to launcher
jt_switch       = $C80F     ; Switch to another app
jt_reserved     = $C812     ; Reserved compatibility slot (safe no-op)
jt_fetch_bank   = $C815     ; Helper: fetch from bank in A

.byte $4C, $40, $C8         ; $C800: JMP load_disk ($C840)
.byte $4C, $60, $C8         ; $C803: JMP load_reu ($C860)
.byte $4C, $00, $10         ; $C806: JMP $1000 (run app)
.byte $4C, $80, $C8         ; $C809: JMP preload ($C880)
.byte $4C, $00, $C9         ; $C80C: JMP return_to_launcher ($C900)
.byte $4C, $40, $C9         ; $C80F: JMP switch_app ($C940)
.byte $4C, $FF, $C9         ; $C812: reserved helper -> safe RTS
.byte $4C, $F0, $C8         ; $C815: JMP fetch_bank ($C8F0)

; $C818: deprecated logger compatibility slot
.byte $4C, $FF, $C9         ; $C818: deprecated logger -> safe RTS
.byte $4C, $C0, $C9         ; $C81B: JMP mark_loaded (token in A)
.byte $00,$00               ; $C81E-$C81F: reserved

; $C820-$C83F: Data Area (32 bytes)
; $C820: target_bank - bank to load/switch to
; $C821: filename_len
; $C822-$C823: launcher-side app-size scratch (not consumed by the shim)
; $C824-$C82F: filename (12 bytes)
; $C830: load_end_lo (KERNAL LOAD end addr low byte, saved by preload)
; $C831: load_end_hi (KERNAL LOAD end addr high byte, saved by preload)
; $C832-$C833: unused
; $C834: current_bank - currently running app
; $C835: last_saved
; $C836-$C838: retired bitmap bytes (reserved; authoritative state is in REU)
; $C839: storage_drive - shim-global default storage drive for D8/D9 app dialogs
;        This persists across app switches and is shared by apps that use the
;        common file-dialog default-drive contract.
; $C83A: reserved (formerly log_index)
; $C83B: readyos_bank - direct physical ReadyOS bank number (Skip)
; $C83C: launcher_flags - launcher-owned one-shot state flags
; $C83D: reu_lookup_scratch - one-byte ReadyOS-bank DMA scratch
; $C83E: token_scratch; $C83F: reserved

.byte $00                   ; $C820: target_bank
.byte $08                   ; $C821: filename_len
.byte $00, $00              ; $C822-$C823: launcher app-size scratch
.byte "LAUNCHER    "        ; $C824-$C82F: filename (12 bytes)
.byte $00,$00               ; $C830-$C831: load_end_lo, load_end_hi
.byte $00,$00               ; $C832-$C833: unused
.byte $00                   ; $C834: current_bank
.byte $FF                   ; $C835: last_saved
.byte $00                   ; $C836: reserved (retired bitmap byte)
.byte $00                   ; $C837: reserved (retired bitmap byte)
.byte $00                   ; $C838: reserved (retired bitmap byte)
.byte $08                   ; $C839: storage_drive (default drive 8)
.byte $00                   ; $C83A: reserved (retired debug-ring head)
.byte READYOS_REU_BANK_SKIP     ; $C83B: readyos_bank (physical Skip)
.byte $00                   ; $C83C: launcher_flags
.byte $00                   ; $C83D: reu_lookup_scratch
.byte $00,$00               ; $C83E-$C83F: reserved

;-----------------------------------------------------------------------------
; $C840: load_disk - Load app from disk and run (32 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $21, $C8         ; LDA $C821 (filename len)
.byte $A2, $24              ; LDX #$24
.byte $A0, $C8              ; LDY #$C8 (filename at $C824)
.byte $20, $BD, $FF         ; JSR $FFBD (SETNAM)
.byte $A9, $00              ; LDA #0
.byte $AE, $39, $C8         ; LDX $C839 (storage drive)
.byte $A0, $01              ; LDY #1
.byte $20, $BA, $FF         ; JSR $FFBA (SETLFS)
.byte $A9, $00              ; LDA #0
.byte $20, $D5, $FF         ; JSR $FFD5 (LOAD)
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00       ; Padding to $C860

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
;-----------------------------------------------------------------------------
.byte $A9, $00              ; LDA #0 (launcher token -> ReadyOS bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)

.byte $AD, $21, $C8         ; LDA $C821 (len)
.byte $A2, $24              ; LDX #$24
.byte $A0, $C8              ; LDY #$C8 (filename at $C824)
.byte $20, $BD, $FF         ; JSR SETNAM

.byte $A9, $00              ; LDA #0
.byte $AE, $39, $C8         ; LDX $C839 (storage drive)
.byte $A0, $01              ; LDY #1
.byte $20, $BA, $FF         ; JSR SETLFS

.byte $A9, $00              ; LDA #0
.byte $20, $D5, $FF         ; JSR LOAD

.byte $8E, $30, $C8         ; STX $C830 (end addr lo)
.byte $8C, $31, $C8         ; STY $C831 (end addr hi)

.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)

.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $C0, $C9         ; JSR mark_loaded ($C9C0)

.byte $A9, $00              ; LDA #0
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)

.byte $60                   ; RTS

.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00

;-----------------------------------------------------------------------------
; $C8E0: stash_to_bank - Stash $1000-$C5FF to REU bank in A (16 bytes)
;-----------------------------------------------------------------------------
.byte $20, $60, $C9         ; JSR reu_setup_logical ($C960)
.byte $A9, $90              ; LDA #$90 (STASH command)
.byte $8D, $01, $DF         ; STA $DF01 - execute transfer
.byte $60                   ; RTS
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C8F0: fetch_bank - Fetch from REU bank in A to $1000-$C5FF (16 bytes)
;-----------------------------------------------------------------------------
.byte $20, $60, $C9         ; JSR reu_setup_logical ($C960)
.byte $A9, $91              ; LDA #$91 (FETCH command)
.byte $8D, $01, $DF         ; STA $DF01 - execute transfer
.byte $60                   ; RTS
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; Page 4: $C900-$C9FF - Main routines
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; $C900: return_to_launcher (64 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $C0, $C9         ; JSR mark_loaded ($C9C0)
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
.byte $20, $C0, $C9         ; JSR mark_loaded ($C9C0)
.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)
.byte $AD, $20, $C8         ; LDA $C820
.byte $8D, $34, $C8         ; STA $C834
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C960: reu_setup_logical - Resolve token via the ReadyOS bank, then setup regs
;-----------------------------------------------------------------------------
.byte $C9, $00              ; CMP #0 (logical launcher bank?)
.byte $D0, $06              ; BNE lookup_app_bank
.byte $AD, $3B, $C8         ; LDA $C83B (ReadyOS bank contains launcher)
.byte $4C, $A0, $C9         ; JMP reu_setup ($C9A0)
.byte $AA                   ; lookup_app_bank: TAX (preserve logical token)
.byte $A9, $3D              ; LDA #<$C83D
.byte $8D, $02, $DF         ; STA $DF02 (C64 addr lo)
.byte $A9, $C8              ; LDA #>$C83D
.byte $8D, $03, $DF         ; STA $DF03 (C64 addr hi)
.byte $8A                   ; TXA
.byte $18                   ; CLC
.byte $69, $40              ; ADC #<$B740 (mapping offset + token)
.byte $8D, $04, $DF         ; STA $DF04 (REU offset lo)
.byte $A9, $B7              ; LDA #>$B740
.byte $69, $00              ; ADC #0 (include low-byte carry)
.byte $8D, $05, $DF         ; STA $DF05 (REU offset hi)
.byte $AD, $3B, $C8         ; LDA $C83B (ReadyOS global physical bank)
.byte $8D, $06, $DF         ; STA $DF06
.byte $A9, $01              ; LDA #1
.byte $8D, $07, $DF         ; STA $DF07 (length lo)
.byte $A9, $00              ; LDA #0
.byte $8D, $08, $DF         ; STA $DF08 (length hi)
.byte $A9, $91              ; LDA #$91 (FETCH command)
.byte $8D, $01, $DF         ; STA $DF01 - fetch physical bank byte
.byte $AD, $3D, $C8         ; LDA $C83D (resolved physical bank)
.byte $4C, $A0, $C9         ; JMP reu_setup ($C9A0)
.byte $00,$00               ; Padding to $C9A0

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
; $C9C0: mark_loaded - Commit token status after a successful snapshot stash
; A = logical token. Status $07 means valid, loaded, and resumable.
;-----------------------------------------------------------------------------
.byte $8D, $3E, $C8         ; STA $C83E (preserve token)
.byte $A9, $07              ; LDA #VALID|LOADED|RESUMABLE
.byte $8D, $3D, $C8         ; STA $C83D
.byte $A9, $3D              ; C64 source = $C83D
.byte $8D, $02, $DF
.byte $A9, $C8
.byte $8D, $03, $DF
.byte $AD, $3E, $C8         ; token
.byte $18                   ; CLC
.byte $69, $40              ; + <$B840 (token status table)
.byte $8D, $04, $DF
.byte $A9, $B8              ; >$B840
.byte $69, $00              ; include low-byte carry
.byte $8D, $05, $DF
.byte $AD, $3B, $C8         ; ReadyOS physical bank
.byte $8D, $06, $DF
.byte $A9, $01              ; one byte
.byte $8D, $07, $DF
.byte $A9, $00
.byte $8D, $08, $DF
.byte $A9, $90              ; STASH C64 byte to ReadyOS bank
.byte $8D, $01, $DF
.byte $AD, $3E, $C8         ; preserve caller-visible token in A
.byte $60                   ; RTS
.byte $00,$00,$00,$00       ; reserved through $C9FE
.byte $60                   ; $C9FF: compatibility no-op RTS
