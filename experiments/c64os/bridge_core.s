; Shared trampoline: full C64 RAM and IDE64's 28KB DOS RAM. No KERNAL calls
; while a context is half-restored. Native wrappers restore displaced low RAM.
.include "experiments/c64os/bridge_abi.inc"
.segment "CODE"
core_start:
        jmp start               ; $0400 entry
        jmp failed              ; $0403 cold-load failure recovery
        rti                     ; $0406 guarded NMI vector
start:
        sei
        jsr setup
        lda BRIDGE_CURRENT
        sta $df06
        lda #$80
        sta $df01
        lda #$30
        sta $01
        sta $ff00
        lda #$35
        sta $01
        lda #0
        sta BRIDGE_DIRECTION
        jsr ide_context
        lda BRIDGE_PROOF
        bne proof
        ; Publish validity only after BOTH RAM images are complete.
        jsr setup
        lda #1
        sta BRIDGE_VALID
        sta $df07
        lda #<BRIDGE_VALID
        sta $df02
        lda #>BRIDGE_VALID
        sta $df03
        lda #BRIDGE_CONTROL_BANK
        sta $df06
        lda #>BRIDGE_CONTROL_OFF
        sta $df05
        lda BRIDGE_CURRENT
        lsr
        sec
        sbc #16
        clc
        adc #<(BRIDGE_CONTROL_OFF+5)
        sta $df04
        lda #$90
        sta $df01
        lda BRIDGE_TARGET
        cmp #$ff
        bne restore
        ldx #0
copy_cold:
        lda cold,x
        sta $cf00,x
        inx
        bne copy_cold
        jmp $cf00
failed:
        ; Publish the KERNAL error outside the image we are about to restore.
        jsr setup
        lda #<BRIDGE_LAST_ERROR
        sta $df02
        lda #>BRIDGE_LAST_ERROR
        sta $df03
        lda #<(BRIDGE_CONTROL_OFF+7)
        sta $df04
        lda #>BRIDGE_CONTROL_OFF
        sta $df05
        lda #BRIDGE_CONTROL_BANK
        sta $df06
        lda #1
        sta $df07
        lda #$90
        sta $df01
        jmp restore
proof: ldx #0
        lda #$a5
 damage:
        sta $c600,x
        sta $c700,x
        sta $c800,x
        sta $c900,x
        sta $0100,x
        inx
        bne damage
        ldx #2
 damage_zp:
        sta $00,x
        inx
        bne damage_zp
restore:
        ; No JSR until the target stack has been restored.
        lda #0
        sta $df02
        sta $df03
        sta $df04
        sta $df05
        sta $df07
        sta $df08
        sta $df09
        sta $df0a
        lda BRIDGE_TARGET
        sta $df06
        lda #$81
        sta $df01
        lda #$30
        sta $01
        sta $ff00
        lda #$35
        sta $01
        ldx BRIDGE_SP
        txs
        lda #1
        sta BRIDGE_DIRECTION
        jsr ide_context
        jmp (BRIDGE_RESUME)
setup: lda #0
        sta $df02
        sta $df03
        sta $df04
        sta $df05
        sta $df07
        sta $df08
        sta $df09
        sta $df0a
        rts
ide_context:
        ; OPEN maps external RAM $1000-$7fff: execute below $1000 and stage
        ; through $0800. Do not assume REU DMA can see cartridge SRAM.
        lda BRIDGE_IDE_ROM
        lsr
        lsr
        and #7
        tax
        sta $de60,x
        sta $defe
        lda #$10
        sta BRIDGE_WORK_PAGE
page:
        lda BRIDGE_WORK_PAGE
        sta load_ide+2
        sta store_ide+2
        lda BRIDGE_DIRECTION
        bne fetch_page
        ldx #0
load_ide:
        lda $1000,x
        sta $0800,x
        inx
        bne load_ide
        jsr page_setup
        lda #$90
        sta $df01
        jmp next_page
fetch_page:
        jsr page_setup
        lda #$91
        sta $df01
        ldx #0
copy_page:
        lda $0800,x
store_ide:
        sta $1000,x
        inx
        bne copy_page
next_page:
        inc BRIDGE_WORK_PAGE
        lda BRIDGE_WORK_PAGE
        cmp #$80
        bne page
        sta $deff
        rts
page_setup:
        jsr setup
        lda #8
        sta $df03
        lda BRIDGE_WORK_PAGE
        sta $df05
        lda BRIDGE_CURRENT
        clc
        adc #1
        sta $df06
        lda #1
        sta $df08
        rts
cold:   .incbin "obj/c64os_cold_boot.bin"
        .assert *-core_start <= $0300, error, "bridge core overlaps context fields"
        .res $0300-(*-core_start), 0
        .byte 32,32
        .word 0
        .byte 0,1,0,0,0,0,0,0
