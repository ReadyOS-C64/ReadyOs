; Copied above the BOOTER load range after both ReadyOS images are committed.
; Keep the $0400 trampoline intact so a failed LOAD can restore ReadyOS.
.segment "CODE"
; Temporary storage in the already-displaced trampoline page. Keeping this
; outside the $CF00 code leaves the cold loader below I/O at $D000.
vectors = $0710
video_standard = $072a
boot_device = $0730
part_length = $0731
dir_length = $0732
boot_length = $0733
booter = $0734
partition = $0745
directory = $0765
        lda #$37
        sta $01
        ldx #$ff
        txs
        lda $02a6
        sta video_standard
        ; Recreate the cold KERNAL workspace; native C64 OS boot expects its
        ; low-RAM globals to start clean. Retain IDEDOS's patched I/O vectors.
        ldx #25
save_vectors:
        lda $031a,x
        sta vectors,x
        dex
        bpl save_vectors
        jsr $ff87
        jsr $ff8a
        lda $0706
        beq vectors_ready       ; plain IEC keeps freshly restored ROM vectors
        ldx #25
restore_vectors:
        lda vectors,x
        sta $031a,x
        dex
        bpl restore_vectors
vectors_ready:
        jsr $e453
        jsr $e3bf
        jsr $a644
        lda video_standard
        sta $02a6
        lda #$31
        sta $0314
        lda #$ea
        sta $0315
        lda #1
        sta $cc
        lda #0
        sta $c6
        sta $9d
        sta $02
        ; RAMTAS cleared the KERNAL keyboard-buffer capacity. C64OS retains
        ; the ROM's printable-key enqueue path, which rejects every key when
        ; this limit is zero (its separate command-key path still works).
        ; Restore CINT's normal limit without clearing the $0400 trampoline.
        lda #10
        sta $0289
        sta $028c
        lda #4
        sta $028b
        ; The ROM IRQ can scan a held key while BOOTER is loading. C64OS
        ; repurposes this pointer later: initialize it only on cold entry.
        lda #$48
        sta $028f
        lda #$eb
        sta $0290
        lda #$47
        sta $0318
        lda #$fe
        sta $0319
        lda #$1b
        sta $d011
        ; KERNAL timer setup selects PAL/NTSC using the preserved $02A6,
        ; enables timer A and releases the idle IEC clock line.
        jsr $fddd
        cli
        lda $070a
        beq :+
        inc booter              ; failure probe asks for nonexistent COOTER
:
        lda part_length
        beq directory_boot
        ldx #<partition
        ldy #>partition
        jsr command
        bcs failed
directory_boot:
        lda dir_length
        beq load_boot
        ldx #<directory
        ldy #>directory
        jsr command
        bcs failed
load_boot:
        lda boot_length
        ldx #<booter
        ldy #>booter
        jsr $ffbd
        ldx boot_device
        lda #0
        ldy #1
        jsr $ffba
        ; SETLFS preserves A=0 for LOAD.
        jsr $ffd5
        bcs failed
        lda $0805
        cmp #$9e
        bne failed
        jmp $080d
failed:
        sei
        sta $070b
        lda #32
        sta $0701
        jmp $0403
command:
        jsr $ffbd
        lda #15
        ldx boot_device
        ldy #15
        jsr $ffba
        jsr $ffc0
        php
        pha
        lda #15
        jsr $ffc3
        jsr $ffcc
        pla
        plp
        rts
