        .export _xuzdeflate_stack_pointer
        .export _xuzdeflate_stack_watermark_init
        .export _xuzdeflate_stack_watermark_low
        .importzp sp

; These diagnostics are called only by the persistent $A000 compressor
; coordinator. Keeping them in that packed image avoids consuming the tiny
; production resident-core margin needed by extraction's SEEK gateway.
        .segment "DEFLATE_COORD_CODE"

_xuzdeflate_stack_pointer:
        lda sp
        ldx sp+1
        rts

; Fill every unused byte below the coordinator's entry stack pointer with an
; address-derived value. The pattern has no all-equal blind spot, and this
; code uses only self-modifying absolute operands rather than cc65 ZP scratch.
_xuzdeflate_stack_watermark_init:
        lda     #$00
        sta     watermarkValid+1
        lda     sp
        sta     watermarkInitLimitLo+1
        sta     watermarkScanLimitLo+1
        sta     watermarkInitialLo+1
        lda     sp+1
        sta     watermarkInitLimitHi+1
        sta     watermarkScanLimitHi+1
        sta     watermarkInitialHi+1
        cmp     #$C4
        bcc     watermarkInitInvalid
        cmp     #$C6
        bcc     watermarkInitValid
        bne     watermarkInitInvalid
        lda     sp
        bne     watermarkInitInvalid
watermarkInitValid:
        lda     #$01
        sta     watermarkValid+1
        lda     #<$C400
        sta     watermarkStore+1
        lda     #>$C400
        sta     watermarkStore+2
watermarkInitLoop:
        lda     watermarkStore+2
watermarkInitLimitHi:
        cmp     #$FF
        bne     watermarkInitByte
        lda     watermarkStore+1
watermarkInitLimitLo:
        cmp     #$FF
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
watermarkInitInvalid:
watermarkInitialLo:
        lda     #$FF
watermarkInitialHi:
        ldx     #$FF
        rts

; Return the first changed byte in the reserved stack window, or the entry
; pointer when no filled byte was consumed.
_xuzdeflate_stack_watermark_low:
watermarkValid:
        lda     #$00
        bne     watermarkScanStart
        jmp     watermarkInitialLo
watermarkScanStart:
        lda     #<$C400
        sta     watermarkCompare+1
        lda     #>$C400
        sta     watermarkCompare+2
watermarkScanLoop:
        lda     watermarkCompare+2
watermarkScanLimitHi:
        cmp     #$FF
        bne     watermarkScanByte
        lda     watermarkCompare+1
watermarkScanLimitLo:
        cmp     #$FF
        beq     watermarkScanClean
watermarkScanByte:
        lda     watermarkCompare+1
        eor     watermarkCompare+2
        eor     #$5A
watermarkCompare:
        cmp     $FFFF
        bne     watermarkScanFound
        inc     watermarkCompare+1
        bne     watermarkScanLoop
        inc     watermarkCompare+2
        jmp     watermarkScanLoop
watermarkScanFound:
        lda     watermarkCompare+1
        ldx     watermarkCompare+2
        rts
watermarkScanClean:
        jmp     watermarkInitialLo

; Focused compressor diagnostics keep all mutable coordinator state in their
; fixed screen/REU windows, so they need no C BSS here.  Retain a one-byte
; linker segment for the uZPK v7 descriptor and generic phase loader contract.
        .segment "DEFLATE_COORD_BSS"
        .res    1
