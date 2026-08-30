; Extraction needs the real Store coordinator, inflater, and ZIP reader, but
; never invokes the compressor MATCH or EMIT overlays. One-byte placeholders
; retain the six-phase uZPK v7 layout without exhausting the diagnostic PRG.

        .segment "DEFLATE_MATCH_CODE"
        rts
        .segment "DEFLATE_MATCH_RODATA"
        .byte $00
        .segment "DEFLATE_EMIT_CODE"
        rts
        .segment "DEFLATE_EMIT_RODATA"
        .byte $00
