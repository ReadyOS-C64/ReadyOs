;
; context.s - Context Save/Restore for Ready OS
; Handles saving and restoring full app state via REU DMA
; NOTE: Legacy shim module; production shim is generated from src/boot/boot_asm.s.
;

.export _context_save, _context_restore

; REU registers
REU_STATUS   = $DF00
REU_COMMAND  = $DF01
REU_C64_LO   = $DF02
REU_C64_HI   = $DF03
REU_REU_LO   = $DF04
REU_REU_HI   = $DF05
REU_REU_BANK = $DF06
REU_LEN_LO   = $DF07
REU_LEN_HI   = $DF08

; Commands
REU_CMD_STASH = $90
REU_CMD_FETCH = $91

; Memory regions
ZP_START     = $00
ZP_SIZE      = $100
STACK_START  = $0100
STACK_SIZE   = $100
SCREEN_START = $0400
SCREEN_SIZE  = 1000     ; 40x25 = 1000 bytes
COLOR_START  = $D800
COLOR_SIZE   = 1000
APP_START    = $1000
APP_END      = $C600
APP_SIZE     = $B600    ; 46,592 bytes ($1000-$C5FF)

; REU offsets for saved state (within app's 64KB bank)
REU_OFF_CPU      = $0000    ; 8 bytes - CPU registers
REU_OFF_ZP       = $0100    ; 256 bytes - zero page
REU_OFF_STACK    = $0200    ; 256 bytes - hardware stack
REU_OFF_VIC      = $0300    ; 64 bytes - VIC registers
REU_OFF_SCREEN   = $0400    ; 1024 bytes - screen memory
REU_OFF_COLOR    = $0800    ; 1024 bytes - color RAM (CPU copy)
REU_OFF_APP      = $1000    ; Rest - app memory

.segment "DATA"

; Current bank for save/restore operations
work_bank:      .byte 0

; Saved CPU state
cpu_a:          .byte 0
cpu_x:          .byte 0
cpu_y:          .byte 0
cpu_sp:         .byte 0
cpu_status:     .byte 0
cpu_pc_lo:      .byte 0
cpu_pc_hi:      .byte 0

; Temp buffer for color RAM (can't DMA from $D800)
.segment "BSS"
color_buffer:   .res 1000

.segment "CODE"

;-----------------------------------------------------------------------------
; _context_save - Save full app context to REU
; Input: work_bank should contain the target REU bank
; Saves: Zero page, stack, screen, color RAM, VIC regs, app memory
;-----------------------------------------------------------------------------
.proc _context_save
        ; Disable interrupts
        sei

        ; Save CPU registers first (before we clobber them)
        sta cpu_a
        stx cpu_x
        sty cpu_y
        php
        pla
        sta cpu_status
        tsx
        stx cpu_sp

        ; Get bank from temp_bank (set by caller in syscalls.s)
        ; For now use a fixed location - caller sets work_bank
        lda work_bank
        sta REU_REU_BANK

        ;----- Save Zero Page via DMA -----
        lda #<ZP_START
        sta REU_C64_LO
        lda #>ZP_START
        sta REU_C64_HI
        lda #<REU_OFF_ZP
        sta REU_REU_LO
        lda #>REU_OFF_ZP
        sta REU_REU_HI
        lda #<ZP_SIZE
        sta REU_LEN_LO
        lda #>ZP_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND

        ;----- Save Hardware Stack via DMA -----
        lda #<STACK_START
        sta REU_C64_LO
        lda #>STACK_START
        sta REU_C64_HI
        lda #<REU_OFF_STACK
        sta REU_REU_LO
        lda #>REU_OFF_STACK
        sta REU_REU_HI
        lda #<STACK_SIZE
        sta REU_LEN_LO
        lda #>STACK_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND

        ;----- Save Screen Memory via DMA -----
        lda #<SCREEN_START
        sta REU_C64_LO
        lda #>SCREEN_START
        sta REU_C64_HI
        lda #<REU_OFF_SCREEN
        sta REU_REU_LO
        lda #>REU_OFF_SCREEN
        sta REU_REU_HI
        lda #<SCREEN_SIZE
        sta REU_LEN_LO
        lda #>SCREEN_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND

        ;----- Save Color RAM via CPU (can't DMA from $D800) -----
        ; Copy color RAM to buffer first
        ldx #0
@copy_color:
        lda COLOR_START,x
        sta color_buffer,x
        lda COLOR_START+$100,x
        sta color_buffer+$100,x
        lda COLOR_START+$200,x
        sta color_buffer+$200,x
        inx
        bne @copy_color

        ; Copy remaining 232 bytes (1000 - 768)
        ldx #0
@copy_color_rest:
        lda COLOR_START+$300,x
        sta color_buffer+$300,x
        inx
        cpx #232
        bne @copy_color_rest

        ; Now DMA the buffer to REU
        lda #<color_buffer
        sta REU_C64_LO
        lda #>color_buffer
        sta REU_C64_HI
        lda #<REU_OFF_COLOR
        sta REU_REU_LO
        lda #>REU_OFF_COLOR
        sta REU_REU_HI
        lda #<COLOR_SIZE
        sta REU_LEN_LO
        lda #>COLOR_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND

        ;----- Save VIC Registers -----
        ; Copy VIC regs ($D000-$D03F) to buffer, then DMA
        ; Actually VIC is at $D000-$D02E (47 bytes)
        ldx #46
@copy_vic:
        lda $D000,x
        sta vic_buffer,x
        dex
        bpl @copy_vic

        lda #<vic_buffer
        sta REU_C64_LO
        lda #>vic_buffer
        sta REU_C64_HI
        lda #<REU_OFF_VIC
        sta REU_REU_LO
        lda #>REU_OFF_VIC
        sta REU_REU_HI
        lda #47
        sta REU_LEN_LO
        lda #0
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND

        ;----- Save App Memory via DMA (46KB) -----
        ; This is the big one - $1000 to $C5FF
        lda #<APP_START
        sta REU_C64_LO
        lda #>APP_START
        sta REU_C64_HI
        lda #<REU_OFF_APP
        sta REU_REU_LO
        lda #>REU_OFF_APP
        sta REU_REU_HI
        lda #<APP_SIZE
        sta REU_LEN_LO
        lda #>APP_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND

        ;----- Save CPU state -----
        lda #<cpu_a
        sta REU_C64_LO
        lda #>cpu_a
        sta REU_C64_HI
        lda #<REU_OFF_CPU
        sta REU_REU_LO
        lda #>REU_OFF_CPU
        sta REU_REU_HI
        lda #8              ; 8 bytes of CPU state
        sta REU_LEN_LO
        lda #0
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND

        ; Re-enable interrupts
        cli
        rts

vic_buffer:
        .res 47
.endproc

;-----------------------------------------------------------------------------
; _context_restore - Restore full app context from REU
; Input: work_bank should contain the source REU bank
; Restores: Zero page, stack, screen, color RAM, VIC regs, app memory
;-----------------------------------------------------------------------------
.proc _context_restore
        ; Disable interrupts
        sei

        lda work_bank
        sta REU_REU_BANK

        ;----- Restore CPU state first (to our temp vars) -----
        lda #<cpu_a
        sta REU_C64_LO
        lda #>cpu_a
        sta REU_C64_HI
        lda #<REU_OFF_CPU
        sta REU_REU_LO
        lda #>REU_OFF_CPU
        sta REU_REU_HI
        lda #8
        sta REU_LEN_LO
        lda #0
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ;----- Restore App Memory via DMA (46KB) -----
        lda #<APP_START
        sta REU_C64_LO
        lda #>APP_START
        sta REU_C64_HI
        lda #<REU_OFF_APP
        sta REU_REU_LO
        lda #>REU_OFF_APP
        sta REU_REU_HI
        lda #<APP_SIZE
        sta REU_LEN_LO
        lda #>APP_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ;----- Restore VIC Registers -----
        lda #<vic_buffer
        sta REU_C64_LO
        lda #>vic_buffer
        sta REU_C64_HI
        lda #<REU_OFF_VIC
        sta REU_REU_LO
        lda #>REU_OFF_VIC
        sta REU_REU_HI
        lda #47
        sta REU_LEN_LO
        lda #0
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ; Copy back to VIC
        ldx #46
@restore_vic:
        lda vic_buffer,x
        sta $D000,x
        dex
        bpl @restore_vic

        ;----- Restore Color RAM via CPU -----
        ; Fetch to buffer first
        lda #<color_buffer
        sta REU_C64_LO
        lda #>color_buffer
        sta REU_C64_HI
        lda #<REU_OFF_COLOR
        sta REU_REU_LO
        lda #>REU_OFF_COLOR
        sta REU_REU_HI
        lda #<COLOR_SIZE
        sta REU_LEN_LO
        lda #>COLOR_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ; Copy buffer to color RAM
        ldx #0
@restore_color:
        lda color_buffer,x
        sta COLOR_START,x
        lda color_buffer+$100,x
        sta COLOR_START+$100,x
        lda color_buffer+$200,x
        sta COLOR_START+$200,x
        inx
        bne @restore_color

        ldx #0
@restore_color_rest:
        lda color_buffer+$300,x
        sta COLOR_START+$300,x
        inx
        cpx #232
        bne @restore_color_rest

        ;----- Restore Screen Memory via DMA -----
        lda #<SCREEN_START
        sta REU_C64_LO
        lda #>SCREEN_START
        sta REU_C64_HI
        lda #<REU_OFF_SCREEN
        sta REU_REU_LO
        lda #>REU_OFF_SCREEN
        sta REU_REU_HI
        lda #<SCREEN_SIZE
        sta REU_LEN_LO
        lda #>SCREEN_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ;----- Restore Hardware Stack via DMA -----
        lda #<STACK_START
        sta REU_C64_LO
        lda #>STACK_START
        sta REU_C64_HI
        lda #<REU_OFF_STACK
        sta REU_REU_LO
        lda #>REU_OFF_STACK
        sta REU_REU_HI
        lda #<STACK_SIZE
        sta REU_LEN_LO
        lda #>STACK_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ;----- Restore Zero Page via DMA -----
        ; Do this last as it affects our working memory
        lda #<ZP_START
        sta REU_C64_LO
        lda #>ZP_START
        sta REU_C64_HI
        lda #<REU_OFF_ZP
        sta REU_REU_LO
        lda #>REU_OFF_ZP
        sta REU_REU_HI
        lda #<ZP_SIZE
        sta REU_LEN_LO
        lda #>ZP_SIZE
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ; Restore CPU registers
        ldx cpu_sp
        txs
        lda cpu_status
        pha
        lda cpu_a
        ldx cpu_x
        ldy cpu_y
        plp

        ; Re-enable interrupts
        cli
        rts

vic_buffer:
        .res 47
.endproc

; Import shared data
.import work_bank
