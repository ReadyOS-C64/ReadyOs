;
; loader.s - Ready OS Loader Shim
; Resident at $0800-$0FFF, handles app loading and switching
; NOTE: Legacy shim source; production shim is generated from src/boot/boot_asm.s.
;
; This shim stays in memory and provides:
;   - Load app from disk to $1000
;   - Load app from REU to $1000 via DMA
;   - Save/restore launcher state
;   - Jump to app entry point
;

.export _shim_init
.export _shim_load_disk
.export _shim_load_reu
.export _shim_save_to_reu
.export _shim_restore_from_reu

; Zero page locations we use (safe area)
ZP_PTR      = $FB       ; 2 bytes for pointer
ZP_TMP      = $FD       ; 2 bytes temp

; App memory layout
APP_LOAD_ADDR   = $1000     ; Apps load here
APP_MAX_SIZE    = $B600     ; Current snapshot window max ($1000-$C5FF)

; REU registers
REU_STATUS      = $DF00
REU_COMMAND     = $DF01
REU_C64_LO      = $DF02
REU_C64_HI      = $DF03
REU_REU_LO      = $DF04
REU_REU_HI      = $DF05
REU_REU_BANK    = $DF06
REU_LEN_LO      = $DF07
REU_LEN_HI      = $DF08

; REU commands
REU_CMD_STASH   = $90
REU_CMD_FETCH   = $91

; KERNAL routines
SETNAM          = $FFBD
SETLFS          = $FFBA
LOAD            = $FFD5
READST          = $FFB7

; Screen for status messages
SCREEN          = $0400

.segment "CODE"

;-----------------------------------------------------------------------------
; _shim_init - Initialize the loader shim
; Called once at startup
;-----------------------------------------------------------------------------
.proc _shim_init
        ; Clear app name buffer
        lda #0
        sta app_name
        sta app_name_len
        sta app_bank
        sta app_size_lo
        sta app_size_hi
        sta app_loaded
        rts
.endproc

;-----------------------------------------------------------------------------
; _shim_load_disk - Load app from disk to $1000
; Input: A/X = pointer to filename (null-terminated)
;        Y = filename length
; Returns: A = 0 on success, 1 on error
;          app_size_lo/hi = bytes loaded
;-----------------------------------------------------------------------------
.proc _shim_load_disk
        ; Save filename pointer
        sta ZP_PTR
        stx ZP_PTR+1
        sty app_name_len

        ; Copy filename to our buffer
        ldy #0
@copy_name:
        lda (ZP_PTR),y
        sta app_name,y
        iny
        cpy app_name_len
        bne @copy_name

        ; SETNAM - set filename
        lda app_name_len
        ldx #<app_name
        ldy #>app_name
        jsr SETNAM

        ; SETLFS - device 8, secondary 0 (load to specified address)
        lda #$00        ; Logical file
        ldx #$08        ; Device 8
        ldy #$00        ; Secondary 0 = use load address in file, but we override
        jsr SETLFS

        ; LOAD to $1000
        lda #$00        ; 0 = load
        ldx #<APP_LOAD_ADDR
        ldy #>APP_LOAD_ADDR
        jsr LOAD
        bcs @error

        ; Calculate size loaded (end address in X/Y minus start)
        stx ZP_TMP
        sty ZP_TMP+1

        ; Size = end - start
        sec
        lda ZP_TMP
        sbc #<APP_LOAD_ADDR
        sta app_size_lo
        lda ZP_TMP+1
        sbc #>APP_LOAD_ADDR
        sta app_size_hi

        ; Mark as loaded
        lda #1
        sta app_loaded

        lda #0          ; Success
        rts

@error:
        lda #0
        sta app_loaded
        lda #1          ; Error
        rts
.endproc

;-----------------------------------------------------------------------------
; _shim_load_reu - Load app from REU to $1000 via DMA
; Input: A = REU bank number
;        X/Y = size (lo/hi)
; Returns: A = 0 on success
;-----------------------------------------------------------------------------
.proc _shim_load_reu
        ; Save parameters
        sta app_bank
        stx app_size_lo
        sty app_size_hi

        ; Setup REU DMA: fetch from REU to $1000
        lda #<APP_LOAD_ADDR
        sta REU_C64_LO
        lda #>APP_LOAD_ADDR
        sta REU_C64_HI

        lda #$00
        sta REU_REU_LO
        sta REU_REU_HI

        lda app_bank
        sta REU_REU_BANK

        lda app_size_lo
        sta REU_LEN_LO
        lda app_size_hi
        sta REU_LEN_HI

        ; Execute DMA fetch
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ; Mark as loaded
        lda #1
        sta app_loaded

        lda #0          ; Success
        rts
.endproc

;-----------------------------------------------------------------------------
; _shim_save_to_reu - Save memory region to REU
; Input: A = REU bank
;        X = C64 address high byte (low byte = 0)
;        Y = size in pages (256-byte blocks)
;-----------------------------------------------------------------------------
.proc _shim_save_to_reu
        sta REU_REU_BANK

        lda #$00
        sta REU_C64_LO
        sta REU_REU_LO
        sta REU_REU_HI

        stx REU_C64_HI

        ; Size = Y * 256
        lda #$00
        sta REU_LEN_LO
        sty REU_LEN_HI

        lda #REU_CMD_STASH
        sta REU_COMMAND

        rts
.endproc

;-----------------------------------------------------------------------------
; _shim_restore_from_reu - Restore memory region from REU
; Input: A = REU bank
;        X = C64 address high byte
;        Y = size in pages
;-----------------------------------------------------------------------------
.proc _shim_restore_from_reu
        sta REU_REU_BANK

        lda #$00
        sta REU_C64_LO
        sta REU_REU_LO
        sta REU_REU_HI

        stx REU_C64_HI

        lda #$00
        sta REU_LEN_LO
        sty REU_LEN_HI

        lda #REU_CMD_FETCH
        sta REU_COMMAND

        rts
.endproc

;-----------------------------------------------------------------------------
; run_loaded_app - Jump to loaded app at $1000
; Called after loading, never returns
;-----------------------------------------------------------------------------
.export _shim_run_app
.proc _shim_run_app
        jmp APP_LOAD_ADDR
.endproc

;-----------------------------------------------------------------------------
; Data section - must be in shim's memory space
;-----------------------------------------------------------------------------
.segment "DATA"

app_name:       .res 16, 0      ; Filename buffer
app_name_len:   .byte 0         ; Filename length
app_bank:       .byte 0         ; REU bank for current app
app_size_lo:    .byte 0         ; App size low byte
app_size_hi:    .byte 0         ; App size high byte
app_loaded:     .byte 0         ; 1 if app is loaded in RAM
