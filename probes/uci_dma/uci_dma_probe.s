;
; Standalone Ultimate UCI + Ultimate DOS REU load probe.
; Keep its transaction loop aligned with AGENTS.md: async PUSH, LAST/MORE only,
; drain DATA_AV and STAT_AV, one DATA_ACC, then fully quiescent IDLE.
;

        .segment "LOADADDR"
        .word $0801

        .segment "STARTUP"
        .word basic_next
        .word 10
        .byte $9e, "2061", 0
basic_next:
        .word 0

CHROUT      = $FFD2
GETIN       = $FFE4
SETLFS      = $FFBA
SETNAM      = $FFBD
OPEN        = $FFC0
CLOSE       = $FFC3
READST      = $FFB7
CPU_PORT    = $0001
RESULTS     = $3000
LOAD_BUF    = $4000
VERIFY_BUF  = $5000
SNAP1_BUF   = $6000
SNAP2_BUF   = $6100
SNAP3_BUF   = $6200
.ifdef PROBE_D64
PROBE_VERSION = $40
.else
PROBE_VERSION = $41
.endif

REU_COMMAND = $DF01
REU_C64_LO  = $DF02
REU_C64_HI  = $DF03
REU_REU_LO  = $DF04
REU_REU_HI  = $DF05
REU_REU_BANK = $DF06
REU_LEN_LO  = $DF07
REU_LEN_HI  = $DF08

REU_CMD_STASH = $90
REU_CMD_FETCH = $91

name_ptr   = $04

UCI_STAT_DATA  = $80
UCI_STAT_STAT  = $40
UCI_STATE_MASK = $30
UCI_STATE_IDLE = $00
UCI_STATE_LAST = $20
UCI_STATE_MORE = $30
UCI_STAT_BUSY  = $01
UCI_STAT_ACCEPT = $02
UCI_STAT_ABORT = $04
UCI_STAT_ERROR = $08
UCI_QUIET_MASK = $3F
; Match the production transport's finite bound through the 64 MHz top end.
UCI_STATE_WAIT_PASSES = $08

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
        PRINT title_msg
        jsr cr
        PRINT start_prompt_msg
wait_start_key:
        jsr GETIN
        beq wait_start_key
        PRINT running_msg
        jsr cr
        jsr init_results
        jsr probe_uci
        bcs have_uci
        PRINT no_uci_msg
        jsr cr
        jmp done

have_uci:
        PRINT using_msg
        lda uci_base_hi
        jsr print_hex_a
        lda uci_base_lo
        jsr print_hex_a
        PRINT bus_msg
        lda softiec_bus
        jsr print_hex_a
        jsr cr
        jsr print_softiec_summary
        jsr dos_identify
        jsr print_response_summary
        jsr print_data_text
        jsr ctrl_get_drvinfo
        jsr print_response_summary
        jsr print_drvinfo
        jsr reset_udos_root
        jsr plain_stat_probe
        jsr copy_ui_stat_probe
        jsr cd_usb_root
        bcs cd_usb_ok
        PRINT usb_fail_msg
        PRINT fail_msg
        jsr cr
        jmp done
cd_usb_ok:
        lda selected_usb
        sta RESULTS+$0B
        jsr dos_get_path
        jsr print_response_summary
        jsr print_data_text
        PRINT mount_d81_msg
        jsr dos_mount_d81
        jsr print_response_summary
        PRINT cd_d81_msg
        jsr cr
        lda #<d81_name
        sta name_ptr
        lda #>d81_name
        sta name_ptr+1
        jsr dos_cd
        jsr print_response_summary
        jsr status_ok
        bcs cd_d81_ok
        jsr select_d81_abs_path
        PRINT cd_d81_abs_msg
        jsr cr
        jsr dos_cd
        jsr print_response_summary
cd_d81_ok:
        jsr status_ok
        bcs cd_d81_ready
        PRINT d81_fail_msg
        PRINT fail_msg
        jsr cr
        jmp done
cd_d81_ready:
        jsr dos_get_path
        jsr print_response_summary
        jsr print_data_text
        jsr dir_peek
        PRINT close_msg
        jsr dos_close
        jsr print_response_summary
        lda #<file1_name
        sta name_ptr
        lda #>file1_name
        sta name_ptr+1
        lda #$11
        sta expected_byte
        lda #$00
        sta reu_off_hi
        sta reu_off_lo
        lda #>SNAP1_BUF
        sta snapshot_hi
        lda #$10
        sta result_base_offset
        PRINT file1_msg
        jsr do_file_dos_probe

        lda #<file2_name
        sta name_ptr
        lda #>file2_name
        sta name_ptr+1
        lda #$42
        sta expected_byte
        lda #$01
        sta reu_off_hi
        lda #$00
        sta reu_off_lo
        lda #>SNAP2_BUF
        sta snapshot_hi
        lda #$20
        sta result_base_offset
        PRINT file2_msg
        jsr do_file_dos_probe

        lda #<file3_name
        sta name_ptr
        lda #>file3_name
        sta name_ptr+1
        lda #$83
        sta expected_byte
        lda #$02
        sta reu_off_hi
        lda #$00
        sta reu_off_lo
        lda #>SNAP3_BUF
        sta snapshot_hi
        lda #$30
        sta result_base_offset
        PRINT file3_msg
        jsr do_file_dos_probe

done:
        jsr print_final_summary
        PRINT done_msg
        jsr cr
forever:
        jsr GETIN
        jmp forever

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
        lda #'D'
        sta RESULTS+1
        lda #'M'
        sta RESULTS+2
        lda #'A'
        sta RESULTS+3
        lda #PROBE_VERSION
        sta RESULTS+10
        rts

store_result_y:
        sta result_value
        tya
        clc
        adc result_base_offset
        tax
        lda result_value
        sta RESULTS,x
        rts

probe_uci:
        PRINT probe_msg
        jsr cr
        lda #<$DF1C
        ldx #>$DF1C
        jsr try_base
        bcs probe_ok
        lda #<$DE1C
        ldx #>$DE1C
        jsr try_base
        bcs probe_ok
        lda #<$DFFC
        ldx #>$DFFC
        jsr try_base
        bcs probe_ok
        clc
        rts
probe_ok:
        lda #$01
        sta RESULTS+4
        lda uci_base_lo
        sta RESULTS+5
        lda uci_base_hi
        sta RESULTS+6
        lda uci_id_byte
        sta RESULTS+7
        lda uci_status_byte
        sta RESULTS+8
        lda softiec_bus
        sta RESULTS+9
        sec
        rts

try_base:
        jsr set_uci_base
        PRINT base_msg
        lda uci_base_hi
        jsr print_hex_a
        lda uci_base_lo
        jsr print_hex_a
        PRINT id_msg
        jsr uci_id
        sta uci_id_byte
        jsr print_hex_a
        PRINT stat_msg
        jsr uci_status
        sta uci_status_byte
        jsr print_hex_a
        PRINT bus_msg
        jsr uci_softiec
        sta softiec_bus
        jsr print_hex_a
        jsr cr

        lda uci_id_byte
        and #$7F
        cmp #$49
        bne try_base_no
        beq try_base_yes
try_base_no:
        clc
        rts
try_base_yes:
        sec
        rts

softiec_ok:
        lda softiec_bus
        beq softiec_bad
        cmp #$FF
        beq softiec_bad
        sec
        rts
softiec_bad:
        clc
        rts

reset_udos_root:
        PRINT cd_root_msg
        jsr cr
        lda #<root_name
        sta name_ptr
        lda #>root_name
        sta name_ptr+1
        jsr dos_cd
        jsr print_response_summary
        rts

plain_stat_probe:
        PRINT plain_stat_msg
        jsr cr
        PRINT close_msg
        jsr dos_close
        jsr print_response_summary
        lda #<file1_name
        sta name_ptr
        lda #>file1_name
        sta name_ptr+1
        PRINT stat_file_msg
        jsr dos_file_stat
        jsr print_response_summary
        jsr print_data_text
        lda data_len
        sta RESULTS+$41
        lda stat_len
        sta RESULTS+$42
        lda stat_buf
        sta RESULTS+$43
        lda stat_buf+1
        sta RESULTS+$44
        lda data_buf
        sta RESULTS+$45
        lda data_buf+1
        sta RESULTS+$46
        lda data_buf+8
        sta RESULTS+$47
        bcs plain_stat_transport_ok
        lda #$41
        sta RESULTS+$40
        PRINT fail_msg
        jsr cr
        rts
plain_stat_transport_ok:
        jsr file_stat_ok
        bcs plain_stat_ok
        lda #$42
        sta RESULTS+$40
        PRINT fail_msg
        jsr cr
        rts
plain_stat_ok:
        lda #$55
        sta RESULTS+$40
        PRINT ok_msg
        jsr cr
        rts

copy_ui_stat_probe:
        PRINT copy_ui_stat_msg
        jsr cr
        jsr dos_copy_ui_path
        jsr print_response_summary
        jsr print_data_text
        jsr status_ok
        bcs copy_ui_ready
        lda #$51
        sta RESULTS+$48
        PRINT fail_msg
        jsr cr
        rts
copy_ui_ready:
        PRINT close_msg
        jsr dos_close
        jsr print_response_summary
        lda #<file1_name
        sta name_ptr
        lda #>file1_name
        sta name_ptr+1
        PRINT stat_file_msg
        jsr dos_file_stat
        jsr print_response_summary
        jsr print_data_text
        lda data_len
        sta RESULTS+$49
        lda stat_len
        sta RESULTS+$4A
        lda stat_buf
        sta RESULTS+$4B
        lda stat_buf+1
        sta RESULTS+$4C
        lda data_buf
        sta RESULTS+$4D
        lda data_buf+1
        sta RESULTS+$4E
        lda data_buf+8
        sta RESULTS+$4F
        bcs copy_ui_stat_transport_ok
        lda #$52
        sta RESULTS+$48
        PRINT fail_msg
        jsr cr
        rts
copy_ui_stat_transport_ok:
        jsr file_stat_ok
        bcs copy_ui_stat_ok
        lda #$53
        sta RESULTS+$48
        PRINT fail_msg
        jsr cr
        rts
copy_ui_stat_ok:
        lda #$55
        sta RESULTS+$48
        PRINT ok_msg
        jsr cr
        rts

cd_usb_root:
        PRINT cd_root_msg
        jsr cr
        lda #<root_name
        sta name_ptr
        lda #>root_name
        sta name_ptr+1
        jsr dos_cd
        jsr print_response_summary
        lda #$01
        sta selected_usb
        PRINT cd_usb1_msg
        jsr cr
        lda #<usb1_name
        sta name_ptr
        lda #>usb1_name
        sta name_ptr+1
        jsr dos_cd
        jsr print_response_summary
        jsr status_ok
        bcs cd_usb_root_ok
        PRINT cd_usb1_abs_msg
        jsr cr
        lda #<usb1_abs_name
        sta name_ptr
        lda #>usb1_abs_name
        sta name_ptr+1
        jsr dos_cd
        jsr print_response_summary
        jsr status_ok
        bcs cd_usb_root_ok
        lda #$00
        sta selected_usb
        PRINT cd_usb0_msg
        jsr cr
        lda #<usb0_name
        sta name_ptr
        lda #>usb0_name
        sta name_ptr+1
        jsr dos_cd
        jsr print_response_summary
        jsr status_ok
        bcs cd_usb_root_ok
        PRINT cd_usb0_abs_msg
        jsr cr
        lda #<usb0_abs_name
        sta name_ptr
        lda #>usb0_abs_name
        sta name_ptr+1
        jsr dos_cd
        jsr print_response_summary
        jmp status_ok
cd_usb_root_ok:
        sec
        rts

select_d81_abs_path:
        lda selected_usb
        beq select_d81_usb0
        lda #<d81_usb1_abs_name
        sta name_ptr
        lda #>d81_usb1_abs_name
        sta name_ptr+1
        rts
select_d81_usb0:
        lda #<d81_usb0_abs_name
        sta name_ptr
        lda #>d81_usb0_abs_name
        sta name_ptr+1
        rts

do_file:
        jsr clear_buffers
        PRINT stat_file_msg
        jsr dos_file_stat
        jsr print_response_summary
        bcs file_stat_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$12
        ldy #$00
        jsr store_result_y
        rts
file_stat_transport_ok:
        jsr file_stat_ok
        bcs file_stat_ready
        PRINT fail_msg
        jsr cr
        lda #$13
        ldy #$00
        jsr store_result_y
        rts
file_stat_ready:
        jsr print_file_stat
        lda stat_size_lo
        ldy #$07
        jsr store_result_y
        lda stat_size_hi
        iny
        jsr store_result_y

        PRINT close_msg
do_file_open_start:
        jsr dos_close
        jsr print_response_summary
do_file_open_no_close:
        PRINT open_msg
        jsr dos_open_read
        jsr print_response_summary
        bcs open_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$01
        ldy #$00
        jsr store_result_y
        rts

do_file_dos_nostat:
        jsr clear_buffers
        lda #$02
        ldy #$07
        jsr store_result_y
        lda #$01
        iny
        jsr store_result_y
        PRINT nostat_file_msg
        jmp do_file_open_start

do_file_dos_probe:
        jsr clear_buffers
        PRINT close_msg
        jsr dos_close
        jsr print_response_summary
        PRINT stat_file_msg
        jsr dos_file_stat
        jsr print_response_summary
        jsr print_data_text
        jmp do_file_open_start
open_transport_ok:
        jsr status_ok
        bcs open_rc_ok
        PRINT fail_msg
        jsr cr
        lda #$05
        ldy #$00
        jsr store_result_y
        rts
open_rc_ok:
        lda #$11
        ldy #$00
        jsr store_result_y

        PRINT hdr_read_msg
        jsr dos_read_header
        jsr print_response_summary
        bcs hdr_read_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$02
        ldy #$00
        jsr store_result_y
        rts
hdr_read_transport_ok:
        jsr data_or_status_ok
        bcs hdr_read_rc_ok
        PRINT fail_msg
        jsr cr
        lda #$06
        ldy #$00
        jsr store_result_y
        rts
hdr_read_rc_ok:
        jsr verify_header_buffer
        bcs header_ok
        PRINT fail_msg
        jsr cr
        lda #$03
        ldy #$00
        jsr store_result_y
        rts
header_ok:
        PRINT ok_msg
        jsr cr

        jsr read_ram_chunks
        bcs ram_read_ok
        PRINT fail_msg
        jsr cr
        lda ram_error
        ldy #$00
        jsr store_result_y
        rts
ram_read_ok:
        jsr verify_load_buffer
        bcs ram_ok
        PRINT fail_msg
        jsr cr
        lda #$0D
        ldy #$00
        jsr store_result_y
        rts
ram_ok:
        PRINT ok_msg
        jsr cr
        lda #$01
        ldy #$04
        jsr store_result_y
        lda ram_chunk_count
        ldy #$06
        jsr store_result_y

        PRINT seek_msg
        jsr dos_seek_payload
        jsr print_response_summary
        bcs seek_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$0E
        ldy #$00
        jsr store_result_y
        rts
seek_transport_ok:
        jsr status_ok
        bcs seek_rc_ok
        PRINT fail_msg
        jsr cr
        lda #$0F
        ldy #$00
        jsr store_result_y
        rts
seek_rc_ok:
        PRINT reu_load1_msg
        lda #$00
        sta reu_off_lo
        lda #$80
        sta reu_len_lo
        jsr dos_load_reu_chunk
        jsr print_response_summary
        bcs reu_load1_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$07
        ldy #$00
        jsr store_result_y
        rts
reu_load1_transport_ok:
        jsr status_ok
        bcs reu_load1_rc_ok
        PRINT fail_msg
        jsr cr
        lda #$08
        ldy #$00
        jsr store_result_y
        rts
reu_load1_rc_ok:
        PRINT reu_load2_msg
        lda #$80
        sta reu_off_lo
        lda #$80
        sta reu_len_lo
        jsr dos_load_reu_chunk
        jsr print_response_summary
        bcs reu_load2_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$09
        ldy #$00
        jsr store_result_y
        rts
reu_load2_transport_ok:
        jsr status_ok
        bcs reu_load2_rc_ok
        PRINT fail_msg
        jsr cr
        lda #$0A
        ldy #$00
        jsr store_result_y
        rts
reu_load2_rc_ok:
        lda #$22
        ldy #$00
        jsr store_result_y
        lda stat_buf
        ldy #$01
        jsr store_result_y
        lda stat_buf+1
        iny
        jsr store_result_y
        lda stat_buf+2
        iny
        jsr store_result_y
        PRINT reu_msg
        jsr clear_verify
        jsr fetch_from_reu
        jsr verify_fetch_buffer
        bcs reu_ok
        PRINT fail_msg
        jsr cr
        lda #$04
        ldy #$00
        jsr store_result_y
        rts
reu_ok:
        PRINT ok_msg
        jsr cr
        jsr snapshot_verify
        PRINT close_msg
        jsr dos_close
        jsr print_response_summary
        lda #$01
        ldy #$05
        jsr store_result_y
        lda snapshot_hi
        ldy #$09
        jsr store_result_y
        lda #$55
        ldy #$00
        jsr store_result_y
        rts

dir_peek:
        PRINT dir_open_msg
        jsr dos_open_dir
        jsr print_response_summary
        jsr status_ok
        bcc dir_peek_done
        lda #$02
        sta dir_count
dir_peek_loop:
        PRINT dir_read_msg
        jsr dos_read_dir
        jsr print_response_summary
        jsr print_data_text
        dec dir_count
        bne dir_peek_loop
dir_peek_done:
        rts

do_file_softiec:
        jsr clear_buffers
        PRINT softiec_load_su_msg
        jsr softiec_load_su
        jsr print_response_summary
        bcs softiec_su_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$31
        ldy #$00
        jsr store_result_y
        rts
softiec_su_transport_ok:
        jsr status_ok
        bcs softiec_su_status_ok
        PRINT fail_msg
        jsr cr
        lda #$33
        ldy #$00
        jsr store_result_y
        rts
softiec_su_status_ok:
        jsr verify_header_buffer
        bcs softiec_su_ok
        PRINT fail_msg
        jsr cr
        lda #$32
        ldy #$00
        jsr store_result_y
        rts
softiec_su_ok:
        PRINT ok_msg
        jsr cr

        PRINT softiec_load_ex_msg
        jsr softiec_load_ex
        jsr print_response_summary
        bcs softiec_ex_transport_ok
        PRINT fail_msg
        jsr cr
        lda #$34
        ldy #$00
        jsr store_result_y
        rts
softiec_ex_transport_ok:
        jsr status_ok
        bcs softiec_ex_status_ok
        PRINT fail_msg
        jsr cr
        lda #$35
        ldy #$00
        jsr store_result_y
        rts
softiec_ex_status_ok:
        jsr verify_load_buffer_softiec
        bcs softiec_ram_ok
        PRINT fail_msg
        jsr cr
        lda #$0D
        ldy #$00
        jsr store_result_y
        rts
softiec_ram_ok:
        PRINT ok_msg
        jsr cr
        lda #$01
        ldy #$04
        jsr store_result_y
        ldy #$06
        jsr store_result_y
        lda #$02
        ldy #$07
        jsr store_result_y
        lda #$01
        iny
        jsr store_result_y

        PRINT reu_msg
        jsr stash_to_reu
        jsr clear_verify
        jsr fetch_from_reu
        jsr verify_fetch_buffer
        bcs softiec_reu_ok
        PRINT fail_msg
        jsr cr
        lda #$04
        ldy #$00
        jsr store_result_y
        rts
softiec_reu_ok:
        PRINT ok_msg
        jsr cr
        jsr snapshot_verify
        lda #$01
        ldy #$05
        jsr store_result_y
        lda snapshot_hi
        ldy #$09
        jsr store_result_y
        lda #$55
        ldy #$00
        jsr store_result_y
        rts

read_ram_chunks:
        lda #$00
        sta load_off
        sta ram_chunk_count
read_ram_chunk_loop:
        PRINT ram_read_msg
        lda load_off
        jsr print_hex_a
        jsr dos_read_ram_64
        jsr print_response_summary
        bcs read_ram_transport_ok
        lda #$0B
        sta ram_error
        clc
        rts
read_ram_transport_ok:
        lda data_full
        bne read_ram_count_fail
        lda data_len
        cmp #$40
        bne read_ram_count_fail
        lda stat_len
        beq read_ram_one_ok
        jsr status_ok
        bcs read_ram_one_ok
        lda #$0C
        sta ram_error
        clc
        rts
read_ram_count_fail:
        lda #$10
        sta ram_error
        clc
        rts
read_ram_one_ok:
        inc ram_chunk_count
        lda load_off
        clc
        adc #$40
        sta load_off
        lda ram_chunk_count
        cmp #$04
        bne read_ram_chunk_loop
        sec
        rts

dos_identify:
        jsr sync_interface
        bcs dos_identify_sync_ok
        jmp command_fail
dos_identify_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$01
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

ctrl_get_drvinfo:
        jsr sync_interface
        bcs ctrl_drvinfo_sync_ok
        jmp command_fail
ctrl_drvinfo_sync_ok:
        lda #$04
        jsr uci_write_cmd
        lda #$29
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_close:
        jsr sync_interface
        bcs dos_close_sync_ok
        jmp command_fail
dos_close_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$03
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_cd:
        jsr sync_interface
        bcs dos_cd_sync_ok
        jmp command_fail
dos_cd_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$11
        jsr uci_write_cmd
        ldy #$00
dos_cd_name:
        lda (name_ptr),y
        beq dos_cd_push
        jsr uci_write_cmd
        iny
        bne dos_cd_name
dos_cd_push:
        jsr uci_push_cmd
        jmp drain_response

dos_get_path:
        jsr sync_interface
        bcs dos_get_path_sync_ok
        jmp command_fail
dos_get_path_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$12
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_copy_ui_path:
        jsr sync_interface
        bcs dos_copy_ui_sync_ok
        jmp command_fail
dos_copy_ui_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$15
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_mount_d81:
        jsr sync_interface
        bcs dos_mount_sync_ok
        jmp command_fail
dos_mount_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$23
        jsr uci_write_cmd
        lda #$08
        jsr uci_write_cmd
        ldy #$00
dos_mount_name:
        lda d81_name,y
        beq dos_mount_push
        jsr uci_write_cmd
        iny
        bne dos_mount_name
dos_mount_push:
        jsr uci_push_cmd
        jmp drain_response

dos_open_dir:
        jsr sync_interface
        bcs dos_open_dir_sync_ok
        jmp command_fail
dos_open_dir_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$13
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_read_dir:
        jsr sync_interface
        bcs dos_read_dir_sync_ok
        jmp command_fail
dos_read_dir_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$14
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_file_stat:
        jsr sync_interface
        bcs dos_file_stat_sync_ok
        jmp command_fail
dos_file_stat_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$08
        jsr uci_write_cmd
        ldy #$00
dos_file_stat_name:
        lda (name_ptr),y
        beq dos_file_stat_push
        jsr uci_write_cmd
        iny
        bne dos_file_stat_name
dos_file_stat_push:
        jsr uci_push_cmd
        jmp drain_response

dos_open_read:
        jsr sync_interface
        bcs dos_open_sync_ok
        jmp command_fail
dos_open_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$02
        jsr uci_write_cmd
        lda #$01
        jsr uci_write_cmd
        ldy #$00
dos_open_name:
        lda (name_ptr),y
        beq dos_open_push
        jsr uci_write_cmd
        iny
        bne dos_open_name
dos_open_push:
        jsr uci_push_cmd
        jmp drain_response

dos_read_header:
        jsr sync_interface
        bcs dos_read_header_sync_ok
        jmp command_fail
dos_read_header_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$04
        jsr uci_write_cmd
        lda #$02
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_read_ram_64:
        jsr sync_interface
        bcs dos_read_ram_sync_ok
        jmp command_fail
dos_read_ram_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$04
        jsr uci_write_cmd
        ; Ultimate DOS memory-read payload length: 64 bytes.  This is command
        ; data, not a UCI polling bound; keep it independent of timeout tuning.
        lda #$40
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response_to_load

dos_seek_payload:
        jsr sync_interface
        bcs dos_seek_sync_ok
        jmp command_fail
dos_seek_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$06
        jsr uci_write_cmd
        lda #$02
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_write_cmd
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_load_reu_chunk:
        jsr sync_interface
        bcs dos_load_reu_sync_ok
        jmp command_fail
dos_load_reu_sync_ok:
        lda #$01
        jsr uci_write_cmd
        lda #$21
        jsr uci_write_cmd
        lda reu_off_lo
        jsr uci_write_cmd
        lda reu_off_hi
        jsr uci_write_cmd
        lda #$02
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        lda reu_len_lo
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

softiec_load_su:
        jsr sync_interface
        bcc command_fail
        lda #$05
        jsr uci_write_cmd
        lda #$10
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_write_cmd
        lda #<LOAD_BUF
        jsr uci_write_cmd
        lda #>LOAD_BUF
        jsr uci_write_cmd
        ldy #$00
softiec_load_su_name:
        lda (name_ptr),y
        beq softiec_load_su_push
        jsr uci_write_cmd
        iny
        bne softiec_load_su_name
softiec_load_su_push:
        jsr uci_push_cmd
        jmp drain_response

softiec_load_ex:
        jsr sync_interface
        bcc command_fail
        lda #$05
        jsr uci_write_cmd
        lda #$11
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

command_fail:
        jsr abort_and_recover
        clc
        rts

sync_interface:
        ; Explicit recovery for stale state. Normal commands never clear an
        ; ERROR: they report transport failure and let the next sync recover.
        lda #$FF
        sta timeout_hi
        lda #UCI_STATE_WAIT_PASSES
        sta timeout_outer
        ldy #$00
sync_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        beq sync_no_error
        jsr uci_clear_error
        jmp sync_loop
sync_no_error:
        lda last_status
        and #UCI_STAT_ABORT
        beq sync_no_abort
        ; ABORT_P is an outstanding asynchronous request, not permission to
        ; request another one. Service the pending operation to quiet IDLE.
        jmp sync_wait_pending
sync_no_abort:
        lda last_status
        and #UCI_STAT_DATA
        beq sync_no_data
        jsr uci_read_data
        jmp sync_loop
sync_no_data:
        lda last_status
        and #UCI_STAT_STAT
        beq sync_no_stat
        jsr uci_read_stat
        jmp sync_loop
sync_no_stat:
        lda last_status
        and #UCI_STATE_MASK
        cmp #UCI_STATE_LAST
        beq sync_accept
        cmp #UCI_STATE_MORE
        beq sync_accept
        lda last_status
        and #UCI_QUIET_MASK
        beq sync_ok
sync_wait_pending:
        dey
        bne sync_loop
        dec timeout_hi
        bne sync_loop
        dec timeout_outer
        beq sync_timeout
        lda #$FF
        sta timeout_hi
        jmp sync_loop
sync_timeout:
        clc
        rts
sync_accept:
        lda last_status
        and #UCI_STATE_MASK
        sta current_state
        jsr uci_accept_data
        lda current_state
        jsr wait_accept_advance
        bcc sync_accept_fail
        jmp sync_loop
sync_accept_fail:
        clc
        rts
sync_ok:
        sec
        rts

abort_and_recover:
        ; ABORT is asynchronous: request once unless already pending, then
        ; drain/clear/accept as needed until all control bits clear at IDLE.
        jsr uci_status
        sta last_status
        lda last_status
        and #UCI_STAT_ABORT
        bne abort_recover_wait
        jsr uci_abort
abort_recover_wait:
        jsr sync_interface
        rts

wait_data_state:
        lda #$FF
        sta timeout_hi
        lda #UCI_STATE_WAIT_PASSES
        sta timeout_outer
        ldy #$00
wait_state_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        bne wait_state_fail
        lda last_status
        and #UCI_STATE_MASK
        cmp #UCI_STATE_LAST
        beq wait_state_ok
        cmp #UCI_STATE_MORE
        beq wait_state_ok
        ; PUSH_CMD is asynchronous. IDLE here means the Ultimate has not yet
        ; observed it; only LAST/MORE begins a response.
        dey
        bne wait_state_loop
        dec timeout_hi
        bne wait_state_loop
        dec timeout_outer
        beq wait_state_fail
        lda #$FF
        sta timeout_hi
        jmp wait_state_loop
wait_state_fail:
        clc
        rts
wait_state_ok:
        sec
        rts

wait_accept_advance:
        ; DATA_ACC is asynchronous just like PUSH_CMD.  Require evidence that
        ; the drained block advanced before treating the same MORE value as a
        ; new block; instruction timing must never provide this handshake.
        sta accept_state
        lda #$FF
        sta timeout_hi
        lda #UCI_STATE_WAIT_PASSES
        sta timeout_outer
        ldy #$00
accept_advance_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        bne accept_advance_fail
        lda last_status
        and #UCI_STATE_MASK
        cmp accept_state
        bne accept_advance_ok
accept_advance_wait:
        dey
        bne accept_advance_loop
        dec timeout_hi
        bne accept_advance_loop
        dec timeout_outer
        beq accept_advance_fail
        lda #$FF
        sta timeout_hi
        jmp accept_advance_loop
accept_advance_fail:
        clc
        rts
accept_advance_ok:
        sec
        rts

wait_idle:
        lda #$FF
        sta timeout_hi
        lda #UCI_STATE_WAIT_PASSES
        sta timeout_outer
        ldy #$00
wait_idle_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        bne wait_idle_fail
        lda last_status
        and #UCI_QUIET_MASK
        beq wait_idle_ok
        dey
        bne wait_idle_loop
        dec timeout_hi
        bne wait_idle_loop
        dec timeout_outer
        beq wait_idle_fail
        lda #$FF
        sta timeout_hi
        jmp wait_idle_loop
wait_idle_fail:
        clc
        rts
wait_idle_ok:
        sec
        rts

drain_response:
        ; State-driven UCI transaction: LAST/MORE, drain DATA_AV and STAT_AV,
        ; then DATA_ACC. No instruction-count delay may provide pacing.
        lda #$00
        sta data_len
        sta stat_len
        jsr wait_data_state
        bcs drain_loop
        jmp drain_failed
drain_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        beq drain_no_error
        jmp drain_failed
drain_no_error:
        lda last_status
        and #UCI_STATE_MASK
        sta current_state
        cmp #UCI_STATE_LAST
        beq drain_state_ok
        cmp #UCI_STATE_MORE
        beq drain_state_ok
        jsr wait_data_state
        bcs drain_loop
        jmp drain_failed
drain_state_ok:
        ; Queue flags drive draining; this 4096-poll failure bound is larger
        ; than the documented 896-byte data plus 256-byte status capacity.
        lda #$10
        sta timeout_hi
        ldy #$00
drain_bytes:
        dey
        bne drain_poll
        dec timeout_hi
        beq drain_bytes_fail
drain_poll:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        bne drain_bytes_fail
        lda last_status
        and #UCI_STAT_DATA
        beq drain_check_stat
        jsr uci_read_data
        ldx data_len
        cpx #$20
        bcs drain_reset_guard
        sta data_buf,x
        inc data_len
        jmp drain_reset_guard
drain_check_stat:
        lda last_status
        and #UCI_STAT_STAT
        beq drain_wait_byte
        jsr uci_read_stat
        ldx stat_len
        cpx #$08
        bcs drain_reset_guard
        sta stat_buf,x
        inc stat_len
drain_reset_guard:
        jmp drain_bytes
drain_wait_byte:
        jsr uci_accept_data
        lda current_state
        cmp #UCI_STATE_LAST
        bne drain_more
        jsr wait_idle
        bcs drain_done
        jmp drain_failed
drain_more:
        lda current_state
        jsr wait_accept_advance
        bcc drain_more_fail
        jsr wait_data_state
        bcc drain_more_fail
        jmp drain_loop
drain_more_fail:
drain_bytes_fail:
drain_failed:
        jsr abort_and_recover
        clc
        rts
drain_done:
        sec
        rts

drain_response_to_load:
        lda #$00
        sta data_len
        sta data_full
        sta stat_len
        jsr wait_data_state
        bcs drain_load_loop
        jmp drain_load_failed
drain_load_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        beq drain_load_no_error
        jmp drain_load_failed
drain_load_no_error:
        lda last_status
        and #UCI_STATE_MASK
        sta current_state
        cmp #UCI_STATE_LAST
        beq drain_load_state_ok
        cmp #UCI_STATE_MORE
        beq drain_load_state_ok
        jsr wait_data_state
        bcs drain_load_loop
        jmp drain_load_failed
drain_load_state_ok:
        ; Apply the same non-pacing failure bound to direct-to-load drains.
        lda #$10
        sta timeout_hi
        ldy #$00
drain_load_bytes:
        dey
        bne drain_load_poll
        dec timeout_hi
        beq drain_load_bytes_fail
drain_load_poll:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        bne drain_load_bytes_fail
        lda last_status
        and #UCI_STAT_DATA
        beq drain_load_check_stat
        jsr uci_read_data
        pha
        ldx data_len
        txa
        clc
        adc load_off
        tax
        pla
        sta LOAD_BUF,x
        inc data_len
        bne drain_load_reset_guard
        inc data_full
        jmp drain_load_reset_guard
drain_load_check_stat:
        lda last_status
        and #UCI_STAT_STAT
        beq drain_load_wait_byte
        jsr uci_read_stat
        ldx stat_len
        cpx #$08
        bcs drain_load_reset_guard
        sta stat_buf,x
        inc stat_len
drain_load_reset_guard:
        jmp drain_load_bytes
drain_load_wait_byte:
        jsr uci_accept_data
        lda current_state
        cmp #UCI_STATE_LAST
        bne drain_load_more
        jsr wait_idle
        bcs drain_load_done
        jmp drain_load_failed
drain_load_more:
        lda current_state
        jsr wait_accept_advance
        bcc drain_load_more_fail
        jsr wait_data_state
        bcc drain_load_more_fail
        jmp drain_load_loop
drain_load_more_fail:
drain_load_bytes_fail:
drain_load_failed:
        jsr abort_and_recover
        clc
        rts
drain_load_done:
        sec
        rts

clear_buffers:
        jsr clear_load
clear_verify:
        ldx #$00
        lda #$00
clear_verify_loop:
        sta VERIFY_BUF,x
        inx
        bne clear_verify_loop
        rts

clear_load:
        ldx #$00
        lda #$00
clear_load_loop:
        sta LOAD_BUF,x
        inx
        bne clear_load_loop
        rts

verify_header_buffer:
        lda data_len
        cmp #$02
        bcc verify_fail
        lda data_buf
        bne verify_fail
        lda data_buf+1
        cmp #$40
        bne verify_fail
        sec
        rts

verify_load_buffer:
        lda ram_chunk_count
        cmp #$04
        bne verify_fail
        ldx #$00
verify_load_loop:
        lda LOAD_BUF,x
        cmp expected_byte
        bne verify_fail
        inx
        bne verify_load_loop
        sec
        rts

verify_load_buffer_softiec:
        ldx #$00
verify_load_softiec_loop:
        lda LOAD_BUF,x
        cmp expected_byte
        bne verify_fail
        inx
        bne verify_load_softiec_loop
        sec
        rts

verify_fetch_buffer:
        ldx #$00
verify_fetch_loop:
        lda VERIFY_BUF,x
        cmp expected_byte
        bne verify_fail
        inx
        bne verify_fetch_loop
        sec
        rts
verify_fail:
        stx fail_offset
        clc
        rts

fetch_from_reu:
        lda #<VERIFY_BUF
        sta REU_C64_LO
        lda #>VERIFY_BUF
        sta REU_C64_HI
        lda #$00
        sta REU_REU_LO
        lda reu_off_hi
        sta REU_REU_HI
        lda #$02
        sta REU_REU_BANK
        lda #$00
        sta REU_LEN_LO
        lda #$01
        sta REU_LEN_HI
        lda #REU_CMD_FETCH
        sta REU_COMMAND
        rts

stash_to_reu:
        lda #<LOAD_BUF
        sta REU_C64_LO
        lda #>LOAD_BUF
        sta REU_C64_HI
        lda #$00
        sta REU_REU_LO
        lda reu_off_hi
        sta REU_REU_HI
        lda #$02
        sta REU_REU_BANK
        lda #$00
        sta REU_LEN_LO
        lda #$01
        sta REU_LEN_HI
        lda #REU_CMD_STASH
        sta REU_COMMAND
        rts

snapshot_verify:
        lda snapshot_hi
        sta snapshot_store+2
        ldx #$00
snapshot_loop:
        lda VERIFY_BUF,x
snapshot_store:
        sta SNAP1_BUF,x
        inx
        bne snapshot_loop
        rts

status_ok:
        lda stat_len
        beq status_not_ok
        lda stat_buf
        cmp #'0'
        bne status_not_ok
        lda stat_buf+1
        cmp #'0'
        bne status_not_ok
status_is_ok:
        sec
        rts
status_not_ok:
        clc
        rts

data_or_status_ok:
        lda stat_len
        beq data_or_status_from_data
        jmp status_ok
data_or_status_from_data:
        lda data_len
        ora data_full
        beq status_not_ok
        sec
        rts

file_stat_ok:
        jsr data_or_status_ok
        bcc file_stat_not_ok
        lda data_len
        cmp #$0B
        bcc file_stat_not_ok
        lda data_buf
        cmp #$02
        bne file_stat_not_ok
        lda data_buf+1
        cmp #$01
        bne file_stat_not_ok
        lda data_buf+2
        ora data_buf+3
        bne file_stat_not_ok
        lda data_buf+8
        cmp #'P'
        bne file_stat_not_ok
        lda data_buf+9
        cmp #'R'
        bne file_stat_not_ok
        lda data_buf+10
        cmp #'G'
        bne file_stat_not_ok
        lda data_buf
        sta stat_size_lo
        lda data_buf+1
        sta stat_size_hi
        sec
        rts
file_stat_not_ok:
        clc
        rts

print_response_summary:
        php
        PRINT data_msg
        lda data_full
        jsr print_hex_a
        lda data_len
        jsr print_hex_a
        PRINT stat_count_msg
        lda stat_len
        jsr print_hex_a
        PRINT rc_msg
        lda stat_buf
        jsr print_hex_a
        PRINT end_msg
        lda stat_buf+2
        jsr print_hex_a
        lda stat_buf+1
        jsr print_hex_a
        jsr cr
        plp
        rts

print_data_text:
        php
        lda data_len
        ora data_full
        beq print_data_text_done
        PRINT text_msg
        ldx #$00
print_data_text_loop:
        cpx data_len
        beq print_data_text_cr
        lda data_buf,x
        beq print_data_text_cr
        jsr print_safe_char
        inx
        cpx #$20
        bne print_data_text_loop
print_data_text_cr:
        jsr cr
print_data_text_done:
        plp
        rts

print_drvinfo:
        php
        lda data_len
        cmp #$04
        bcc print_drvinfo_done
        PRINT drv_msg
        lda data_buf
        jsr print_hex_a
        PRINT drv_a_msg
        lda data_buf+1
        jsr print_hex_a
        lda data_buf+2
        jsr print_hex_a
        lda data_buf+3
        jsr print_hex_a
        lda data_buf
        cmp #$02
        bcc print_drvinfo_cr
        lda data_len
        cmp #$07
        bcc print_drvinfo_cr
        PRINT drv_b_msg
        lda data_buf+4
        jsr print_hex_a
        lda data_buf+5
        jsr print_hex_a
        lda data_buf+6
        jsr print_hex_a
print_drvinfo_cr:
        jsr cr
        lda data_buf
        sta RESULTS+$0C
        lda data_buf+1
        sta RESULTS+$0D
        lda data_buf+2
        sta RESULTS+$0E
        lda data_buf+3
        sta RESULTS+$0F
print_drvinfo_done:
        plp
        rts

print_file_stat:
        php
        PRINT file_stat_msg
        lda stat_size_hi
        jsr print_hex_a
        lda stat_size_lo
        jsr print_hex_a
        PRINT file_stat_ext_msg
        lda data_buf+8
        jsr print_safe_char
        lda data_buf+9
        jsr print_safe_char
        lda data_buf+10
        jsr print_safe_char
        jsr cr
        plp
        rts

print_final_summary:
        PRINT summary_msg
        lda RESULTS+$0B
        jsr print_hex_a
        PRINT summary_plain_msg
        lda RESULTS+$40
        jsr print_hex_a
        PRINT summary_copy_ui_msg
        lda RESULTS+$48
        jsr print_hex_a
        PRINT summary_f1_msg
        lda RESULTS+$10
        jsr print_hex_a
        PRINT summary_f2_msg
        lda RESULTS+$20
        jsr print_hex_a
        PRINT summary_f3_msg
        lda RESULTS+$30
        jsr print_hex_a
        jsr cr
        rts

print_softiec_summary:
        PRINT softiec_msg
        jsr softiec_ok
        bcs print_softiec_yes
        PRINT no_msg
        jmp print_softiec_bus
print_softiec_yes:
        PRINT yes_msg
print_softiec_bus:
        PRINT bus_msg
        lda softiec_bus
        jsr print_hex_a
        jsr cr
        rts

print_safe_char:
        cmp #$20
        bcc print_dot
        cmp #$7F
        bcs print_dot
        jmp CHROUT
print_dot:
        lda #'.'
        jmp CHROUT

print_z:
        sta print_z_load+1
        sty print_z_load+2
print_z_loop:
print_z_load:
        lda $FFFF
        beq print_z_done
        jsr CHROUT
        inc print_z_load+1
        bne print_z_loop
        inc print_z_load+2
        jmp print_z_loop
print_z_done:
        rts

cr:
        lda #$0D
        jmp CHROUT

print_hex_a:
        pha
        lsr
        lsr
        lsr
        lsr
        jsr print_nibble
        pla
        and #$0F
print_nibble:
        cmp #$0A
        bcc print_digit
        clc
        adc #('A' - $0A)
        jmp CHROUT
print_digit:
        clc
        adc #'0'
        jmp CHROUT

set_uci_base:
        sta uci_base_lo
        stx uci_base_hi
        sta uci_status_abs+1
        sta uci_push_abs+1
        sta uci_accept_abs+1
        sta uci_abort_abs+1
        sta uci_clear_abs+1
        stx uci_status_abs+2
        stx uci_push_abs+2
        stx uci_accept_abs+2
        stx uci_abort_abs+2
        stx uci_clear_abs+2

        lda uci_base_lo
        sec
        sbc #$01
        sta uci_softiec_abs+1
        lda uci_base_hi
        sbc #$00
        sta uci_softiec_abs+2

        lda uci_base_lo
        clc
        adc #$01
        sta uci_write_abs+1
        sta uci_id_abs+1
        lda uci_base_hi
        adc #$00
        sta uci_write_abs+2
        sta uci_id_abs+2

        lda uci_base_lo
        clc
        adc #$02
        sta uci_data_abs+1
        lda uci_base_hi
        adc #$00
        sta uci_data_abs+2

        lda uci_base_lo
        clc
        adc #$03
        sta uci_stat_abs+1
        lda uci_base_hi
        adc #$00
        sta uci_stat_abs+2
        rts

uci_write_cmd:
        sta uci_value
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda uci_value
uci_write_abs:
        sta $DF1D
        pla
        sta CPU_PORT
        plp
        lda uci_value
        rts

uci_id:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_id_abs:
        lda $DF1D
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

uci_status:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_status_abs:
        lda $DF1C
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

uci_read_data:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_data_abs:
        lda $DF1E
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

uci_read_stat:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_stat_abs:
        lda $DF1F
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

uci_push_cmd:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$01
uci_push_abs:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

uci_accept_data:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$02
uci_accept_abs:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

uci_abort:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$04
uci_abort_abs:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

uci_clear_error:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$08
uci_clear_abs:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

uci_softiec:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_softiec_abs:
        lda $DF1B
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

        .segment "RODATA"
.ifdef PROBE_D64
title_msg:  .byte "UCI DOS REU PROBE V40", 0
.else
title_msg:  .byte "UCI DOS REU PROBE V41", 0
.endif
probe_msg:  .byte "PROBING UCI", 0
base_msg:   .byte "BASE $", 0
id_msg:     .byte " ID:", 0
stat_msg:   .byte " ST:", 0
bus_msg:    .byte " BUS:", 0
softiec_msg: .byte "SOFTIEC:", 0
yes_msg:    .byte "YES", 0
no_msg:     .byte "NO", 0
using_msg:  .byte "UCI OK $", 0
no_uci_msg: .byte "UCI NOT FOUND", 0
drv_msg:    .byte "DRV:", 0
drv_a_msg:  .byte " A:", 0
drv_b_msg:  .byte " B:", 0
file1_msg:  .byte "FILE UDMA1", 13, 0
file2_msg: .byte "FILE UDMA2", 13, 0
file3_msg: .byte "FILE UDMA3", 13, 0
close_msg:  .byte " CLOSE", 0
open_msg:   .byte " OPEN", 0
hdr_read_msg: .byte " READ HDR", 0
ram_read_msg: .byte " READ RAM", 0
seek_msg:   .byte " SEEK +2", 0
reu_load1_msg: .byte " LOAD_REU 1", 0
reu_load2_msg: .byte " LOAD_REU 2", 0
reu_msg:    .byte " REU VERIFY ", 0
softiec_load_su_msg: .byte " LOAD_SU", 0
softiec_load_ex_msg: .byte " LOAD_EX", 0
cd_root_msg: .byte "CD /", 0
cd_usb1_msg: .byte "CD USB1", 0
cd_usb1_abs_msg: .byte "CD /USB1", 0
cd_usb0_msg: .byte "CD USB0", 0
cd_usb0_abs_msg: .byte "CD /USB0", 0
.ifdef PROBE_D64
cd_d81_msg: .byte "CD UCI40.D64", 0
mount_d81_msg: .byte "MOUNT8 UCI40.D64", 13, 0
.else
cd_d81_msg: .byte "CD UCI41.D81", 0
mount_d81_msg: .byte "MOUNT8 UCI41.D81", 13, 0
.endif
cd_d81_abs_msg: .byte "CD /USBX/UCI.D81", 0
usb_fail_msg: .byte "USB ROOT ", 0
d81_fail_msg: .byte "D81 PATH ", 0
dir_open_msg: .byte " OPEN DIR", 0
dir_read_msg: .byte " READ DIR", 0
stat_file_msg: .byte " STAT", 0
nostat_file_msg: .byte " OPEN DIRECT", 0
data_msg:   .byte " D:", 0
stat_count_msg: .byte " S:", 0
text_msg:   .byte " TXT:", 0
rc_msg:     .byte " RC:", 0
end_msg:    .byte " END:", 0
summary_msg: .byte "SUM USB:", 0
summary_plain_msg: .byte " P:", 0
summary_copy_ui_msg: .byte " C:", 0
summary_f1_msg: .byte " F1:", 0
summary_f2_msg: .byte " F2:", 0
summary_f3_msg: .byte " F3:", 0
file_stat_msg: .byte " SIZE:", 0
file_stat_ext_msg: .byte " EXT:", 0
plain_stat_msg: .byte "PLAIN STAT UDMA1", 0
copy_ui_stat_msg: .byte "COPYUI STAT UDMA1", 0
ok_msg:     .byte "OK", 0
fail_msg:   .byte "FAIL", 0
done_msg:   .byte "PROBE DONE", 0
start_prompt_msg: .byte "PRESS KEY TO START", 0
running_msg: .byte "RUNNING", 0
; Ultimate DOS command spelling that matched the known-good D64 probe run.
file1_name: .byte "udma1", 0
file2_name: .byte "udma2", 0
file3_name: .byte "udma3", 0
root_name: .byte "/", 0
usb1_name:  .byte "USB1", 0
usb1_abs_name: .byte "/USB1", 0
usb0_name:  .byte "USB0", 0
usb0_abs_name: .byte "/USB0", 0
; Generated by build.sh so the path used inside Ultimate DOS exactly matches
; the fresh remote image mounted by physical-hardware automation.
        .include "uci_dma_image_name.inc"

        .segment "DATA"
uci_base_lo:     .byte <$DF1C
uci_base_hi:     .byte >$DF1C
uci_value:       .byte 0
uci_id_byte:     .byte 0
uci_status_byte: .byte 0
softiec_bus:     .byte 0
selected_usb:    .byte 1
last_status:     .byte 0
current_state:   .byte 0
accept_state:    .byte 0
timeout_hi:      .byte 0
timeout_outer:   .byte 0
data_len:        .byte 0
data_full:       .byte 0
stat_len:        .byte 0
expected_byte:   .byte 0
reu_off_hi:      .byte 0
reu_off_lo:      .byte 0
reu_len_lo:      .byte 0
load_off:        .byte 0
ram_chunk_count: .byte 0
ram_error:       .byte 0
stat_size_lo:    .byte 0
stat_size_hi:    .byte 0
result_base_offset: .byte 0
result_value:    .byte 0
fail_offset:     .byte 0
snapshot_hi:     .byte >SNAP1_BUF
dir_count:       .byte 0
data_buf:        .res 32, 0
stat_buf:        .res 8, 0
