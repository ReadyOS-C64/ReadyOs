; Resident cold-start bridge. The actual unpacker is stored immediately after
; the $1000-$2FFF core, copied to $A000, and runs with BASIC ROM hidden. Once
; it expands the UI from the launcher-owned REU package, normal UI entry begins.

        .export _uz_cold_start
        .import _uz_cold_boot_run, _uzip_ui_main, _uzip_ui_warm_main
        .import _uzip_ui_resume_marker
        .import __BOOT_CODE_LOAD__, __BOOT_CODE_RUN__, __BOOT_CODE_SIZE__

        .segment "COLD_DATA"
boot_cpu_port:
        .byte   0
boot_left:
        .word   0

        .segment "COLD_CODE"
_uz_cold_start:
        ; ReadyOS snapshot restores restart execution at $1000 rather than at
        ; the old PC. A valid restored UI skips the destructive cold unpack.
        lda     _uzip_ui_resume_marker
        cmp     #$55
        bne     cold_start
        lda     _uzip_ui_resume_marker+1
        cmp     #$5A
        bne     cold_start
        lda     _uzip_ui_resume_marker+2
        cmp     #$57
        bne     cold_start
        lda     _uzip_ui_resume_marker+3
        cmp     #$31
        bne     cold_start
        jsr     _uzip_ui_warm_main
        rts

cold_start:
        lda     #<__BOOT_CODE_SIZE__
        sta     boot_left
        lda     #>__BOOT_CODE_SIZE__
        sta     boot_left+1

copy_byte:
copy_source:
        lda     __BOOT_CODE_LOAD__
copy_target:
        sta     __BOOT_CODE_RUN__
        inc     copy_source+1
        bne     :+
        inc     copy_source+2
:
        inc     copy_target+1
        bne     :+
        inc     copy_target+2
:
        lda     boot_left
        bne     :+
        dec     boot_left+1
:
        dec     boot_left
        lda     boot_left
        ora     boot_left+1
        bne     copy_byte

        lda     $01
        sta     boot_cpu_port
        and     #$FE
        sta     $01
        jsr     _uz_cold_boot_run
        pha
        lda     boot_cpu_port
        sta     $01
        pla
        beq     boot_ok

        ; Cold resource/decode failure: leave a stable, visible numeric marker
        ; rather than entering a partial UI image.
        sta     $0400
        lda     #2
        sta     $D020
        lda     #0
        sta     $D021
        lda     $0400
        ora     #$30
        sta     $0400
        lda     #2
        sta     $D800
        ldx     #0
        rts

boot_ok:
        jsr     _uzip_ui_main
        rts
