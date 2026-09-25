; Disk-only scalar, array, allocation and slot-0 examples.
; Call/result frame and ZP scratch follow the ReadyBASIC module ABI.
; Private runtime addresses are generated from the matching runtime link.
.setcpu "6502"
.include "sample_runtime.inc"
.segment "CODE"
rb_ptr_lo = $FB
rb_ptr_hi = $FC
RB_CF = $C200
RB_RF = $C300
RB_PAGEBUF = $C500
RB_REU_HEAP_OFF = $0C00
RB_HEAP_PAGES = 192
CF_NUM0_LO = RB_CF+$10
CF_NUM0_HI = RB_CF+$11
CF_NUM1_LO = RB_CF+$12
CF_NUM1_HI = RB_CF+$13
CF_PTR0_LO = RB_CF+$40
CF_PTR0_HI = RB_CF+$41
CF_COUNT0_LO = RB_CF+$42
CF_STR_LEN = RB_CF+$50
CF_STR_BUF = RB_CF+$60
RF_STATUS = RB_RF
RF_ERROR = RB_RF+1
RF_TAG = RB_RF+2
RF_VAL_LO = RB_RF+3
RF_VAL_HI = RB_RF+4
RF_COUNT_LO = RB_RF+5
RF_COUNT_HI = RB_RF+6
RF_ARRAY_BUF = RB_RF+$80
RB_VAL_INT = 1
RB_VAL_ARRAYI = 3

.export cmd_copy_low_end
.export cmd_echo1_low, cmd_add16_low, cmd_hiddenram_hidden, cmd_sumnumarray_low, cmd_rangenumarray_low, cmd_tempscratch_low, cmd_fail_low, cmd_slot0_low, cmd_cpyrst_low, cmd_copy_low

cmd_echo1_low:
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        sta RF_VAL_LO
        rts
cmd_echo1_low_end:

cmd_add16_low:
        clc
        lda CF_NUM0_LO
        adc CF_NUM1_LO
        sta RF_VAL_LO
        lda CF_NUM0_HI
        adc CF_NUM1_HI
        sta RF_VAL_HI
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        rts
cmd_add16_low_end:

cmd_hiddenram_hidden:
        lda #0
        sta RF_VAL_LO
        sta RF_VAL_HI
        ldy #0
@loop:
        cpy CF_STR_LEN
        beq @done
        lda RF_VAL_LO
        sta rb_ptr_lo
        lda CF_STR_BUF,y
        cmp #$C1
        bcc @ascii_case
        cmp #$DB
        bcs @ascii_case
        sec
        sbc #$80
        jmp @sum
@ascii_case:
        cmp #'a'
        bcc @sum
        cmp #'z' + 1
        bcs @sum
        sec
        sbc #$20
@sum:
        clc
        adc rb_ptr_lo
        sta RF_VAL_LO
        lda RF_VAL_HI
        adc #0
        sta RF_VAL_HI
        iny
        jmp @loop
@done:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        rts
cmd_hiddenram_hidden_end:

cmd_sumnumarray_low:
        lda CF_PTR0_LO
        sta rb_ptr_lo
        lda CF_PTR0_HI
        sta rb_ptr_hi
        lda #0
        sta RF_VAL_LO
        sta RF_VAL_HI
        ldx CF_COUNT0_LO
        beq @done
@loop:
        ldy #1
        clc
        lda RF_VAL_LO
        adc (rb_ptr_lo),y
        sta RF_VAL_LO
        dey
        lda RF_VAL_HI
        adc (rb_ptr_lo),y
        sta RF_VAL_HI
        clc
        lda rb_ptr_lo
        adc #2
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       dex
        bne @loop
@done:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        rts
cmd_sumnumarray_low_end:

cmd_rangenumarray_low:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_ARRAYI
        sta RF_TAG
        lda CF_NUM1_LO
        sta RF_COUNT_LO
        lda CF_NUM1_HI
        sta RF_COUNT_HI
        lda CF_NUM0_LO
        sta rb_ptr_lo
        lda CF_NUM0_HI
        sta rb_ptr_hi
        ldx CF_NUM1_LO
        beq @done
        ldy #0
@loop:
        lda rb_ptr_hi
        sta RF_ARRAY_BUF,y
        iny
        lda rb_ptr_lo
        sta RF_ARRAY_BUF,y
        iny
        inc rb_ptr_lo
        bne :+
        inc rb_ptr_hi
:       dex
        bne @loop
@done:
        rts
cmd_rangenumarray_low_end:

cmd_tempscratch_low:
        jsr rb_temp_alloc
        rts
cmd_tempscratch_low_end:

cmd_fail_low:
        lda CF_NUM0_LO
        bne :+
        lda #$7F
:       sta RF_STATUS
        sta RF_ERROR
        lda #RB_VAL_INT
        sta RF_TAG
        lda #0
        sta RF_VAL_LO
        sta RF_VAL_HI
        rts
cmd_fail_low_end:

cmd_slot0_low:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #30
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_slot0_low_end:

cmd_cpyrst_low:
        lda #0
        sta rb_copy_count
        sta RF_STATUS
        sta RF_VAL_LO
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        rts
cmd_cpyrst_low_end:

cmd_copy_low:
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda rb_copy_count
        sta RF_VAL_LO
        lda #RB_VAL_INT
        sta RF_TAG
        rts
cmd_copy_low_end:

rb_temp_alloc:
        jsr rb_len_to_pages
        bcs @bad
        jsr rb_find_pages
        bcs @bad
        jsr rb_mark_pages_used
        jsr rb_mark_pages_free
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda rb_needed_pages
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
@bad:
        lda #$26
        jmp rb_overlay_fail


rb_len_to_pages:
        lda CF_NUM0_LO
        ora CF_NUM0_HI
        beq @bad
        lda CF_NUM0_HI
        sta rb_needed_pages
        lda CF_NUM0_LO
        beq @check
        inc rb_needed_pages
@check:
        lda rb_needed_pages
        beq @bad
        cmp #RB_HEAP_PAGES + 1
        bcs @bad
        clc
        rts
@bad:
        sec
        rts


rb_find_pages:
        jsr rb_fetch_heap_bitmap
        lda #0
        sta rb_found_page
@outer:
        lda rb_found_page
        clc
        adc rb_needed_pages
        cmp #RB_HEAP_PAGES + 1
        bcc :+
        sec
        rts
:       ldx #0
@inner:
        txa
        clc
        adc rb_found_page
        tay
        lda RB_PAGEBUF,y
        bne @next_start
        inx
        cpx rb_needed_pages
        bcc @inner
        clc
        rts
@next_start:
        inc rb_found_page
        jmp @outer


rb_mark_pages_used:
        ldx #0
@loop:
        txa
        clc
        adc rb_found_page
        tay
        lda #1
        sta RB_PAGEBUF,y
        inx
        cpx rb_needed_pages
        bcc @loop
        rts


rb_mark_pages_free:
        ldx #0
@loop:
        txa
        clc
        adc rb_found_page
        tay
        lda #0
        sta RB_PAGEBUF,y
        inx
        cpx rb_needed_pages
        bcc @loop
        rts


rb_fetch_heap_bitmap:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #<RB_REU_HEAP_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_HEAP_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        rts


rb_overlay_fail:
        sta RF_STATUS
        sta RF_ERROR
        rts
rb_needed_pages: .byte 0
rb_found_page: .byte 0
