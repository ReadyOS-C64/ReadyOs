
        .segment "LOADADDR"
        .word $0801

        .segment "STARTUP"
        .word basic_next
        .word 10
        .byte $9e, "2061", 0
basic_next:
        .word 0

        .import _launcher_uci_dma_detect
        .import _launcher_uci_dma_load_prg
        .import _launcher_uci_dma_quiesce
        .import _launcher_uci_dma_clear_stage
        .import _launcher_uci_dma_name
        .import _launcher_uci_dma_reu_bank
        .import _launcher_uci_dma_reu_offset
        .import _launcher_uci_dma_max_len
        .import _launcher_uci_dma_expected_load_addr
        .import _launcher_uci_dma_loaded_size
        .import _launcher_uci_dma_last_error
        .import _launcher_uci_dma_dbg_stat0
        .import _launcher_uci_dma_dbg_stat1
        .import _launcher_uci_dma_trace
        .import _launcher_uci_dma_fail_trace
        .import _launcher_uci_dma_image_dir
        .import _launcher_uci_dma_image_name
        .import _launcher_uci_dma_mount_name
        .import _launcher_uci_dma_assume_mounted

        .export _utime_stage_begin
        .export _utime_stage_end

CHROUT      = $FFD2
GETIN       = $FFE4
RESULTS     = $3000
STAGE_COUNT = $0D
current_name = $04

        .macro PRINT label
        lda #<label
        ldy #>label
        jsr print_z
        .endmacro

        .segment "CODE"

start:
        lda #$93
        jsr CHROUT
        lda #$01
        sta $0286
        jsr init_results
        PRINT title_msg
        jsr cr
        PRINT path_msg
        PRINT image_dir
        lda #'/'
        jsr CHROUT
        PRINT image_name
        jsr cr
        PRINT start_prompt_msg
wait_start_key:
        jsr GETIN
        beq wait_start_key
        PRINT running_msg
        jsr cr
        jsr _launcher_uci_dma_detect
        cmp #$00
        bne have_uci
        lda #$02
        sta done_code
        lda #WORKLOAD_COUNT
        sta failures
        jsr update_runtime_results
        PRINT no_uci_msg
        jsr cr
        jmp done
have_uci:
        PRINT uci_msg
        jsr cr
        jsr workload_run
        lda failures
        beq all_ok
        lda #$04
        bne set_done
all_ok:
        lda #$01
set_done:
        sta done_code
        jsr update_runtime_results
done:
        jsr print_summary
        jmp *

load_one_launcher_timed:
        jsr print_progress
        lda #$31
        sta $052d

        lda #<image_dir
        sta _launcher_uci_dma_image_dir
        lda #>image_dir
        sta _launcher_uci_dma_image_dir+1
        lda #<image_name
        sta _launcher_uci_dma_image_name
        sta _launcher_uci_dma_mount_name
        lda #>image_name
        sta _launcher_uci_dma_image_name+1
        sta _launcher_uci_dma_mount_name+1
        lda current_name
        sta _launcher_uci_dma_name
        lda current_name+1
        sta _launcher_uci_dma_name+1
        lda current_bank
        sta _launcher_uci_dma_reu_bank
        lda current_off
        sta _launcher_uci_dma_reu_offset
        lda current_off+1
        sta _launcher_uci_dma_reu_offset+1
        lda current_max
        sta _launcher_uci_dma_max_len
        lda current_max+1
        sta _launcher_uci_dma_max_len+1
        lda current_expected
        sta _launcher_uci_dma_expected_load_addr
        lda current_expected+1
        sta _launcher_uci_dma_expected_load_addr+1
        lda image_ready
        sta _launcher_uci_dma_assume_mounted

        jsr read_jiffy16
        sta tick_start
        stx tick_start+1
        jsr _launcher_uci_dma_load_prg
        sta last_ok
        tax
        beq load_failed_quiesce
        lda #$01
        sta image_ready
        sta _launcher_uci_dma_assume_mounted
        jsr _launcher_uci_dma_quiesce
        jsr _launcher_uci_dma_clear_stage
        jmp after_loader_call
load_failed_quiesce:
        jsr apply_launcher_failure_state
        jsr _launcher_uci_dma_quiesce
after_loader_call:
        jsr read_jiffy16
        sec
        sbc tick_start
        sta last_ticks
        txa
        sbc tick_start+1
        sta last_ticks+1
        jsr add_total_ticks
        jsr add_phase_ticks
        jsr update_max_load
        lda last_ok
        bne load_succeeded
        jsr store_first_failure
        clc
        rts
load_succeeded:
        inc successes
        jsr add_loaded_kb
        sec
        rts

apply_launcher_failure_state:
        lda _launcher_uci_dma_last_error
        cmp #$01
        beq fail_disable_path
        cmp #$08
        bcc fail_keep_path
        cmp #$11
        bcs fail_keep_path
fail_disable_path:
        lda #$00
        sta image_ready
        sta _launcher_uci_dma_assume_mounted
        rts
fail_keep_path:
        lda #$01
        sta image_ready
        sta _launcher_uci_dma_assume_mounted
        rts

add_total_ticks:
        lda total_ticks
        clc
        adc last_ticks
        sta total_ticks
        lda total_ticks+1
        adc last_ticks+1
        sta total_ticks+1
        rts

add_phase_ticks:
        lda item_index
        beq add_first_ticks
        lda subsequent_ticks
        clc
        adc last_ticks
        sta subsequent_ticks
        lda subsequent_ticks+1
        adc last_ticks+1
        sta subsequent_ticks+1
        rts
add_first_ticks:
        lda first_ticks
        clc
        adc last_ticks
        sta first_ticks
        lda first_ticks+1
        adc last_ticks+1
        sta first_ticks+1
        rts

update_max_load:
        lda max_load_ticks+1
        cmp last_ticks+1
        bcc set_max_load
        bne max_load_done
        lda max_load_ticks
        cmp last_ticks
        bcs max_load_done
set_max_load:
        lda last_ticks
        sta max_load_ticks
        lda last_ticks+1
        sta max_load_ticks+1
        ldy #$00
copy_max_name:
        cpy #$0f
        beq term_max_name
        lda (current_name),y
        sta max_load_name,y
        beq max_load_done
        iny
        bne copy_max_name
term_max_name:
        lda #$00
        sta max_load_name,y
max_load_done:
        rts

add_loaded_kb:
        lda _launcher_uci_dma_loaded_size
        clc
        adc #$ff
        lda _launcher_uci_dma_loaded_size+1
        adc #$03
        lsr
        lsr
        clc
        adc total_kb
        sta total_kb
        lda total_kb+1
        adc #$00
        sta total_kb+1
        rts

store_first_failure:
        lda first_fail_error
        bne first_failure_done
        lda item_index
        sta first_fail_item
        lda _launcher_uci_dma_last_error
        sta first_fail_error
        lda _launcher_uci_dma_dbg_stat0
        sta first_fail_dbg0
        lda _launcher_uci_dma_dbg_stat1
        sta first_fail_dbg1
        lda _launcher_uci_dma_fail_trace
        sta first_fail_trace
        lda _launcher_uci_dma_fail_trace+1
        sta first_fail_trace+1
        lda _launcher_uci_dma_fail_trace+2
        sta first_fail_trace+2
        lda _launcher_uci_dma_fail_trace+3
        sta first_fail_trace+3
        ldy #$00
copy_fail_name:
        cpy #$0f
        beq term_fail_name
        lda (current_name),y
        sta first_fail_name,y
        beq first_failure_done
        iny
        bne copy_fail_name
term_fail_name:
        lda #$00
        sta first_fail_name,y
first_failure_done:
        rts

read_jiffy16:
        lda $A2
        ldx $A1
        rts

_utime_stage_begin:
        sta stage_id
        jsr read_jiffy16
        sta stage_start
        stx stage_start+1
        rts

_utime_stage_end:
        jsr read_jiffy16
        sta stage_now
        stx stage_now+1
        lda stage_now
        sec
        sbc stage_start
        sta stage_delta
        lda stage_now+1
        sbc stage_start+1
        sta stage_delta+1
        lda stage_id
        asl
        tax
        lda stage_ticks,x
        clc
        adc stage_delta
        sta stage_ticks,x
        lda stage_ticks+1,x
        adc stage_delta+1
        sta stage_ticks+1,x
        lda item_index
        beq stage_end_done
        lda stage_id
        asl
        tax
        lda stage_subseq_ticks,x
        clc
        adc stage_delta
        sta stage_subseq_ticks,x
        lda stage_subseq_ticks+1,x
        adc stage_delta+1
        sta stage_subseq_ticks+1,x
stage_end_done:
        rts

init_results:
        ldx #$00
        lda #$00
clear_results:
        sta RESULTS,x
        inx
        cpx #$80
        bne clear_results
        lda #'U'
        sta RESULTS
        lda #'T'
        sta RESULTS+1
        lda #'I'
        sta RESULTS+2
        lda #'M'
        sta RESULTS+3
        lda #$04
        sta RESULTS+4
        lda #WORKLOAD_COUNT
        sta RESULTS+6
        jsr update_runtime_results
        rts

update_runtime_results:
        lda #$04
        sta RESULTS+4
        lda done_code
        sta RESULTS+5
        lda item_index
        sta RESULTS+6
        lda failures
        sta RESULTS+7
        lda total_ticks
        sta RESULTS+8
        lda total_ticks+1
        sta RESULTS+9
        lda total_kb
        sta RESULTS+10
        lda total_kb+1
        sta RESULTS+11
        lda max_load_ticks
        sta RESULTS+12
        lda max_load_ticks+1
        sta RESULTS+13
        lda successes
        sta RESULTS+14
        lda #STAGE_COUNT
        sta RESULTS+15
        lda first_ticks
        sta RESULTS+16
        lda first_ticks+1
        sta RESULTS+17
        lda subsequent_ticks
        sta RESULTS+18
        lda subsequent_ticks+1
        sta RESULTS+19
        ldx #$00
copy_stages_result:
        lda stage_ticks,x
        sta RESULTS+20,x
        inx
        cpx #(STAGE_COUNT * 2)
        bne copy_stages_result
        ldx #$00
copy_subseq_stages_result:
        lda stage_subseq_ticks,x
        sta RESULTS+80,x
        inx
        cpx #(STAGE_COUNT * 2)
        bne copy_subseq_stages_result
        ldx #$00
copy_max_result:
        lda max_load_name,x
        sta RESULTS+48,x
        inx
        cpx #$10
        bne copy_max_result
        lda first_fail_item
        sta RESULTS+64
        lda first_fail_error
        sta RESULTS+65
        lda first_fail_dbg0
        sta RESULTS+66
        lda first_fail_dbg1
        sta RESULTS+67
        ldx #$00
copy_first_trace:
        lda first_fail_trace,x
        sta RESULTS+68,x
        inx
        cpx #$04
        bne copy_first_trace
        ldx #$00
copy_fail_result:
        lda first_fail_name,x
        sta RESULTS+72,x
        inx
        cpx #$08
        bne copy_fail_result
        rts

print_progress:
        lda #$13
        jsr CHROUT
        PRINT loading_msg
        lda item_index
        jsr print_hex_a
        lda #'/'
        jsr CHROUT
        lda #WORKLOAD_COUNT
        jsr print_hex_a
        lda #' '
        jsr CHROUT
        ldy #$00
progress_name:
        lda (current_name),y
        beq progress_done
        jsr CHROUT
        iny
        cpy #$0f
        bne progress_name
progress_done:
        jsr cr
        rts

print_summary:
        lda #$13
        jsr CHROUT
        PRINT done_msg
        lda done_code
        jsr print_hex_a
        PRINT item_msg
        lda item_index
        jsr print_hex_a
        PRINT fail_msg
        lda failures
        jsr print_hex_a
        jsr cr
        PRINT succ_msg
        lda successes
        jsr print_hex_a
        PRINT kb_msg
        lda total_kb+1
        jsr print_hex_a
        lda total_kb
        jsr print_hex_a
        PRINT ticks_msg
        lda total_ticks+1
        jsr print_hex_a
        lda total_ticks
        jsr print_hex_a
        jsr cr
        PRINT max_msg
        lda max_load_ticks+1
        jsr print_hex_a
        lda max_load_ticks
        jsr print_hex_a
        lda #' '
        jsr CHROUT
        lda #<max_load_name
        ldy #>max_load_name
        jsr print_z
        jsr cr
        lda failures
        beq no_fail_print
        PRINT first_fail_msg
        lda first_fail_item
        jsr print_hex_a
        lda #'/'
        jsr CHROUT
        lda first_fail_error
        jsr print_hex_a
        lda #' '
        jsr CHROUT
        lda #<first_fail_name
        ldy #>first_fail_name
        jsr print_z
        jsr cr
no_fail_print:
        PRINT probe_done_msg
        jsr cr
        rts

print_z:
        sta pz+1
        sty pz+2
        ldy #$00
pz:
        lda $FFFF,y
        beq print_z_done
        jsr CHROUT
        iny
        bne pz
print_z_done:
        rts

print_hex_a:
        pha
        lsr
        lsr
        lsr
        lsr
        jsr print_hex_nibble
        pla
        and #$0f
print_hex_nibble:
        cmp #$0a
        bcc print_hex_digit
        adc #$06
print_hex_digit:
        adc #'0'
        jsr CHROUT
        rts

cr:
        lda #$0d
        jsr CHROUT
        rts

        .segment "RODATA"
workload_run:

        lda #<name_editor
        sta current_name
        lda #>name_editor
        sta current_name+1
        lda #$40
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_0_ok
        inc failures
@item_0_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_readyshell
        sta current_name
        lda #>name_readyshell
        sta current_name+1
        lda #$41
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_1_ok
        inc failures
@item_1_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rsparser
        sta current_name
        lda #>name_rsparser
        sta current_name+1
        lda #$80
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_2_ok
        inc failures
@item_2_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rsvm
        sta current_name
        lda #>name_rsvm
        sta current_name+1
        lda #$80
        sta current_bank
        lda #<$3800
        sta current_off
        lda #>$3800
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_3_ok
        inc failures
@item_3_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rsdrvilst
        sta current_name
        lda #>name_rsdrvilst
        sta current_name+1
        lda #$80
        sta current_bank
        lda #<$7000
        sta current_off
        lda #>$7000
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_4_ok
        inc failures
@item_4_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rsldv
        sta current_name
        lda #>name_rsldv
        sta current_name+1
        lda #$81
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_5_ok
        inc failures
@item_5_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rsstv
        sta current_name
        lda #>name_rsstv
        sta current_name+1
        lda #$80
        sta current_bank
        lda #<$A800
        sta current_off
        lda #>$A800
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_6_ok
        inc failures
@item_6_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rsfops
        sta current_name
        lda #>name_rsfops
        sta current_name+1
        lda #$81
        sta current_bank
        lda #<$3800
        sta current_off
        lda #>$3800
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_7_ok
        inc failures
@item_7_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rscat
        sta current_name
        lda #>name_rscat
        sta current_name+1
        lda #$81
        sta current_bank
        lda #<$7000
        sta current_off
        lda #>$7000
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_8_ok
        inc failures
@item_8_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rscopy
        sta current_name
        lda #>name_rscopy
        sta current_name+1
        lda #$81
        sta current_bank
        lda #<$A800
        sta current_off
        lda #>$A800
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_9_ok
        inc failures
@item_9_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rsedit
        sta current_name
        lda #>name_rsedit
        sta current_name+1
        lda #$82
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$3800
        sta current_max
        lda #>$3800
        sta current_max+1
        lda #<$8E00
        sta current_expected
        lda #>$8E00
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_10_ok
        inc failures
@item_10_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_simplefiles
        sta current_name
        lda #>name_simplefiles
        sta current_name+1
        lda #$42
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_11_ok
        inc failures
@item_11_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_clipmgr
        sta current_name
        lda #>name_clipmgr
        sta current_name+1
        lda #$43
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_12_ok
        inc failures
@item_12_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_readybasic
        sta current_name
        lda #>name_readybasic
        sta current_name+1
        lda #$44
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_13_ok
        inc failures
@item_13_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_cal26
        sta current_name
        lda #>name_cal26
        sta current_name+1
        lda #$45
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_14_ok
        inc failures
@item_14_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_tasklist
        sta current_name
        lda #>name_tasklist
        sta current_name+1
        lda #$46
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_15_ok
        inc failures
@item_15_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_reuviewer
        sta current_name
        lda #>name_reuviewer
        sta current_name+1
        lda #$47
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_16_ok
        inc failures
@item_16_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_sysinfo
        sta current_name
        lda #>name_sysinfo
        sta current_name+1
        lda #$48
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_17_ok
        inc failures
@item_17_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_quicknotes
        sta current_name
        lda #>name_quicknotes
        sta current_name+1
        lda #$49
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_18_ok
        inc failures
@item_18_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_calcplus
        sta current_name
        lda #>name_calcplus
        sta current_name+1
        lda #$4A
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_19_ok
        inc failures
@item_19_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_hexview
        sta current_name
        lda #>name_hexview
        sta current_name+1
        lda #$4B
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_20_ok
        inc failures
@item_20_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_simplecells
        sta current_name
        lda #>name_simplecells
        sta current_name+1
        lda #$4C
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_21_ok
        inc failures
@item_21_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_game2048
        sta current_name
        lda #>name_game2048
        sta current_name+1
        lda #$4D
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_22_ok
        inc failures
@item_22_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_deminer
        sta current_name
        lda #>name_deminer
        sta current_name+1
        lda #$4E
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_23_ok
        inc failures
@item_23_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_dizzy
        sta current_name
        lda #>name_dizzy
        sta current_name+1
        lda #$4F
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_24_ok
        inc failures
@item_24_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_readyirc
        sta current_name
        lda #>name_readyirc
        sta current_name+1
        lda #$50
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_25_ok
        inc failures
@item_25_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_ucitest
        sta current_name
        lda #>name_ucitest
        sta current_name+1
        lda #$51
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_26_ok
        inc failures
@item_26_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_rirc_rrnet
        sta current_name
        lda #>name_rirc_rrnet
        sta current_name+1
        lda #$52
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_27_ok
        inc failures
@item_27_ok:
        inc item_index
        jsr update_runtime_results


        lda #<name_readme
        sta current_name
        lda #>name_readme
        sta current_name+1
        lda #$53
        sta current_bank
        lda #<$0000
        sta current_off
        lda #>$0000
        sta current_off+1
        lda #<$B600
        sta current_max
        lda #>$B600
        sta current_max+1
        lda #<$1000
        sta current_expected
        lda #>$1000
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_28_ok
        inc failures
@item_28_ok:
        inc item_index
        jsr update_runtime_results

        rts

WORKLOAD_COUNT = 29
name_editor: .byte $65, $64, $69, $74, $6F, $72, 0
name_readyshell: .byte $72, $65, $61, $64, $79, $73, $68, $65, $6C, $6C, 0
name_rsparser: .byte $72, $73, $70, $61, $72, $73, $65, $72, 0
name_rsvm: .byte $72, $73, $76, $6D, 0
name_rsdrvilst: .byte $72, $73, $64, $72, $76, $69, $6C, $73, $74, 0
name_rsldv: .byte $72, $73, $6C, $64, $76, 0
name_rsstv: .byte $72, $73, $73, $74, $76, 0
name_rsfops: .byte $72, $73, $66, $6F, $70, $73, 0
name_rscat: .byte $72, $73, $63, $61, $74, 0
name_rscopy: .byte $72, $73, $63, $6F, $70, $79, 0
name_rsedit: .byte $72, $73, $65, $64, $69, $74, 0
name_simplefiles: .byte $73, $69, $6D, $70, $6C, $65, $66, $69, $6C, $65, $73, 0
name_clipmgr: .byte $63, $6C, $69, $70, $6D, $67, $72, 0
name_readybasic: .byte $72, $65, $61, $64, $79, $62, $61, $73, $69, $63, 0
name_cal26: .byte $63, $61, $6C, $32, $36, 0
name_tasklist: .byte $74, $61, $73, $6B, $6C, $69, $73, $74, 0
name_reuviewer: .byte $72, $65, $75, $76, $69, $65, $77, $65, $72, 0
name_sysinfo: .byte $73, $79, $73, $69, $6E, $66, $6F, 0
name_quicknotes: .byte $71, $75, $69, $63, $6B, $6E, $6F, $74, $65, $73, 0
name_calcplus: .byte $63, $61, $6C, $63, $70, $6C, $75, $73, 0
name_hexview: .byte $68, $65, $78, $76, $69, $65, $77, 0
name_simplecells: .byte $73, $69, $6D, $70, $6C, $65, $63, $65, $6C, $6C, $73, 0
name_game2048: .byte $67, $61, $6D, $65, $32, $30, $34, $38, 0
name_deminer: .byte $64, $65, $6D, $69, $6E, $65, $72, 0
name_dizzy: .byte $64, $69, $7A, $7A, $79, 0
name_readyirc: .byte $72, $65, $61, $64, $79, $69, $72, $63, 0
name_ucitest: .byte $75, $63, $69, $74, $65, $73, $74, 0
name_rirc_rrnet: .byte $72, $69, $72, $63, $2D, $72, $72, $6E, $65, $74, 0
name_readme: .byte $72, $65, $61, $64, $6D, $65, 0
image_dir: .byte $2F, $75, $73, $62, $31, 0
image_name: .byte $75, $63, $69, $2D, $74, $69, $6D, $69, $6E, $67, $2D, $32, $30, $32, $36, $30, $38, $31, $31, $2D, $31, $30, $30, $31, $30, $31, $2D, $36, $34, $6D, $68, $7A, $2D, $33, $32, $30, $37, $32, $2E, $64, $38, $31, 0
title_msg: .byte $55, $43, $49, $20, $54, $49, $4D, $49, $4E, $47, $20, $4C, $41, $55, $4E, $43, $48, $45, $52, 0
path_msg: .byte "PATH ", 0
uci_msg: .byte "UCI OK", 0
no_uci_msg: .byte "NO UCI", 0
loading_msg: .byte "LOADING ", 0
done_msg: .byte "DONE:", 0
item_msg: .byte " ITEMS:", 0
fail_msg: .byte " FAIL:", 0
succ_msg: .byte "SUCCESS:", 0
kb_msg: .byte " KB:", 0
ticks_msg: .byte " TICKS:", 0
max_msg: .byte "MAX:", 0
first_fail_msg: .byte "FIRST FAIL:", 0
probe_done_msg: .byte "PROBE DONE", 0
start_prompt_msg: .byte "PRESS KEY TO START", 0
running_msg: .byte "RUNNING", 0

        .segment "DATA"
done_code:       .byte 0
item_index:      .byte 0
successes:       .byte 0
failures:        .byte 0
image_ready:     .byte 0
last_ok:         .byte 0
tick_start:      .word 0
last_ticks:      .word 0
total_ticks:     .word 0
first_ticks:     .word 0
subsequent_ticks:.word 0
total_kb:        .word 0
max_load_ticks:  .word 0
current_bank:    .byte 0
current_off:     .word 0
current_max:     .word 0
current_expected:.word 0
first_fail_item: .byte 0
first_fail_error:.byte 0
first_fail_dbg0: .byte 0
first_fail_dbg1: .byte 0
first_fail_trace:.res 4
max_load_name:   .res 16
first_fail_name: .res 16
stage_id:        .byte 0
stage_start:     .word 0
stage_now:       .word 0
stage_delta:     .word 0
stage_ticks:     .res STAGE_COUNT * 2
stage_subseq_ticks: .res STAGE_COUNT * 2
