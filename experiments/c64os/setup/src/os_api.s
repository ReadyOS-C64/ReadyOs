; Resolved once by C64 OS initextern. No installed KERNAL addresses are baked in.
        .setcpu "6502"
        .export init_api
        .macro api name, module, offset
        .export name
name:   .byte module, offset, 0
        .endmacro
        .segment "CODE"
init_api:
        ldx #<externs
        ldy #>externs
        jmp $02FC
externs:
        api getsfref, $F2, $15
        api timeque, $EC, $00
        api reuconf, $FE, $18
        api pgfetch, $FE, $1B
        api pgstash, $FE, $1E
        api pgfree, $FE, $0F
        api pgavail, $FE, $06
        api pgalloc, $FE, $15
        api loadlib, $F2, $2A
        api unldlib, $F2, $2D
        api loadutil, $F2, $1E
        api quitapp, $F2, $21
        api markredraw, $F6, $03
        api layerpush, $F6, $06
        api layerpop, $F6, $09
        ; The OS event loop owns dequeueing; the app only reads events.
        api readkcmd, $FC, $12
        api readkprnt, $FC, $18
        api os_initmouse, $FC, $00
        api os_killmouse, $FC, $03
        api os_fopen, $F0, $06
        api os_fread, $F0, $09
        api os_fwrite, $F0, $0C
        api os_fclose, $F0, $0F
        api os_ferror, $F0, $03
        api os_pet2scr, $FA, $0C
        api os_copen, $F0, $1B
        api os_cread, $F0, $1E
        api os_cwrite, $F0, $21
        api os_cclose, $F0, $24
        .byte $FF
