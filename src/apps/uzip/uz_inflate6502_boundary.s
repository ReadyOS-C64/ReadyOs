; Fixed C/assembly callback bridge for the modified zlib6502 phase.
;
; Piotr Fusik's compact decoder deliberately aliases cc65 ptr1-ptr4 and sreg
; for its hot state. A normal C callback may clobber all of them, so every
; refill/flush/finalize boundary passes through this bridge. The caller's X/Y
; registers and the ten owned zero-page bytes are restored exactly; A returns
; the callback's boolean result.

        .export _uzif_refill_saved
        .export _uzif_flush_saved
        .export _uzif_finish_saved

        .import _uz_inflate6502_refill_boundary
        .import _uz_inflate6502_flush_boundary
        .import _uz_inflate6502_finish_boundary
        .importzp sreg, tmp1, tmp2, ptr1, ptr2, ptr3, ptr4

.ifdef UZIP_READYOS_APP
        .segment "INFLATE_CODE"
.else
        .segment "JOB_CODE"
.endif

.macro CALLBACK_BRIDGE name, target
.proc name
        txa
        pha
        tya
        pha
        jsr save_codec_zp
        jsr target
        sta boundary_result
        jsr restore_codec_zp
        pla
        tay
        pla
        tax
        lda boundary_result
        rts
.endproc
.endmacro

CALLBACK_BRIDGE _uzif_refill_saved, _uz_inflate6502_refill_boundary
CALLBACK_BRIDGE _uzif_flush_saved, _uz_inflate6502_flush_boundary
CALLBACK_BRIDGE _uzif_finish_saved, _uz_inflate6502_finish_boundary

.proc save_codec_zp
        lda sreg
        sta saved_codec_zp
        lda sreg+1
        sta saved_codec_zp+1
        lda tmp1
        sta saved_codec_zp+2
        lda tmp2
        sta saved_codec_zp+3
        lda ptr1
        sta saved_codec_zp+4
        lda ptr1+1
        sta saved_codec_zp+5
        lda ptr2
        sta saved_codec_zp+6
        lda ptr2+1
        sta saved_codec_zp+7
        lda ptr3
        sta saved_codec_zp+8
        lda ptr3+1
        sta saved_codec_zp+9
        lda ptr4
        sta saved_codec_zp+10
        lda ptr4+1
        sta saved_codec_zp+11
        rts
.endproc

.proc restore_codec_zp
        lda saved_codec_zp
        sta sreg
        lda saved_codec_zp+1
        sta sreg+1
        lda saved_codec_zp+2
        sta tmp1
        lda saved_codec_zp+3
        sta tmp2
        lda saved_codec_zp+4
        sta ptr1
        lda saved_codec_zp+5
        sta ptr1+1
        lda saved_codec_zp+6
        sta ptr2
        lda saved_codec_zp+7
        sta ptr2+1
        lda saved_codec_zp+8
        sta ptr3
        lda saved_codec_zp+9
        sta ptr3+1
        lda saved_codec_zp+10
        sta ptr4
        lda saved_codec_zp+11
        sta ptr4+1
        rts
.endproc

.ifdef UZIP_READYOS_APP
        .segment "INFLATE_BSS"
.else
        .segment "JOB_BSS"
.endif

saved_codec_zp:
        .res 12
boundary_result:
        .res 1
