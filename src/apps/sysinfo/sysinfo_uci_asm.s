;
; sysinfo_uci_asm.s - tiny UCI register accessors for System Info
; Low-level register operations only. sysinfo_uci.c owns synchronization,
; async PUSH/ABORT waits, complete queue drains, DATA_ACC, and quiet IDLE.
;

        .export _sysinfo_uci_asm_write_cmd
        .export _sysinfo_uci_asm_set_base
        .export _sysinfo_uci_asm_id
        .export _sysinfo_uci_asm_status
        .export _sysinfo_uci_asm_read_data
        .export _sysinfo_uci_asm_read_stat
        .export _sysinfo_uci_asm_push_cmd
        .export _sysinfo_uci_asm_accept_data
        .export _sysinfo_uci_asm_abort
        .export _sysinfo_uci_asm_clear_error
        .export _sysinfo_uci_asm_read_u64_turbo
        .export _sysinfo_uci_asm_read_u64_turbo_enable
        .export _sysinfo_uci_asm_read_softiec_bus
        .export _sysinfo_rom_asm_read_chargen
        .export _sysinfo_rom_asm_read_kernal

CPU_PORT    = $0001

_sysinfo_uci_asm_set_base:
        sta uci_base_lo
        stx uci_base_hi

        sta uci_status+1
        sta uci_push+1
        sta uci_accept+1
        sta uci_abort+1
        sta uci_clear_error+1
        stx uci_status+2
        stx uci_push+2
        stx uci_accept+2
        stx uci_abort+2
        stx uci_clear_error+2

        lda uci_base_lo
        sec
        sbc #$01
        sta uci_softiec+1
        lda uci_base_hi
        sbc #$00
        sta uci_softiec+2

        lda uci_base_lo
        clc
        adc #$01
        sta uci_write_cmd+1
        sta uci_id+1
        lda uci_base_hi
        adc #$00
        sta uci_write_cmd+2
        sta uci_id+2

        lda uci_base_lo
        clc
        adc #$02
        sta uci_data+1
        lda uci_base_hi
        adc #$00
        sta uci_data+2

        lda uci_base_lo
        clc
        adc #$03
        sta uci_stat+1
        lda uci_base_hi
        adc #$00
        sta uci_stat+2
        rts

_sysinfo_uci_asm_write_cmd:
        sta uci_value
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda uci_value
uci_write_cmd:
        sta $DF1D
        pla
        sta CPU_PORT
        plp
        lda uci_value
        rts

_sysinfo_uci_asm_id:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_id:
        lda $DF1D
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

_sysinfo_uci_asm_status:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_status:
        lda $DF1C
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

_sysinfo_uci_asm_read_data:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_data:
        lda $DF1E
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

_sysinfo_uci_asm_read_stat:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_stat:
        lda $DF1F
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

_sysinfo_uci_asm_push_cmd:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$01
uci_push:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

_sysinfo_uci_asm_accept_data:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$02
uci_accept:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

_sysinfo_uci_asm_abort:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$04
uci_abort:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

_sysinfo_uci_asm_clear_error:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda #$08
uci_clear_error:
        sta $DF1C
        pla
        sta CPU_PORT
        plp
        rts

_sysinfo_uci_asm_read_u64_turbo:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda $D031
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

_sysinfo_uci_asm_read_u64_turbo_enable:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
        lda $D030
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

_sysinfo_uci_asm_read_softiec_bus:
        php
        sei
        lda CPU_PORT
        pha
        and #$F8
        ora #$07
        sta CPU_PORT
uci_softiec:
        lda $DF1B
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

uci_base_lo:
        .byte <$DF1C
uci_base_hi:
        .byte >$DF1C
uci_value:
        .byte $00

_sysinfo_rom_asm_read_chargen:
        sta read_chargen+1
        txa
        clc
        adc #$D0
        sta read_chargen+2
        php
        sei
        lda CPU_PORT
        pha
        ora #$03
        and #$FB
        sta CPU_PORT
read_chargen:
        lda $D000
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts

_sysinfo_rom_asm_read_kernal:
        sta read_kernal+1
        txa
        clc
        adc #$E0
        sta read_kernal+2
        php
        sei
        lda CPU_PORT
        pha
        ora #$02
        sta CPU_PORT
read_kernal:
        lda $E000
        tax
        pla
        sta CPU_PORT
        plp
        txa
        rts
