; Self-seeded diagnostics never invoke the extraction image. Keep the normal
; six-phase uZPK v7 header/linker symbols while omitting the real inflater from
; their size-constrained cold diagnostic package.

        .segment "INFLATE_CODE"
        rts

        .segment "INFLATE_RODATA"
        .byte   $00

        .segment "INFLATE_BSS"
        .res    1
