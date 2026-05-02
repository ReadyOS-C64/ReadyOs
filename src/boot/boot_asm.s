;-----------------------------------------------------------------------------
; boot_asm.s - Ready OS Boot Loader
;
; Memory Map:
;   $0801-$08FF: Boot loader (this code, freed after boot)
;   $C800-$C9FF: Shim (512 bytes) - copied here, stays resident
;                (Using $C800 to avoid cc65 runtime overwriting $0800)
;   $1000-$C5FF: App snapshot window ($B600 / 46592 bytes)
;
; REU Banks:
;   Bank 0: Launcher
;   Bank 1: Editor
;   Bank 2: HexCalc
;-----------------------------------------------------------------------------

.segment "LOADADDR"
.word $0801

.segment "STARTUP"

basic_stub:
.word nextline
.word 10
.byte $9E
.byte "2061"
.byte 0
nextline:
.word 0

.segment "CODE"

;=============================================================================
; ZP variable assignments
;=============================================================================
frame_counter   = $02
cursor_visible  = $03
old_irq_lo      = $04
old_irq_hi      = $05
ptr1_lo         = $07
ptr1_hi         = $08
ptr2_lo         = $09
ptr2_hi         = $0A
; $0B-$0D: KERNAL I/O vars (LDTND, DFLTN, DFLTO) - DO NOT USE

BOOT_TITLE_ROW  = $06A8
BOOT_TITLE_COL  = $DAA8
TITLE_VARIANT_LEN = (msg_variant_end - msg_variant)
TITLE_VERSION_LEN = (msg_version_end - msg_version)
TITLE_TEXT_LEN = (TITLE_VARIANT_LEN + 1 + TITLE_VERSION_LEN)
TITLE_FRAME_LEN = (TITLE_TEXT_LEN + 4)
TITLE_LEFT_COL = ((40 - TITLE_FRAME_LEN) / 2)
TITLE_TEXT_COL = (TITLE_LEFT_COL + 2)
TITLE_VERSION_COL = (TITLE_TEXT_COL + TITLE_VARIANT_LEN + 1)
TITLE_RIGHT_COL = (TITLE_LEFT_COL + TITLE_FRAME_LEN - 1)

;=============================================================================
; Boot entry point
;=============================================================================
start:
    ; Blue screen
    lda #6
    sta $D020
    sta $D021

    ;--- Clear screen RAM ($0400-$07E7) with spaces ---
    lda #$20
    ldx #0
@clear1:
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne @clear1
    ; Clear last partial page ($07E0-$07E7)
    ldx #0
@clear2:
    lda #$20
    sta $07E0,x
    inx
    cpx #8
    bne @clear2

    ;--- Fill color RAM ($D800-$DBE7) with white (1) ---
    lda #1
    ldx #0
@color1:
    sta $D800,x
    sta $D900,x
    sta $DA00,x
    sta $DB00,x
    inx
    bne @color1

    ;=================================================================
    ; Step 2: Read C64 character ROM and render READY logo + OS font
    ;=================================================================
    ; Bank in character ROM: SEI, write $33 to $01
    sei
    lda $01
    pha                     ; save processor port
    lda #$33
    sta $01                 ; char ROM visible at $D000

    ; Copy font data for R, E, A, D, Y (5 letters x 8 bytes = 40 bytes)
    ; ROM layout: each char is 8 bytes at $D000 + (char_index * 8)
    ; R=18*8=$90, E=5*8=$28, A=1*8=$08, D=4*8=$20, Y=25*8=$C8
    ldx #7
@copy_R:
    lda $D090,x            ; R
    sta font_buf,x
    dex
    bpl @copy_R
    ldx #7
@copy_E:
    lda $D028,x            ; E
    sta font_buf+8,x
    dex
    bpl @copy_E
    ldx #7
@copy_A:
    lda $D008,x            ; A
    sta font_buf+16,x
    dex
    bpl @copy_A
    ldx #7
@copy_D:
    lda $D020,x            ; D -- NOTE: $D020 in char ROM, not VIC register!
    sta font_buf+24,x
    dex
    bpl @copy_D
    ldx #7
@copy_Y:
    lda $D0C8,x            ; Y
    sta font_buf+32,x
    dex
    bpl @copy_Y

    ; O and S font data is hardcoded as 5x5 square patterns in os_small_font_buf

    ; Restore processor port
    pla
    sta $01
    cli

    ;--- Render the 8x35 READY logo to screen ---
    ; 8 rows, each row: 5 letters, each letter: 7 bits (drop rightmost blank col)
    ; Screen starts at row 1 ($0429) so READY doesn't touch top border
    lda #<$0429
    sta ptr1_lo
    lda #>$0429
    sta ptr1_hi

    ldx #0                  ; font row index (0-7)
@row_loop:
    ldy #0                  ; screen column (0-34)
    stx frame_counter       ; temporarily save row index

    ; For each of 5 letters
    lda font_buf,x          ; letter 0 (R), row X
    jsr render_byte
    ldx frame_counter
    lda font_buf+8,x        ; letter 1 (E), row X
    jsr render_byte
    ldx frame_counter
    lda font_buf+16,x       ; letter 2 (A), row X
    jsr render_byte
    ldx frame_counter
    lda font_buf+24,x       ; letter 3 (D), row X
    jsr render_byte
    ldx frame_counter
    lda font_buf+32,x       ; letter 4 (Y), row X
    jsr render_byte

    ; Advance ptr1 by 40 to next screen row
    clc
    lda ptr1_lo
    adc #40
    sta ptr1_lo
    lda ptr1_hi
    adc #0
    sta ptr1_hi

    ldx frame_counter
    inx
    cpx #8
    bne @row_loop

    ;--- Set color RAM for logo rows (rows 0-7) to white (1) ---
    ; Already done in the full color fill above

    ;=================================================================
    ; Step 3: Draw "OS" under READY (5x5, dark gray)
    ;=================================================================
    ; Render 5x5 OS at rows 10-14, col 13
    lda #<($0400 + 10*40 + 13)
    sta ptr1_lo
    lda #>($0400 + 10*40 + 13)
    sta ptr1_hi
    ldx #0
@os_rows:
    ldy #0
    stx frame_counter
    lda os_small_font_buf,x
    jsr render_byte_5
    lda #$20
    sta (ptr1_lo),y
    iny
    sta (ptr1_lo),y
    iny
    ldx frame_counter
    lda os_small_font_buf+5,x
    jsr render_byte_5

    ; Advance ptr1 by 40 to next screen row
    clc
    lda ptr1_lo
    adc #40
    sta ptr1_lo
    lda ptr1_hi
    adc #0
    sta ptr1_hi

    ldx frame_counter
    inx
    cpx #5
    bne @os_rows

    ; Set OS color to yellow (7)
    lda #<($D800 + 10*40 + 13)
    sta ptr1_lo
    lda #>($D800 + 10*40 + 13)
    sta ptr1_hi
    ldx #5
@os_color_row:
    ldy #0
@os_color_col:
    lda #7
    sta (ptr1_lo),y
    iny
    cpy #12
    bne @os_color_col
    clc
    lda ptr1_lo
    adc #40
    sta ptr1_lo
    lda ptr1_hi
    adc #0
    sta ptr1_hi
    dex
    bne @os_color_row

    ;=================================================================
    ; Step 4: Setup flashing 2x2 cursor at row 6-7, col 35-36
    ;=================================================================
    ; Bottom-aligned with READY logo (rows 1-8), right of Y
    ; Row 6 = $0400 + 6*40 = $04F0
    ; Row 7 = $0400 + 7*40 = $0518
    lda #$A0                ; filled block
    sta $04F0 + 35
    sta $04F0 + 36
    sta $0518 + 35
    sta $0518 + 36
    ; Set cursor color to light green (13)
    lda #13
    sta $D8F0 + 36
    sta $D8F0 + 37
    sta $D918 + 36
    sta $D918 + 37

    ; Remove stray blocks at right of READY (row 6-7, col 35)
    lda #$20
    sta $04F0 + 35
    sta $0518 + 35

    ; Init variables
    lda #0
    sta frame_counter
    sta anim_spinner_ctr
    sta anim_spinner_idx
    sta anim_spinner_rot
    sta anim_spinner_style
    sta anim_shadow_phase
    lda #1
    sta cursor_visible

    ;=================================================================
    ; Step 4b: Display variant + version text at row 17 with PETSCII spinner
    ;          cells. The variant is baked at build time from the config file,
    ;          so boot keeps its original simple, resident-only memory shape.
    ;=================================================================
    ldx #0
    lda spinner_chars,x
    sta BOOT_TITLE_ROW + TITLE_LEFT_COL
    lda spinner_chars_ccw,x
    sta BOOT_TITLE_ROW + TITLE_RIGHT_COL
    lda #13
    sta BOOT_TITLE_COL + TITLE_LEFT_COL
    sta BOOT_TITLE_COL + TITLE_RIGHT_COL

    lda #$20
    sta BOOT_TITLE_ROW + TITLE_LEFT_COL + 1
    sta BOOT_TITLE_ROW + TITLE_RIGHT_COL - 1

    ldx #0
@write_variant:
    lda msg_variant,x
    jsr ascii_to_screen
    sta BOOT_TITLE_ROW + TITLE_TEXT_COL,x
    inx
    cpx #TITLE_VARIANT_LEN
    bne @write_variant

    lda #$20
    sta BOOT_TITLE_ROW + TITLE_TEXT_COL + TITLE_VARIANT_LEN

    ldx #0
@write_ver:
    lda msg_version,x
    jsr ascii_to_screen
    sta BOOT_TITLE_ROW + TITLE_VERSION_COL,x
    inx
    cpx #TITLE_VERSION_LEN
    bne @write_ver

    ;=================================================================
    ; Step 5: Install flash IRQ
    ;=================================================================
    sei

    ; Save old IRQ vector
    lda $0314
    sta old_irq_lo
    lda $0315
    sta old_irq_hi

    ; Set new IRQ vector
    lda #<raster_irq_handler
    sta $0314
    lda #>raster_irq_handler
    sta $0315

    ; Set raster line to 251 (bottom border - lightweight flash handler)
    lda $D011
    and #$7F                ; clear bit 7 of $D011 (raster high bit)
    sta $D011
    lda #251
    sta $D012

    ; Enable raster IRQ
    lda $D01A
    ora #$01
    sta $D01A

    cli

    ;=================================================================
    ; Step 6: Boot phases with progress messages
    ;=================================================================

    ;--- Phase 1: "INSTALLING KERNEL..." ---
    ldx #<msg_kernel
    ldy #>msg_kernel
    lda #(msg_kernel_end - msg_kernel)
    jsr print_progress      ; prints at row 19, col 4

    ; Copy shim to $C800 (512 bytes in two chunks)
    ldx #$00
copy_shim:
    dex
    lda shim_data,x
    sta $C800,x
    lda shim_data+256,x
    sta $C900,x
    cpx #$00
    bne copy_shim

    ; Verify shim installation - check first JMP opcode
    lda $C800
    cmp #$4C
    bne @verify_jmp
    lda $C801
    cmp #$40
    bne @verify_jmp
    lda $C802
    cmp #$C8
    beq @verify_ok
@verify_jmp:
    jmp verify_fail
@verify_ok:

    ;--- Phase 2: "INITIALIZING MEMORY..." ---
    ldx #<msg_memory
    ldy #>msg_memory
    lda #(msg_memory_end - msg_memory)
    jsr print_progress

    ; Initialize REU allocation table at $C600-$C7FF
    ldx #0
    lda #0
init_reu_table:
    sta $C600,x
    sta $C700,x
    inx
    bne init_reu_table

    ; Set magic byte
    lda #$A5
    sta $C700

    ;--- Phase 3: "LOADING LAUNCHER..." ---
    ldx #<msg_launcher
    ldy #>msg_launcher
    lda #(msg_launcher_end - msg_launcher)
    jsr print_progress

    ; Set filename "LAUNCHER"
    lda #8
    ldx #<filename
    ldy #>filename
    jsr $FFBD               ; SETNAM

    ; Set file parameters
    lda #0
    ldx #8
    ldy #1
    jsr $FFBA               ; SETLFS

    ; Disable sprites during LOAD - VIC sprite DMA steals cycles that
    ; interfere with KERNAL serial bus timing during disk I/O
    lda #0
    sta $D015               ; sprites off

    ; Load the file (cursor animates during disk I/O via IRQ)
    lda #0
    jsr $FFD5               ; LOAD
    bcs load_error

    ; Keep sprites disabled (OS is rendered in screen RAM)
    lda #0
    sta $D015

    ;--- Phase 4: "READY." ---
    ldx #<msg_ready
    ldy #>msg_ready
    lda #(msg_ready_end - msg_ready)
    jsr print_progress

    ; Brief delay (~0.5s) to let user see "READY."
    ; Software delay: 200 outer × 256 inner × ~11 cycles ≈ 563K cycles ≈ 0.56s
    ldy #200
@delay_outer:
    ldx #0
@delay_inner:
    nop
    nop
    nop
    dex
    bne @delay_inner
    dey
    bne @delay_outer

    ;=================================================================
    ; Step 8: Cleanup and launch
    ;=================================================================
    sei

    ; Restore IRQ vector
    lda old_irq_lo
    sta $0314
    lda old_irq_hi
    sta $0315

    ; Disable raster IRQ
    lda #0
    sta $D01A

    ; Acknowledge any pending
    lda #$FF
    sta $D019

    ; Disable sprites
    lda #0
    sta $D015

    ; Restore theme border
    lda #14
    sta $D020

    cli

    ; Jump to launcher
    jmp $1000

;=============================================================================
; verify_fail - Flash border red
;=============================================================================
verify_fail:
    inc $D020
    jmp verify_fail

;=============================================================================
; load_error - Red border + error message via KERNAL
;=============================================================================
load_error:
    ; Disable raster IRQ first
    sei
    lda old_irq_lo
    sta $0314
    lda old_irq_hi
    sta $0315
    lda #0
    sta $D01A
    sta $D015               ; disable sprites
    lda #$FF
    sta $D019
    cli

    lda #2
    sta $D020
    ldx #0
@err_loop:
    lda err_msg,x
    beq @err_halt
    jsr $FFD2
    inx
    bne @err_loop
@err_halt:
    jmp @err_halt

;=============================================================================
; render_byte - Render 7 bits of font data to screen (drop rightmost column)
; Input: A = font byte, Y = current screen column offset
;        ptr1 = screen row base address
; Output: Y advanced by 7
;=============================================================================
render_byte:
    sta ptr2_lo             ; temp save font byte
    ldx #7                  ; 7 bits (skip bit 0, rightmost blank column)
@bit_loop:
    asl ptr2_lo             ; shift high bit into carry
    bcc @space
    lda #$A0               ; filled block
    .byte $2C              ; BIT abs (skip next 2 bytes)
@space:
    lda #$20               ; space
    sta (ptr1_lo),y
    iny
    dex
    bne @bit_loop
    rts

;=============================================================================
; render_byte_5 - Render 5 bits of font data to screen (5x5 letters)
; Input: A = font byte (bits 7-3 used), Y = current screen column offset
;        ptr1 = screen row base address
; Output: Y advanced by 5
;=============================================================================
render_byte_5:
    sta ptr2_lo             ; temp save font byte
    ldx #5                  ; 5 bits (use bits 7-3)
@bit_loop_5:
    asl ptr2_lo             ; shift high bit into carry
    bcc @space_5
    lda #$A0               ; filled block
    .byte $2C              ; BIT abs (skip next 2 bytes)
@space_5:
    lda #$20               ; space
    sta (ptr1_lo),y
    iny
    dex
    bne @bit_loop_5
    rts

;=============================================================================
; generate_sprite - Build sprite data from 8x8 font at 3x horizontal, 3x vert
; Input: ptr2 = pointer to 8-byte font data
;        X/Y = lo/hi of destination address (e.g. $0340)
; Each font bit becomes 3 sprite pixels; with X-expand = 6 screen pixels (square)
; Generates 7 font rows × 3 = 21 sprite rows (63 bytes)
;=============================================================================
generate_sprite:
    stx @d0 + 1
    sty @d0 + 2
    stx @d1 + 1
    sty @d1 + 2
    stx @d2 + 1
    sty @d2 + 2

    ldx #0                  ; font row index (0-6)
    ldy #0                  ; sprite byte offset

@gen_row:
    lda #3
    sta gen_repeat          ; 3 sprite rows per font row

@gen_rep:
    ; Read font byte
    sty gen_offset          ; save sprite offset
    stx gen_save_x          ; save font row
    txa
    tay
    lda (ptr2_lo),y         ; read font byte for this row
    sta gen_font_byte       ; save full byte
    ldy gen_offset          ; restore sprite offset
    ldx gen_save_x

    ; 3x expand: high nibble -> sprite byte 0 + carry bits for byte 1
    lda gen_font_byte
    lsr a
    lsr a
    lsr a
    lsr a                   ; high nibble in A
    stx gen_save_x
    tax
    lda triple_hi,x         ; sprite byte 0 (top 8 bits of 12-bit expansion)
@d0:
    sta $0340,y
    iny

    lda triple_carry,x      ; carry bits from high nibble (bits 7-4 of byte 1)
    sta gen_temp
    lda gen_font_byte
    and #$0F                ; low nibble
    tax
    lda triple_hi,x         ; reuse triple_hi for low nibble
    lsr a
    lsr a
    lsr a
    lsr a                   ; shift to bits 3-0
    ora gen_temp            ; combine with carry -> sprite byte 1
@d1:
    sta $0340,y
    iny

    lda triple_lo,x         ; low nibble -> sprite byte 2
@d2:
    sta $0340,y
    iny

    ldx gen_save_x
    dec gen_repeat
    bne @gen_rep

    inx
    cpx #5                  ; 5 font rows (3/4 size, square pixels with 3x rep + Y-expand)
    bne @gen_row

    rts

;=============================================================================
; print_progress - Write a message to screen RAM
; Input: X/Y = lo/hi pointer to message, A = message length
; Uses sequential row tracking: row 19 first, then 20, 21, 22
;=============================================================================
print_progress:
    stx ptr2_lo             ; msg pointer lo
    sty ptr2_hi             ; msg pointer hi
    sta ptr1_lo             ; msg length

    ; Get current progress row from progress_row counter
    ldx progress_row

    ; Calculate screen address: $0400 + row*40 + 9 (col offset)
    ; Keep carry so row 19 ($06F8) correctly becomes $0701 instead of $0601.
    lda screen_rows_lo,x
    clc
    adc #9                  ; col 9 offset
    sta ptr1_hi             ; reuse as dest lo (temp)
    lda screen_rows_hi,x
    adc #0

    ; Now ptr1_hi = dest lo (with col offset), A = dest hi
    ; We need to set up for the copy
    sta @dest_hi_smc + 2    ; self-modifying: store high byte
    lda ptr1_hi
    sta @dest_hi_smc + 1    ; self-modifying: store low byte

    ; Also set up color RAM address (dest + $D400)
    lda screen_rows_lo,x
    clc
    adc #9                  ; row 19+, col 9 (5 chars farther right)
    sta @color_smc + 1
    lda screen_rows_hi,x
    adc #$D4                ; color RAM is $D400 higher than $0400 base
    sta @color_smc + 2

    ; Increment progress row for next call
    inc progress_row

    ; Copy message to screen (with ASCII->screen code conversion)
    ldy #0
@copy:
    cpy ptr1_lo             ; reached length?
    beq @set_color
    lda (ptr2_lo),y
    jsr ascii_to_screen
@dest_hi_smc:
    sta $0400,y             ; self-modified address
    iny
    bne @copy

@set_color:
    ; Set color RAM to light blue (14) for the message
    ldy #0
    lda #14
@color_loop:
    cpy ptr1_lo
    beq @done
@color_smc:
    sta $D800,y             ; self-modified address
    iny
    bne @color_loop

@done:
    rts

;=============================================================================
; ascii_to_screen - Convert ASCII/PETSCII to screen codes
; Input: A = ASCII char
; Output: A = screen code
;=============================================================================
ascii_to_screen:
    cmp #$41                ; 'A'
    bcc @not_upper
    cmp #$5B                ; 'Z'+1
    bcs @not_upper
    sec
    sbc #$40                ; A-Z -> 1-26
    rts
@not_upper:
    cmp #$61                ; 'a'
    bcc @not_lower
    cmp #$7B                ; 'z'+1
    bcs @not_lower
    sec
    sbc #$60                ; a-z -> 1-26
    rts
@not_lower:
    cmp #$2E                ; '.'
    bne @not_dot
    rts                     ; '.' screen code is $2E
@not_dot:
    ; Space and other punctuation pass through
    rts

;=============================================================================
; Raster IRQ handler - dual-rate anim (version spinners + cursor blinks + shadow)
; NOTE: anim_spinner_ctr/idx/shadow_phase are in regular memory, NOT ZP,
;       because ZP $0B-$0D are KERNAL I/O vars needed by LOAD
;=============================================================================
raster_irq_handler:
    ; Check if raster IRQ
    lda $D019
    and #$01
    bne @is_raster
    jmp @not_raster          ; OLD pattern: branch-if-NOT to bottom

@is_raster:

    ; --- Spinner step + shadow motion (every 12 frames) ---
    inc anim_spinner_ctr
    lda anim_spinner_ctr
    cmp #12
    bcc @no_spinner

    lda #0
    sta anim_spinner_ctr

    ; Next chunky PETSCII spinner frame
    inc anim_spinner_idx
    lda anim_spinner_idx
    and #$03
    sta anim_spinner_idx
    beq @check_spinner_style
    jmp @draw_spinner

    ; Switch spinner glyph family after 3 full rotations.
@check_spinner_style:
    inc anim_spinner_rot
    lda anim_spinner_rot
    cmp #3
    bcc @draw_spinner

    lda #0
    sta anim_spinner_rot
    lda anim_spinner_style
    eor #$04
    sta anim_spinner_style

@draw_spinner:
    lda anim_spinner_idx
    clc
    adc anim_spinner_style
    tax
    lda spinner_chars,x
    sta BOOT_TITLE_ROW + TITLE_LEFT_COL
    lda spinner_chars_ccw,x
    sta BOOT_TITLE_ROW + TITLE_RIGHT_COL

    ; Advance shadow phase and update shadow positions
    inc anim_shadow_phase
    lda anim_shadow_phase
    and #$07
    sta anim_shadow_phase
    tax

    ; Shadow O X = main X (188) + dx
    lda shadow_dx,x
    clc
    adc #188
    sta $D004
    ; Shadow S X = main X (240) + dx
    lda shadow_dx,x
    clc
    adc #240
    sta $D006
    ; Shadow Y = main Y (102) + dy (same for both shadow sprites)
    lda shadow_dy,x
    clc
    adc #102
    sta $D005
    sta $D007

@no_spinner:
    ; --- Cursor flash (every 24 frames = 4x border rate) ---
    inc frame_counter
    lda frame_counter
    cmp #24
    bcc @no_toggle

    lda #0
    sta frame_counter

    ; Toggle cursor visibility
    lda cursor_visible
    eor #$01
    sta cursor_visible
    tax

    ; Update cursor blocks (row 6-7, col 36-37)
    lda cursor_chars,x
    sta $04F0 + 36
    sta $04F0 + 37
    sta $0518 + 36
    sta $0518 + 37

@no_toggle:
    ; Acknowledge raster IRQ AT THE END (matching old working pattern)
    lda #$01
    sta $D019
    pla
    tay
    pla
    tax
    pla
    rti

@not_raster:
    ; AT THE BOTTOM (matching old working pattern)
    jmp ($0004)

;=============================================================================
; Data
;=============================================================================

; Font buffer: 5 letters x 8 bytes = 40 bytes
font_buf:
    .res 40, 0

; OS font data: 5x5 square/blocky patterns (bits 7-3 used)
os_small_font_buf:
    ; O (5x5, filled with single center hole)
    .byte $F8, $F8, $D8, $F8, $F8
    ; S (5x5, notches at top-right and bottom-left)
    .byte $F8, $E0, $F8, $38, $F8

; 3x horizontal expansion tables (4 bits -> 12 bits, spread across 3 bytes)
triple_hi:
    .byte $00,$00,$03,$03,$1C,$1C,$1F,$1F,$E0,$E0,$E3,$E3,$FC,$FC,$FF,$FF
triple_carry:
    .byte $00,$70,$80,$F0,$00,$70,$80,$F0,$00,$70,$80,$F0,$00,$70,$80,$F0
triple_lo:
    .byte $00,$07,$38,$3F,$C0,$C7,$F8,$FF,$00,$07,$38,$3F,$C0,$C7,$F8,$FF

; Cursor character lookup: index 0=hidden, 1=visible
cursor_chars:
    .byte $20, $A0

; Spinner character lookup:
;   entries 0-3  = chunky T-shape cycle
;   entries 4-7  = corner/quarter-block cycle
spinner_chars:
    .byte $6B, $72, $73, $71
    .byte $70, $6E, $7D, $6D

; Right spinner mirrors the left across the version text.
; This keeps the phase aligned while the apparent rotation runs opposite.
spinner_chars_ccw:
    .byte $73, $72, $6B, $71
    .byte $6E, $70, $6D, $7D

; Shadow offset tables (smooth circular oscillation)
shadow_dx:
    .byte 2, 3, 3, 4, 3, 2, 1, 1
shadow_dy:
    .byte 1, 2, 3, 3, 3, 2, 1, 1

; Progress row counter (starts at 0 = first progress row)
progress_row:
    .byte 0

; Screen row address lookup table (rows 19-22)
screen_rows_lo:
    .byte <($0400 + 19*40)  ; row 19 = $06F8
    .byte <($0400 + 20*40)  ; row 20 = $0720
    .byte <($0400 + 21*40)  ; row 21 = $0748
    .byte <($0400 + 22*40)  ; row 22 = $0770
screen_rows_hi:
    .byte >($0400 + 19*40)
    .byte >($0400 + 20*40)
    .byte >($0400 + 21*40)
    .byte >($0400 + 22*40)

; Progress messages (ASCII - converted to screen codes at runtime)
msg_kernel:
    .byte "INSTALLING KERNEL..."
msg_kernel_end:
msg_memory:
    .byte "INITIALIZING MEMORY..."
msg_memory_end:
msg_launcher:
    .byte "LOADING LAUNCHER..."
msg_launcher_end:
msg_ready:
    .byte "READY."
msg_ready_end:

.include "../generated/msg_version.inc"
.include "../generated/msg_variant.inc"

err_msg:
    .byte 13, "    LOAD ERROR!", 13, 0

filename:
    .byte "LAUNCHER"

; Temp variables for sprite generation
gen_offset:
    .byte 0
gen_save_x:
    .byte 0
gen_repeat:
    .byte 0
gen_font_byte:
    .byte 0
gen_temp:
    .byte 0

; Animation variables (regular memory, NOT ZP - $0B-$0D are KERNAL I/O vars)
anim_spinner_ctr:
    .byte 0
anim_spinner_idx:
    .byte 0
anim_spinner_rot:
    .byte 0
anim_spinner_style:
    .byte 0
anim_shadow_phase:
    .byte 0

.segment "RODATA"

;=============================================================================
; SHIM - 512 bytes at $C800-$C9FF (moved from $0800 to avoid cc65 conflict)
;=============================================================================
;
; BYTE ALIGNMENT VERIFICATION (all offsets from shim_data start):
;
; Page 1 ($C800-$C8FF):
;   $C800 (offset $00): Jump table (24 bytes) + log JMP + padding = 32 bytes
;   $C820 (offset $20): Data area = 32 bytes
;   $C840 (offset $40): load_disk = 32 bytes
;   $C860 (offset $60): load_reu = 32 bytes
;   $C880 (offset $80): preload = 78 bytes + padding
;   $C8E0 (offset $E0): stash_to_bank = 16 bytes
;   $C8F0 (offset $F0): fetch_bank = 16 bytes
;   Page 1 total: 256 bytes
;
; Page 2 ($C900-$C9FF):
;   $C900 (offset $100): return_to_launcher = 64 bytes
;   $C940 (offset $140): switch_app = 32 bytes
;   $C960 (offset $160): debug_log_step = 64 bytes
;   $C9A0 (offset $1A0): reu_setup = 32 bytes
;   $C9C0 (offset $1C0): set_bitmap = 32 bytes
;   $C9E0 (offset $1E0): log_byte = 32 bytes
;   Page 2 total: 256 bytes
;
; TOTAL: 512 bytes
;=============================================================================
shim_data:
.include "readyos_shim.inc"
