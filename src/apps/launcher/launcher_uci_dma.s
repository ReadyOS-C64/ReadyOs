;
; launcher_uci_dma.s - Launcher-local Ultimate DOS REU loader.
;
; This module deliberately uses Ultimate DOS UCI only. It does not use SoftIEC.
; The experimental launcher supplies an Ultimate DOS directory path from
; apps.cfg; this module changes into that path, then loads plain filenames from
; inside the image filesystem.
;

        .export _launcher_uci_dma_detect
        .export _launcher_uci_dma_load_prg
        .export _launcher_uci_dma_available
        .export _launcher_uci_dma_name
        .export _launcher_uci_dma_reu_bank
        .export _launcher_uci_dma_reu_offset
        .export _launcher_uci_dma_max_len
        .export _launcher_uci_dma_expected_load_addr
        .export _launcher_uci_dma_loaded_size
        .export _launcher_uci_dma_last_error
        .export _launcher_uci_dma_dbg_stat0
        .export _launcher_uci_dma_dbg_stat1
        .export _launcher_uci_dma_image_dir
        .export _launcher_uci_dma_image_name
        .export _launcher_uci_dma_mount_name

CPU_PORT    = $0001

UCI_STAT_DATA  = $80
UCI_STAT_STAT  = $40
UCI_STATE_MASK = $30
UCI_STATE_IDLE = $00
UCI_STATE_LAST = $20
UCI_STATE_MORE = $30
UCI_STAT_ABORT = $04
UCI_STAT_ERROR = $08

ERR_NO_UCI  = $01
ERR_OPEN    = $02
ERR_STAT    = $03
ERR_SIZE    = $04
ERR_HEADER  = $05
ERR_SEEK    = $06
ERR_LOAD    = $07
ERR_PATH    = $08
ERR_CD_DIR  = $09
ERR_CD_DIR_STATUS = $0A
ERR_MOUNT   = $0B
ERR_MOUNT_STATUS = $0C
ERR_CD_IMAGE = $0D
ERR_CD_IMAGE_STATUS = $0E
ERR_CD_ROOT = $0F
ERR_CD_ROOT_STATUS = $10

        .macro BCC_FAR target
        bcs :+
        jmp target
:
        .endmacro

        .macro BEQ_FAR target
        bne :+
        jmp target
:
        .endmacro

        .macro BNE_FAR target
        beq :+
        jmp target
:
        .endmacro

        .segment "CODE"

_launcher_uci_dma_detect:
        lda #<$DF1C
        ldx #>$DF1C
        jsr try_base
        bcs detect_yes
        lda #<$DE1C
        ldx #>$DE1C
        jsr try_base
        bcs detect_yes
        lda #<$DFFC
        ldx #>$DFFC
        jsr try_base
        bcs detect_yes
        lda #$00
        sta _launcher_uci_dma_available
        tax
        rts
detect_yes:
        lda #$01
        sta _launcher_uci_dma_available
        tax
        rts

try_base:
        jsr set_uci_base
        jsr uci_id
        and #$7F
        cmp #$49
        beq try_base_yes
        clc
        rts
try_base_yes:
        sec
        rts

_launcher_uci_dma_load_prg:
        lda #'0'
        jsr debug_stage
        lda #$00
        sta _launcher_uci_dma_loaded_size
        sta _launcher_uci_dma_loaded_size+1
        sta _launcher_uci_dma_last_error
        jsr _launcher_uci_dma_detect
        bne load_have_uci
        lda #ERR_NO_UCI
        jmp load_fail
load_have_uci:
        lda #'1'
        jsr debug_stage
        jsr dos_identify
        BCC_FAR load_no_uci_fail
        jsr ctrl_get_drvinfo
        BCC_FAR load_no_uci_fail
        lda _launcher_uci_dma_name
        sta open_name_abs+1
        lda _launcher_uci_dma_name+1
        sta open_name_abs+2
        lda _launcher_uci_dma_image_dir
        ora _launcher_uci_dma_image_dir+1
        beq load_path_missing
        lda _launcher_uci_dma_image_name
        ora _launcher_uci_dma_image_name+1
        beq load_path_missing
        lda _launcher_uci_dma_mount_name
        ora _launcher_uci_dma_mount_name+1
        bne load_have_image_path
load_path_missing:
        lda #ERR_PATH
        jmp load_fail
load_have_image_path:
        lda #<root_name
        sta cd_name_abs+1
        lda #>root_name
        sta cd_name_abs+2
        jsr dos_cd
        BCC_FAR load_cd_root_fail

        lda _launcher_uci_dma_image_dir
        sta cd_check_abs+1
        sta cd_root_check_abs+1
        sta cd_retry_slash_check_abs+1
        sta cd_retry_nonroot_check_abs+1
        sta cd_name_abs+1
        lda _launcher_uci_dma_image_dir+1
        sta cd_check_abs+2
        sta cd_root_check_abs+2
        sta cd_retry_slash_check_abs+2
        sta cd_retry_nonroot_check_abs+2
        sta cd_name_abs+2
        ldy #$00
cd_empty_check:
cd_check_abs:
        lda $FFFF,y
        bne cd_path_has_text
        lda #ERR_PATH
        jmp load_fail
cd_path_has_text:
        cmp #'/'
        bne cd_path_not_root
        iny
cd_root_check_abs:
        lda $FFFF,y
        beq cd_dir_ready
cd_path_not_root:
        lda #'2'
        jsr debug_stage
        jsr dos_cd
        BCC_FAR load_cd_dir_fail
        jsr status_ok
        bcs cd_dir_ready
        ldy #$00
cd_retry_slash_check_abs:
        lda $FFFF,y
        cmp #'/'
        bne cd_dir_status_retry_fail
        iny
cd_retry_nonroot_check_abs:
        lda $FFFF,y
        beq cd_dir_status_retry_fail
        lda cd_check_abs+1
        clc
        adc #$01
        sta cd_name_abs+1
        lda cd_check_abs+2
        adc #$00
        sta cd_name_abs+2
        jsr dos_cd
        BCC_FAR load_cd_dir_fail
        jsr status_ok
        bcs cd_dir_ready
cd_dir_status_retry_fail:
        jmp load_cd_dir_status_fail

cd_dir_ready:
        lda _launcher_uci_dma_image_name
        sta cd_image_check_abs+1
        sta cd_name_abs+1
        lda _launcher_uci_dma_image_name+1
        sta cd_image_check_abs+2
        sta cd_name_abs+2
        lda _launcher_uci_dma_mount_name
        sta cd_mount_name_abs+1
        lda _launcher_uci_dma_mount_name+1
        sta cd_mount_name_abs+2
        ldy #$00
cd_image_empty_check:
cd_image_check_abs:
        lda $FFFF,y
        bne cd_image_has_text
        lda #ERR_PATH
        jmp load_fail
cd_image_has_text:
        lda #'3'
        jsr debug_stage
        jsr dos_mount_image
        BCC_FAR load_mount_fail
        lda #'4'
        jsr debug_stage
        jsr dos_cd
        BCC_FAR load_cd_image_fail
        jsr status_ok
        bcs cd_image_status_ok
        lda stat_len
        BEQ_FAR cd_image_status_ok
        lda _launcher_uci_dma_mount_name
        sta cd_name_abs+1
        lda _launcher_uci_dma_mount_name+1
        sta cd_name_abs+2
        jsr dos_cd
        BCC_FAR load_cd_image_fail
        jsr status_ok
        bcs cd_image_status_ok
        lda stat_len
        BEQ_FAR cd_image_status_ok
        jmp load_cd_image_status_fail
cd_image_status_ok:
        lda #'5'
        jsr debug_stage

        jsr dos_open_read
        BCC_FAR load_open_fail
        jsr status_ok
        BCC_FAR load_open_fail
        lda #'7'
        jsr debug_stage

        jsr dos_file_info
        BCC_FAR load_stat_fail
        jsr data_or_status_ok
        BCC_FAR load_stat_fail
        lda data_len
        cmp #$04
        BCC_FAR load_stat_fail
        lda data_buf+2
        ora data_buf+3
        BNE_FAR load_size_fail
        lda data_buf
        sec
        sbc #$02
        sta remaining_lo
        lda data_buf+1
        sbc #$00
        sta remaining_hi
        BCC_FAR load_size_fail
        lda remaining_lo
        ora remaining_hi
        BEQ_FAR load_size_fail
        lda _launcher_uci_dma_max_len+1
        cmp remaining_hi
        bcc load_size_fail_local
        bne load_size_ok
        lda _launcher_uci_dma_max_len
        cmp remaining_lo
        bcs load_size_ok
load_size_fail_local:
        jmp load_size_fail
load_size_ok:
        lda #$00
        sta _launcher_uci_dma_loaded_size
        sta _launcher_uci_dma_loaded_size+1

        jsr dos_read_header
        BCC_FAR load_header_fail
        jsr data_or_status_ok
        BCC_FAR load_header_fail
        lda #'8'
        jsr debug_stage
        lda data_len
        cmp #$02
        BCC_FAR load_header_fail
        lda data_buf
        cmp _launcher_uci_dma_expected_load_addr
        BNE_FAR load_header_fail
        lda data_buf+1
        cmp _launcher_uci_dma_expected_load_addr+1
        BNE_FAR load_header_fail

        jsr dos_seek_payload
        BCC_FAR load_seek_fail
        jsr status_ok
        BCC_FAR load_seek_fail
        lda #'9'
        jsr debug_stage

        lda _launcher_uci_dma_reu_offset
        sta current_off_lo
        lda _launcher_uci_dma_reu_offset+1
        sta current_off_hi
load_loop:
        lda remaining_lo
        ora remaining_hi
        beq load_success
        lda remaining_lo
        sta chunk_lo
        lda remaining_hi
        sta chunk_hi
        lda #'9'
        jsr debug_stage
        jsr dos_load_reu_chunk
        BCC_FAR load_load_fail
        jsr status_ok
        BCC_FAR load_load_fail

        lda current_off_lo
        clc
        adc chunk_lo
        sta current_off_lo
        lda current_off_hi
        adc chunk_hi
        sta current_off_hi

        lda _launcher_uci_dma_loaded_size
        clc
        adc chunk_lo
        sta _launcher_uci_dma_loaded_size
        lda _launcher_uci_dma_loaded_size+1
        adc chunk_hi
        sta _launcher_uci_dma_loaded_size+1

        lda remaining_lo
        sec
        sbc chunk_lo
        sta remaining_lo
        lda remaining_hi
        sbc chunk_hi
        sta remaining_hi
        jmp load_loop

load_success:
        jsr dos_close
        lda #$00
        sta _launcher_uci_dma_last_error
        lda #$01
        tax
        rts
load_stat_fail:
        lda #ERR_STAT
        jmp load_fail_close
load_size_fail:
        lda #ERR_SIZE
        jmp load_fail_close
load_open_fail:
        lda #ERR_OPEN
        jmp load_fail_close
load_header_fail:
        lda #ERR_HEADER
        jmp load_fail_close
load_seek_fail:
        lda #ERR_SEEK
        jmp load_fail_close
load_load_fail:
        lda #ERR_LOAD
        jmp load_fail_close
load_cd_root_fail:
        lda #ERR_CD_ROOT
        jmp load_fail_close
load_cd_root_status_fail:
        lda #ERR_CD_ROOT_STATUS
        jmp load_fail_close
load_cd_dir_fail:
        lda #ERR_CD_DIR
        jmp load_fail_close
load_cd_dir_status_fail:
        lda #ERR_CD_DIR_STATUS
        jmp load_fail_close
load_mount_fail:
        lda #ERR_MOUNT
        jmp load_fail_close
load_mount_status_fail:
        lda #ERR_MOUNT_STATUS
        jmp load_fail_close
load_cd_image_fail:
        lda #ERR_CD_IMAGE
        jmp load_fail_close
load_cd_image_status_fail:
        lda #ERR_CD_IMAGE_STATUS
        jmp load_fail_close
load_path_fail:
        lda #ERR_PATH
load_fail_close:
        pha
        jsr dos_close
        pla
load_fail:
        sta _launcher_uci_dma_last_error
        lda stat_buf
        sta _launcher_uci_dma_dbg_stat0
        lda stat_buf+1
        sta _launcher_uci_dma_dbg_stat1
        lda #$00
        tax
        rts
load_no_uci_fail:
        lda #ERR_NO_UCI
        jmp load_fail

dos_identify:
        jsr sync_interface
        BCC_FAR command_fail
        lda #$01
        jsr uci_write_cmd
        lda #$01
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

ctrl_get_drvinfo:
        jsr sync_interface
        BCC_FAR command_fail
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
        BCC_FAR command_fail
        lda #$01
        jsr uci_write_cmd
        lda #$03
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_cd:
        jsr sync_interface
        BCC_FAR command_fail
        lda #$01
        jsr uci_write_cmd
        lda #$11
        jsr uci_write_cmd
        ldy #$00
dos_cd_name:
cd_name_abs:
        lda $FFFF,y
        beq dos_cd_push
        jsr uci_write_cmd
        iny
        bne dos_cd_name
dos_cd_push:
        jsr uci_push_cmd
        jmp drain_response

dos_mount_image:
        jsr sync_interface
        BCC_FAR command_fail
        lda #$01
        jsr uci_write_cmd
        lda #$23
        jsr uci_write_cmd
        lda #$08
        jsr uci_write_cmd
        ldy #$00
dos_mount_name:
cd_mount_name_abs:
        lda $FFFF,y
        beq dos_mount_push
        jsr uci_write_cmd
        iny
        bne dos_mount_name
dos_mount_push:
        jsr uci_push_cmd
        jmp drain_response

dos_open_read:
        jsr sync_interface
        BCC_FAR command_fail
        lda #$01
        jsr uci_write_cmd
        lda #$02
        jsr uci_write_cmd
        lda #$01
        jsr uci_write_cmd
        ldy #$00
dos_open_name:
open_name_abs:
        lda $FFFF,y
        beq dos_open_push
        jsr uci_write_cmd
        iny
        bne dos_open_name
dos_open_push:
        jsr uci_push_cmd
        jmp drain_response

dos_read_header:
        jsr sync_interface
        BCC_FAR command_fail
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

dos_file_info:
        jsr sync_interface
        BCC_FAR command_fail
        lda #$01
        jsr uci_write_cmd
        lda #$07
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

dos_seek_payload:
        jsr sync_interface
        BCC_FAR command_fail
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
        BCC_FAR command_fail
        lda #$01
        jsr uci_write_cmd
        lda #$21
        jsr uci_write_cmd
        lda current_off_lo
        jsr uci_write_cmd
        lda current_off_hi
        jsr uci_write_cmd
        lda _launcher_uci_dma_reu_bank
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        lda chunk_lo
        jsr uci_write_cmd
        lda chunk_hi
        jsr uci_write_cmd
        lda #$00
        jsr uci_write_cmd
        jsr uci_write_cmd
        jsr uci_push_cmd
        jmp drain_response

command_fail:
        clc
        rts

debug_stage:
        ; Hardware note: this visible screen store is part of the
        ; C64U-proven UCI transaction shape. Replacing the stage writes with
        ; private BSS stores made the same logical loader fall back on real
        ; hardware, so do not "clean this up" without retesting on C64U.
        sta $052C
        rts

sync_interface:
        lda #$40
        sta timeout_hi
sync_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        beq sync_no_error
        jsr uci_clear_error
        lda #$40
        sta timeout_hi
        jmp sync_loop
sync_no_error:
        lda last_status
        and #UCI_STAT_ABORT
        beq sync_no_abort
        jsr uci_abort
        lda #$40
        sta timeout_hi
        jmp sync_loop
sync_no_abort:
        lda last_status
        and #UCI_STAT_DATA
        beq sync_no_data
        jsr uci_read_data
        lda #$40
        sta timeout_hi
        jmp sync_loop
sync_no_data:
        lda last_status
        and #UCI_STAT_STAT
        beq sync_no_stat
        jsr uci_read_stat
        lda #$40
        sta timeout_hi
        jmp sync_loop
sync_no_stat:
        lda last_status
        and #UCI_STATE_MASK
        cmp #UCI_STATE_LAST
        beq sync_accept
        cmp #UCI_STATE_MORE
        beq sync_accept
        lda last_status
        and #$31
        beq sync_ok
        dec timeout_hi
        bne sync_loop
        jsr uci_abort
        clc
        rts
sync_accept:
        jsr uci_accept_data
        lda #$40
        sta timeout_hi
        jmp sync_loop
sync_ok:
        sec
        rts

wait_data_state:
        lda #$80
        sta timeout_hi
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
        cmp #UCI_STATE_IDLE
        beq wait_state_ok
        dey
        bne wait_state_loop
        dec timeout_hi
        bne wait_state_loop
wait_state_fail:
        clc
        rts
wait_state_ok:
        sec
        rts

wait_idle:
        lda #$40
        sta timeout_hi
        ldy #$00
wait_idle_loop:
        jsr uci_status
        sta last_status
        lda last_status
        and #$31
        beq wait_idle_ok
        dey
        bne wait_idle_loop
        dec timeout_hi
        bne wait_idle_loop
        clc
        rts
wait_idle_ok:
        sec
        rts

drain_response:
        lda #$00
        sta data_len
        sta stat_len
        jsr wait_data_state
        bcs drain_loop
        clc
        rts
drain_loop:
        jsr uci_status
        sta last_status
        and #UCI_STAT_ERROR
        beq drain_no_error
        clc
        rts
drain_no_error:
        lda last_status
        and #UCI_STATE_MASK
        sta current_state
        beq drain_done
        cmp #UCI_STATE_LAST
        beq drain_state_ok
        cmp #UCI_STATE_MORE
        beq drain_state_ok
        jsr wait_data_state
        bcs drain_loop
        clc
        rts
drain_state_ok:
        lda #$10
        sta timeout_hi
        ldy #$00
drain_bytes:
        jsr uci_status
        sta last_status
        and #UCI_STAT_DATA
        beq drain_check_stat
        jsr uci_read_data
        ldx data_len
        cpx #$20
        bcs drain_data_count
        sta data_buf,x
drain_data_count:
        lda data_len
        cmp #$FF
        beq drain_reset_guard
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
        lda #$10
        sta timeout_hi
        ldy #$00
        jmp drain_bytes
drain_wait_byte:
        dey
        bne drain_bytes
        dec timeout_hi
        bne drain_bytes
        jsr uci_accept_data
        lda current_state
        cmp #UCI_STATE_LAST
        bne drain_more
        jsr wait_idle
        sec
        rts
drain_more:
        jsr wait_data_state
        bcc drain_more_fail
        jmp drain_loop
drain_more_fail:
        clc
        rts
drain_done:
        sec
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
        beq status_not_ok
        sec
        rts

set_uci_base:
        sta uci_base_lo
        stx uci_base_hi
        lda uci_base_lo
        sta uci_status_abs+1
        sta uci_push_abs+1
        sta uci_accept_abs+1
        sta uci_abort_abs+1
        sta uci_clear_abs+1
        clc
        adc #$01
        sta uci_write_abs+1
        sta uci_id_abs+1
        clc
        adc #$01
        sta uci_data_abs+1
        clc
        adc #$01
        sta uci_stat_abs+1
        lda uci_base_hi
        sta uci_status_abs+2
        sta uci_push_abs+2
        sta uci_accept_abs+2
        sta uci_abort_abs+2
        sta uci_clear_abs+2
        sta uci_write_abs+2
        sta uci_id_abs+2
        sta uci_data_abs+2
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

        .segment "RODATA"

root_name: .byte "/", 0

        .segment "BSS"

_launcher_uci_dma_available:           .res 1
_launcher_uci_dma_name:                .res 2
_launcher_uci_dma_reu_bank:            .res 1
_launcher_uci_dma_reu_offset:          .res 2
_launcher_uci_dma_max_len:             .res 2
_launcher_uci_dma_expected_load_addr:  .res 2
_launcher_uci_dma_loaded_size:         .res 2
_launcher_uci_dma_last_error:          .res 1
_launcher_uci_dma_dbg_stat0:           .res 1
_launcher_uci_dma_dbg_stat1:           .res 1
_launcher_uci_dma_image_dir:           .res 2
_launcher_uci_dma_image_name:          .res 2
_launcher_uci_dma_mount_name:          .res 2

uci_base_lo:       .res 1
uci_base_hi:       .res 1
uci_value:         .res 1
last_status:       .res 1
current_state:     .res 1
timeout_hi:        .res 1
data_len:          .res 1
stat_len:          .res 1
remaining_lo:      .res 1
remaining_hi:      .res 1
chunk_lo:          .res 1
chunk_hi:          .res 1
current_off_lo:    .res 1
current_off_hi:    .res 1
dma_stage_breadcrumb: .res 1
data_buf:          .res 32
stat_buf:          .res 8
