; Probe-only cc65 software-stack watermark. This uses self-modifying absolute
; operands so it does not borrow any cc65 runtime pointer while C is live. A
; $C300-$C3FF red zone below the reserved $C400 floor makes an overrun visible.

        .export _xuzinflate_stack_watermark_init
        .export _xuzinflate_stack_watermark_low
        .export _xuzinflate_stack_initial
        .importzp sp

        .segment "CODE"

_xuzinflate_stack_watermark_init:
        lda     sp
        sta     _xuzinflate_stack_initial
        sta     watermarkStore+1
        lda     sp+1
        sta     _xuzinflate_stack_initial+1
        sta     watermarkStore+2
        lda     #<$C300
        sta     watermarkStore+1
        lda     #>$C300
        sta     watermarkStore+2
watermarkInitLoop:
        lda     watermarkStore+2
        cmp     _xuzinflate_stack_initial+1
        bne     watermarkInitByte
        lda     watermarkStore+1
        cmp     _xuzinflate_stack_initial
        beq     watermarkInitDone
watermarkInitByte:
        lda     watermarkStore+1
        eor     watermarkStore+2
        eor     #$5A
watermarkStore:
        sta     $FFFF
        inc     watermarkStore+1
        bne     watermarkInitLoop
        inc     watermarkStore+2
        jmp     watermarkInitLoop
watermarkInitDone:
        rts

; Return the lowest address whose address-derived marker was changed. If no
; byte below the initial stack pointer changed, return the initial pointer.
_xuzinflate_stack_watermark_low:
        lda     #<$C300
        sta     watermarkLoad+1
        lda     #>$C300
        sta     watermarkLoad+2
watermarkScanLoop:
        lda     watermarkLoad+2
        cmp     _xuzinflate_stack_initial+1
        bne     watermarkScanByte
        lda     watermarkLoad+1
        cmp     _xuzinflate_stack_initial
        beq     watermarkScanClean
watermarkScanByte:
watermarkLoad:
        lda     $FFFF
        sta     watermarkActual
        lda     watermarkLoad+1
        eor     watermarkLoad+2
        eor     #$5A
        cmp     watermarkActual
        bne     watermarkScanFound
        inc     watermarkLoad+1
        bne     watermarkScanLoop
        inc     watermarkLoad+2
        jmp     watermarkScanLoop
watermarkScanFound:
        lda     watermarkLoad+1
        ldx     watermarkLoad+2
        rts
watermarkScanClean:
        lda     _xuzinflate_stack_initial
        ldx     _xuzinflate_stack_initial+1
        rts

        .segment "BSS"

_xuzinflate_stack_initial:
        .res    2
watermarkActual:
        .res    1
