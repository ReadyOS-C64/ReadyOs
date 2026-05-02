.setcpu "6502"

.segment "STARTUP"

CART_BANK    = $DE00
CART_CONTROL_EX = $DE01
CART_CONTROL = $DE02
KERNAL_CINT  = $FF81
KERNAL_IOINIT = $FF84
KERNAL_RESTOR = $FF8A

REU_COMMAND  = $DF01
REU_STATUS   = $DF00
REU_C64_LO   = $DF02
REU_C64_HI   = $DF03
REU_REU_LO   = $DF04
REU_REU_HI   = $DF05
REU_REU_BANK = $DF06
REU_LEN_LO   = $DF07
REU_LEN_HI   = $DF08

REU_CMD_STASH = $90
REU_CMD_FETCH = $91

APP_LOAD_START = $1000
APP_WINDOW_LEN = $B600
SHIM_STORAGE_DRIVE = $C839
REU_MAGIC_VALUE = $A5
DBG_RING_BASE = $C7A0
DBG_RING_HEAD = $C7DF
BOOTDBG0     = $C7E0
BOOTDBG1     = $C7E1
BOOTDBG2     = $C7E2
BOOTDBG3     = $C7E3
BOOTDBG4     = $C7E4
BOOTDBG5     = $C7E5
BOOTDBG6     = $C7E6
BOOTDBG7     = $C7E7
HANDOFF_RAM = $0800
VIC_SPR_EN   = $D015
VIC_IRQ_ACK  = $D019
VIC_IRQ_EN   = $D01A
BOOT_VERIFY_BANK = $04
BOOT_SRC_BUF  = $B000
BOOT_DST_BUF  = $B100

dst_lo       = $F0
dst_hi       = $F1
rem_lo       = $F2
rem_hi       = $F3
src_lo       = $F4
src_hi       = $F5
chunk_lo     = $F6
chunk_hi     = $F7
cart_bank_zp = $F8
reu_bank_zp  = $F9
reu_off_lo   = $FA
reu_off_hi   = $FB
pages_left   = $FC

start:
    sei
    cld
    ldx #$FF
    txs
    lda #$37
    sta $01
    lda #$2F
    sta $00
    lda #$00
    sta CART_BANK
    lda #$40
    sta CART_CONTROL_EX
    lda #$07
    sta CART_CONTROL

    jsr init_reu_state
    lda #'S'
    jsr dbg_put
    jsr copy_shim
    lda #'H'
    jsr dbg_put
    jsr preload_host_bank0
    lda #'0'
    jsr dbg_put
    jsr preload_payload_bank1
    lda #'1'
    jsr dbg_put
    jsr preload_data_bank2
    lda #'2'
    jsr dbg_put
    jsr verify_boot_reu_roundtrip
    lda #$04
    sta CART_CONTROL
    lda #$37
    sta $01
    lda #$2F
    sta $00
    jsr KERNAL_IOINIT
    jsr KERNAL_RESTOR
    jsr KERNAL_CINT
    lda #'T'
    jsr dbg_put
    lda #$07
    sta CART_CONTROL
    jsr restore_host_ram
    lda #'J'
    jsr dbg_put
    jsr install_handoff_stub
    lda #'K'
    jsr dbg_put
    jmp HANDOFF_RAM

dbg_put:
    ldx DBG_RING_HEAD
    sta DBG_RING_BASE,x
    inx
    cpx #$3F
    bcc :+
    ldx #$00
:
    stx DBG_RING_HEAD
    rts

install_handoff_stub:
    ldx #$00
@copy:
    lda handoff_stub,x
    sta HANDOFF_RAM,x
    inx
    cpx #handoff_stub_end-handoff_stub
    bcc @copy
    rts

copy_shim:
    ldx #$00
@loop:
    lda shim_data,x
    sta $C800,x
    lda shim_data+$0100,x
    sta $C900,x
    inx
    bne @loop
    rts

init_reu_state:
    ldx #$00
    lda #$00
@clear:
    sta $C600,x
    sta $C700,x
    inx
    bne @clear
    lda #REU_MAGIC_VALUE
    sta $C700
    lda #$08
    sta SHIM_STORAGE_DRIVE
    rts

preload_host_bank0:
    jsr clear_app_window
    lda #XEFPROBE_HOST_CART_BANK
    sta cart_bank_zp
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<XEFPROBE_HOST_PAYLOAD_LEN
    sta rem_lo
    lda #>XEFPROBE_HOST_PAYLOAD_LEN
    sta rem_hi
    jsr copy_payload_from_cart
    lda #$00
    sta reu_bank_zp
    lda #$00
    sta reu_off_lo
    sta reu_off_hi
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<APP_WINDOW_LEN
    sta rem_lo
    lda #>APP_WINDOW_LEN
    sta rem_hi
    jsr reu_stash
    rts

preload_payload_bank1:
    jsr clear_app_window
    lda #XEFPROBE_PAYLOAD_CART_BANK
    sta cart_bank_zp
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<XEFPROBE_PAYLOAD_LEN
    sta rem_lo
    lda #>XEFPROBE_PAYLOAD_LEN
    sta rem_hi
    jsr copy_payload_from_cart
    lda #$01
    sta reu_bank_zp
    lda #$00
    sta reu_off_lo
    sta reu_off_hi
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<APP_WINDOW_LEN
    sta rem_lo
    lda #>APP_WINDOW_LEN
    sta rem_hi
    jsr reu_stash
    lda #$02
    sta $C836
    rts

preload_data_bank2:
    jsr clear_app_window
    lda #XEFPROBE_DATA_CART_BANK
    sta cart_bank_zp
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<XEFPROBE_DATA_LEN
    sta rem_lo
    lda #>XEFPROBE_DATA_LEN
    sta rem_hi
    jsr copy_payload_from_cart
    lda #$02
    sta reu_bank_zp
    lda #$00
    sta reu_off_lo
    sta reu_off_hi
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<XEFPROBE_DATA_LEN
    sta rem_lo
    lda #>XEFPROBE_DATA_LEN
    sta rem_hi
    jsr reu_stash
    rts

restore_host_ram:
    jsr clear_app_window
    lda #XEFPROBE_HOST_CART_BANK
    sta cart_bank_zp
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<XEFPROBE_HOST_PAYLOAD_LEN
    sta rem_lo
    lda #>XEFPROBE_HOST_PAYLOAD_LEN
    sta rem_hi
    jsr copy_payload_from_cart
    rts

clear_app_window:
    lda #<APP_LOAD_START
    sta dst_lo
    lda #>APP_LOAD_START
    sta dst_hi
    lda #<APP_WINDOW_LEN
    sta rem_lo
    lda #>APP_WINDOW_LEN
    sta rem_hi
    jmp clear_region

clear_region:
    lda rem_lo
    ora rem_hi
    beq @done
@page_loop:
    lda rem_hi
    beq @tail
    ldy #$00
    lda #$00
@copy_page:
    sta (dst_lo),y
    iny
    bne @copy_page
    inc dst_hi
    dec rem_hi
    jmp @page_loop
@tail:
    ldy #$00
    lda #$00
@copy_tail:
    cpy rem_lo
    beq @done
    sta (dst_lo),y
    iny
    bne @copy_tail
@done:
    rts

copy_payload_from_cart:
@bank_loop:
    lda rem_lo
    ora rem_hi
    beq @done
    lda cart_bank_zp
    sta CART_BANK
    lda #<$8000
    sta src_lo
    lda #>$8000
    sta src_hi
    jsr copy_cart_half
    lda rem_lo
    ora rem_hi
    beq @advance_bank
    lda #<$A000
    sta src_lo
    lda #>$A000
    sta src_hi
    jsr copy_cart_half
@advance_bank:
    inc cart_bank_zp
    jmp @bank_loop
@done:
    rts

copy_cart_half:
    lda rem_hi
    cmp #$20
    bcc @short_chunk
    bne @full_chunk
    lda rem_lo
    beq @full_chunk
@full_chunk:
    lda #$00
    sta chunk_lo
    lda #$20
    sta chunk_hi
    jmp copy_chunk
@short_chunk:
    lda rem_lo
    sta chunk_lo
    lda rem_hi
    sta chunk_hi
    jmp copy_chunk

copy_chunk:
    lda chunk_hi
    sta pages_left
@page_copy:
    lda pages_left
    beq @tail_copy
    ldy #$00
@page_bytes:
    lda (src_lo),y
    sta (dst_lo),y
    iny
    bne @page_bytes
    inc src_hi
    inc dst_hi
    dec pages_left
    jmp @page_copy
@tail_copy:
    ldy #$00
@tail_bytes:
    cpy chunk_lo
    beq @advance
    lda (src_lo),y
    sta (dst_lo),y
    iny
    bne @tail_bytes
@advance:
    clc
    lda dst_lo
    adc chunk_lo
    sta dst_lo
    bcc :+
    inc dst_hi
:
    clc
    lda src_lo
    adc chunk_lo
    sta src_lo
    bcc :+
    inc src_hi
:
    sec
    lda rem_lo
    sbc chunk_lo
    sta rem_lo
    lda rem_hi
    sbc chunk_hi
    sta rem_hi
    rts

reu_stash:
    lda dst_lo
    sta REU_C64_LO
    lda dst_hi
    sta REU_C64_HI
    lda reu_off_lo
    sta REU_REU_LO
    lda reu_off_hi
    sta REU_REU_HI
    lda reu_bank_zp
    sta REU_REU_BANK
    lda rem_lo
    sta REU_LEN_LO
    lda rem_hi
    sta REU_LEN_HI
    lda #REU_CMD_STASH
    sta REU_COMMAND
    rts

reu_fetch:
    lda dst_lo
    sta REU_C64_LO
    lda dst_hi
    sta REU_C64_HI
    lda reu_off_lo
    sta REU_REU_LO
    lda reu_off_hi
    sta REU_REU_HI
    lda reu_bank_zp
    sta REU_REU_BANK
    lda rem_lo
    sta REU_LEN_LO
    lda rem_hi
    sta REU_LEN_HI
    lda #REU_CMD_FETCH
    sta REU_COMMAND
    rts

verify_boot_reu_roundtrip:
    lda #'B'
    jsr dbg_put
    lda #$00
    sta BOOTDBG0
    sta BOOTDBG1
    sta BOOTDBG2
    sta BOOTDBG3
    sta BOOTDBG4
    sta BOOTDBG5
    sta BOOTDBG6
    sta BOOTDBG7

    lda #$5A
    sta REU_REU_BANK
    lda REU_REU_BANK
    sta BOOTDBG0
    lda REU_STATUS
    sta BOOTDBG1

    ldx #$00
    lda #$33
    sta chunk_lo
@fill:
    lda chunk_lo
    sta BOOT_SRC_BUF,x
    lda #$00
    sta BOOT_DST_BUF,x
    clc
    lda chunk_lo
    adc #$05
    sta chunk_lo
    inx
    bne @fill

    lda #BOOT_VERIFY_BANK
    sta reu_bank_zp
    lda #$00
    sta reu_off_lo
    sta reu_off_hi
    lda #<BOOT_SRC_BUF
    sta dst_lo
    lda #>BOOT_SRC_BUF
    sta dst_hi
    lda #$00
    sta rem_lo
    lda #$01
    sta rem_hi
    jsr reu_stash
    lda REU_REU_BANK
    sta BOOTDBG2
    lda REU_STATUS
    sta BOOTDBG3

    lda #<BOOT_DST_BUF
    sta dst_lo
    lda #>BOOT_DST_BUF
    sta dst_hi
    lda #$00
    sta rem_lo
    lda #$01
    sta rem_hi
    jsr reu_fetch
    lda REU_REU_BANK
    sta BOOTDBG4
    lda REU_STATUS
    sta BOOTDBG5

    ldx #$00
    lda #$33
    sta chunk_lo
@verify:
    lda BOOT_DST_BUF,x
    cmp chunk_lo
    bne @fail
    clc
    lda chunk_lo
    adc #$05
    sta chunk_lo
    inx
    bne @verify
    lda #'b'
    jsr dbg_put
    lda #$01
    sta BOOTDBG6
    rts

@fail:
    lda BOOT_DST_BUF,x
    sta BOOTDBG6
    lda chunk_lo
    sta BOOTDBG7
    lda #'X'
    jsr dbg_put
    rts

.segment "RODATA"

.include "../generated/xefprobe_layout.inc"

shim_data:
    .incbin "../../bin/easyflash_shim.bin"

handoff_stub:
    lda #'G'
    sta DBG_RING_BASE+8
    lda #$09
    sta DBG_RING_HEAD
    lda #$04
    sta CART_CONTROL
    lda #'H'
    sta DBG_RING_BASE+9
    lda #$0A
    sta DBG_RING_HEAD
    lda #$37
    sta $01
    lda #'I'
    sta DBG_RING_BASE+10
    lda #$0B
    sta DBG_RING_HEAD
    lda #$2F
    sta $00
    lda #'J'
    sta DBG_RING_BASE+11
    lda #$0C
    sta DBG_RING_HEAD
    lda #$00
    sta VIC_IRQ_EN
    sta VIC_SPR_EN
    lda #$FF
    sta VIC_IRQ_ACK
    lda #'L'
    sta DBG_RING_BASE+12
    lda #$0D
    sta DBG_RING_HEAD
    sei
    jmp $1000
    lda #'!'
    sta DBG_RING_BASE+13
    lda #$0E
    sta DBG_RING_HEAD
@spin:
    jmp @spin
handoff_stub_end:
