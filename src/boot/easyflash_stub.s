.setcpu "6502"

.segment "STARTUP"

LOADER_SRC  = $8000
LOADER_DST  = $0800
LOADER_PAGES = $20
src_ptr = $FB
dst_ptr = $FD

start:
    sei
    lda #$00
    sta $DE00

    lda #<LOADER_SRC
    sta src_ptr
    lda #>LOADER_SRC
    sta src_ptr+1
    lda #<LOADER_DST
    sta dst_ptr
    lda #>LOADER_DST
    sta dst_ptr+1

    ldx #LOADER_PAGES
@page_loop:
    ldy #$00
@copy_loop:
    lda (src_ptr),y
    sta (dst_ptr),y
    iny
    bne @copy_loop
    inc src_ptr+1
    inc dst_ptr+1
    dex
    bne @page_loop

    jmp LOADER_DST

.segment "VECTORS"
.word 0
.word start
.word 0
