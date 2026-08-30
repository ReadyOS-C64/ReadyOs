; Focused REU, compressor, and archive-creation diagnostics never parse an
; input ZIP. Preserve the uZPK v7 six-phase descriptor without carrying the
; production reader in their already-full cold package stream.

        .segment "ZIP_READ_CODE"
        .byte $60
