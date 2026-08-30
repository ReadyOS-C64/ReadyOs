; Focused transport/ZIP-reader diagnostics do not create compressed data.
; Keep their real job phase, but seed one-byte placeholders for the three
; unused compressor images so the six-phase uZPK v7 descriptor remains
; structurally identical without exhausting the cold package stream.

        .segment "DEFLATE_MATCH_CODE"
        .byte $60
        .segment "DEFLATE_MATCH_RODATA"
        .byte $00
        .segment "DEFLATE_EMIT_CODE"
        .byte $60
        .segment "DEFLATE_EMIT_RODATA"
        .byte $00
        .segment "DEFLATE_COORD_CODE"
        .byte $60
        .segment "DEFLATE_COORD_RODATA"
        .byte $00
        .segment "DEFLATE_COORD_BSS"
        .res 1
