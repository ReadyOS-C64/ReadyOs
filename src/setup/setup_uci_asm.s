; SETUP UCI register accessors. setup_uci.c owns synchronization, async
; PUSH/ABORT, full queue drains, DATA_ACC, and final quiet IDLE.

        .export _setup_uci_asm_write_cmd, _setup_uci_asm_set_base
        .export _setup_uci_asm_id, _setup_uci_asm_status
        .export _setup_uci_asm_read_data, _setup_uci_asm_read_stat
        .export _setup_uci_asm_push_cmd, _setup_uci_asm_accept_data
        .export _setup_uci_asm_accept_more_transition
        .export _setup_uci_asm_abort, _setup_uci_asm_clear_error

CPU_PORT = $0001

_setup_uci_asm_set_base:
        sta base_lo
        stx base_hi
        sta status+1
        sta push+1
        sta accept+1
        sta more_write+1
        sta more_read+1
        sta abort+1
        sta clear_error+1
        stx status+2
        stx push+2
        stx accept+2
        stx more_write+2
        stx more_read+2
        stx abort+2
        stx clear_error+2
        lda base_lo
        clc
        adc #1
        sta write_cmd+1
        sta id+1
        lda base_hi
        adc #0
        sta write_cmd+2
        sta id+2
        lda base_lo
        clc
        adc #2
        sta data+1
        lda base_hi
        adc #0
        sta data+2
        lda base_lo
        clc
        adc #3
        sta stat+1
        lda base_hi
        adc #0
        sta stat+2
        rts

.macro MAP_IO
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
.endmacro
.macro RESTORE_IO
        pla
        sta CPU_PORT
        plp
.endmacro

_setup_uci_asm_write_cmd:
        sta value
        MAP_IO
        lda value
write_cmd: sta $DF1D
        RESTORE_IO
        lda value
        rts
_setup_uci_asm_id:
        MAP_IO
id:     lda $DF1D
        tax
        RESTORE_IO
        txa
        rts
_setup_uci_asm_status:
        MAP_IO
status: lda $DF1C
        tax
        RESTORE_IO
        txa
        rts
_setup_uci_asm_read_data:
        MAP_IO
data:   lda $DF1E
        tax
        RESTORE_IO
        txa
        rts
_setup_uci_asm_read_stat:
        MAP_IO
stat:   lda $DF1F
        tax
        RESTORE_IO
        txa
        rts
_setup_uci_asm_push_cmd:
        MAP_IO
        lda #1
push:   sta $DF1C
        RESTORE_IO
        rts
_setup_uci_asm_accept_data:
        MAP_IO
        lda #2
accept: sta $DF1C
        RESTORE_IO
        rts

; __fastcall__ old state in A. The bound detects failure; it is not pacing.
_setup_uci_asm_accept_more_transition:
        sta old_state
        MAP_IO
        lda #2
more_write: sta $DF1C
        ldx #$10
        ldy #0
accept_more_wait:
more_read:
        lda $DF1C
        and #$30
        cmp old_state
        bne accept_more_seen
        dey
        bne accept_more_wait
        dex
        bne accept_more_wait
        lda #0
        sta result
        beq accept_more_done
accept_more_seen:
        lda #1
        sta result
accept_more_done:
        RESTORE_IO
        lda result
        rts
_setup_uci_asm_abort:
        MAP_IO
        lda #4
abort:  sta $DF1C
        RESTORE_IO
        rts
_setup_uci_asm_clear_error:
        MAP_IO
        lda #8
clear_error: sta $DF1C
        RESTORE_IO
        rts

base_lo: .byte 0
base_hi: .byte 0
value: .byte 0
old_state: .byte 0
result: .byte 0
