;
; reu.s - REU DMA Transfer Routines for Ready OS
; Handles 16MB REU (256 x 64KB banks) for context switching
;

.export _reu_stash, _reu_fetch, _reu_transfer
.export _reu_detect, _reu_stash_byte, _reu_fetch_byte

; REU Register Addresses
REU_STATUS   = $DF00    ; Status register (read-only)
REU_COMMAND  = $DF01    ; Command register
REU_C64_LO   = $DF02    ; C64 address low byte
REU_C64_HI   = $DF03    ; C64 address high byte
REU_REU_LO   = $DF04    ; REU address low byte
REU_REU_HI   = $DF05    ; REU address high byte
REU_REU_BANK = $DF06    ; REU bank (0-255 for 16MB)
REU_LEN_LO   = $DF07    ; Transfer length low byte
REU_LEN_HI   = $DF08    ; Transfer length high byte
REU_IRQ_MASK = $DF09    ; Interrupt mask
REU_ADDR_CTL = $DF0A    ; Address control

; REU Commands
REU_CMD_STASH = $90     ; C64 -> REU (with autoload, no FF00 trigger)
REU_CMD_FETCH = $91     ; REU -> C64 (with autoload, no FF00 trigger)
REU_CMD_SWAP  = $92     ; Exchange both directions
REU_CMD_VERIFY = $93    ; Compare C64 <-> REU

.segment "CODE"

;-----------------------------------------------------------------------------
; _reu_detect - Check if REU is present
; Returns: 1 if REU detected, 0 if not
;-----------------------------------------------------------------------------
.proc _reu_detect
        ; Try to write and read back from REU registers
        lda #$00
        sta REU_REU_BANK
        lda REU_REU_BANK
        cmp #$00
        bne @no_reu

        lda #$FF
        sta REU_REU_BANK
        lda REU_REU_BANK
        cmp #$FF
        bne @no_reu

        ; REU detected
        lda #$01
        ldx #$00
        rts

@no_reu:
        lda #$00
        ldx #$00
        rts
.endproc

;-----------------------------------------------------------------------------
; _reu_stash - Copy data from C64 memory to REU
; void reu_stash(unsigned int c64_addr, unsigned char bank,
;                unsigned int reu_addr, unsigned int length)
;
; Stack layout on entry (sp points to c64_addr low):
;   sp+0,1 = c64_addr
;   sp+2   = bank
;   sp+3,4 = reu_addr
;   sp+5,6 = length
;-----------------------------------------------------------------------------
.proc _reu_stash
        ; Get length from stack (last param pushed first = top of stack before call)
        ; CC65 passes params: rightmost first on stack, AX = leftmost
        ; Actually for 5+ param calls, all on stack
        jsr setup_reu_params
        lda #REU_CMD_STASH
        sta REU_COMMAND
        rts
.endproc

;-----------------------------------------------------------------------------
; _reu_fetch - Copy data from REU to C64 memory
; void reu_fetch(unsigned int c64_addr, unsigned char bank,
;                unsigned int reu_addr, unsigned int length)
;-----------------------------------------------------------------------------
.proc _reu_fetch
        jsr setup_reu_params
        lda #REU_CMD_FETCH
        sta REU_COMMAND
        rts
.endproc

;-----------------------------------------------------------------------------
; _reu_transfer - General transfer with command parameter
; void reu_transfer(unsigned int c64_addr, unsigned char bank,
;                   unsigned int reu_addr, unsigned int length,
;                   unsigned char command)
;
; Parameters via stack (CC65 cdecl):
;   sp+0,1 = c64_addr
;   sp+2   = bank
;   sp+3,4 = reu_addr
;   sp+5,6 = length
;   sp+7   = command
;-----------------------------------------------------------------------------
.proc _reu_transfer
        jsr setup_reu_params
        ; Get command byte
        ldy #7
        lda (sp),y
        sta REU_COMMAND
        rts
.endproc

;-----------------------------------------------------------------------------
; setup_reu_params - Helper to load REU registers from stack
; Used by stash, fetch, transfer
;-----------------------------------------------------------------------------
.proc setup_reu_params
        ; Disable interrupts during setup
        sei

        ; C64 address (sp+0,1)
        ldy #0
        lda (sp),y
        sta REU_C64_LO
        iny
        lda (sp),y
        sta REU_C64_HI

        ; REU bank (sp+2)
        iny
        lda (sp),y
        sta REU_REU_BANK

        ; REU address (sp+3,4)
        iny
        lda (sp),y
        sta REU_REU_LO
        iny
        lda (sp),y
        sta REU_REU_HI

        ; Transfer length (sp+5,6)
        iny
        lda (sp),y
        sta REU_LEN_LO
        iny
        lda (sp),y
        sta REU_LEN_HI

        ; Re-enable interrupts
        cli
        rts
.endproc

;-----------------------------------------------------------------------------
; _reu_stash_byte - Store a single byte to REU
; void reu_stash_byte(unsigned char value, unsigned char bank, unsigned int addr)
; A = value, stack has bank and addr
;-----------------------------------------------------------------------------
.proc _reu_stash_byte
        ; Store value temporarily
        sta tmp_byte

        ; Setup for 1-byte transfer
        lda #<tmp_byte
        sta REU_C64_LO
        lda #>tmp_byte
        sta REU_C64_HI

        ; Bank from stack
        ldy #0
        lda (sp),y
        sta REU_REU_BANK

        ; REU address
        iny
        lda (sp),y
        sta REU_REU_LO
        iny
        lda (sp),y
        sta REU_REU_HI

        ; Length = 1
        lda #$01
        sta REU_LEN_LO
        lda #$00
        sta REU_LEN_HI

        ; Execute stash
        lda #REU_CMD_STASH
        sta REU_COMMAND
        rts

tmp_byte:
        .byte 0
.endproc

;-----------------------------------------------------------------------------
; _reu_fetch_byte - Fetch a single byte from REU
; unsigned char reu_fetch_byte(unsigned char bank, unsigned int addr)
; Returns byte in A
;-----------------------------------------------------------------------------
.proc _reu_fetch_byte
        ; Bank from A (first param)
        sta REU_REU_BANK

        ; REU address from stack
        ldy #0
        lda (sp),y
        sta REU_REU_LO
        iny
        lda (sp),y
        sta REU_REU_HI

        ; C64 temp location
        lda #<tmp_byte
        sta REU_C64_LO
        lda #>tmp_byte
        sta REU_C64_HI

        ; Length = 1
        lda #$01
        sta REU_LEN_LO
        lda #$00
        sta REU_LEN_HI

        ; Execute fetch
        lda #REU_CMD_FETCH
        sta REU_COMMAND

        ; Return the byte
        lda tmp_byte
        ldx #$00
        rts

tmp_byte:
        .byte 0
.endproc

; Import CC65 runtime stack pointer
.import sp
