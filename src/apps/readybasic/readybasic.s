;
; readybasic.s - lean ReadyBASIC REU plugin command spine
;
; Load address: $1000
; Visible resident core: $1200-$2ABF
; Low command overlay: $A900-$BFFF, under BASIC ROM
; Shared call/result buffers: $C200-$C5FF
; BASIC workspace: $2AC1-$9FFF
; Runtime zero page/stack snapshot: loader-assigned core bank, $0A00/$0B00
; Hidden helper shadow: ReadyBASIC core REU bank offset $3000
; Bridge state/trampolines: $C000-$C1FF
; $C600-$C7FF: deliberately unused app-private snapshot room
;
; BUILD CONTRACT: this source depends on ca65 plus the custom
; cfg/ready_app_readybasic.cfg load/run layout.  Its cold-load seed is a compact
; $1000-$7FFF PRG whose bytes are relocated at startup into $A000-$C5FF and
; loader-assigned REU resource banks.  Do not build it with ready_app.cfg or
; move a run segment without updating both the seed region and
; build_support/verify_readybasic_plugin.py.
;

        .setcpu "6502"

        .export rb_hotkey_pending

; ---------------------------------------------------------------------------
; ROM/KERNAL entry points
; ---------------------------------------------------------------------------

CHRGET          = $0073
CHRGOT          = $0079
BASIC_READY     = $A474
BASIC_NEXT_STMT = $A7AE
BASIC_GONE      = $A7E4
BASIC_EVAL      = $AE86
BASIC_FRMEVL    = $AD9E
BASIC_FRMNUM    = $AD8A
BASIC_CHKCOM    = $AEFD
BASIC_SYNERR    = $AF08
BASIC_PTRGET    = $B08B
BASIC_FADD      = $B867
BASIC_GIVAYF    = $B391
BASIC_PUTNEW    = $B4CA
BASIC_FRESTR    = $B6A3
BASIC_MOVFM     = $BBA2
BASIC_MOVMF     = $BBD4
BASIC_GETADR    = $B7F7
BASIC_LINPRT    = $BDCD
BASIC_RESTORE_VECTORS = $E453
BASIC_RESET_TXTPTR = $A68E
BASIC_INIT_ZP   = $E3BF

K_CHROUT        = $FFD2
K_CLRCHN        = $FFCC
K_CHKIN         = $FFC6
K_CLOSE         = $FFC3
K_OPEN          = $FFC0
K_PLOT          = $FFF0
K_READST        = $FFB7
K_RESTOR        = $FF8A
K_SETLFS        = $FFBA
K_SETNAM        = $FFBD
K_CHRIN         = $FFCF

; ---------------------------------------------------------------------------
; BASIC/KERNAL workspace
; ---------------------------------------------------------------------------

VALTYP          = $0D
INTFLG          = $0E
LINNUM          = $14
TXTTAB          = $2B
VARTAB          = $2D
ARYTAB          = $2F
STREND          = $31
FRETOP          = $33
MEMSIZ          = $37
CURLIN          = $39
TXTPTR          = $7A
DFLTN           = $99
MSGFLG          = $9D
KBD_SCREEN_MODE = $D0
VARPNT          = $47
SUBFLG          = $10
DSCPTR          = $64
DSCPTR_HI       = $65
FAC_EXP         = $61
FAC_MANT1       = $62
FAC_MANT2       = $63
KEYD_COUNT      = $00C6
LSTX            = $00C5
SFDX            = $00CB
RPTFLG          = $028A
KOUNT           = $028B
DELAY           = $028C
SHFLAG          = $028D
LSTSHF          = $028E
KEYLOG_VEC      = $028F
KEYD_BUFFER     = $0277
COLOR_CODE      = $0286
KERNAL_MEMTOP   = $0281
KERNAL_MEMBOT   = $0283
IMAIN_VEC       = $0302
KERNAL_CHRIN_VEC = $0324
KERNAL_GETIN_VEC = $032A
CIA1_PRA        = $DC00
CIA1_PRB        = $DC01
CIA1_DDRA       = $DC02
CIA1_DDRB       = $DC03
CIA2_PRA        = $DD00
CIA2_DDRA       = $DD02

BASIC_START     = $2AC1
BASIC_SENTINEL  = BASIC_START - 1
BASIC_LIMIT     = $A000
BASIC_BYTES_FREE = BASIC_LIMIT - (BASIC_START + 2)
BASIC_INPUT_BUF = $0200
BASIC_INPUT_MAX = $58
RUNTIME_ZP_BUF  = $C400
RUNTIME_STACK_BUF = $C500

CPU_DDR         = $0000
CPU_PORT        = $0001
SCREEN          = $0400
COLOR_RAM       = $D800
VIC_MEM         = $D018
VIC_CTRL1       = $D011
VIC_CTRL2       = $D016
VIC_SPR_X_MSB   = $D010
VIC_SPR_ENABLE  = $D015
VIC_SPR_EXP_Y   = $D017
VIC_SPR_PRIORITY= $D01B
VIC_SPR_MCOLOR  = $D01C
VIC_SPR_EXP_X   = $D01D
VIC_SPR_COLL    = $D01E
VIC_BG_COLL     = $D01F
VIC_SPR_MCOLOR0 = $D025
VIC_SPR_MCOLOR1 = $D026
VIC_SPR_COLOR0  = $D027
VIC_BORDER      = $D020
VIC_BG          = $D021
SID_BASE        = $D400
SID_V1_FREQ_LO  = $D400
SID_V1_FREQ_HI  = $D401
SID_V1_PW_LO    = $D402
SID_V1_PW_HI    = $D403
SID_V1_CTRL     = $D404
SID_V1_AD       = $D405
SID_V1_SR       = $D406
SID_FILTER_LO   = $D415
SID_FILTER_HI   = $D416
SID_FILTER_RES  = $D417
SID_MODE_VOL    = $D418

SHIM_RETURN     = $C80C
SHIM_SWITCH     = $C80F
SHIM_TARGET_BANK = $C820
SHIM_CURRENT_BANK = $C834
SHIM_READYOS_BANK = $C83B
SHIM_LAUNCHER_FLAGS = $C83C
SHIM_LAUNCHER_FLAG_SUPPRESS_STARTUP = $01

; Scratch pointers outside cc65's reserved zero-page runtime.
rb_ptr_lo       = $FB
rb_ptr_hi       = $FC
rb_ptr2_lo      = $FD
rb_ptr2_hi      = $FE

RB_MAGIC_READY  = $52
RB_MAGIC_RUN    = $B2
RB_MAGIC2       = $A6
RB_STATE_MAGIC1 = $72
RB_STATE_MAGIC2 = $62
RB_RESUME_READY = 0
RB_RESUME_RUN   = 1
TOKEN_THEN      = $A7
TOKEN_END       = $80
TOKEN_REM       = $8F
TOKEN_PLUS      = $AA
TOKEN_EQUAL     = $B2
KEY_CTRL_B      = 2
KEY_F2          = 137
KEY_F4          = 138
KEY_MATRIX_B    = $1C
KEY_MATRIX_F1   = $04
KEY_MATRIX_F3   = $05
SHFLAG_SHIFT    = $01
SHFLAG_CTRL     = $04
JIFFY_LOW       = $00A2
RB_HOTKEY_RELEASE_TIMEOUT = 120
APP_BANK_MIN    = 1
APP_BANK_MAX_PLUS_ONE = 65

RAM_UNDER_BASIC = $FD
RAM_UNDER_BASIC_KEEP_KERNAL = $FE
VIC_MEM_LOWERCASE = $16

; ---------------------------------------------------------------------------
; ReadyBASIC plugin ABI constants
; ---------------------------------------------------------------------------

RB_SLOT0_BASE   = $A800
RB_SLOT1_BASE   = $B000
RB_SLOT2_BASE   = $B800
RB_LOW_BASE     = RB_SLOT0_BASE
RB_HIDDEN_BASE  = $A000
RB_SHARED       = $C200
RB_CF           = $C200
RB_RF           = $C300
RB_DESC_BUF     = $C480
RB_CMDBUF       = $C4A0
RB_PAGEBUF      = $C500

CF_CMD_ID       = RB_CF + $00
CF_PARAM_COUNT  = RB_CF + $01
CF_NUM0_LO      = RB_CF + $10
CF_NUM0_HI      = RB_CF + $11
CF_NUM1_LO      = RB_CF + $12
CF_NUM1_HI      = RB_CF + $13
CF_NUM2_LO      = RB_CF + $14
CF_NUM2_HI      = RB_CF + $15
CF_NUM3_LO      = RB_CF + $16
CF_NUM3_HI      = RB_CF + $17
CF_NUM4_LO      = RB_CF + $18
CF_NUM4_HI      = RB_CF + $19
CF_NUM5_LO      = RB_CF + $1A
CF_NUM5_HI      = RB_CF + $1B
CF_FLOAT0       = RB_CF + $20
CF_FLOAT1       = RB_CF + $25
CF_FLOAT2       = RB_CF + $2A
CF_PTR0_LO      = RB_CF + $40
CF_PTR0_HI      = RB_CF + $41
CF_COUNT0_LO    = RB_CF + $42
CF_COUNT0_HI    = RB_CF + $43
CF_STR_LEN      = RB_CF + $50
CF_STR_BUF      = RB_CF + $60

RF_STATUS       = RB_RF + $00
RF_ERROR        = RB_RF + $01
RF_TAG          = RB_RF + $02
RF_VAL_LO       = RB_RF + $03
RF_VAL_HI       = RB_RF + $04
RF_COUNT_LO     = RB_RF + $05
RF_COUNT_HI     = RB_RF + $06
RF_FLOAT        = RB_RF + $08
RF_STR_LEN      = RB_RF + $10
RF_STR_BUF      = RB_RF + $20
RF_ARRAY_BUF    = RB_RF + $80
RF_RECT_BUF     = RF_ARRAY_BUF + $20

RB_VAL_NONE     = 0
RB_VAL_INT      = 1
RB_VAL_STRING   = 2
RB_VAL_ARRAYI   = 3
RB_VAL_FLOAT    = 4

RB_OUT_NONE     = 0
RB_OUT_INT      = 1
RB_OUT_STRING   = 2
RB_OUT_ARRAYI   = 3
RB_OUT_FLOAT    = 4

RB_MODULE_SYSTEM = 1
RB_MODULE_GFX    = 3
RB_MODULE_SID    = 4
RB_SUBMOD_COMMON = 0
RB_SUBMOD_LEGACY_LOW = 1
RB_SUBMOD_PROOF_SLOT1 = 2
RB_SUBMOD_PROOF_SLOT2 = 3
RB_SUBMOD_PROOF_SPAN = 4
RB_SUBMOD_PROOF_OVERLAY = 5
RB_SUBMOD_GFXCORE = 16
RB_SUBMOD_GFXPRIM = 17
RB_SUBMOD_GFXSPR  = 18
RB_SUBMOD_INPUTEV = 19
RB_SUBMOD_GFXPOLY = 20
RB_SUBMOD_GFXDL   = 21
RB_SUBMOD_GFXTILE = 22
RB_SUBMOD_SIDCORE = 23
RB_SLOT_LEGACY_LOW = $01
RB_SLOT_PROOF_1 = $02
RB_SLOT_PROOF_2 = $04
RB_SLOT_PROOF_12 = RB_SLOT_PROOF_1 | RB_SLOT_PROOF_2
RB_SLOT_COMMON = $80
RB_SLOT_INVALID = $FF

RB_REU_TYPE_CORE= 14
RB_REU_TYPE_CODE= 15
RB_REU_HEADER_OFF  = $0000
RB_REUCB_BANK_TYPE_OFF = $B840
RB_REUCB_TOKEN_STATUS_OFF = $BA40
RB_REUCB_TOKEN_APP_OFF = $C000
RB_REUCB_APP_REG_OFF = $BC00
RB_REUCB_APP_REG_COUNT = 64
RB_REU_DESC_OFF    = $1000
RB_REU_SLOT_STATE_OFF = $2000
RB_REU_CALL_OFF    = $0400
RB_REU_RESULT_OFF  = $0400
RB_REU_DEBUG_OFF   = $0600
RB_REU_HANDLE_OFF  = $0800
RB_REU_HEAP_OFF    = $0C00
RB_REU_RUNTIME_ZP_OFF = $0A00
RB_REU_RUNTIME_STACK_OFF = $0B00
RB_REU_HIDDEN_SHADOW_OFF = $3000
RB_REU_COMMON_LIMIT= $4000
RB_REU_DATA_OFF    = $4000
RB_CODE_GFXSPR_OFF = $5000
RB_CODE_INPUTEV_OFF= $5800
RB_CODE_GFXPOLY_OFF= $6000
RB_CODE_GFXDL_OFF  = $6800
RB_CODE_GFXTILE_OFF= $7000
RB_CODE_SIDCORE_OFF= $7800

RB_CMD_DESC_SIZE   = 32
RB_CMD_DESC_COUNT  = 128
RB_CMD_DESC_PER_PAGE = 8
RB_MAX_NAME        = 15
RB_MAX_STR         = 64
RB_HANDLE_COUNT    = 128
RB_HANDLE_DESC_SIZE= 4
RB_HANDLE_PAGE_SLOTS = 64
RB_HEAP_PAGES      = 192
RB_HEAP_PAGE_BASE  = >RB_REU_DATA_OFF
RB_HANDLE_TYPE_BUFFER = 1
RB_HANDLE_TYPE_SCREEN_TC = 2
RB_HANDLE_TYPE_GFXSURF = 3
RB_HANDLE_TYPE_POINTBUF = 4
RB_HANDLE_TYPE_DLIST = 5
RB_HANDLE_TYPE_CHARSET = 6
RB_HANDLE_TYPE_TILESET = 7
RB_HANDLE_TYPE_TILEMAP = 8
RB_SCREEN_BYTES    = $03E8
RB_SCREEN_HANDLE_PAGES = 8

RB_GFX_MODE_TEXT   = 0
RB_GFX_MODE_HIRES  = 1
RB_GFX_MODE_MBITMAP= 2
RB_GFX_MODE_TILE   = 3
RB_GFX_MODE_MTILE  = 4
RB_GFX_SURF_PAGES  = 40
RB_GFX_SCREEN      = $CC00
RB_GFX_SPRITES     = $CA00
RB_GFX_BITMAP      = $E000
RB_GFX_COLOR       = $D800
RB_GFX_SPR_PTRS    = $CFF8
RB_GFX_VICMEM      = $38
RB_GFX_TARGET_HANDLE = $C4F0
RB_GFX_TARGET_BANK   = $C4F1
RB_GFX_TARGET_PAGE   = $C4F2
RB_GFX_TARGET_PAGES  = $C4F3
RB_GFX_MODE_STATE    = $C4F4
RB_DL_MAX_RECORDS    = 31
RB_DL_REC_SIZE       = 8

SIG_ZECHO1      = 1
SIG_ZADD16      = 2
SIG_UPPER       = 3
SIG_LOWER       = 4
SIG_ZHIDDENRAM  = 5
SIG_ZSUMNUMARRAY = 6
SIG_ZRANGENUMARRAY = 7
SIG_BUFNEW      = 8
SIG_BUFFILL     = 9
SIG_BUFFREE     = 10
SIG_ZTEMPSCRATCH = 11
SIG_ZFAIL       = 12
SIG_FREEMEM     = 13
SIG_SCRCAP      = 14
SIG_SCRPUT      = 15
SIG_FADD        = 16
SIG_ZPAUSE      = SIG_BUFFREE
SIG_ERRCODE     = 17
SIG_ERRLINE     = 18
SIG_GFXMODE     = 19
SIG_GFXSURF     = 20
SIG_PLOT        = 21
SIG_LINE        = 22
SIG_SPRSET      = 23
SIG_KEYNONE     = 24
SIG_POLY        = 25

CMD_ZECHO1      = 1
CMD_ZADD16      = 2
CMD_UPPER       = 3
CMD_LOWER       = 4
CMD_ZHIDDENRAM  = 5
CMD_ZSUMNUMARRAY = 6
CMD_ZRANGENUMARRAY = 7
CMD_BUFNEW      = 8
CMD_BUFFILL     = 9
CMD_BUFFREE     = 10
CMD_ZTEMPSCRATCH = 11
CMD_ZFAIL       = 12
CMD_FREEMEM     = 13
CMD_SCRCAP      = 14
CMD_SCRPUT      = 15
CMD_FADD        = 16
CMD_ZPAUSE      = 17
CMD_ERRCODE     = 18
CMD_ERRLINE     = 19
CMD_ZSLOT0      = 20
CMD_ZSLOT1      = 21
CMD_ZSLOT2      = 22
CMD_ZSPAN       = 23
CMD_ZOVL1       = 24
CMD_ZOVL2       = 25
CMD_ZCPYRST     = 26
CMD_ZCOPY       = 27
CMD_ZMODLOAD    = 28
CMD_ZDM1        = 29
CMD_ZDM2S       = 30
CMD_ZDOV1       = 31
CMD_ZDOV2       = 32
CMD_GFXMODE     = 33
CMD_GFXTEXT     = 34
CMD_GFXCLEAR    = 35
CMD_GFXSURF     = 36
CMD_GFXTARGET   = 37
CMD_GFXBLIT     = 38
CMD_GFXSYNC     = 39
CMD_PLOT        = 40
CMD_POINT       = 41
CMD_LINE        = 42
CMD_RECT        = 43
CMD_FRECT       = 44
CMD_SPRSET      = 45
CMD_SPRMOVE     = 46
CMD_SPRCOLOR    = 47
CMD_SPRSCAN     = 48
CMD_SPRCOLL     = 49
CMD_JOY         = 50
CMD_KEYP        = 51
CMD_KEYSCAN     = 52
CMD_KEYLAST     = 53
CMD_PNT         = 54
CMD_SPRROW      = 55
CMD_CIRCLE      = 56
CMD_FCIRCLE     = 57
CMD_TILE        = 58
CMD_CHARAT      = 59
CMD_SPREXPAND   = 60
CMD_SPRPRI      = 61
CMD_SPRMULTI    = 62
CMD_SPRMCOLOR   = 63
CMD_SPRSIZE     = 64
CMD_SPRCOL      = 65
CMD_SPRMCLR     = 66
CMD_SPRMUL      = 67
CMD_SPRMCO      = 68
CMD_POLY        = 69
CMD_FPOLY       = 70
CMD_PBUFNEW     = 71
CMD_PBUFSET     = 72
CMD_PBUFFREE    = 73
CMD_POLYH       = 74
CMD_FPOLYH      = 75
CMD_DLNEW       = 76
CMD_DLCLR       = 77
CMD_DLPLOT      = 78
CMD_DLLINE      = 79
CMD_DLRECT      = 80
CMD_DLFRECT     = 81
CMD_DLDRAW      = 82
CMD_CHRNEW      = 83
CMD_CHRROW      = 84
CMD_CHRUSE      = 85
CMD_TSNEW       = 86
CMD_TSSET       = 87
CMD_TMNEW       = 88
CMD_TMSET       = 89
CMD_TMDRAW      = 90
CMD_MCELL       = 91
CMD_MCBG        = 92
CMD_SIDCLR      = 93
CMD_SILENCE     = 94
CMD_VOL         = 95
CMD_FREQ        = 96
CMD_NOTE        = 97
CMD_PULSE       = 98
CMD_ADSR        = 99
CMD_ENV         = 100
CMD_WAVE        = 101
CMD_GATE        = 102
CMD_CTRL        = 103
CMD_VOICE       = 104
CMD_FILTER      = 105
CMD_FILT        = 106
CMD_SOUND       = 107
CMD_SND         = 108

; REU registers.
REU_CMD         = $DF01
REU_C64_LO      = $DF02
REU_C64_HI      = $DF03
REU_ADDR_LO     = $DF04
REU_ADDR_HI     = $DF05
REU_BANK        = $DF06
REU_LEN_LO      = $DF07
REU_LEN_HI      = $DF08

; ---------------------------------------------------------------------------
; $1000 app entry
; ---------------------------------------------------------------------------

        .segment "LOADADDR"
        .word $1000

        .segment "ENTRY"

        .import __HIDDEN_LOAD__, __HIDDEN_RUN__, __HIDDEN_SIZE__
        .import __BRIDGE_LOAD__, __BRIDGE_RUN__, __BRIDGE_SIZE__
        .import __LOWPACK_LOAD__, __LOWPACK_RUN__, __LOWPACK_SIZE__
        .import __SLOTPACK1_LOAD__, __SLOTPACK1_RUN__, __SLOTPACK1_SIZE__
        .import __SLOTPACK2_LOAD__, __SLOTPACK2_RUN__, __SLOTPACK2_SIZE__
        .import __SPANPACK_LOAD__, __SPANPACK_RUN__, __SPANPACK_SIZE__
        .import __OVL1PACK_LOAD__, __OVL1PACK_RUN__, __OVL1PACK_SIZE__
        .import __OVL2PACK_LOAD__, __OVL2PACK_RUN__, __OVL2PACK_SIZE__
        .import __OVL3PACK_LOAD__, __OVL3PACK_RUN__, __OVL3PACK_SIZE__
        .import __OVL4PACK_LOAD__, __OVL4PACK_RUN__, __OVL4PACK_SIZE__
        .import __OVL5PACK_LOAD__, __OVL5PACK_RUN__, __OVL5PACK_SIZE__
        .import __OVL6PACK_LOAD__, __OVL6PACK_RUN__, __OVL6PACK_SIZE__

rb_entry_src    = $FB
rb_entry_dst    = $FD

entry:
        lda rb_entry_magic2
        cmp #RB_MAGIC2
        bne @cold
        lda rb_entry_magic
        cmp #RB_MAGIC_READY
        beq @warm
        cmp #RB_MAGIC_RUN
        beq @warm
@cold:
        sei
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda CPU_PORT
        sta rb_entry_cpu
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        lda #<__HIDDEN_LOAD__
        sta rb_entry_src
        lda #>__HIDDEN_LOAD__
        sta rb_entry_src+1
        lda #<__HIDDEN_RUN__
        sta rb_entry_dst
        lda #>__HIDDEN_RUN__
        sta rb_entry_dst+1
        lda #<__HIDDEN_SIZE__
        sta rb_entry_len
        lda #>__HIDDEN_SIZE__
        sta rb_entry_len+1
        jsr entry_copy_block

        lda rb_entry_cpu
        sta CPU_PORT

        lda #<__BRIDGE_LOAD__
        sta rb_entry_src
        lda #>__BRIDGE_LOAD__
        sta rb_entry_src+1
        lda #<__BRIDGE_RUN__
        sta rb_entry_dst
        lda #>__BRIDGE_RUN__
        sta rb_entry_dst+1
        lda #<__BRIDGE_SIZE__
        sta rb_entry_len
        lda #>__BRIDGE_SIZE__
        sta rb_entry_len+1
        jsr entry_copy_block
        lda #RB_MAGIC_READY
        sta rb_entry_magic
        lda #RB_MAGIC2
        sta rb_entry_magic2
        jmp rb_boot
@warm:
        sei
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda CPU_PORT
        sta rb_entry_cpu
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        lda #<__HIDDEN_RUN__
        sta REU_C64_LO
        lda #>__HIDDEN_RUN__
        sta REU_C64_HI
        lda #<RB_REU_HIDDEN_SHADOW_OFF
        sta REU_ADDR_LO
        lda #>RB_REU_HIDDEN_SHADOW_OFF
        sta REU_ADDR_HI
        lda rb_reu_core_bank
        sta REU_BANK
        lda #<__HIDDEN_SIZE__
        sta REU_LEN_LO
        lda #>__HIDDEN_SIZE__
        sta REU_LEN_HI
        lda #$91
        sta REU_CMD
        lda rb_entry_cpu
        sta CPU_PORT
        jmp rb_boot

entry_copy_block:
        ldy #0
@loop:
        lda rb_entry_len
        ora rb_entry_len+1
        beq @done
        lda (rb_entry_src),y
        sta (rb_entry_dst),y
        inc rb_entry_src
        bne :+
        inc rb_entry_src+1
:       inc rb_entry_dst
        bne :+
        inc rb_entry_dst+1
:       sec
        lda rb_entry_len
        sbc #1
        sta rb_entry_len
        lda rb_entry_len+1
        sbc #0
        sta rb_entry_len+1
        jmp @loop
@done:
        rts

        .byte "READYBASIC REU",0

rb_entry_len:   .word 0
rb_entry_magic: .byte 0
rb_entry_magic2:.byte 0
rb_entry_cpu:   .byte 0

        .segment "RESIDENT"

rb_imain:
        lda #1
        sta rb_prompt_active
        jmp (rb_orig_imain_lo)

rb_chrin:
        jsr rb_call_orig_chrin
        php
        pha
        lda #0
        lda rb_hotkey_pending
        bne @dispatch
        pla
        plp
        rts
@dispatch:
        pla
        plp
        lda #1
        sta rb_hotkey_chrin_dispatch
        jmp rb_dispatch_pending_hotkey

rb_call_orig_chrin:
        jmp (rb_orig_chrin_lo)

        .segment "ENTRY"

rb_queue_hotkey_line:
        stx rb_hotkey_pending
        lda #1
        sta KEYD_COUNT
        lda #13
        sta KEYD_BUFFER
        rts

        .segment "RESIDENT"

rb_clear_hotkey_input_state:
        lda #0
        sta rb_hotkey_pending
        sta rb_prompt_active
        sta KEYD_COUNT
        ldx #9
@keyd:
        sta KEYD_BUFFER,x
        dex
        bpl @keyd
        ldx #4
@scan:
        sta RPTFLG,x
        dex
        bpl @scan
        sta KBD_SCREEN_MODE
        lda #$40
        sta LSTX
        sta SFDX
        jsr rb_scan_hotkey_matrix
        sta rb_hotkey_suppress
        rts

        .segment "ENTRY"

rb_scan_hotkey_matrix:
        lda CIA1_PRA
        pha
        lda CIA1_DDRA
        pha
        lda CIA1_DDRB
        pha
        lda #$FF
        sta CIA1_DDRA
        lda #0
        sta CIA1_DDRB
        lda #$7F
        sta CIA1_PRA
        lda CIA1_PRB
        and #$04
        bne @function_keys
        lda #$F7
        sta CIA1_PRA
        lda CIA1_PRB
        and #$10
        bne @function_keys
        ldx #KEY_CTRL_B
        jmp @restore
@function_keys:
        lda #$FD
        sta CIA1_PRA
        lda CIA1_PRB
        and #$80
        beq @shifted
        lda #$BF
        sta CIA1_PRA
        lda CIA1_PRB
        and #$10
        bne @none
@shifted:
        lda #$FE
        sta CIA1_PRA
        lda CIA1_PRB
        and #$10
        beq @f2
        lda CIA1_PRB
        and #$20
        beq @f4
@none:
        ldx #0
        jmp @restore
@f2:
        ldx #KEY_F2
        jmp @restore
@f4:
        ldx #KEY_F4
@restore:
        pla
        sta CIA1_DDRB
        pla
        sta CIA1_DDRA
        pla
        sta CIA1_PRA
        txa
        rts

rb_wait_hotkey_release:
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC_KEEP_KERNAL
        sta CPU_PORT
        cli
        jsr hidden_wait_hotkey_release
        pla
        sta CPU_PORT
        jmp rb_clear_hotkey_input_state

        .segment "HIDDEN"

hidden_wait_hotkey_release:
        lda rb_hotkey_suppress
        beq @done
        sta rb_hotkey_release_key
        lda JIFFY_LOW
        sta rb_hotkey_release_start
@wait:
        jsr rb_scan_hotkey_matrix
        cmp rb_hotkey_release_key
        bne @done
        lda JIFFY_LOW
        sec
        sbc rb_hotkey_release_start
        cmp #RB_HOTKEY_RELEASE_TIMEOUT
        bcc @wait
@done:
        rts

        .segment "ENTRY"

rb_dispatch_pending_hotkey:
        lda rb_hotkey_pending
        tax
        lda #0
        sta rb_hotkey_pending
        stx rb_hotkey_suppress
        txa
        cmp #KEY_CTRL_B
        beq @launcher
        cmp #KEY_F2
        beq @next
        cmp #KEY_F4
        beq @prev
@done:
        rts
@launcher:
        jsr rb_wait_hotkey_release
        jmp rb_hotkey_return_ready
@next:
        jsr call_hidden_select_next_app
        lda rb_found_kind
        bne rb_hotkey_switch
        jmp rb_hotkey_no_switch
@prev:
        jsr call_hidden_select_prev_app
        lda rb_found_kind
        bne rb_hotkey_switch
rb_hotkey_no_switch:
        jsr rb_wait_hotkey_release
        lda rb_hotkey_chrin_dispatch
        beq @no_switch_return
        lda #0
        sta rb_hotkey_chrin_dispatch
        ldx #$F8
        txs
        cli
        jmp BASIC_READY
@no_switch_return:
        rts
rb_hotkey_switch:
        lda rb_tmp_lo
        sta rb_hotkey_target_bank
        jsr rb_wait_hotkey_release
        jsr rb_prepare_ready_resume
        jsr prepare_shim_yield
        lda rb_hotkey_target_bank
        sta SHIM_TARGET_BANK
        jmp SHIM_SWITCH

        .segment "RESIDENT"

        .segment "PADLOW"
        .res $0000, 0

; ---------------------------------------------------------------------------
; Visible resident core.
; ---------------------------------------------------------------------------

        .segment "RESIDENT"

rb_boot:
        sei
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda #$37
        sta CPU_PORT
        lda rb_magic2
        cmp #RB_MAGIC2
        bne rb_cold_start
        lda rb_magic
        cmp #RB_MAGIC_RUN
        beq rb_resume_running
        cmp #RB_MAGIC_READY
        beq rb_resume_ready

rb_cold_start:
        jsr K_RESTOR
        jsr BASIC_RESTORE_VECTORS
        jsr install_basic_chrget
        jsr init_basic_workspace
        jsr install_vectors
        jsr rb_clear_hotkey_input_state
        lda #1
        sta rb_seed_cold
        jsr call_hidden_seed_plugin_reu
        jsr prepare_basic_console
        jsr rb_draw_header
        jsr position_basic_prompt
        lda #RB_MAGIC2
        sta rb_magic2
        lda #RB_MAGIC_READY
        sta rb_magic
        sta rb_entry_magic
        lda #RB_MAGIC2
        sta rb_entry_magic2
        cli
        jmp BASIC_READY

rb_resume_ready:
        jsr install_vectors
        jsr rb_clear_hotkey_input_state
        lda #0
        sta rb_seed_cold
        jsr call_hidden_seed_plugin_reu
        lda #RB_MAGIC_READY
        sta rb_entry_magic
        lda #RB_MAGIC2
        sta rb_entry_magic2
        cli
        jmp restore_basic_runtime_state

rb_resume_running:
        jsr install_vectors
        jsr rb_clear_hotkey_input_state
        lda #0
        sta rb_seed_cold
        jsr call_hidden_seed_plugin_reu
        lda #RB_MAGIC_RUN
        sta rb_entry_magic
        lda #RB_MAGIC2
        sta rb_entry_magic2
        cli
        jmp restore_basic_runtime_state

rb_execute:
        lda #0
        sta rb_prompt_active
        lda rb_hotkey_pending
        beq @peek
        jmp rb_dispatch_pending_hotkey
@peek:
        jsr rb_peek_next_nonspace
        tax
@normal_tx:
        txa
@normal:
        jsr rb_fold_a
        cmp #'J'
        beq @maybe_jump
        cmp #'L'
        beq @maybe_label
        cmp #'P'
        beq @maybe_proc
        cmp #'F'
        beq @maybe_func
        cmp #'E'
        beq @maybe_e
        cmp #'R'
        beq @maybe_repeat
        cmp #'U'
        beq @maybe_until
        cmp #TOKEN_END
        beq @maybe_endp
        jmp rb_maybe_bare_command
@maybe_jump:
        jsr rb_match_jump
        bcc rb_maybe_bare_command
        jmp rb_go_statement
@maybe_label:
        jsr rb_match_label
        bcc rb_maybe_bare_command
        jmp rb_label_statement
@maybe_proc:
        lda rb_peek_lo
        sta rb_ptr_lo
        lda rb_peek_hi
        sta rb_ptr_hi
        ldy #1
        lda (rb_ptr_lo),y
        jsr rb_fold_a
        cmp #'R'
        bne rb_maybe_bare_command
        jsr rb_match_proc
        bcc rb_maybe_bare_command
        jmp BASIC_SYNERR
@maybe_func:
        jsr rb_match_func
        bcc rb_maybe_bare_command
        jmp BASIC_SYNERR
@maybe_e:
        jsr rb_match_exec
        bcc :+
        jmp rb_exec_statement
:       jsr rb_match_endp
        bcc :+
        jmp rb_endp_statement
:
        jsr rb_match_exit
        bcc rb_maybe_bare_command
        jmp cmd_exit
@maybe_repeat:
        jsr rb_match_repeat
        bcc rb_maybe_bare_command
        jmp rb_repeat_statement
@maybe_until:
        jsr rb_match_until
        bcc rb_maybe_bare_command
        jmp rb_until_statement
@maybe_endp:
        jsr rb_match_endp
        bcc rb_call_orig_execute
        jmp rb_endp_statement

rb_call_orig_execute:
        jmp (rb_orig_execute_lo)

rb_maybe_bare_command:
        lda rb_peek_lo
        sta TXTPTR
        lda rb_peek_hi
        sta TXTPTR+1
        jsr rb_parse_command_name
        bcc @fallback
        jsr CHRGOT
        cmp #'('
        bne @fallback
        jsr rb_lookup_command
        bcc @fallback
        jsr CHRGET
        jsr rb_plugin_statement_found
        jsr rb_expr_expect_close
        jmp BASIC_NEXT_STMT
@fallback:
        lda rb_peek_lo
        sta rb_stmt_lo
        lda rb_peek_hi
        sta rb_stmt_hi
        jsr rb_expr_exec_assignment
        bcc :+
        jmp BASIC_NEXT_STMT
:
        lda rb_peek_lo
        bne :+
        dec rb_peek_hi
:
        dec rb_peek_lo
        lda rb_peek_lo
        sta TXTPTR
        lda rb_peek_hi
        sta TXTPTR+1
        jmp rb_call_orig_execute

rb_peek_next_nonspace:
        lda TXTPTR
        sta rb_ptr_lo
        lda TXTPTR+1
        sta rb_ptr_hi
@next:
        inc rb_ptr_lo
        bne :+
        inc rb_ptr_hi
:       ldy #0
        lda (rb_ptr_lo),y
        cmp #' '
        beq @next
        ldx rb_ptr_lo
        stx rb_peek_lo
        ldx rb_ptr_hi
        stx rb_peek_hi
        rts

rb_match_exit:
        lda rb_peek_lo
        sta rb_ptr_lo
        lda rb_peek_hi
        sta rb_ptr_hi
        ldy #1
        lda (rb_ptr_lo),y
        and #$DF
        cmp #'X'
        bne @no
        iny
        lda (rb_ptr_lo),y
        and #$DF
        cmp #'I'
        bne @no
        iny
        lda (rb_ptr_lo),y
        and #$DF
        cmp #'T'
        bne @no
        tya
        clc
        adc rb_peek_lo
        sta TXTPTR
        lda rb_peek_hi
        adc #0
        sta TXTPTR+1
        sec
        rts
@no:
        clc
        rts

rb_match_proc:
        ldx #<rb_kw_proc
        ldy #>rb_kw_proc
        jmp rb_match_keyword_xy

rb_match_func:
        ldx #<rb_kw_func
        ldy #>rb_kw_func
        jmp rb_match_keyword_xy

rb_match_exec:
        ldx #<rb_kw_exec
        ldy #>rb_kw_exec
        jmp rb_match_keyword_xy

rb_match_ret:
        ldx #<rb_kw_ret
        ldy #>rb_kw_ret
        jmp rb_match_keyword_xy

rb_match_repeat:
        ldx #<rb_kw_repeat
        ldy #>rb_kw_repeat
        jmp rb_match_keyword_xy

rb_match_until:
        ldx #<rb_kw_until
        ldy #>rb_kw_until
        jmp rb_match_keyword_xy

rb_match_label:
        ldx #<rb_kw_label
        ldy #>rb_kw_label
        jmp rb_match_keyword_xy

rb_match_jump:
        ldx #<rb_kw_jump
        ldy #>rb_kw_jump
        jmp rb_match_keyword_xy

rb_match_endp:
        lda rb_peek_lo
        sta rb_ptr_lo
        lda rb_peek_hi
        sta rb_ptr_hi
        ldy #0
        lda (rb_ptr_lo),y
        cmp #TOKEN_END
        bne @ascii
        iny
        lda (rb_ptr_lo),y
        jsr rb_fold_a
        cmp #'P'
        bne @no
        iny
        lda (rb_ptr_lo),y
        jsr rb_is_name_char
        bcs @no
        clc
        lda rb_peek_lo
        adc #1
        sta TXTPTR
        lda rb_peek_hi
        adc #0
        sta TXTPTR+1
        sec
        rts
@ascii:
        ldx #<rb_kw_endp
        ldy #>rb_kw_endp
        jmp rb_match_keyword_xy
@no:
        clc
        rts

rb_match_keyword_xy:
        stx rb_ptr2_lo
        sty rb_ptr2_hi
rb_match_keyword:
        lda rb_peek_lo
        sta rb_ptr_lo
        lda rb_peek_hi
        sta rb_ptr_hi
        ldy #0
@loop:
        lda (rb_ptr2_lo),y
        beq @boundary
        sta rb_kw_char
        lda (rb_ptr_lo),y
        jsr rb_fold_a
        cmp rb_kw_char
        bne @no
        iny
        bne @loop
@boundary:
        lda (rb_ptr_lo),y
        jsr rb_is_name_char
        bcs @no
        tya
        sec
        sbc #1
        clc
        adc rb_peek_lo
        sta TXTPTR
        lda rb_peek_hi
        adc #0
        sta TXTPTR+1
        sec
        rts
@no:
        clc
        rts

rb_fold_a:
        cmp #'a'
        bcc @done
        cmp #'z' + 1
        bcs @done
        sec
        sbc #$20
@done:
        rts

rb_is_name_char:
        cmp #'A'
        bcc @digit
        cmp #'Z' + 1
        bcc @yes
@digit:
        cmp #'0'
        bcc @no
        cmp #'9' + 1
        bcs @no
@yes:
        sec
        rts
@no:
        clc
        rts

cmd_exit:
        lda TXTPTR+1
        cmp #>BASIC_START
        bcs rb_exit_running
rb_hotkey_return_ready:
        lda #RB_RESUME_READY
        sta RUNTIME_MODE
        lda #RB_MAGIC_READY
        bne rb_exit_store_magic
rb_exit_running:
        lda #RB_RESUME_RUN
        sta RUNTIME_MODE
        lda #RB_MAGIC_RUN
rb_exit_store_magic:
        sta rb_magic
        sta rb_entry_magic
        lda #RB_MAGIC2
        sta rb_magic2
        sta rb_entry_magic2
        sei
        jsr rb_clear_hotkey_input_state
        lda SHIM_LAUNCHER_FLAGS
        ora #SHIM_LAUNCHER_FLAG_SUPPRESS_STARTUP
        sta SHIM_LAUNCHER_FLAGS
        jsr prepare_shim_yield
        jmp SHIM_RETURN

        .segment "RESIDENT"

prepare_shim_yield:
        jsr call_hidden_save_state
        lda rb_hotkey_chrin_dispatch
        beq @stack_done
        lda #0
        sta rb_hotkey_chrin_dispatch
        lda RUNTIME_MODE
        cmp #RB_RESUME_READY
        bne @stack_done
        lda #$F8
        sta RUNTIME_SP
@stack_done:
        jsr K_CLRCHN
        lda CPU_PORT
        and #RAM_UNDER_BASIC_KEEP_KERNAL
        sta CPU_PORT
        jsr hidden_restore_vectors
        rts

call_hidden_save_state:
        lda #<save_basic_runtime_state
        sta rb_lookup_index
        lda #>save_basic_runtime_state
        sta rb_lookup_slots
        bne call_hidden_common

call_hidden_seed_plugin_reu:
        lda #<rb_seed_plugin_reu_hidden
        sta rb_lookup_index
        lda #>rb_seed_plugin_reu_hidden
        sta rb_lookup_slots
        bne call_hidden_common

call_hidden_common:
        php
        sei
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda CPU_PORT
        sta rb_common_saved_cpu
        and #RAM_UNDER_BASIC_KEEP_KERNAL
        sta CPU_PORT
        jsr call_hidden_jmp
        lda rb_common_saved_cpu
        sta CPU_PORT
        plp
        rts

call_hidden_jmp:
        jmp (rb_lookup_index)

restore_basic_runtime_state:
        sei
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda CPU_PORT
        sta rb_saved_cpu
        and #RAM_UNDER_BASIC_KEEP_KERNAL
        sta CPU_PORT
        jmp hidden_restore_basic_runtime_state

restore_basic_runtime_state_fallback:
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda #$37
        sta CPU_PORT
        jmp rb_cold_start

restore_basic_finish_ready:
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda #$37
        sta CPU_PORT
        ldx RUNTIME_SP
        txs
        cli
        jmp BASIC_READY

restore_basic_finish_run:
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda #$37
        sta CPU_PORT
        ldx RUNTIME_SP
        txs
        cli
        jmp BASIC_NEXT_STMT

init_basic_workspace:
        jsr force_basic_workspace_pointers
        lda #0
        sta BASIC_SENTINEL
        sta BASIC_START
        sta BASIC_START+1
        rts

force_basic_workspace_pointers:
        jsr set_basic_memory_bounds
        lda #<BASIC_START
        sta TXTTAB
        lda #>BASIC_START
        sta TXTTAB+1
        lda #<(BASIC_START + 2)
        sta VARTAB
        sta ARYTAB
        sta STREND
        lda #>(BASIC_START + 2)
        sta VARTAB+1
        sta ARYTAB+1
        sta STREND+1
        lda #<BASIC_LIMIT
        sta FRETOP
        sta MEMSIZ
        lda #>BASIC_LIMIT
        sta FRETOP+1
        sta MEMSIZ+1
        jmp BASIC_RESET_TXTPTR

install_basic_chrget:
        jmp BASIC_INIT_ZP

set_basic_memory_bounds:
        lda #<BASIC_LIMIT
        sta KERNAL_MEMTOP
        lda #>BASIC_LIMIT
        sta KERNAL_MEMTOP+1
        lda #<BASIC_SENTINEL
        sta KERNAL_MEMBOT
        lda #>BASIC_SENTINEL
        sta KERNAL_MEMBOT+1
        rts

prepare_basic_console:
        lda #0
        sta KEYD_COUNT
        lda #VIC_MEM_LOWERCASE
        sta VIC_MEM
        jsr K_CLRCHN
        lda #147
        jsr K_CHROUT
        lda #1
        sta COLOR_CODE
        rts

position_basic_prompt:
        ldx #39
        lda #$20
@clear_row:
        sta SCREEN + 120,x
        dex
        bpl @clear_row
        clc
        ldx #3
        ldy #0
        jsr K_PLOT
        lda #0
        sta KEYD_COUNT
        rts

rb_print_z:
        ldy #0
@loop:
        lda (rb_ptr_lo),y
        beq @done
        jsr K_CHROUT
        iny
        bne @loop
@done:
        rts

; ---------------------------------------------------------------------------
; Native PROC/FUNC dispatch.
; ---------------------------------------------------------------------------

RB_ROUT_PROC    = 1
RB_ROUT_FUNC    = 2
RB_PROC_DEPTH   = 4
RB_LOOP_DEPTH   = 4

rb_repeat_statement:
        jsr CHRGET
        lda rb_loop_depth
        cmp #RB_LOOP_DEPTH
        bcc :+
        lda #$23
        jmp rb_runtime_error
:       tax
        lda TXTPTR
        sta rb_loop_txt_lo,x
        lda TXTPTR+1
        sta rb_loop_txt_hi,x
        lda CURLIN
        sta rb_loop_cur_lo,x
        lda CURLIN+1
        sta rb_loop_cur_hi,x
        inc rb_loop_depth
        jmp BASIC_NEXT_STMT

rb_until_statement:
        jsr CHRGET
        lda rb_loop_depth
        bne :+
        lda #$24
        jmp rb_runtime_error
:       jsr rb_skip_spaces
        jsr BASIC_FRMNUM
        lda FAC_EXP
        beq @again
        dec rb_loop_depth
        jmp BASIC_NEXT_STMT
@again:
        ldx rb_loop_depth
        dex
        lda rb_loop_txt_lo,x
        sta TXTPTR
        lda rb_loop_txt_hi,x
        sta TXTPTR+1
        lda rb_loop_cur_lo,x
        sta CURLIN
        lda rb_loop_cur_hi,x
        sta CURLIN+1
        jmp BASIC_NEXT_STMT

rb_label_statement:
        jsr CHRGET
        jsr rb_parse_command_name
        bcs :+
        jmp BASIC_SYNERR
:       jmp BASIC_NEXT_STMT

rb_go_statement:
        jsr CHRGET
        jsr rb_parse_command_name
        bcs :+
        jmp BASIC_SYNERR
:       jsr rb_find_label
        bcs :+
        lda #$27
        jmp rb_runtime_error
:       jmp BASIC_NEXT_STMT

rb_exec_statement:
        jsr CHRGET
        jsr rb_parse_exec_name
        bcs :+
        jmp BASIC_SYNERR
:       lda TXTPTR
        sta rb_actual_lo
        lda TXTPTR+1
        sta rb_actual_hi
        jsr rb_find_routine
        bcs :+
        lda #$20
        jmp rb_runtime_error
:       lda #0
        sta CF_PARAM_COUNT
        sta rb_bind_expr_mode
        lda rb_found_kind
        cmp #RB_ROUT_PROC
        beq :+
        jmp BASIC_SYNERR
:
        jsr rb_bind_exec_args
        bcs :+
        jmp BASIC_SYNERR
:       lda rb_proc_depth
        cmp #RB_PROC_DEPTH
        bcc :+
        lda #$21
        jmp rb_runtime_error
:       tax
        lda TXTPTR
        sta rb_proc_ret_lo,x
        lda TXTPTR+1
        sta rb_proc_ret_hi,x
        lda CURLIN
        sta rb_proc_cur_lo,x
        lda CURLIN+1
        sta rb_proc_cur_hi,x
        inc rb_proc_depth
        lda rb_found_line_lo
        sta CURLIN
        lda rb_found_line_hi
        sta CURLIN+1
        lda rb_form_lo
        sta TXTPTR
        lda rb_form_hi
        sta TXTPTR+1
        jmp BASIC_NEXT_STMT

rb_endp_statement:
        lda rb_proc_depth
        bne :+
        lda #$22
        jmp rb_runtime_error
:       dec rb_proc_depth
        ldx rb_proc_depth
        jmp rb_proc_restore

rb_proc_restore:
        lda rb_proc_ret_lo,x
        sta TXTPTR
        lda rb_proc_ret_hi,x
        sta TXTPTR+1
        lda rb_proc_cur_lo,x
        sta CURLIN
        lda rb_proc_cur_hi,x
        sta CURLIN+1
        jmp BASIC_NEXT_STMT

rb_inc_stmt:
        inc rb_stmt_lo
        bne :+
        inc rb_stmt_hi
:       rts

rb_stmt_chr:
        lda rb_stmt_lo
        sta rb_ptr_lo
        lda rb_stmt_hi
        sta rb_ptr_hi
        ldy #0
        lda (rb_ptr_lo),y
        rts

rb_parse_exec_name:
        jsr rb_skip_spaces
        ldx #0
@loop:
        jsr rb_raw_chrgot
        jsr rb_fold_a
        jsr rb_is_name_char
        bcc @done
        cpx #RB_MAX_NAME
        bcs @bad
        sta RB_CMDBUF,x
        inx
        jsr rb_raw_chrget
        jmp @loop
@done:
        stx rb_cmd_len
        beq @bad
        sec
        rts
@bad:
        clc
        rts

rb_find_routine:
        lda #<BASIC_START
        sta rb_scan_line_lo
        lda #>BASIC_START
        sta rb_scan_line_hi
@line:
        lda rb_scan_line_lo
        sta rb_ptr_lo
        lda rb_scan_line_hi
        sta rb_ptr_hi
        ldy #0
        lda (rb_ptr_lo),y
        sta rb_next_line_lo
        iny
        lda (rb_ptr_lo),y
        sta rb_next_line_hi
        ora rb_next_line_lo
        bne :+
        jmp @miss
:
        ldy #2
        lda (rb_ptr_lo),y
        sta rb_found_line_lo
        iny
        lda (rb_ptr_lo),y
        sta rb_found_line_hi
        clc
        lda rb_scan_line_lo
        adc #4
        sta rb_stmt_lo
        lda rb_scan_line_hi
        adc #0
        sta rb_stmt_hi
@stmt:
        jsr rb_skip_stmt_spaces
        jsr rb_stmt_chr
        beq @next
        cmp #':'
        bne :+
        jsr rb_inc_stmt
        jmp @stmt
:       cmp #TOKEN_REM
        beq @next
        lda rb_stmt_lo
        sta rb_peek_lo
        lda rb_stmt_hi
        sta rb_peek_hi
        jsr rb_match_proc
        bcc @try_func
        lda #RB_ROUT_PROC
        bne @kw
@try_func:
        lda rb_stmt_lo
        sta rb_peek_lo
        lda rb_stmt_hi
        sta rb_peek_hi
        jsr rb_match_func
        bcc @skip_stmt
        lda #RB_ROUT_FUNC
@kw:
        sta rb_found_kind
        jsr CHRGET
        jsr rb_skip_spaces
        jsr rb_compare_found_name
        bcs @found
@skip_stmt:
        jsr rb_skip_to_stmt_end
        jmp @stmt
@next:
        lda rb_next_line_lo
        sta rb_scan_line_lo
        lda rb_next_line_hi
        sta rb_scan_line_hi
        jmp @line
@found:
        lda TXTPTR
        sta rb_def_lo
        lda TXTPTR+1
        sta rb_def_hi
        sec
        rts
@miss:
        clc
        rts

rb_find_label:
        lda #<BASIC_START
        sta rb_scan_line_lo
        lda #>BASIC_START
        sta rb_scan_line_hi
@line:
        lda rb_scan_line_lo
        sta rb_ptr_lo
        lda rb_scan_line_hi
        sta rb_ptr_hi
        ldy #0
        lda (rb_ptr_lo),y
        sta rb_next_line_lo
        iny
        lda (rb_ptr_lo),y
        sta rb_next_line_hi
        ora rb_next_line_lo
        beq @miss
        clc
        lda rb_scan_line_lo
        adc #4
        sta rb_stmt_lo
        lda rb_scan_line_hi
        adc #0
        sta rb_stmt_hi
        jsr rb_skip_stmt_spaces
        lda rb_stmt_lo
        sta rb_peek_lo
        lda rb_stmt_hi
        sta rb_peek_hi
        jsr rb_match_label
        bcc @next
        jsr CHRGET
        jsr rb_skip_spaces
        jsr rb_compare_found_name
        bcs @found
@next:
        lda rb_next_line_lo
        sta rb_scan_line_lo
        lda rb_next_line_hi
        sta rb_scan_line_hi
        jmp @line
@found:
        ldy #2
        lda (rb_ptr_lo),y
        sta CURLIN
        iny
        lda (rb_ptr_lo),y
        sta CURLIN+1
        sec
        rts
@miss:
        clc
        rts

rb_skip_stmt_spaces:
        ldy #0
@loop:
        jsr rb_stmt_chr
        cmp #' '
        bne @done
        jsr rb_inc_stmt
        jmp @loop
@done:
        rts

rb_skip_to_stmt_end:
        ldy #0
@loop:
        jsr rb_stmt_chr
        beq @done
        cmp #':'
        beq @done
        cmp #TOKEN_REM
        beq @line_done
        jsr rb_inc_stmt
        jmp @loop
@line_done:
        lda #0
@done:
        rts

rb_compare_found_name:
        ldy #0
@loop:
        cpy rb_cmd_len
        beq @boundary
        lda (TXTPTR),y
        jsr rb_fold_a
        cmp RB_CMDBUF,y
        bne @no
        iny
        jmp @loop
@boundary:
        lda (TXTPTR),y
        jsr rb_fold_a
        jsr rb_is_name_char
        bcs @no
        tya
        clc
        adc TXTPTR
        sta TXTPTR
        bcc :+
        inc TXTPTR+1
:       sec
        rts
@no:
        clc
        rts

rb_bind_exec_args:
        lda rb_def_lo
        sta rb_form_lo
        lda rb_def_hi
        sta rb_form_hi
@loop:
        jsr rb_next_formal
        bcs @formal
        jmp rb_actual_at_end
@formal:
        lda TXTPTR
        sta rb_form_lo
        lda TXTPTR+1
        sta rb_form_hi
        lda #0
        sta SUBFLG
        jsr BASIC_PTRGET
        lda VARPNT
        sta rb_formal_lo
        lda VARPNT+1
        sta rb_formal_hi
        lda TXTPTR
        sta rb_form_next_lo
        lda TXTPTR+1
        sta rb_form_next_hi
        lda VALTYP
        cmp #$FF
        bne :+
        jmp @string
:
        lda INTFLG
        cmp #$80
        beq @int
        jmp @float
@int:
@int_input:
        lda #RB_OUT_INT
        jsr rb_save_formal_value
        jsr rb_exec_next_actual
        bcs :+
        rts
:       jsr rb_start_numeric_actual
        lda rb_bind_expr_mode
        pha
        lda rb_formal_lo
        pha
        lda rb_formal_hi
        pha
        lda rb_form_next_lo
        pha
        lda rb_form_next_hi
        pha
        lda CF_PARAM_COUNT
        pha
        jsr BASIC_FRMNUM
        jsr BASIC_GETADR
        pla
        sta CF_PARAM_COUNT
        pla
        sta rb_form_next_hi
        pla
        sta rb_form_next_lo
        pla
        sta rb_formal_hi
        pla
        sta rb_formal_lo
        pla
        sta rb_bind_expr_mode
        jsr rb_finish_numeric_actual
@got_int:
        lda TXTPTR
        sta rb_actual_lo
        lda TXTPTR+1
        sta rb_actual_hi
        ldy #0
        lda rb_formal_lo
        sta rb_ptr_lo
        lda rb_formal_hi
        sta rb_ptr_hi
        lda LINNUM+1
        sta (rb_ptr_lo),y
        iny
        lda LINNUM
        sta (rb_ptr_lo),y
        inc CF_PARAM_COUNT
        jmp @advance_form
@float:
        lda #RB_OUT_FLOAT
        jsr rb_save_formal_value
        jsr rb_exec_next_actual
        bcs :+
        rts
:       jsr rb_start_numeric_actual
        lda rb_bind_expr_mode
        pha
        lda rb_formal_lo
        pha
        lda rb_formal_hi
        pha
        lda rb_form_next_lo
        pha
        lda rb_form_next_hi
        pha
        lda CF_PARAM_COUNT
        pha
        jsr BASIC_FRMNUM
        pla
        sta CF_PARAM_COUNT
        pla
        sta rb_form_next_hi
        pla
        sta rb_form_next_lo
        pla
        sta rb_formal_hi
        pla
        sta rb_formal_lo
        pla
        sta rb_bind_expr_mode
        jsr rb_finish_numeric_actual
        ldx rb_formal_lo
        ldy rb_formal_hi
        jsr BASIC_MOVMF
        lda TXTPTR
        sta rb_actual_lo
        lda TXTPTR+1
        sta rb_actual_hi
        inc CF_PARAM_COUNT
        jmp @advance_form
@string:
@string_input:
        lda #RB_OUT_STRING
        jsr rb_save_formal_value
        jsr rb_exec_next_actual
        bcs :+
        rts
:       lda rb_bind_expr_mode
        pha
        lda rb_formal_lo
        pha
        lda rb_formal_hi
        pha
        lda rb_form_next_lo
        pha
        lda rb_form_next_hi
        pha
        lda CF_PARAM_COUNT
        pha
        jsr rb_parse_string_value_current
        pla
        sta CF_PARAM_COUNT
        pla
        sta rb_form_next_hi
        pla
        sta rb_form_next_lo
        pla
        sta rb_formal_hi
        pla
        sta rb_formal_lo
        pla
        sta rb_bind_expr_mode
        lda TXTPTR
        sta rb_actual_lo
        lda TXTPTR+1
        sta rb_actual_hi
        jsr rb_stage_cf_string_result
        lda rb_formal_lo
        sta rb_out_ptr_lo
        lda rb_formal_hi
        sta rb_out_ptr_hi
        lda #RB_OUT_STRING
        sta rb_out_type
        lda #1
        sta rb_out_count
        jsr rb_commit_result
        inc CF_PARAM_COUNT
@advance_form:
        lda rb_form_next_lo
        sta rb_form_lo
        lda rb_form_next_hi
        sta rb_form_hi
        jmp @loop

rb_stage_cf_string_result:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_STRING
        sta RF_TAG
        lda CF_STR_LEN
        sta RF_STR_LEN
        ldy #0
@loop:
        cpy CF_STR_LEN
        beq @done
        lda CF_STR_BUF,y
        sta RF_STR_BUF,y
        iny
        jmp @loop
@done:
        rts

rb_next_formal:
        lda rb_form_lo
        sta TXTPTR
        lda rb_form_hi
        sta TXTPTR+1
        jsr rb_skip_spaces
        cmp #'('
        bne :+
        jsr CHRGET
        jsr rb_skip_spaces
:       cmp #')'
        bne :+
        jsr CHRGET
        lda TXTPTR
        sta rb_form_lo
        lda TXTPTR+1
        sta rb_form_hi
        jmp @no
:
        cmp #','
        bne :+
        jsr CHRGET
        jsr rb_skip_spaces
:       cmp #0
        beq @no
        cmp #':'
        beq @no
        sec
        rts
@no:
        clc
        rts

rb_exec_next_actual:
        lda rb_actual_lo
        sta TXTPTR
        lda rb_actual_hi
        sta TXTPTR+1
        jsr rb_skip_spaces
        ldx CF_PARAM_COUNT
        bne @comma
        cmp #'('
        beq @take
@comma:
        cmp #','
        beq :+
        clc
        rts
:
@take:  jsr CHRGET
        jsr rb_skip_spaces
        jsr rb_mark_wrapped_actual
        lda TXTPTR
        sta rb_actual_lo
        lda TXTPTR+1
        sta rb_actual_hi
        sec
        rts

rb_actual_at_end:
        lda rb_actual_lo
        sta TXTPTR
        lda rb_actual_hi
        sta TXTPTR+1
        jsr rb_skip_spaces
        cmp #')'
        bne :+
        jsr CHRGET
        jsr rb_skip_spaces
        lda rb_bind_expr_mode
        bne @ok
:
        lda rb_bind_expr_mode
        bne @bad
        cmp #0
        beq @ok
        cmp #':'
        beq @ok
@bad:
        clc
        rts
@ok:
        sec
        rts

; ---------------------------------------------------------------------------
; Command parsing and dispatch.
; ---------------------------------------------------------------------------

rb_plugin_statement_found:
        lda RB_DESC_BUF
        sta CF_CMD_ID
        lda #0
        sta CF_PARAM_COUNT
        sta rb_command_precomputed
        jsr rb_clear_result_frame
        jsr rb_parse_by_signature
        lda rb_command_precomputed
        bne @commit
        jsr rb_stash_call_frame
        jsr rb_load_and_call_command
        jsr rb_stash_result_frame
@commit:
        jmp rb_commit_result

rb_parse_command_name:
        jsr rb_skip_spaces
        ldx #0
@loop:
        jsr rb_raw_chrgot
        cmp #$A5
        bne @not_fn_token
        cpx #RB_MAX_NAME - 1
        bcc :+
        jmp @too_long
:       lda #'F'
        sta RB_CMDBUF,x
        inx
        lda #'N'
        sta RB_CMDBUF,x
        inx
        jsr rb_raw_chrget
        jmp @loop
@not_fn_token:
        cmp #$B8
        bne @not_fre_token
        cpx #RB_MAX_NAME - 2
        bcc :+
        jmp @too_long
:
        lda #'F'
        sta RB_CMDBUF,x
        inx
        lda #'R'
        sta RB_CMDBUF,x
        inx
        lda #'E'
        sta RB_CMDBUF,x
        inx
        jsr rb_raw_chrget
        jmp @loop
@not_fre_token:
        cmp #$FF
        bne @not_pi_token
        cpx #RB_MAX_NAME - 1
        bcc :+
        jmp @too_long
:
        lda #'P'
        sta RB_CMDBUF,x
        inx
        lda #'I'
        sta RB_CMDBUF,x
        inx
        jsr rb_raw_chrget
        jmp @loop
@not_pi_token:
        cmp #$C1
        bcc @not_shifted_upper
        cmp #$DB
        bcs @not_shifted_upper
        sec
        sbc #$80
        jmp @folded_case
@not_shifted_upper:
        cmp #'a'
        bcc :+
        cmp #'z' + 1
        bcs :+
        sec
        sbc #$20
@folded_case:
:       cmp #'A'
        bcc @maybe_digit
        cmp #'Z' + 1
        bcc @store
@maybe_digit:
        cpx #0
        beq @done
        cmp #'0'
        bcc @done
        cmp #'9' + 1
        bcs @done
@store:
        cpx #RB_MAX_NAME
        bcs @too_long
        sta RB_CMDBUF,x
        inx
        jsr rb_raw_chrget
        jmp @loop
@done:
        stx rb_cmd_len
        beq @bad
        jsr rb_raw_chrgot
        cmp #','
        beq @bad
        jsr rb_skip_spaces
        sec
        rts
@too_long:
@bad:
        clc
        rts

rb_raw_chrgot:
        ldy #0
        lda (TXTPTR),y
        rts

rb_raw_chrget:
        inc TXTPTR
        bne @done
        inc TXTPTR+1
@done:
        rts

rb_skip_spaces:
@loop:
        ldy #0
        jsr CHRGOT
        cmp #' '
        bne @done
        jsr CHRGET
        jmp @loop
@done:
        ldy #0
        rts

rb_parse_arg_sep:
        lda CF_PARAM_COUNT
        beq @first
        jsr BASIC_CHKCOM
        jmp rb_mark_wrapped_actual
@first:
        jsr rb_skip_spaces
        jmp rb_mark_wrapped_actual

rb_mark_wrapped_actual:
        lda #0
        sta rb_actual_wrapped
        jsr rb_skip_spaces
        cmp #'('
        bne :+
        inc rb_actual_wrapped
:       rts

rb_finish_numeric_actual:
        lda rb_actual_wrapped
        beq @done
        jsr rb_skip_spaces
        cmp #')'
        beq :+
        jmp BASIC_SYNERR
:       jsr CHRGET
@done:
        rts

rb_start_numeric_actual:
        lda rb_actual_wrapped
        beq @done
        jsr CHRGET
        jsr rb_skip_spaces
@done:
        rts

rb_lookup_command:
        lda #<hidden_lookup_command
        sta rb_lookup_index
        lda #>hidden_lookup_command
        sta rb_lookup_slots
        jsr call_hidden_common
        lda rb_found_kind
        beq @miss
        sec
        rts
@miss:
        clc
        rts

rb_parse_by_signature:
        lda RB_DESC_BUF+14
        beq @bad
        cmp #SIG_POLY + 1
        bcs @bad
        asl
        tax
        lda rb_parse_sig_table-2,x
        sta rb_ptr_lo
        lda rb_parse_sig_table-1,x
        sta rb_ptr_hi
        jmp (rb_ptr_lo)
@bad:
        jmp BASIC_SYNERR

rb_parse_sig_table:
        .word parse_sig_zecho1
        .word parse_sig_zadd16
        .word parse_sig_string_out
        .word parse_sig_string_out
        .word parse_sig_zhiddenram
        .word parse_sig_zsumnumarray
        .word parse_sig_zrangenumarray
        .word parse_sig_bufnew
        .word parse_sig_buffill
        .word rb_parse_num0
        .word parse_sig_bufnew
        .word parse_sig_zfail
        .word parse_sig_no_args
        .word rb_parse_out_int_current
        .word rb_parse_num0
        .word parse_sig_fadd
        .word parse_sig_errcode
        .word parse_sig_errline
        .word parse_sig_gfxmode
        .word parse_sig_gfxsurf
        .word parse_sig_num3
        .word parse_sig_num5
        .word parse_sig_sprset
        .word parse_sig_no_args
        .word parse_sig_poly

parse_sig_zecho1:
        jsr rb_parse_out_int_current
        jmp rb_precompute_zecho1

parse_sig_zadd16:
        jsr rb_parse_num0
        jsr rb_parse_num1
        jmp rb_parse_out_int

parse_sig_string_out:
        jsr rb_parse_string_value
        jmp rb_parse_out_string

parse_sig_zhiddenram:
        jsr rb_parse_string_value
        jmp rb_parse_out_int

parse_sig_zsumnumarray:
        jsr rb_parse_int_array_input
        jsr rb_parse_out_int
        jmp rb_resolve_int_array_input_ptr

parse_sig_zrangenumarray:
        jsr rb_parse_num0
        jsr rb_parse_num1
        lda CF_NUM1_LO
        sta rb_saved_count_lo
        lda CF_NUM1_HI
        sta rb_saved_count_hi
        jmp rb_parse_out_int_array

parse_sig_bufnew:
        jsr rb_parse_num0
        jmp rb_parse_out_int

parse_sig_buffill:
        jsr rb_parse_num0
        jmp rb_parse_num1

parse_sig_zfail:
        jsr rb_parse_num0
        jmp rb_parse_out_int

parse_sig_fadd:
        jsr rb_parse_float0
        jsr rb_parse_float1
        jsr rb_parse_out_float
        jmp rb_compute_fadd_result

parse_sig_errcode:
        jsr rb_parse_out_int_current
        jmp rb_precompute_errcode

parse_sig_errline:
        jsr rb_parse_out_int_current
        jmp rb_precompute_errline

parse_sig_no_args:
        jmp rb_parse_no_args

parse_sig_num3:
        jsr rb_parse_num0
        jsr rb_parse_num1
        jmp rb_parse_num2

parse_sig_num5:
        jsr rb_parse_num0
        jsr rb_parse_num1
        jsr rb_parse_num2
        jsr rb_parse_num3
        jmp rb_parse_num4

parse_sig_sprset:
        jsr rb_parse_num0
        jsr rb_parse_num1
        jsr rb_parse_num2
        jmp rb_parse_num3

parse_sig_gfxmode:
        jsr rb_skip_spaces
        cmp #')'
        beq :+
        jsr rb_parse_string_value_current
:       rts

parse_sig_gfxsurf:
        jsr rb_parse_string_value_current
        jsr rb_skip_spaces
        cmp #')'
        beq :+
        jsr rb_parse_out_int
:       rts

parse_sig_poly:
        lda #0
        sta CF_COUNT0_HI
        jsr rb_parse_int_array_arg
        jsr rb_resolve_int_array_input_ptr
        jsr rb_parse_num1
        jmp rb_parse_num2

rb_parse_no_args:
        jsr rb_skip_spaces
        cmp #0
        beq @ok
        cmp #':'
        beq @ok
        cmp #')'
        beq @ok
        jmp BASIC_SYNERR
@ok:
        rts

        .segment "HIDDEN"

hidden_lookup_command:
        lda #<RB_REU_DESC_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_DESC_OFF
        sta rb_reu_off_hi
        lda #0
        sta rb_lookup_index
@page:
        lda rb_lookup_index
        cmp #RB_CMD_DESC_COUNT
        bcc :+
        jmp @miss
:       lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        lda #<RB_PAGEBUF
        sta rb_ptr_lo
        lda #>RB_PAGEBUF
        sta rb_ptr_hi
        lda #RB_CMD_DESC_PER_PAGE
        sta rb_lookup_slots
@slot:
        lda rb_lookup_index
        cmp #RB_CMD_DESC_COUNT
        bcc :+
        jmp @miss
:       ldy #15
        lda (rb_ptr_lo),y
        cmp rb_cmd_len
        bne @next
        ldy #0
@cmp:
        cpy rb_cmd_len
        beq @match
        tya
        pha
        clc
        adc #16
        tay
        lda (rb_ptr_lo),y
        sta rb_lookup_char
        pla
        tay
        lda rb_lookup_char
        cmp RB_CMDBUF,y
        bne @next
        iny
        jmp @cmp
@match:
        ldy #0
@copy:
        lda (rb_ptr_lo),y
        sta RB_DESC_BUF,y
        iny
        cpy #RB_CMD_DESC_SIZE
        bcc @copy
        lda #1
        sta rb_found_kind
        rts
@next:
        clc
        lda rb_ptr_lo
        adc #RB_CMD_DESC_SIZE
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       inc rb_lookup_index
        dec rb_lookup_slots
        bne @slot
        inc rb_reu_off_hi
        jmp @page
@miss:
        lda #0
        sta rb_found_kind
        rts

        .segment "RESIDENT"

rb_precompute_zecho1:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #1
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        lda #1
        sta rb_command_precomputed
        rts

rb_precompute_errcode:
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        lda rb_last_error
        sta RF_VAL_LO
        lda #1
        sta rb_command_precomputed
        rts

rb_precompute_errline:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda rb_last_line_lo
        sta RF_VAL_LO
        lda rb_last_line_hi
        sta RF_VAL_HI
        lda #1
        sta rb_command_precomputed
        rts

rb_parse_num0:
        lda #CF_NUM0_LO-RB_CF
        bne rb_parse_num_to_slot
rb_parse_num1:
        lda #CF_NUM1_LO-RB_CF
        bne rb_parse_num_to_slot
rb_parse_num2:
        lda #CF_NUM2_LO-RB_CF
        bne rb_parse_num_to_slot
rb_parse_num3:
        lda #CF_NUM3_LO-RB_CF
        bne rb_parse_num_to_slot
rb_parse_num4:
        lda #CF_NUM4_LO-RB_CF
        bne rb_parse_num_to_slot
rb_parse_num5:
        lda #CF_NUM5_LO-RB_CF
rb_parse_num_to_slot:
        sta rb_target_off
        jsr rb_parse_arg_sep
        lda rb_target_off
        cmp #CF_NUM0_LO-RB_CF
        beq :+
        jsr rb_save_num0
:
        jsr rb_start_numeric_actual
        lda rb_target_off
        pha
        lda CF_PARAM_COUNT
        pha
        jsr BASIC_FRMNUM
        jsr BASIC_GETADR
        pla
        sta CF_PARAM_COUNT
        pla
        sta rb_target_off
        jsr rb_finish_numeric_actual
        lda rb_target_off
        cmp #CF_NUM0_LO-RB_CF
        beq :+
        jsr rb_restore_num0
:
        ldy rb_target_off
        lda LINNUM
        sta RB_CF,y
        iny
        lda LINNUM+1
        sta RB_CF,y
        inc CF_PARAM_COUNT
        rts

rb_parse_float0:
        lda #CF_FLOAT0-RB_CF
        bne rb_parse_float_to_slot
rb_parse_float1:
        lda #CF_FLOAT1-RB_CF
        bne rb_parse_float_to_slot
rb_parse_float_to_slot:
        sta rb_target_off
        jsr rb_parse_arg_sep
        lda rb_target_off
        cmp #CF_FLOAT0-RB_CF
        beq :+
        jsr rb_save_float0
:       jsr rb_start_numeric_actual
        lda rb_target_off
        pha
        lda CF_PARAM_COUNT
        pha
        jsr BASIC_FRMNUM
        pla
        sta CF_PARAM_COUNT
        pla
        sta rb_target_off
        jsr rb_finish_numeric_actual
        lda rb_target_off
        cmp #CF_FLOAT0-RB_CF
        beq :+
        jsr rb_restore_float0
:       clc
        lda rb_target_off
        adc #<RB_CF
        tax
        lda #>RB_CF
        adc #0
        tay
        jsr BASIC_MOVMF
        inc CF_PARAM_COUNT
        rts

rb_parse_out_int:
        jsr rb_parse_arg_sep
rb_parse_out_int_current:
        lda #0
        sta SUBFLG
        jsr BASIC_PTRGET
        lda VALTYP
        beq :+
        jmp rb_parse_type_error
:       lda INTFLG
        cmp #$80
        beq :+
        jmp rb_parse_type_error
:       lda #RB_OUT_INT
        sta rb_out_type
        lda VARPNT
        sta rb_out_ptr_lo
        lda VARPNT+1
        sta rb_out_ptr_hi
        ldy #0
        lda #0
        sta (VARPNT),y
        iny
        sta (VARPNT),y
        lda #1
        sta rb_out_count
        inc CF_PARAM_COUNT
        rts

rb_parse_out_string:
        jsr rb_parse_arg_sep
rb_parse_out_string_current:
        lda #0
        sta SUBFLG
        jsr BASIC_PTRGET
        lda VALTYP
        cmp #$FF
        beq :+
        jmp rb_parse_type_error
:       lda #RB_OUT_STRING
        sta rb_out_type
        lda VARPNT
        sta rb_out_ptr_lo
        lda VARPNT+1
        sta rb_out_ptr_hi
        ldy #0
        lda #0
        sta (VARPNT),y
        iny
        sta (VARPNT),y
        iny
        sta (VARPNT),y
        lda #1
        sta rb_out_count
        inc CF_PARAM_COUNT
        rts

rb_parse_out_float:
        jsr rb_parse_arg_sep
rb_parse_out_float_current:
        lda #0
        sta SUBFLG
        jsr BASIC_PTRGET
        lda VALTYP
        beq :+
        jmp rb_parse_type_error
:       lda INTFLG
        cmp #$80
        bne :+
        jmp rb_parse_type_error
:       lda #RB_OUT_FLOAT
        sta rb_out_type
        lda VARPNT
        sta rb_out_ptr_lo
        lda VARPNT+1
        sta rb_out_ptr_hi
        lda #1
        sta rb_out_count
        inc CF_PARAM_COUNT
        ldy #0
        lda #0
@clear:
        sta (VARPNT),y
        iny
        cpy #5
        bcc @clear
        rts

rb_parse_out_int_array:
        jsr rb_parse_arg_sep
        lda #0
        sta SUBFLG
        jsr BASIC_PTRGET
        lda VALTYP
        beq :+
        jmp rb_parse_type_error
:       lda INTFLG
        cmp #$80
        beq :+
        jmp rb_parse_type_error
:       lda #RB_OUT_ARRAYI
        sta rb_out_type
        jsr rb_normalize_int_array_ptr_from_varpnt
        lda rb_ptr2_lo
        sta rb_out_ptr_lo
        sta rb_ptr2_lo
        lda rb_ptr2_hi
        sta rb_out_ptr_hi
        sta rb_ptr2_hi
        lda rb_saved_count_lo
        sta rb_out_count_lo
        lda rb_saved_count_hi
        sta rb_out_count_hi
        jsr rb_clear_int_array_output
        lda #1
        sta rb_out_count
        inc CF_PARAM_COUNT
        rts

rb_parse_int_array_arg:
        jsr rb_parse_arg_sep
        lda #0
        sta SUBFLG
        jsr BASIC_PTRGET
        lda VALTYP
        beq :+
        jmp rb_parse_type_error
:       lda INTFLG
        cmp #$80
        beq :+
        jmp rb_parse_type_error
:       jsr rb_normalize_int_array_ptr_from_varpnt
        sec
        lda rb_ptr2_lo
        sbc ARYTAB
        sta CF_PTR0_LO
        lda rb_ptr2_hi
        sbc ARYTAB+1
        sta CF_PTR0_HI
        inc CF_PARAM_COUNT
        rts

rb_parse_int_array_input:
        jsr rb_parse_int_array_arg
        jsr rb_parse_num2
        lda CF_NUM2_LO
        sta CF_COUNT0_LO
        sta rb_saved_count_lo
        lda CF_NUM2_HI
        sta CF_COUNT0_HI
        sta rb_saved_count_hi
        rts

rb_parse_string_value:
        jsr rb_parse_arg_sep
rb_parse_string_value_current:
        lda CF_PARAM_COUNT
        pha
        jsr BASIC_FRMEVL
        jsr rb_stage_fac_string_to_cf
        jsr BASIC_FRESTR
        pla
        sta CF_PARAM_COUNT
        inc CF_PARAM_COUNT
        rts

rb_parse_type_error:
        jmp BASIC_SYNERR

; ---------------------------------------------------------------------------
; Expression-style command calls.
; ---------------------------------------------------------------------------

RB_EXPR_INT     = 1
RB_EXPR_STRING  = 2
RB_EXPR_FLOAT   = 3

rb_eval:
        lda TXTPTR
        sta rb_eval_save_lo
        lda TXTPTR+1
        sta rb_eval_save_hi
        jsr CHRGET
rb_eval_current:
        jsr rb_parse_command_name
        bcs :+
        jmp rb_eval_fallback
:       jsr CHRGOT
        cmp #'('
        beq :+
        jmp rb_eval_fallback
:       jsr rb_lookup_command
        bcs :+
        jsr rb_eval_func
        bcs rb_expr_return_result
        jmp rb_eval_fallback
:       jsr CHRGET
        lda RB_DESC_BUF
        sta CF_CMD_ID
        lda #0
        sta CF_PARAM_COUNT
        sta rb_out_count
        sta rb_command_precomputed
        sta INTFLG
        lda #RB_EXPR_INT
        sta rb_expr_type
        jsr rb_clear_result_frame
        jsr rb_parse_expr_signature
        jsr rb_expr_expect_close
        lda rb_command_precomputed
        bne rb_expr_return_result
        jsr rb_stash_call_frame
        jsr rb_load_and_call_command
        jsr rb_stash_result_frame
        lda RF_STATUS
        bne @error
        jmp rb_expr_return_result
@error:
        lda RF_ERROR
        bne @runtime
        lda RF_STATUS
@runtime:
        jmp rb_runtime_error
rb_expr_return_result:
        lda RF_TAG
        cmp #RB_VAL_STRING
        beq rb_expr_return_string
        lda rb_expr_type
        cmp #RB_EXPR_STRING
        beq rb_expr_return_string
        lda RF_TAG
        cmp #RB_VAL_INT
        beq @int
        cmp #RB_VAL_FLOAT
        beq rb_expr_return_float
        bne rb_expr_bad_type
@int:
        lda RF_VAL_HI
        ldy RF_VAL_LO
        jsr BASIC_GIVAYF
        clc
        rts

rb_expr_return_float:
        lda #0
        sta VALTYP
        sta INTFLG
        lda #<RF_FLOAT
        ldy #>RF_FLOAT
        jsr BASIC_MOVFM
        clc
        rts

rb_expr_return_string:
        lda RF_TAG
        cmp #RB_VAL_STRING
        bne rb_expr_bad_type
        lda RF_STR_LEN
        beq @empty
        jsr rb_alloc_string_heap
        bcs @string_error
        jmp @descriptor
@empty:
        lda #0
        sta rb_ptr_lo
        sta rb_ptr_hi
@descriptor:
        lda #$FF
        sta VALTYP
        lda RF_STR_LEN
        sta FAC_EXP
        lda rb_ptr_lo
        sta FAC_MANT1
        lda rb_ptr_hi
        sta FAC_MANT2
        lda #<FAC_EXP
        sta DSCPTR
        lda #0
        sta DSCPTR_HI
        jsr BASIC_PUTNEW
        clc
        rts
@string_error:
        lda #$02
        jmp rb_runtime_error
rb_expr_bad_type:
        jmp BASIC_SYNERR

rb_eval_fallback:
        lda rb_eval_save_lo
        sta TXTPTR
        lda rb_eval_save_hi
        sta TXTPTR+1
        jmp BASIC_EVAL

rb_eval_func:
        lda TXTPTR
        sta rb_actual_lo
        lda TXTPTR+1
        sta rb_actual_hi
        jsr rb_find_routine
        bcs :+
        clc
        rts
:       lda rb_found_kind
        cmp #RB_ROUT_FUNC
        beq :+
        clc
        rts
:       lda #0
        sta CF_PARAM_COUNT
        sta rb_exec_out_type
        lda rb_func_depth
        cmp #RB_PROC_DEPTH
        bcc :+
        lda #33
        jmp rb_runtime_error
:       tax
        lda #0
        sta rb_form_save_count,x
        inc rb_func_depth
        lda #1
        sta rb_bind_expr_mode
        lda #1
        sta rb_expr_type
        lda rb_scan_line_lo
        pha
        lda rb_scan_line_hi
        pha
        jsr rb_bind_exec_args
        pla
        sta rb_scan_line_hi
        pla
        sta rb_scan_line_lo
        bcs :+
        jsr rb_restore_func_formals
        jmp BASIC_SYNERR
:       lda TXTPTR
        sta rb_eval_after_lo
        lda TXTPTR+1
        sta rb_eval_after_hi
        jsr rb_expr_run_to_return
        bcs :+
        jsr rb_restore_func_formals
        jmp BASIC_SYNERR
:       lda rb_exec_out_type
        cmp #RB_OUT_STRING
        beq @string
        cmp #RB_OUT_INT
        beq @int
        lda rb_eval_after_lo
        pha
        lda rb_eval_after_hi
        pha
        jsr BASIC_FRMNUM
        pla
        sta rb_eval_after_hi
        pla
        sta rb_eval_after_lo
        ldx #<RF_FLOAT
        ldy #>RF_FLOAT
        jsr BASIC_MOVMF
        lda #0
        sta RF_STATUS
        lda #RB_VAL_FLOAT
        sta RF_TAG
        lda #RB_EXPR_FLOAT
        sta rb_expr_type
        jsr rb_restore_func_formals
        jmp @restore
@int:
        lda rb_eval_after_lo
        pha
        lda rb_eval_after_hi
        pha
        jsr BASIC_FRMNUM
        pla
        sta rb_eval_after_hi
        pla
        sta rb_eval_after_lo
        jsr BASIC_GETADR
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda LINNUM
        sta RF_VAL_LO
        lda LINNUM+1
        sta RF_VAL_HI
        lda #RB_EXPR_INT
        sta rb_expr_type
        jsr rb_restore_func_formals
        jsr @restore
        sec
        rts
@string:
        lda rb_eval_after_lo
        pha
        lda rb_eval_after_hi
        pha
        jsr BASIC_FRMEVL
        jsr rb_stage_fac_string_result
        jsr BASIC_FRESTR
        pla
        sta rb_eval_after_hi
        pla
        sta rb_eval_after_lo
        lda #RB_EXPR_STRING
        sta rb_expr_type
        jsr rb_restore_func_formals
@restore:
        lda rb_eval_after_lo
        sta TXTPTR
        lda rb_eval_after_hi
        sta TXTPTR+1
        sec
        rts

rb_expr_run_to_return:
        jsr rb_expr_next_body_line
        bcs @line
        rts
@next_line:
        jsr rb_expr_next_body_line
        bcs @line
        rts
@line:
        clc
        lda rb_scan_line_lo
        adc #4
        sta rb_stmt_lo
        lda rb_scan_line_hi
        adc #0
        sta rb_stmt_hi
@stmt:
        jsr rb_skip_stmt_spaces
        jsr rb_stmt_chr
        beq @next_line
        cmp #':'
        bne :+
        jsr rb_inc_stmt
        jmp @stmt
:       cmp #TOKEN_REM
        beq @next_line
        lda rb_stmt_lo
        sta rb_peek_lo
        lda rb_stmt_hi
        sta rb_peek_hi
        jsr rb_match_ret
        bcs @ret
        lda rb_stmt_lo
        sta rb_peek_lo
        lda rb_stmt_hi
        sta rb_peek_hi
        jsr rb_match_endp
        bcs @no
        jsr rb_expr_exec_assignment
        bcs @stmt
        clc
        rts
@ret:
        jsr CHRGET
        jsr rb_skip_spaces
        jsr rb_expr_detect_ret_type
        sec
        rts
@no:
        clc
        rts

rb_expr_exec_assignment:
        jsr rb_expr_assignment_shape
        bcs :+
        rts
:       lda rb_stmt_lo
        sta TXTPTR
        lda rb_stmt_hi
        sta TXTPTR+1
        lda #0
        sta SUBFLG
        jsr BASIC_PTRGET
        lda VARPNT
        sta rb_exec_out_lo
        lda VARPNT+1
        sta rb_exec_out_hi
        lda VALTYP
        pha
        lda INTFLG
        pha
        jsr rb_skip_spaces
        cmp #TOKEN_EQUAL
        beq :+
        cmp #'='
        beq :+
        pla
        pla
        clc
        rts
:       jsr CHRGET
        pla
        sta rb_tmp_lo
        pla
        cmp #$FF
        beq @string
        jsr rb_expr_push_state
        jsr BASIC_FRMNUM
        jsr rb_expr_pop_state
        lda rb_tmp_lo
        cmp #$80
        bne @float
        jsr BASIC_GETADR
        lda rb_exec_out_lo
        sta rb_ptr_lo
        lda rb_exec_out_hi
        sta rb_ptr_hi
        ldy #0
        lda LINNUM+1
        sta (rb_ptr_lo),y
        iny
        lda LINNUM
        sta (rb_ptr_lo),y
        jmp @done
@float:
        ldx rb_exec_out_lo
        ldy rb_exec_out_hi
        jsr BASIC_MOVMF
        jmp @done
@string:
        jsr rb_expr_push_state
        jsr BASIC_FRMEVL
        jsr rb_expr_pop_state
        jsr rb_stage_fac_string_result
        jsr BASIC_FRESTR
        lda #RB_OUT_STRING
        sta rb_out_type
        lda rb_exec_out_lo
        sta rb_out_ptr_lo
        lda rb_exec_out_hi
        sta rb_out_ptr_hi
        lda #1
        sta rb_out_count
        jsr rb_commit_result
@done:
        lda TXTPTR
        sta rb_stmt_lo
        lda TXTPTR+1
        sta rb_stmt_hi
        sec
        rts

rb_expr_push_state:
        pla
        sta rb_ptr_lo
        pla
        sta rb_ptr_hi
        lda rb_scan_line_lo
        pha
        lda rb_scan_line_hi
        pha
        lda rb_eval_after_lo
        pha
        lda rb_eval_after_hi
        pha
        lda rb_ptr_hi
        pha
        lda rb_ptr_lo
        pha
        rts

rb_expr_pop_state:
        pla
        sta rb_ptr_lo
        pla
        sta rb_ptr_hi
        pla
        sta rb_eval_after_hi
        pla
        sta rb_eval_after_lo
        pla
        sta rb_scan_line_hi
        pla
        sta rb_scan_line_lo
        lda rb_ptr_hi
        pha
        lda rb_ptr_lo
        pha
        rts

rb_expr_assignment_shape:
        lda rb_stmt_lo
        sta rb_ptr_lo
        lda rb_stmt_hi
        sta rb_ptr_hi
        ldy #0
@spaces:
        lda (rb_ptr_lo),y
        cmp #' '
        bne @first
        iny
        bne @spaces
@first:
        jsr rb_fold_a
        cmp #'A'
        bcc @no
        cmp #'Z' + 1
        bcs @no
        iny
@name:
        lda (rb_ptr_lo),y
        cmp #'%'
        beq @type
        cmp #'$'
        beq @type
        jsr rb_fold_a
        jsr rb_is_name_char
        bcc @after_name
        iny
        bne @name
@type:
        iny
@after_name:
        lda (rb_ptr_lo),y
        cmp #' '
        bne @equal
        iny
        bne @after_name
@equal:
        cmp #TOKEN_EQUAL
        beq @ok
        cmp #'='
        beq @ok
@no:
        clc
        rts
@ok:
        sec
        rts

rb_expr_next_body_line:
        lda rb_scan_line_lo
        sta rb_ptr_lo
        lda rb_scan_line_hi
        sta rb_ptr_hi
        ldy #0
        lda (rb_ptr_lo),y
        sta rb_scan_line_lo
        iny
        lda (rb_ptr_lo),y
        sta rb_scan_line_hi
        lda rb_scan_line_lo
        ora rb_scan_line_hi
        bne :+
        clc
        rts
:       sec
        rts

rb_expr_detect_ret_type:
        cmp #'$'
        beq @typed_string
        cmp #'%'
        beq @typed_int
        cmp #$22
        beq @string
        ldy #0
@scan:
        lda (TXTPTR),y
        cmp #'$'
        beq @string
        cmp #'%'
        beq @int
        jsr rb_fold_a
        jsr rb_is_name_char
        bcc @float
        iny
        bne @scan
@typed_string:
        jsr CHRGET
        jsr rb_skip_spaces
@string:
        lda #RB_OUT_STRING
        sta rb_exec_out_type
        rts
@typed_int:
        jsr CHRGET
        jsr rb_skip_spaces
@int:
        lda #RB_OUT_INT
        sta rb_exec_out_type
        rts
@float:
        lda #RB_OUT_FLOAT
        sta rb_exec_out_type
        rts

rb_stage_fac_string_result:
        lda VALTYP
        cmp #$FF
        beq :+
        jmp rb_parse_type_error
:
        lda DSCPTR
        sta rb_ptr2_lo
        lda DSCPTR_HI
        sta rb_ptr2_hi
        ldy #0
        lda (rb_ptr2_lo),y
        cmp #RB_MAX_STR + 1
        bcc :+
        lda #RB_MAX_STR
:       sta RF_STR_LEN
        ldy #1
        lda (rb_ptr2_lo),y
        sta rb_ptr_lo
        iny
        lda (rb_ptr2_lo),y
        sta rb_ptr_hi
        ldy #0
@copy:
        cpy RF_STR_LEN
        beq @ready
        lda (rb_ptr_lo),y
        sta RF_STR_BUF,y
        iny
        jmp @copy
@ready:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_STRING
        sta RF_TAG
        rts

rb_stage_fac_string_to_cf:
        lda VALTYP
        cmp #$FF
        beq :+
        jmp rb_parse_type_error
:       lda DSCPTR
        sta rb_ptr2_lo
        lda DSCPTR_HI
        sta rb_ptr2_hi
        ldy #0
        lda (rb_ptr2_lo),y
        cmp #RB_MAX_STR + 1
        bcc :+
        lda #RB_MAX_STR
:       sta CF_STR_LEN
        ldy #1
        lda (rb_ptr2_lo),y
        sta rb_ptr_lo
        iny
        lda (rb_ptr2_lo),y
        sta rb_ptr_hi
        ldy #0
@copy:
        cpy CF_STR_LEN
        beq @done
        lda (rb_ptr_lo),y
        sta CF_STR_BUF,y
        iny
        jmp @copy
@done:
        rts

rb_save_num0:
        ldx rb_num_save_depth
        cpx #4
        bcc :+
        rts
:       lda CF_NUM0_LO
        sta rb_save_num0_lo,x
        lda CF_NUM0_HI
        sta rb_save_num0_hi,x
        inc rb_num_save_depth
        rts

rb_restore_num0:
        ldx rb_num_save_depth
        bne :+
        rts
:       dex
        stx rb_num_save_depth
        lda rb_save_num0_lo,x
        sta CF_NUM0_LO
        lda rb_save_num0_hi,x
        sta CF_NUM0_HI
        rts

rb_save_float0:
        lda rb_float_save_depth
        cmp #4
        bcc :+
        rts
:       asl
        asl
        clc
        adc rb_float_save_depth
        tay
        ldx #0
@loop:
        lda CF_FLOAT0,x
        sta rb_save_float0_buf,y
        iny
        inx
        cpx #5
        bcc @loop
        inc rb_float_save_depth
        rts

rb_restore_float0:
        lda rb_float_save_depth
        bne :+
        rts
:       sec
        sbc #1
        sta rb_float_save_depth
        asl
        asl
        clc
        adc rb_float_save_depth
        tay
        ldx #0
@loop:
        lda rb_save_float0_buf,y
        sta CF_FLOAT0,x
        iny
        inx
        cpx #5
        bcc @loop
        rts

rb_compute_fadd_result:
        lda #<CF_FLOAT0
        ldy #>CF_FLOAT0
        jsr BASIC_MOVFM
        lda #<CF_FLOAT1
        ldy #>CF_FLOAT1
        jsr BASIC_FADD
        ldx #<RF_FLOAT
        ldy #>RF_FLOAT
        jsr BASIC_MOVMF
        lda #0
        sta RF_STATUS
        lda #RB_VAL_FLOAT
        sta RF_TAG
        lda #1
        sta rb_command_precomputed
        rts

rb_save_formal_value:
        sta rb_save_type_tmp
        lda rb_func_depth
        bne :+
        rts
:       sec
        sbc #1
        sta rb_save_depth_tmp
        asl
        asl
        clc
        adc CF_PARAM_COUNT
        tax
        cpx #16
        bcc :+
        rts
:       lda rb_save_type_tmp
        sta rb_form_save_type,x
        lda rb_formal_lo
        sta rb_form_save_lo,x
        sta rb_ptr_lo
        lda rb_formal_hi
        sta rb_form_save_hi,x
        sta rb_ptr_hi
        ldy #0
        lda (rb_ptr_lo),y
        sta rb_form_save_val0,x
        iny
        lda (rb_ptr_lo),y
        sta rb_form_save_val1,x
        iny
        lda (rb_ptr_lo),y
        sta rb_form_save_val2,x
        iny
        lda (rb_ptr_lo),y
        sta rb_form_save_val3,x
        iny
        lda (rb_ptr_lo),y
        sta rb_form_save_val4,x
        ldy rb_save_depth_tmp
        lda CF_PARAM_COUNT
        clc
        adc #1
        sta rb_form_save_count,y
        rts

rb_restore_func_formals:
        lda rb_func_depth
        bne :+
        rts
:       dec rb_func_depth
        ldy rb_func_depth
        lda rb_form_save_count,y
        beq @done
        sta rb_save_count_tmp
        tya
        asl
        asl
        sta rb_save_idx
@loop:
        ldx rb_save_idx
        lda rb_form_save_lo,x
        sta rb_ptr_lo
        lda rb_form_save_hi,x
        sta rb_ptr_hi
        ldy #0
        lda rb_form_save_val0,x
        sta (rb_ptr_lo),y
        iny
        lda rb_form_save_val1,x
        sta (rb_ptr_lo),y
        lda rb_form_save_type,x
        cmp #RB_OUT_INT
        beq @next
        iny
        lda rb_form_save_val2,x
        sta (rb_ptr_lo),y
        lda rb_form_save_type,x
        cmp #RB_OUT_STRING
        beq @next
        iny
        lda rb_form_save_val3,x
        sta (rb_ptr_lo),y
        iny
        lda rb_form_save_val4,x
        sta (rb_ptr_lo),y
@next:
        inc rb_save_idx
        dec rb_save_count_tmp
        bne @loop
@done:
        rts

rb_parse_expr_signature:
        lda RB_DESC_BUF+14
        cmp #SIG_ZECHO1
        beq parse_expr_zecho1
        cmp #SIG_ZADD16
        beq parse_expr_zadd16
        cmp #SIG_UPPER
        beq parse_expr_string_out
        cmp #SIG_LOWER
        beq parse_expr_string_out
        cmp #SIG_ZHIDDENRAM
        beq parse_expr_int_string
        cmp #SIG_BUFNEW
        beq parse_expr_num0
        cmp #SIG_BUFFILL
        beq parse_expr_num2
        cmp #SIG_BUFFREE
        beq parse_expr_num0
        cmp #SIG_ZTEMPSCRATCH
        beq parse_expr_num0
        cmp #SIG_SCRCAP
        beq parse_expr_no_args
        cmp #SIG_ZSUMNUMARRAY
        beq parse_expr_zsumnumarray
        cmp #SIG_FADD
        beq parse_expr_fadd
        cmp #SIG_ERRCODE
        beq parse_expr_errcode
        cmp #SIG_ERRLINE
        beq parse_expr_errline
        cmp #SIG_GFXMODE
        beq parse_expr_gfxmode
        cmp #SIG_GFXSURF
        beq parse_expr_gfxsurf
        cmp #SIG_KEYNONE
        beq parse_expr_no_args
        jmp BASIC_SYNERR

parse_expr_no_args:
        rts

parse_expr_zecho1:
        jmp rb_precompute_zecho1

parse_expr_zadd16:
        jsr rb_parse_num0
        jmp rb_parse_num1

parse_expr_fadd:
        jsr rb_parse_float0
        jsr rb_parse_float1
        lda #RB_EXPR_FLOAT
        sta rb_expr_type
        jmp rb_compute_fadd_result

parse_expr_errcode:
        jmp rb_precompute_errcode

parse_expr_errline:
        jmp rb_precompute_errline

parse_expr_num0:
        jmp rb_parse_num0

parse_expr_num2:
        jsr rb_parse_num0
        jmp rb_parse_num1

parse_expr_gfxmode:
        jsr rb_skip_spaces
        cmp #')'
        beq :+
        jsr rb_parse_string_value_current
:       rts

parse_expr_gfxsurf:
        jmp rb_parse_string_value_current

parse_expr_int_string:
        jmp rb_parse_string_value

parse_expr_zsumnumarray:
        jsr rb_parse_int_array_input
        jmp rb_resolve_int_array_input_ptr

parse_expr_string_out:
        jsr rb_parse_string_value
        lda #RB_EXPR_STRING
        sta rb_expr_type
        rts

rb_expr_expect_close:
        jsr rb_skip_spaces
        cmp #')'
        beq :+
        jmp BASIC_SYNERR
:       jmp CHRGET

rb_resolve_int_array_input_ptr:
        clc
        lda CF_PTR0_LO
        adc ARYTAB
        sta CF_PTR0_LO
        lda CF_PTR0_HI
        adc ARYTAB+1
        sta CF_PTR0_HI
        rts

rb_normalize_int_array_ptr_from_varpnt:
        lda VARPNT
        sta rb_ptr2_lo
        lda VARPNT+1
        sta rb_ptr2_hi
        ldy #0
        lda (rb_ptr2_lo),y
        cmp #$C1
        bcc @done
        cmp #$DB
        bcs @done
        iny
        lda (rb_ptr2_lo),y
        cmp #$80
        bne @done
        ldy #4
        lda (rb_ptr2_lo),y
        beq @done
        cmp #11
        bcs @done
        clc
        lda rb_ptr2_lo
        adc #7
        sta rb_ptr2_lo
        lda rb_ptr2_hi
        adc #0
        sta rb_ptr2_hi
@done:
        rts

rb_clear_int_array_output:
        lda rb_out_count_hi
        bne @done
        ldx rb_out_count_lo
        beq @done
@loop:
        ldy #0
        lda #0
        sta (rb_ptr2_lo),y
        iny
        sta (rb_ptr2_lo),y
        clc
        lda rb_ptr2_lo
        adc #2
        sta rb_ptr2_lo
        bcc :+
        inc rb_ptr2_hi
:       dex
        bne @loop
@done:
        rts

; ---------------------------------------------------------------------------
; Overlay loading, result commit, and errors.
; ---------------------------------------------------------------------------

rb_clear_result_frame:
        lda #0
        ldx #0
@loop:
        sta RB_RF,x
        inx
        bne @loop
        lda #RB_OUT_NONE
        sta rb_out_type
        sta rb_out_count
        rts

rb_load_and_call_command:
        lda RB_DESC_BUF+4
        ora RB_DESC_BUF+5
        beq @done
        jsr rb_desc_runtime_base
        clc
        lda rb_ptr_lo
        adc RB_DESC_BUF+12
        sta rb_overlay_vec_lo
        lda rb_ptr_hi
        adc RB_DESC_BUF+13
        sta rb_overlay_vec_hi
        jsr rb_slot_resident
        bcs @resident
        lda RB_DESC_BUF+2
        sta rb_reu_off_lo
        lda RB_DESC_BUF+3
        sta rb_reu_off_hi
        lda RB_DESC_BUF+4
        sta rb_reu_len_lo
        lda RB_DESC_BUF+5
        sta rb_reu_len_hi
        clc
        lda rb_ptr_lo
        adc RB_DESC_BUF+10
        sta rb_reu_c64_lo
        lda rb_ptr_hi
        adc RB_DESC_BUF+11
        sta rb_reu_c64_hi
        lda rb_reu_code_bank
        sta rb_reu_bank
        inc rb_copy_count
        jsr rb_fetch_underrom_payload
        jsr rb_mark_slot_resident
@resident:
        jsr rb_call_underrom_payload
@done:
        rts

rb_desc_runtime_base:
        ldx RB_DESC_BUF+8
        lda #>RB_SLOT0_BASE
        cpx #RB_SLOT_LEGACY_LOW
        beq @store
        lda #>RB_SLOT2_BASE
        cpx #RB_SLOT_PROOF_2
        beq @store
        lda #>RB_SLOT1_BASE
@store: ldx #0
        stx rb_ptr_lo
        sta rb_ptr_hi
        rts

rb_slot_resident:
        lda rb_slot0_cmd
        cmp RB_DESC_BUF+6
        bne @miss
        lda rb_slot0_overlay
        cmp RB_DESC_BUF+7
        bne @miss
        sec
        rts
@miss:
        clc
        rts

rb_mark_slot_resident:
        lda RB_DESC_BUF+6
        sta rb_slot0_cmd
        lda RB_DESC_BUF+7
        sta rb_slot0_overlay
        rts

rb_fetch_underrom_payload:
        php
        sei
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda CPU_PORT
        sta rb_saved_cpu
        and #RAM_UNDER_BASIC_KEEP_KERNAL
        sta CPU_PORT
        jsr rb_reu_fetch
        lda rb_saved_cpu
        sta CPU_PORT
        plp
        rts

rb_call_underrom_payload:
        php
        sei
        lda CPU_DDR
        ora #$07
        sta CPU_DDR
        lda CPU_PORT
        sta rb_saved_cpu
        lda #$36
        sta CPU_PORT
        jsr rb_hidden_overlay_jmp
        lda rb_saved_cpu
        sta CPU_PORT
        plp
        rts

rb_hidden_overlay_jmp:
        jmp (rb_overlay_vec_lo)

rb_commit_result:
        lda RF_STATUS
        beq @ok
        lda RF_ERROR
        bne :+
        lda RF_STATUS
:       jmp rb_runtime_error
@ok:
        lda rb_out_count
        beq @done
        lda rb_out_type
        cmp #RB_OUT_INT
        bne :+
        jmp rb_commit_int
:       cmp #RB_OUT_STRING
        bne :+
        jmp rb_commit_string
:       cmp #RB_OUT_ARRAYI
        bne :+
        jmp rb_commit_arrayi
:       cmp #RB_OUT_FLOAT
        bne @done
        jmp rb_commit_float
@done:
        rts

rb_commit_int:
        lda RF_TAG
        cmp #RB_VAL_INT
        bne @done
        lda rb_out_ptr_lo
        sta rb_ptr_lo
        lda rb_out_ptr_hi
        sta rb_ptr_hi
        ldy #0
        lda RF_VAL_HI
        sta (rb_ptr_lo),y
        iny
        lda RF_VAL_LO
        sta (rb_ptr_lo),y
@done:
        rts

rb_commit_float:
        lda RF_TAG
        cmp #RB_VAL_FLOAT
        bne @done
        lda rb_out_ptr_lo
        sta rb_ptr_lo
        lda rb_out_ptr_hi
        sta rb_ptr_hi
        ldy #0
@copy:
        lda RF_FLOAT,y
        sta (rb_ptr_lo),y
        iny
        cpy #5
        bcc @copy
@done:
        rts

rb_commit_string:
        lda RF_TAG
        cmp #RB_VAL_STRING
        bne @done
        lda RF_STR_LEN
        beq @empty
        jsr rb_alloc_string_heap
        bcs rb_commit_string_error
        lda rb_out_ptr_lo
        sta rb_ptr2_lo
        lda rb_out_ptr_hi
        sta rb_ptr2_hi
        ldy #0
        lda RF_STR_LEN
        sta (rb_ptr2_lo),y
        iny
        lda rb_ptr_lo
        sta (rb_ptr2_lo),y
        iny
        lda rb_ptr_hi
        sta (rb_ptr2_lo),y
        rts
@empty:
        lda rb_out_ptr_lo
        sta rb_ptr2_lo
        lda rb_out_ptr_hi
        sta rb_ptr2_hi
        ldy #0
        lda #0
        sta (rb_ptr2_lo),y
        iny
        sta (rb_ptr2_lo),y
        iny
        sta (rb_ptr2_lo),y
@done:
        rts
rb_commit_string_error:
        lda #$02
        jmp rb_runtime_error

rb_alloc_string_heap:
        sec
        lda FRETOP
        sbc RF_STR_LEN
        sta rb_ptr_lo
        lda FRETOP+1
        sbc #0
        sta rb_ptr_hi
        lda rb_ptr_hi
        cmp STREND+1
        bcc @oom
        bne @copy
        lda rb_ptr_lo
        cmp STREND
        bcc @oom
@copy:
        ldy #0
@loop:
        cpy RF_STR_LEN
        beq @done
        lda RF_STR_BUF,y
        sta (rb_ptr_lo),y
        iny
        jmp @loop
@done:
        lda rb_ptr_lo
        sta FRETOP
        lda rb_ptr_hi
        sta FRETOP+1
        clc
        rts
@oom:
        sec
        rts

rb_commit_arrayi:
        lda RF_TAG
        cmp #RB_VAL_ARRAYI
        bne @done
        lda rb_out_ptr_lo
        sta rb_ptr_lo
        lda rb_out_ptr_hi
        sta rb_ptr_hi
        ldx RF_COUNT_LO
        beq @done
        ldy #0
@loop:
        lda RF_ARRAY_BUF,y
        sta (rb_ptr_lo),y
        iny
        lda RF_ARRAY_BUF,y
        sta (rb_ptr_lo),y
        iny
        dex
        beq @done
        jmp @loop
@done:
        rts

rb_runtime_error:
        sta rb_error
        sta rb_last_error
        lda CURLIN+1
        cmp #$FF
        bne @store_line
        lda #0
        sta rb_last_line_lo
        sta rb_last_line_hi
        beq @print
@store_line:
        lda CURLIN
        sta rb_last_line_lo
        lda CURLIN+1
        sta rb_last_line_hi
@print:
        lda #<rb_error_text
        sta rb_ptr_lo
        lda #>rb_error_text
        sta rb_ptr_hi
        jsr rb_print_z
        ldx rb_error
        lda #0
        jsr BASIC_LINPRT
        lda #13
        jsr K_CHROUT
        jmp BASIC_READY

rb_error_text:
        .byte "?RB ERROR ",0

; ---------------------------------------------------------------------------
; REU DMA, registry seed, debug, and handle heap.
; ---------------------------------------------------------------------------

rb_reu_stash:
        lda rb_reu_c64_lo
        sta REU_C64_LO
        lda rb_reu_c64_hi
        sta REU_C64_HI
        lda rb_reu_off_lo
        sta REU_ADDR_LO
        lda rb_reu_off_hi
        sta REU_ADDR_HI
        lda rb_reu_bank
        sta REU_BANK
        lda rb_reu_len_lo
        sta REU_LEN_LO
        lda rb_reu_len_hi
        sta REU_LEN_HI
        lda #$90
        sta REU_CMD
        rts

rb_reu_fetch:
        lda rb_reu_c64_lo
        sta REU_C64_LO
        lda rb_reu_c64_hi
        sta REU_C64_HI
        lda rb_reu_off_lo
        sta REU_ADDR_LO
        lda rb_reu_off_hi
        sta REU_ADDR_HI
        lda rb_reu_bank
        sta REU_BANK
        lda rb_reu_len_lo
        sta REU_LEN_LO
        lda rb_reu_len_hi
        sta REU_LEN_HI
        lda #$91
        sta REU_CMD
        rts

rb_stash_call_frame:
        lda #<RB_CF
        sta rb_reu_c64_lo
        lda #>RB_CF
        sta rb_reu_c64_hi
        lda #<RB_REU_CALL_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_CALL_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #$80
        sta rb_reu_len_lo
        lda #0
        sta rb_reu_len_hi
        jmp rb_reu_stash

rb_stash_result_frame:
        lda #<RB_RF
        sta rb_reu_c64_lo
        lda #>RB_RF
        sta rb_reu_c64_hi
        lda #<RB_REU_RESULT_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_RESULT_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #$80
        sta rb_reu_len_lo
        lda #0
        sta rb_reu_len_hi
        jmp rb_reu_stash

        .segment "REGSEED"

rb_reu_header:
        .byte "RBPL"
        .byte 1
        .byte RB_CMD_DESC_COUNT
        .byte RB_CMD_DESC_SIZE
        .byte RB_MAX_NAME
        .word RB_REU_DESC_OFF
        .word RB_REU_CALL_OFF
        .word RB_REU_RESULT_OFF
        .word RB_REU_HANDLE_OFF
rb_reu_header_end:

.macro CMD_LOW id, sig, label, endlabel, name
        .byte id, RB_MODULE_SYSTEM
        .word 0
        .word __LOWPACK_SIZE__
        .byte RB_SUBMOD_LEGACY_LOW
        .byte 0
        .byte RB_SLOT_LEGACY_LOW
        .byte 1
        .word 0
        .word label - __LOWPACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_LOW_ALL id, sig, label, name
        .byte id, RB_MODULE_SYSTEM
        .word 0
        .word __LOWPACK_SIZE__
        .byte RB_SUBMOD_LEGACY_LOW
        .byte 0
        .byte RB_SLOT_LEGACY_LOW
        .byte 1
        .word 0
        .word label - __LOWPACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_HIDDEN id, sig, label, endlabel, name
        .byte id, RB_MODULE_SYSTEM
        .word 0
        .word __LOWPACK_SIZE__
        .byte RB_SUBMOD_LEGACY_LOW
        .byte 0
        .byte RB_SLOT_LEGACY_LOW
        .byte 1
        .word 0
        .word label - __LOWPACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_SLOT1 id, sig, label, name
        .byte id, 2
        .word __SLOTPACK1_LOAD__ - __LOWPACK_LOAD__
        .word __SLOTPACK1_SIZE__
        .byte RB_SUBMOD_PROOF_SLOT1
        .byte 0
        .byte RB_SLOT_PROOF_1
        .byte 1
        .word 0
        .word label - __SLOTPACK1_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_SLOT2 id, sig, label, name
        .byte id, 2
        .word __SLOTPACK2_LOAD__ - __LOWPACK_LOAD__
        .word __SLOTPACK2_SIZE__
        .byte RB_SUBMOD_PROOF_SLOT2
        .byte 0
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __SLOTPACK2_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_SPAN id, sig, label, name
        .byte id, 2
        .word __SPANPACK_LOAD__ - __LOWPACK_LOAD__
        .word __SPANPACK_SIZE__
        .byte RB_SUBMOD_PROOF_SPAN
        .byte 0
        .byte RB_SLOT_PROOF_12
        .byte 1
        .word 0
        .word label - __SPANPACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_OVL1 id, sig, label, name
        .byte id, 2
        .word RB_CODE_GFXSPR_OFF
        .word __OVL1PACK_SIZE__
        .byte RB_SUBMOD_PROOF_OVERLAY
        .byte 1
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL1PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_OVL2 id, sig, label, name
        .byte id, 2
        .word RB_CODE_INPUTEV_OFF
        .word __OVL2PACK_SIZE__
        .byte RB_SUBMOD_PROOF_OVERLAY
        .byte 2
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL2PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_GFXCORE id, sig, label, name
        .byte id, RB_MODULE_GFX
        .word __SLOTPACK1_LOAD__ - __LOWPACK_LOAD__
        .word __SLOTPACK1_SIZE__
        .byte RB_SUBMOD_GFXCORE
        .byte 0
        .byte RB_SLOT_PROOF_1
        .byte 1
        .word 0
        .word label - __SLOTPACK1_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_GFXPRIM id, sig, label, name
        .byte id, RB_MODULE_GFX
        .word __SLOTPACK2_LOAD__ - __LOWPACK_LOAD__
        .word __SLOTPACK2_SIZE__
        .byte RB_SUBMOD_GFXPRIM
        .byte 0
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __SLOTPACK2_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_GFXSPR id, sig, label, name
        .byte id, RB_MODULE_GFX
        .word RB_CODE_GFXSPR_OFF
        .word __OVL1PACK_SIZE__
        .byte RB_SUBMOD_GFXSPR
        .byte 1
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL1PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_INPUTEV id, sig, label, name
        .byte id, RB_MODULE_GFX
        .word RB_CODE_INPUTEV_OFF
        .word __OVL2PACK_SIZE__
        .byte RB_SUBMOD_INPUTEV
        .byte 2
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL2PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_GFXPOLY id, sig, label, name
        .byte id, RB_MODULE_GFX
        .word RB_CODE_GFXPOLY_OFF
        .word __OVL3PACK_SIZE__
        .byte RB_SUBMOD_GFXPOLY
        .byte 3
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL3PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_GFXDL id, sig, label, name
        .byte id, RB_MODULE_GFX
        .word RB_CODE_GFXDL_OFF
        .word __OVL4PACK_SIZE__
        .byte RB_SUBMOD_GFXDL
        .byte 4
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL4PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_GFXTILE id, sig, label, name
        .byte id, RB_MODULE_GFX
        .word RB_CODE_GFXTILE_OFF
        .word __OVL5PACK_SIZE__
        .byte RB_SUBMOD_GFXTILE
        .byte 5
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL5PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

.macro CMD_SIDCORE id, sig, label, name
        .byte id, RB_MODULE_SID
        .word RB_CODE_SIDCORE_OFF
        .word __OVL6PACK_SIZE__
        .byte RB_SUBMOD_SIDCORE
        .byte 6
        .byte RB_SLOT_PROOF_2
        .byte 1
        .word 0
        .word label - __OVL6PACK_RUN__
        .byte sig, .strlen(name)
        .byte name
        .res 16 - .strlen(name), 0
.endmacro

rb_command_descriptors:
        CMD_LOW_ALL CMD_ZECHO1, SIG_ZECHO1, cmd_zecho1_low, "ZECHO1"
        CMD_LOW CMD_ZADD16, SIG_ZADD16, cmd_zadd16_low, cmd_zadd16_low_end, "ZADD16"
        CMD_LOW CMD_UPPER, SIG_UPPER, cmd_upper_low, cmd_upper_low_end, "UPPER"
        CMD_LOW CMD_LOWER, SIG_LOWER, cmd_lower_low, cmd_lower_low_end, "LOWER"
        CMD_HIDDEN CMD_ZHIDDENRAM, SIG_ZHIDDENRAM, cmd_zhiddenram_hidden, cmd_zhiddenram_hidden_end, "ZHIDDENRAM"
        CMD_LOW CMD_ZSUMNUMARRAY, SIG_ZSUMNUMARRAY, cmd_zsumnumarray_low, cmd_zsumnumarray_low_end, "ZSUMNUMARRAY"
        CMD_LOW CMD_ZRANGENUMARRAY, SIG_ZRANGENUMARRAY, cmd_zrangenumarray_low, cmd_zrangenumarray_low_end, "ZRANGENUMARRAY"
        CMD_LOW_ALL CMD_BUFNEW, SIG_BUFNEW, cmd_bufnew_low, "BUFMAKE"
        CMD_LOW_ALL CMD_BUFFILL, SIG_BUFFILL, cmd_buffill_low, "BUFFILL"
        CMD_LOW_ALL CMD_BUFFREE, SIG_BUFFREE, cmd_buffree_low, "BUFDROP"
        CMD_LOW_ALL CMD_ZTEMPSCRATCH, SIG_ZTEMPSCRATCH, cmd_ztempscratch_low, "ZTEMPSCRATCH"
        CMD_LOW CMD_ZFAIL, SIG_ZFAIL, cmd_zfail_low, cmd_zfail_low_end, "ZFAIL"
        CMD_LOW CMD_FREEMEM, SIG_FREEMEM, cmd_freemem_low, cmd_freemem_low_end, "MEMAVL"
        CMD_LOW_ALL CMD_SCRCAP, SIG_SCRCAP, cmd_scrcap_low, "SCRCAP"
        CMD_LOW CMD_FADD, SIG_FADD, cmd_fadd_low, cmd_fadd_low_end, "FADD"
        CMD_LOW CMD_ZPAUSE, SIG_ZPAUSE, cmd_zpause_low, cmd_zpause_low_end, "ZPAUSE"
        CMD_LOW CMD_ERRCODE, SIG_ERRCODE, cmd_zecho1_low, cmd_zecho1_low_end, "ERRCODE"
        CMD_LOW CMD_ERRLINE, SIG_ERRLINE, cmd_zecho1_low, cmd_zecho1_low_end, "ERRLINE"
        CMD_LOW CMD_ZSLOT0, SIG_SCRCAP, cmd_zslot0_low, cmd_zslot0_low_end, "ZSLOT0"
        CMD_SLOT1 CMD_ZSLOT1, SIG_SCRCAP, cmd_zslot1, "ZSLOT1"
        CMD_SLOT2 CMD_ZSLOT2, SIG_SCRCAP, cmd_zslot2, "ZSLOT2"
        CMD_SPAN CMD_ZSPAN, SIG_SCRCAP, cmd_zspan, "ZSPAN"
        CMD_OVL1 CMD_ZOVL1, SIG_SCRCAP, cmd_zovl1, "ZOVL1"
        CMD_OVL2 CMD_ZOVL2, SIG_SCRCAP, cmd_zovl2, "ZOVL2"
        CMD_LOW CMD_ZCPYRST, SIG_SCRCAP, cmd_zcpyrst_low, cmd_zcpyrst_low_end, "ZCPYRST"
        CMD_LOW CMD_ZCOPY, SIG_SCRCAP, cmd_zcopy_low, cmd_zcopy_low_end, "ZCOPY"
        CMD_SLOT1 CMD_ZMODLOAD, SIG_ZHIDDENRAM, cmd_zmodload, "ZMODLD"
        CMD_GFXCORE CMD_GFXMODE, SIG_GFXMODE, cmd_gfxmode, "GFXMODE"
        CMD_GFXCORE CMD_GFXTEXT, SIG_FREEMEM, cmd_gfxtext, "GFXTEXT"
        CMD_GFXCORE CMD_GFXCLEAR, SIG_BUFFREE, cmd_gfxclear, "GFXCLEAR"
        CMD_LOW_ALL CMD_GFXSURF, SIG_GFXSURF, cmd_gfxsurf_low, "GFXSURF"
        CMD_LOW_ALL CMD_GFXTARGET, SIG_BUFFREE, cmd_gfxtarget_low, "GFXTGT"
        CMD_LOW_ALL CMD_GFXBLIT, SIG_BUFFREE, cmd_gfxblit_low, "GFXBLIT"
        CMD_GFXCORE CMD_GFXSYNC, SIG_FREEMEM, cmd_gfxsync, "GFXSYNC"
        CMD_GFXPRIM CMD_PLOT, SIG_PLOT, cmd_plot, "PLOT"
        CMD_GFXPRIM CMD_PNT, SIG_ZADD16, cmd_point, "PNT"
        CMD_GFXPRIM CMD_LINE, SIG_LINE, cmd_line, "LINE"
        CMD_GFXPRIM CMD_RECT, SIG_LINE, cmd_rect, "RECT"
        CMD_GFXPRIM CMD_FRECT, SIG_LINE, cmd_frect, "FBOX"
        CMD_GFXPRIM CMD_CIRCLE, SIG_SPRSET, cmd_circle, "CIRCLE"
        CMD_GFXPRIM CMD_FCIRCLE, SIG_SPRSET, cmd_fcircle, "FCIRCLE"
        CMD_GFXPRIM CMD_TILE, SIG_SPRSET, cmd_tile, "TILE"
        CMD_GFXPRIM CMD_CHARAT, SIG_SPRSET, cmd_tile, "CHARAT"
        CMD_GFXSPR CMD_SPRSET, SIG_SPRSET, cmd_sprset, "SPRSET"
        CMD_GFXSPR CMD_SPRMOVE, SIG_PLOT, cmd_sprmove, "SPRMOVE"
        CMD_GFXSPR CMD_SPRROW, SIG_LINE, cmd_sprrow, "SPRROW"
        CMD_GFXSPR CMD_SPRSIZE, SIG_PLOT, cmd_sprexpand, "SPRSIZE"
        CMD_GFXSPR CMD_SPRPRI, SIG_BUFFILL, cmd_sprpri, "SPRPRI"
        CMD_GFXSPR CMD_SPRMUL, SIG_BUFFILL, cmd_sprmulti, "SPRMUL"
        CMD_GFXSPR CMD_SPRCOL, SIG_BUFFILL, cmd_sprcolor, "SPRCOL"
        CMD_GFXSPR CMD_SPRMCO, SIG_BUFFILL, cmd_sprmcolor, "SPRMCO"
        CMD_GFXSPR CMD_SPRSCAN, SIG_FREEMEM, cmd_sprscan, "SPRSCAN"
        CMD_GFXSPR CMD_SPRCOLL, SIG_ZFAIL, cmd_sprcoll, "SPRCOLL"
        CMD_INPUTEV CMD_JOY, SIG_ZFAIL, cmd_joy, "JOY"
        CMD_INPUTEV CMD_KEYP, SIG_SCRCAP, cmd_keyp, "KEYP"
        CMD_INPUTEV CMD_KEYSCAN, SIG_KEYNONE, cmd_keyscan, "KEYSCAN"
        CMD_INPUTEV CMD_KEYLAST, SIG_SCRCAP, cmd_keylast, "KEYLAST"
        CMD_GFXPOLY CMD_POLY, SIG_POLY, cmd_poly, "POLY"
        CMD_GFXPOLY CMD_FPOLY, SIG_POLY, cmd_fpoly, "FPOLY"
        CMD_GFXPOLY CMD_PBUFNEW, SIG_BUFNEW, cmd_pbufnew, "PBMAKE"
        CMD_GFXPOLY CMD_PBUFSET, SIG_SPRSET, cmd_pbufset, "PBUFSET"
        CMD_GFXPOLY CMD_PBUFFREE, SIG_BUFFREE, cmd_pbuffree, "PBDROP"
        CMD_GFXPOLY CMD_POLYH, SIG_PLOT, cmd_poly, "POLYH"
        CMD_GFXPOLY CMD_FPOLYH, SIG_PLOT, cmd_fpoly, "FPOLYH"
        CMD_GFXDL CMD_DLNEW, SIG_BUFNEW, cmd_dlnew, "DLMAKE"
        CMD_GFXDL CMD_DLCLR, SIG_BUFFREE, cmd_dlclr, "DLRST"
        CMD_GFXDL CMD_DLPLOT, SIG_SPRSET, cmd_dlplot, "DLPLOT"
        CMD_GFXDL CMD_DLLINE, SIG_LINE, cmd_dlline, "DLLINE"
        CMD_GFXDL CMD_DLRECT, SIG_LINE, cmd_dlrect, "DLRECT"
        CMD_GFXDL CMD_DLFRECT, SIG_LINE, cmd_dlfrect, "DLFBOX"
        CMD_GFXDL CMD_DLDRAW, SIG_BUFFREE, cmd_dldraw, "DLDRAW"
        CMD_GFXTILE CMD_CHRNEW, SIG_BUFNEW, cmd_chrnew, "CHRMAKE"
        CMD_GFXTILE CMD_CHRROW, SIG_SPRSET, cmd_chrrow, "CHRROW"
        CMD_GFXTILE CMD_CHRUSE, SIG_BUFFREE, cmd_chruse, "CHRUSE"
        CMD_GFXTILE CMD_TSNEW, SIG_BUFNEW, cmd_tsnew, "TSMAKE"
        CMD_GFXTILE CMD_TSSET, SIG_SPRSET, cmd_tsset, "TSSET"
        CMD_GFXTILE CMD_TMNEW, SIG_BUFNEW, cmd_tmnew, "TMMAKE"
        CMD_GFXTILE CMD_TMSET, SIG_SPRSET, cmd_tmset, "TMSET"
        CMD_GFXTILE CMD_TMDRAW, SIG_BUFFILL, cmd_tmdraw, "TMDRAW"
        CMD_GFXTILE CMD_MCELL, SIG_LINE, cmd_mcell, "MCELL"
        CMD_GFXTILE CMD_MCBG, SIG_BUFFREE, cmd_mcbg, "MCBG"
        CMD_SIDCORE CMD_SIDCLR, SIG_KEYNONE, cmd_sidclr, "SIDRST"
        CMD_SIDCORE CMD_SILENCE, SIG_KEYNONE, cmd_sidclr, "SIDOFF"
        CMD_SIDCORE CMD_VOL, SIG_BUFFREE, cmd_vol, "VOL"
        CMD_SIDCORE CMD_FREQ, SIG_BUFFILL, cmd_freq, "FRQ"
        CMD_SIDCORE CMD_NOTE, SIG_PLOT, cmd_note, "PITCH"
        CMD_SIDCORE CMD_PULSE, SIG_BUFFILL, cmd_pulse, "PULSE"
        CMD_SIDCORE CMD_ADSR, SIG_LINE, cmd_adsr, "ADSR"
        CMD_SIDCORE CMD_WAVE, SIG_BUFFILL, cmd_wave, "WAVE"
        CMD_SIDCORE CMD_GATE, SIG_BUFFILL, cmd_gate, "GATE"
        CMD_SIDCORE CMD_VOICE, SIG_LINE, cmd_voice, "VOICE"
        CMD_SIDCORE CMD_FILTER, SIG_SPRSET, cmd_filter, "FILTER"
        CMD_SIDCORE CMD_SOUND, SIG_SPRSET, cmd_sound, "SOUND"
        .res (RB_CMD_DESC_COUNT - 94) * RB_CMD_DESC_SIZE, 0
        CMD_LOW_ALL CMD_SCRPUT, SIG_SCRPUT, cmd_scrput_low, "SCRPUT"

; ---------------------------------------------------------------------------
; Hidden helper code, called by visible resident code with BASIC ROM hidden.
; ---------------------------------------------------------------------------

        .segment "HIDDEN"

hidden_calc_basic_free:
        sec
        lda FRETOP
        sbc STREND
        sta rb_free_lo
        lda FRETOP+1
        sbc STREND+1
        sta rb_free_hi
        rts

hidden_print_live_free:
        jsr hidden_calc_basic_free
        lda #0
        sta rb_digit_seen
        ldx #0
@place:
        lda #0
        sta rb_digit_count
@sub:
        sec
        lda rb_free_lo
        sbc hidden_decimal_lo,x
        sta rb_tmp_lo
        lda rb_free_hi
        sbc hidden_decimal_hi,x
        bcc @emit
        sta rb_free_hi
        lda rb_tmp_lo
        sta rb_free_lo
        inc rb_digit_count
        jmp @sub
@emit:
        lda rb_digit_count
        bne @print
        lda rb_digit_seen
        bne @print
        cpx #4
        bne @next
        lda #0
@print:
        ora #'0'
        jsr K_CHROUT
        lda #1
        sta rb_digit_seen
@next:
        inx
        cpx #5
        bcc @place
        rts

hidden_decimal_lo:
        .byte <10000,<1000,<100,<10,<1
hidden_decimal_hi:
        .byte >10000,>1000,>100,>10,>1

hidden_prepare_ready_resume:
        lda #RB_RESUME_READY
        sta RUNTIME_MODE
        lda #RB_MAGIC_READY
        sta rb_magic
        sta rb_entry_magic
        lda #RB_MAGIC2
        sta rb_magic2
        sta rb_entry_magic2
        rts

hidden_install_vectors:
        lda rb_vectors_saved
        bne @install
        lda IMAIN_VEC
        sta rb_orig_imain_lo
        lda IMAIN_VEC+1
        sta rb_orig_imain_hi
        lda $0304
        sta rb_orig_crunch_lo
        lda $0305
        sta rb_orig_crunch_hi
        lda $0306
        sta rb_orig_list_lo
        lda $0307
        sta rb_orig_list_hi
        lda KEYLOG_VEC
        sta rb_orig_keylog_lo
        lda KEYLOG_VEC+1
        sta rb_orig_keylog_hi
        lda $0308
        sta rb_orig_execute_lo
        lda $0309
        sta rb_orig_execute_hi
        lda KERNAL_CHRIN_VEC
        sta rb_orig_chrin_lo
        lda KERNAL_CHRIN_VEC+1
        sta rb_orig_chrin_hi
        lda #1
        sta rb_vectors_saved
@install:
        lda #<rb_imain
        sta IMAIN_VEC
        lda #>rb_imain
        sta IMAIN_VEC+1
        lda #<rb_crunch
        sta $0304
        lda #>rb_crunch
        sta $0305
        lda #<rb_execute
        sta $0308
        lda #>rb_execute
        sta $0309
        lda #<rb_eval
        sta $030A
        lda #>rb_eval
        sta $030B
        lda #<rb_keylog
        sta KEYLOG_VEC
        lda #>rb_keylog
        sta KEYLOG_VEC+1
        lda #<rb_chrin
        sta KERNAL_CHRIN_VEC
        lda #>rb_chrin
        sta KERNAL_CHRIN_VEC+1
        rts

hidden_select_next_app:
        jsr hidden_normalize_current_bank
        sta rb_saved_count_lo
        sta rb_tmp_lo
        lda #0
        sta rb_digit_count
@loop:
        inc rb_tmp_lo
        lda rb_tmp_lo
        cmp #APP_BANK_MAX_PLUS_ONE
        bcc :+
        lda #APP_BANK_MIN
        sta rb_tmp_lo
:       jsr hidden_bank_loaded
        bcs @found
        inc rb_digit_count
        lda rb_digit_count
        cmp #(APP_BANK_MAX_PLUS_ONE - APP_BANK_MIN)
        bcc @loop
        clc
        rts
@found:
        sec
        rts

hidden_select_prev_app:
        jsr hidden_normalize_current_bank
        sta rb_saved_count_lo
        sta rb_tmp_lo
        lda #0
        sta rb_digit_count
@loop:
        lda rb_tmp_lo
        cmp #APP_BANK_MIN
        bne :+
        lda #APP_BANK_MAX_PLUS_ONE
        sta rb_tmp_lo
:       dec rb_tmp_lo
        jsr hidden_bank_loaded
        bcs @found
        inc rb_digit_count
        lda rb_digit_count
        cmp #(APP_BANK_MAX_PLUS_ONE - APP_BANK_MIN)
        bcc @loop
        clc
        rts
@found:
        sec
        rts

hidden_select_next_app_store:
        lda #0
        sta rb_found_kind
        jsr hidden_select_next_app
        bcc @done
        lda #1
        sta rb_found_kind
@done:
        rts

hidden_select_prev_app_store:
        lda #0
        sta rb_found_kind
        jsr hidden_select_prev_app
        bcc @done
        lda #1
        sta rb_found_kind
@done:
        rts

hidden_normalize_current_bank:
        lda SHIM_CURRENT_BANK
        cmp #APP_BANK_MIN
        bcc @default
        cmp #APP_BANK_MAX_PLUS_ONE
        bcs @default
        rts
@default:
        lda #APP_BANK_MIN
        rts

hidden_bank_loaded:
        lda rb_tmp_lo
        cmp #APP_BANK_MAX_PLUS_ONE
        bcs @clear
        clc
        adc #<RB_REUCB_TOKEN_STATUS_OFF
        sta rb_reu_off_lo
        lda #>RB_REUCB_TOKEN_STATUS_OFF
        adc #0
        sta rb_reu_off_hi
        lda #<rb_lookup_char
        sta rb_reu_c64_lo
        lda #>rb_lookup_char
        sta rb_reu_c64_hi
        lda SHIM_READYOS_BANK
        sta rb_reu_bank
        lda #1
        sta rb_reu_len_lo
        lda #0
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        lda rb_lookup_char
        and #$02                    ; schema-v5 TOKEN_LOADED
        beq @clear
        lda rb_tmp_lo
        cmp rb_saved_count_lo
        beq @clear
        sec
        rts
@clear:
        clc
        rts

.if 0
; Retired schema-v4 registry scan.  Schema-v5 token status is authoritative,
; so navigation no longer confuses physical resource-bank numbers with tokens.
hidden_logical_bank_is_registered_app:
        lda rb_tmp_lo
        sta rb_hidden_next_lo
        lda #0
        sta rb_hidden_next_hi
@scan:
        lda #<rb_lookup_char
        sta rb_reu_c64_lo
        lda #>rb_lookup_char
        sta rb_reu_c64_hi
        lda rb_hidden_next_hi
        clc
        adc #<RB_REUCB_APP_REG_OFF
        sta rb_reu_off_lo
        lda #>RB_REUCB_APP_REG_OFF
        adc #0
        sta rb_reu_off_hi
        lda SHIM_READYOS_BANK
        sta rb_reu_bank
        lda #1
        sta rb_reu_len_lo
        lda #0
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        lda rb_lookup_char
        cmp rb_hidden_next_lo
        beq @found
        inc rb_hidden_next_hi
        lda rb_hidden_next_hi
        cmp #RB_REUCB_APP_REG_COUNT
        bcc @scan
        clc
        rts
@found:
        sec
        rts
.endif

hidden_restore_vectors:
        lda rb_vectors_saved
        beq @done
        lda rb_orig_imain_lo
        sta IMAIN_VEC
        lda rb_orig_imain_hi
        sta IMAIN_VEC+1
        lda rb_orig_crunch_lo
        sta $0304
        lda rb_orig_crunch_hi
        sta $0305
        lda rb_orig_list_lo
        sta $0306
        lda rb_orig_list_hi
        sta $0307
        lda rb_orig_keylog_lo
        sta KEYLOG_VEC
        lda rb_orig_keylog_hi
        sta KEYLOG_VEC+1
        lda rb_orig_execute_lo
        sta $0308
        lda rb_orig_execute_hi
        sta $0309
        lda #<BASIC_EVAL
        sta $030A
        lda #>BASIC_EVAL
        sta $030B
        lda rb_orig_chrin_lo
        sta KERNAL_CHRIN_VEC
        lda rb_orig_chrin_hi
        sta KERNAL_CHRIN_VEC+1
@done:
        rts

hidden_restore_basic_runtime_state:
        lda RUNTIME_MAGIC1
        cmp #RB_STATE_MAGIC1
        beq :+
        jmp @fallback
:
        lda RUNTIME_MAGIC2
        cmp #RB_STATE_MAGIC2
        beq :+
        jmp @fallback
:
        lda RUNTIME_LINE_OK
        bne :+
        jmp @fallback
:

        lda #<RUNTIME_STACK_BUF
        sta rb_reu_c64_lo
        lda #>RUNTIME_STACK_BUF
        sta rb_reu_c64_hi
        lda #<RB_REU_RUNTIME_STACK_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_RUNTIME_STACK_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_fetch

        lda #<RUNTIME_ZP_BUF
        sta rb_reu_c64_lo
        lda #>RUNTIME_ZP_BUF
        sta rb_reu_c64_hi
        lda #<RB_REU_RUNTIME_ZP_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_RUNTIME_ZP_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_fetch

        lda RUNTIME_ZP_BUF + CHRGET
        cmp #$E6
        bne @fallback
        lda RUNTIME_ZP_BUF + CHRGET + 1
        cmp #$7A
        bne @fallback
        lda RUNTIME_ZP_BUF + CHRGET + 2
        cmp #$D0
        bne @fallback
        lda RUNTIME_FIRST_LO
        cmp #<BASIC_START
        bne @fallback
        lda RUNTIME_FIRST_HI
        cmp #>BASIC_START
        bne @fallback
        lda RUNTIME_MODE
        cmp #RB_RESUME_RUN
        bne @runtime_ok
        lda RUNTIME_ZP_BUF + TXTPTR + 1
        cmp #>BASIC_START
        bcc @fallback
@runtime_ok:

        jsr set_basic_memory_bounds
        lda #0
        sta KEYD_COUNT
        lda RUNTIME_MODE
        cmp #RB_RESUME_RUN
        beq @copy_runtime
        jsr prepare_basic_console
        jsr rb_draw_header
        jsr position_basic_prompt

@copy_runtime:
        ldx #0
@stack:
        lda RUNTIME_STACK_BUF,x
        sta $0100,x
        inx
        bne @stack
        ldx #2
@zp:
        lda RUNTIME_ZP_BUF,x
        sta $0000,x
        inx
        bne @zp
        lda RUNTIME_MODE
        cmp #RB_RESUME_RUN
        beq @running_resume
        jmp restore_basic_finish_ready
@running_resume:
        jmp restore_basic_finish_run
@fallback:
        jmp restore_basic_runtime_state_fallback

draw_default_header:
        lda #6
        sta VIC_BG
        sta VIC_BORDER
        jsr clear_default_screen
        jsr draw_box_top_row
        jsr draw_box_middle_row
        jsr draw_box_bottom_row
        ldx #0
@title:
        lda default_title_screen,x
        beq @free_label
        sta SCREEN+15,x
        lda #7
        sta COLOR_RAM+15,x
        inx
        bne @title
@free_label:
        ldx #0
@free_label_loop:
        lda default_free_label_screen,x
        beq @free_value
        sta SCREEN+42,x
        lda #15
        sta COLOR_RAM+42,x
        inx
        bne @free_label_loop
@free_value:
        ldx #0
        lda #32
@free_value_loop:
        sta SCREEN+48,x
        lda #13
        sta COLOR_RAM+48,x
        inx
        cpx #5
        bcc @free_value_loop
        ldx #0
@free_suffix_loop:
        lda default_free_suffix_screen,x
        beq @done
        sta SCREEN+53,x
        lda #15
        sta COLOR_RAM+53,x
        inx
        bne @free_suffix_loop
@done:
        rts

clear_default_screen:
        ldx #0
        lda #32
@screen_full:
        sta SCREEN,x
        sta SCREEN+$100,x
        sta SCREEN+$200,x
        inx
        bne @screen_full
        ldx #$E7
@screen_tail:
        sta SCREEN+$300,x
        dex
        bpl @screen_tail
        ldx #0
        lda #1
@color_full:
        sta COLOR_RAM,x
        sta COLOR_RAM+$100,x
        sta COLOR_RAM+$200,x
        inx
        bne @color_full
        ldx #$E7
@color_tail:
        sta COLOR_RAM+$300,x
        dex
        bpl @color_tail
        rts

draw_box_top_row:
        ldx #39
        lda #$40
@loop:
        sta SCREEN,x
        lda #14
        sta COLOR_RAM,x
        lda #$40
        dex
        bpl @loop
        lda #$70
        sta SCREEN
        lda #$6E
        sta SCREEN+39
        rts

draw_box_middle_row:
        ldx #39
        lda #32
@loop:
        sta SCREEN+40,x
        lda #1
        sta COLOR_RAM+40,x
        lda #32
        dex
        bpl @loop
        lda #$5D
        sta SCREEN+40
        sta SCREEN+79
        lda #14
        sta COLOR_RAM+40
        sta COLOR_RAM+79
        rts

draw_box_bottom_row:
        ldx #39
        lda #$40
@loop:
        sta SCREEN+80,x
        lda #14
        sta COLOR_RAM+80,x
        lda #$40
        dex
        bpl @loop
        lda #$6D
        sta SCREEN+80
        lda #$7D
        sta SCREEN+119
        rts

default_title_screen:
        .byte 18,5,1,4,25,2,1,19,9,3,0
default_free_label_screen:
        .byte 6,18,5,5,58,0
default_free_suffix_screen:
        .byte 32,2,1,19,9,3,32,2,25,20,5,19,0

rb_seed_plugin_reu_hidden:
        jsr rb_resolve_reu_banks_hidden
        jsr rb_mark_reu_banks_hidden
        lda rb_seed_cold
        bne :+
        rts
:
        lda #<rb_reu_header
        sta rb_reu_c64_lo
        lda #>rb_reu_header
        sta rb_reu_c64_hi
        lda #<RB_REU_HEADER_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_HEADER_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #rb_reu_header_end-rb_reu_header
        sta rb_reu_len_lo
        lda #0
        sta rb_reu_len_hi
        jsr rb_reu_stash

        jsr refresh_hidden_shadow

        lda #<rb_command_descriptors
        sta rb_reu_c64_lo
        lda #>rb_command_descriptors
        sta rb_reu_c64_hi
        lda #<RB_REU_DESC_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_DESC_OFF
        sta rb_reu_off_hi
        lda #<((RB_CMD_DESC_COUNT * RB_CMD_DESC_SIZE))
        sta rb_reu_len_lo
        lda #>((RB_CMD_DESC_COUNT * RB_CMD_DESC_SIZE))
        sta rb_reu_len_hi
        jsr rb_reu_stash

        lda #<__LOWPACK_LOAD__
        sta rb_reu_c64_lo
        lda #>__LOWPACK_LOAD__
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        sta rb_reu_off_hi
        lda rb_reu_code_bank
        sta rb_reu_bank
        lda #<((__SPANPACK_LOAD__ - __LOWPACK_LOAD__) + __SPANPACK_SIZE__)
        sta rb_reu_len_lo
        lda #>((__SPANPACK_LOAD__ - __LOWPACK_LOAD__) + __SPANPACK_SIZE__)
        sta rb_reu_len_hi
        jsr rb_reu_stash

        ldx #0
@stash_ovl:
        lda rb_builtin_ovl_stash,x
        sta rb_reu_c64_lo
        lda rb_builtin_ovl_stash+1,x
        sta rb_reu_c64_hi
        lda rb_builtin_ovl_stash+2,x
        sta rb_reu_off_lo
        lda rb_builtin_ovl_stash+3,x
        sta rb_reu_off_hi
        lda rb_builtin_ovl_stash+4,x
        sta rb_reu_len_lo
        lda rb_builtin_ovl_stash+5,x
        sta rb_reu_len_hi
        txa
        pha
        lda rb_reu_code_bank
        sta rb_reu_bank
        jsr rb_reu_stash
        pla
        clc
        adc #6
        tax
        cpx #36
        bcc @stash_ovl

        jsr rb_clear_slot_residency
        jmp rb_clear_handle_heap

rb_builtin_ovl_stash:
        .byte <__OVL1PACK_LOAD__, >__OVL1PACK_LOAD__, <RB_CODE_GFXSPR_OFF, >RB_CODE_GFXSPR_OFF, <__OVL1PACK_SIZE__, >__OVL1PACK_SIZE__
        .byte <__OVL2PACK_LOAD__, >__OVL2PACK_LOAD__, <RB_CODE_INPUTEV_OFF, >RB_CODE_INPUTEV_OFF, <__OVL2PACK_SIZE__, >__OVL2PACK_SIZE__
        .byte <__OVL3PACK_LOAD__, >__OVL3PACK_LOAD__, <RB_CODE_GFXPOLY_OFF, >RB_CODE_GFXPOLY_OFF, <__OVL3PACK_SIZE__, >__OVL3PACK_SIZE__
        .byte <__OVL4PACK_LOAD__, >__OVL4PACK_LOAD__, <RB_CODE_GFXDL_OFF, >RB_CODE_GFXDL_OFF, <__OVL4PACK_SIZE__, >__OVL4PACK_SIZE__
        .byte <__OVL5PACK_LOAD__, >__OVL5PACK_LOAD__, <RB_CODE_GFXTILE_OFF, >RB_CODE_GFXTILE_OFF, <__OVL5PACK_SIZE__, >__OVL5PACK_SIZE__
        .byte <__OVL6PACK_LOAD__, >__OVL6PACK_LOAD__, <RB_CODE_SIDCORE_OFF, >RB_CODE_SIDCORE_OFF, <__OVL6PACK_SIZE__, >__OVL6PACK_SIZE__

rb_clear_slot_residency:
        lda #0
        sta rb_slot0_cmd
        sta rb_slot0_overlay
        sta rb_copy_count
        rts

rb_mark_reu_banks_hidden:
        ldx rb_reu_core_bank
        beq :+
        lda #RB_REU_TYPE_CORE
        sta RB_PAGEBUF,x
:
        ldx rb_reu_code_bank
        beq :+
        lda #RB_REU_TYPE_CODE
        sta RB_PAGEBUF,x
:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #<RB_REUCB_BANK_TYPE_OFF
        sta rb_reu_off_lo
        lda #>RB_REUCB_BANK_TYPE_OFF
        sta rb_reu_off_hi
        lda SHIM_READYOS_BANK
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_stash
        rts

rb_resolve_reu_banks_hidden:
        lda #0
        sta rb_reu_core_bank
        sta rb_reu_code_bank
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #<RB_REUCB_BANK_TYPE_OFF
        sta rb_reu_off_lo
        lda #>RB_REUCB_BANK_TYPE_OFF
        sta rb_reu_off_hi
        lda SHIM_READYOS_BANK
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        ldx #0
@scan:
        lda RB_PAGEBUF,x
        cmp #RB_REU_TYPE_CORE
        bne @check_code
        lda rb_reu_core_bank
        bne @next
        stx rb_reu_core_bank
        jmp @next
@check_code:
        cmp #RB_REU_TYPE_CODE
        bne @next
        lda rb_reu_code_bank
        bne @next
        stx rb_reu_code_bank
@next:
        inx
        bne @scan
        rts

rb_clear_handle_heap:
        ldx #0
        lda #0
@zero:
        sta RB_PAGEBUF,x
        inx
        bne @zero
        ldx #0
@stash:
        lda rb_clear_heap_pages,x
        sta rb_reu_off_hi
        lda #0
        sta rb_reu_off_lo
        jsr rb_stash_zero_pagebuf
        inx
        cpx #3
        bcc @stash
        rts

rb_clear_heap_pages:
        .byte >RB_REU_HANDLE_OFF, >(RB_REU_HANDLE_OFF + $0100), >RB_REU_HEAP_OFF

rb_stash_zero_pagebuf:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_stash

save_basic_runtime_state:
        tsx
        txa
        clc
        adc #4
        sta RUNTIME_SP

        lda #0
        sta rb_reu_c64_lo
        sta rb_reu_c64_hi
        lda #<RB_REU_RUNTIME_ZP_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_RUNTIME_ZP_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_stash

        lda #0
        sta rb_reu_c64_lo
        lda #1
        sta rb_reu_c64_hi
        lda #<RB_REU_RUNTIME_STACK_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_RUNTIME_STACK_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_stash

        jsr refresh_hidden_shadow

        lda #RB_STATE_MAGIC1
        sta RUNTIME_MAGIC1
        lda #RB_STATE_MAGIC2
        sta RUNTIME_MAGIC2
        lda TXTTAB
        sta RUNTIME_FIRST_LO
        lda TXTTAB+1
        sta RUNTIME_FIRST_HI
        jmp validate_basic_line_chain

validate_basic_line_chain:
        lda #1
        sta RUNTIME_LINE_OK
        lda #<BASIC_START
        sta rb_ptr_lo
        lda #>BASIC_START
        sta rb_ptr_hi
@line:
        ldy #0
        lda (rb_ptr_lo),y
        sta rb_hidden_next_lo
        iny
        lda (rb_ptr_lo),y
        sta rb_hidden_next_hi
        ora rb_hidden_next_lo
        beq @done
        lda rb_hidden_next_hi
        cmp #>BASIC_START
        bcc @bad
        cmp #>BASIC_LIMIT
        bcs @bad
        cmp rb_ptr_hi
        bcc @bad
        bne @advance
        lda rb_hidden_next_lo
        cmp rb_ptr_lo
        beq @bad
        bcc @bad
@advance:
        lda rb_hidden_next_lo
        sta rb_ptr_lo
        lda rb_hidden_next_hi
        sta rb_ptr_hi
        jmp @line
@done:
        clc
        lda rb_ptr_lo
        adc #2
        sta RUNTIME_END_LO
        lda rb_ptr_hi
        adc #0
        sta RUNTIME_END_HI
        rts
@bad:
        lda #0
        sta RUNTIME_LINE_OK
        rts

rb_hidden_next_lo: .byte 0
rb_hidden_next_hi: .byte 0

refresh_hidden_shadow:
        lda #<__HIDDEN_RUN__
        sta rb_reu_c64_lo
        lda #>__HIDDEN_RUN__
        sta rb_reu_c64_hi
        lda #<RB_REU_HIDDEN_SHADOW_OFF
        sta rb_reu_off_lo
        lda #>RB_REU_HIDDEN_SHADOW_OFF
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #<__HIDDEN_SIZE__
        sta rb_reu_len_lo
        lda #>__HIDDEN_SIZE__
        sta rb_reu_len_hi
        jmp rb_reu_stash

; ---------------------------------------------------------------------------
; Bridge state only.  Code stays out of $C000 unless it must be resident there.
; ---------------------------------------------------------------------------

        .segment "BRIDGE"

rb_draw_header:
        jsr call_hidden_draw_default_header
        jsr rb_update_header_free
        lda #1
        sta COLOR_CODE
        rts

rb_print_live_free:
        lda #<hidden_print_live_free
        sta rb_lookup_index
        lda #>hidden_print_live_free
        sta rb_lookup_slots
        jmp call_hidden_common

rb_update_header_free:
        sec
        jsr K_PLOT
        stx rb_saved_plot_x
        sty rb_saved_plot_y

        clc
        ldx #1
        ldy #8
        jsr K_PLOT
        lda #13
        sta COLOR_CODE
        ldx #5
@blank:
        lda #' '
        jsr K_CHROUT
        dex
        bne @blank

        clc
        ldx #1
        ldy #8
        jsr K_PLOT
        jsr rb_print_live_free

        lda #1
        sta COLOR_CODE
        clc
        ldx rb_saved_plot_x
        ldy rb_saved_plot_y
        jsr K_PLOT
        rts

call_hidden_draw_default_header:
        lda #<draw_default_header
        sta rb_lookup_index
        lda #>draw_default_header
        sta rb_lookup_slots
        jmp call_hidden_common

rb_crunch:
        jsr rb_call_orig_crunch
        sty rb_crunch_len
        ldx #0
@scan:
        cpx rb_crunch_len
        bcs @done
        lda BASIC_INPUT_BUF,x
        cmp #TOKEN_THEN
        bne @advance
@skip:
        inx
        cpx rb_crunch_len
        bcs @done
        lda BASIC_INPUT_BUF,x
        cmp #' '
        beq @skip
        stx rb_peek_lo
        lda #>BASIC_INPUT_BUF
        sta rb_peek_hi
        jsr rb_match_exec
        bcs @matched
        jsr rb_match_jump
        bcc @advance
@matched:
        lda rb_crunch_len
        cmp #BASIC_INPUT_MAX
        bcs @done
        tax
@shift:
        lda BASIC_INPUT_BUF,x
        sta BASIC_INPUT_BUF+1,x
        cpx rb_peek_lo
        beq @insert
        dex
        jmp @shift
@insert:
        ldx rb_peek_lo
        lda #':'
        sta BASIC_INPUT_BUF,x
        inc rb_crunch_len
        inx
        inx
        jmp @scan
@done:
        ldy rb_crunch_len
        rts

@advance:
        inx
        jmp @scan

rb_call_orig_crunch:
        jmp (rb_orig_crunch_lo)

rb_magic:       .byte 0
rb_magic2:      .byte 0
rb_seed_cold:   .byte 0
rb_error:       .byte 0
rb_saved_cpu:   .byte 0
rb_common_saved_cpu:.byte 0
rb_vectors_saved:.byte 0
rb_orig_imain_lo:.byte 0
rb_orig_imain_hi:.byte 0
rb_orig_crunch_lo:.byte 0
rb_orig_crunch_hi:.byte 0
rb_orig_list_lo:.byte 0
rb_orig_list_hi:.byte 0
rb_orig_keylog_lo:.byte 0
rb_orig_keylog_hi:.byte 0
rb_orig_execute_lo:.byte 0
rb_orig_execute_hi:.byte 0
rb_orig_chrin_lo:.byte 0
rb_orig_chrin_hi:.byte 0
rb_peek_lo:     .byte 0
rb_peek_hi:     .byte 0
rb_crunch_len:  .byte 0
rb_eval_save_lo:.byte 0
rb_eval_save_hi:.byte 0
rb_eval_after_lo:.byte 0
rb_eval_after_hi:.byte 0
rb_expr_type:   .byte 0

rb_cmd_len:     .byte 0
rb_lookup_index:.byte 0
rb_lookup_slots:.byte 0
rb_lookup_char: .byte 0
rb_zmod_eof:    .byte 0
rb_saved_msgflg:.byte 0
rb_target_off:  .byte 0
rb_saved_count_lo:.byte 0
rb_saved_count_hi:.byte 0

        .segment "RESIDENT"
rb_num_save_depth:.byte 0
rb_save_num0_lo:.res 4
rb_save_num0_hi:.res 4
rb_float_save_depth:.byte 0
rb_save_float0_buf:.res 20

        .segment "ENTRY"
rb_free_lo:     .byte 0
rb_free_hi:     .byte 0
rb_tmp_lo:      .byte 0
rb_hotkey_target_bank:.byte 0
rb_digit_count: .byte 0
rb_digit_seen:  .byte 0
rb_saved_plot_x:.byte 0
rb_saved_plot_y:.byte 0

        .segment "BRIDGE"
rb_kw_proc:     .byte "PROC",0
rb_kw_func:     .byte "FUNC",0
rb_kw_exec:     .byte "EXEC",0
rb_kw_ret:      .byte "RET",0
rb_kw_repeat:   .byte "REPEAT",0
        .segment "RESIDENT"
rb_kw_until:    .byte "UNTIL",0
rb_kw_label:    .byte "LABEL",0
rb_kw_jump:     .byte "JUMP",0
        .segment "ENTRY"
rb_kw_endp:     .byte "ENDP",0
rb_kw_char:     .byte 0
rb_found_kind:  .byte 0

        .segment "BRIDGE"
rb_found_line_lo:.byte 0
rb_found_line_hi:.byte 0
rb_scan_line_lo:.byte 0
rb_scan_line_hi:.byte 0
rb_next_line_lo:.byte 0
rb_next_line_hi:.byte 0
rb_stmt_lo:     .byte 0
rb_stmt_hi:     .byte 0
rb_def_lo:      .byte 0
rb_def_hi:      .byte 0
rb_form_lo:     .byte 0
rb_form_hi:     .byte 0
rb_form_next_lo:.byte 0
rb_form_next_hi:.byte 0
rb_formal_lo:  .byte 0
rb_formal_hi:  .byte 0
rb_actual_lo:  .byte 0
rb_actual_hi:  .byte 0
rb_actual_wrapped:.byte 0
rb_bind_expr_mode:.byte 0
rb_command_precomputed:.byte 0
rb_exec_out_type:.byte 0
rb_exec_out_lo:.byte 0
rb_exec_out_hi:.byte 0
rb_proc_depth: .byte 0
rb_proc_ret_lo:.res RB_PROC_DEPTH
rb_proc_ret_hi:.res RB_PROC_DEPTH
rb_proc_cur_lo:.res RB_PROC_DEPTH
rb_proc_cur_hi:.res RB_PROC_DEPTH

        .segment "RESIDENT"
rb_loop_depth: .byte 0
rb_loop_txt_lo:.res RB_LOOP_DEPTH
rb_loop_txt_hi:.res RB_LOOP_DEPTH
rb_loop_cur_lo:.res RB_LOOP_DEPTH
rb_loop_cur_hi:.res RB_LOOP_DEPTH
rb_last_error: .byte 0
rb_last_line_lo:.byte 0
rb_last_line_hi:.byte 0
rb_hotkey_pending:.byte 0
rb_hotkey_suppress:.byte 0
rb_prompt_active:.byte 0
rb_hotkey_chrin_dispatch:.byte 0
rb_hotkey_release_key:.byte 0
rb_hotkey_release_start:.byte 0

rb_func_depth:.byte 0
rb_form_save_count:.res RB_PROC_DEPTH
        .segment "BRIDGE"
rb_form_save_type:.res 16
rb_form_save_lo:.res 16
        .segment "RESIDENT"
rb_form_save_hi:.res 16
rb_form_save_val0:.res 16
rb_form_save_val1:.res 16
rb_form_save_val2:.res 16
rb_form_save_val3:.res 16
rb_form_save_val4:.res 16
rb_save_type_tmp:.byte 0
rb_save_depth_tmp:.byte 0
rb_save_count_tmp:.byte 0
rb_save_idx:.byte 0

        .segment "BRIDGE"
RUNTIME_MAGIC1:  .byte 0
RUNTIME_MAGIC2:  .byte 0
RUNTIME_SP:      .byte 0
RUNTIME_MODE:    .byte 0
RUNTIME_LINE_OK: .byte 0
RUNTIME_FIRST_LO:.byte 0
RUNTIME_FIRST_HI:.byte 0
RUNTIME_END_LO:  .byte 0
RUNTIME_END_HI:  .byte 0

rb_out_type:    .byte 0
rb_out_count:   .byte 0
rb_out_ptr_lo:  .byte 0
rb_out_ptr_hi:  .byte 0
rb_out_count_lo:.byte 0
rb_out_count_hi:.byte 0

        .segment "ENTRY"
rb_overlay_vec_lo:.byte 0
rb_overlay_vec_hi:.byte 0
rb_slot0_cmd:   .byte 0
rb_slot0_overlay:.byte 0
rb_copy_count:  .byte 0

rb_reu_c64_lo:  .byte 0
rb_reu_c64_hi:  .byte 0
rb_reu_off_lo:  .byte 0
rb_reu_off_hi:  .byte 0

        .segment "BRIDGE"
rb_reu_bank:    .byte 0
rb_reu_len_lo:  .byte 0
rb_reu_len_hi:  .byte 0
rb_reu_core_bank:.byte 0
rb_reu_code_bank:.byte 0

rb_handle_bank: .byte 0
rb_handle_page: .byte 0
rb_handle_pages:.byte 0
rb_handle_type: .byte 0
rb_handle_index:.byte 0
rb_needed_pages:.byte 0
rb_handle_new_type:.byte 0
rb_found_page:  .byte 0
rb_handle_desc_off:.byte 0
rb_handle_scan_base:.byte 0
rb_fill_page:   .byte 0
rb_copy_page:   .byte 0
rb_copy_chunks: .byte 0
rb_copy_len_lo: .byte 0
rb_copy_len_hi: .byte 0

install_vectors:
        lda #<hidden_install_vectors
        sta rb_lookup_index
        lda #>hidden_install_vectors
        sta rb_lookup_slots
        jmp call_hidden_common

rb_keylog:
        lda DFLTN
        bne @orig
        lda rb_prompt_active
        bne @prompt
        lda TXTPTR+1
        cmp #>BASIC_START
        bcc @prompt
        bne @orig
        lda TXTPTR
        cmp #<BASIC_START
        bcs @orig
@prompt:
        lda rb_hotkey_pending
        bne @consume
        lda SFDX
        cmp #KEY_MATRIX_B
        bne @maybe_f2
        lda SHFLAG
        and #SHFLAG_CTRL
        beq @orig
        ldx #KEY_CTRL_B
        bne @hotkey
@maybe_f2:
        cmp #KEY_MATRIX_F1
        bne @maybe_f4
        lda SHFLAG
        and #SHFLAG_SHIFT
        beq @orig
        ldx #KEY_F2
        bne @hotkey
@maybe_f4:
        cmp #KEY_MATRIX_F3
        bne @orig
        lda SHFLAG
        and #SHFLAG_SHIFT
        beq @orig
        ldx #KEY_F4
@hotkey:
        cpx rb_hotkey_suppress
        beq @consume
        stx rb_hotkey_suppress
        jsr rb_queue_hotkey_line
@consume:
        lda #$40
        sta LSTX
        sta SFDX
        lda #0
        sta SHFLAG
        rts
@orig:
        jmp (rb_orig_keylog_lo)

call_hidden_select_next_app:
        lda #<hidden_select_next_app_store
        bne call_hidden_select_app
call_hidden_select_prev_app:
        lda #<hidden_select_prev_app_store
call_hidden_select_app:
        .assert >hidden_select_next_app_store = >hidden_select_prev_app_store, error, "ReadyBASIC selector stubs crossed a hidden page"
        sta rb_lookup_index
        lda #>hidden_select_next_app_store
        sta rb_lookup_slots
        jmp call_hidden_common

rb_prepare_ready_resume:
        lda #<hidden_prepare_ready_resume
        sta rb_lookup_index
        lda #>hidden_prepare_ready_resume
        sta rb_lookup_slots
        jmp call_hidden_common

; ---------------------------------------------------------------------------
; Packed command submodule 0. This 2KB slot image is copied from the
; loader-assigned ReadyBASIC code bank
; into $A800-$AFFF on demand, then reused while residency metadata matches.
; ---------------------------------------------------------------------------

        .segment "LOWPACK"

cmd_zecho1_low:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #1
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_zecho1_low_end:

cmd_zadd16_low:
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
cmd_zadd16_low_end:

cmd_zslot0_low:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #30
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_zslot0_low_end:

cmd_zcpyrst_low:
        lda #0
        sta rb_copy_count
        sta RF_STATUS
        sta RF_VAL_LO
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        rts
cmd_zcpyrst_low_end:

cmd_zcopy_low:
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda rb_copy_count
        sta RF_VAL_LO
        lda #RB_VAL_INT
        sta RF_TAG
        rts
cmd_zcopy_low_end:

cmd_fadd_low:
        rts
cmd_fadd_low_end:

cmd_zpause_low:
        lda CF_NUM0_LO
        beq @done
        sta RF_COUNT_LO
@outer:
        ldx #$20
@middle:
        ldy #$ff
@inner:
        dey
        bne @inner
        dex
        bne @middle
        dec RF_COUNT_LO
        bne @outer
@done:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts
cmd_zpause_low_end:

cmd_upper_low:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_STRING
        sta RF_TAG
        lda CF_STR_LEN
        sta RF_STR_LEN
        ldy #0
@loop:
        cpy CF_STR_LEN
        beq @done
        lda CF_STR_BUF,y
        cmp #$C1
        bcc @ascii_case
        cmp #$DB
        bcs @ascii_case
        sec
        sbc #$80
        jmp @store
@ascii_case:
        cmp #'a'
        bcc :+
        cmp #'z' + 1
        bcs :+
        sec
        sbc #$20
@store:
:       sta RF_STR_BUF,y
        iny
        jmp @loop
@done:
        rts
cmd_upper_low_end:

cmd_lower_low:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_STRING
        sta RF_TAG
        lda CF_STR_LEN
        sta RF_STR_LEN
        ldy #0
@loop:
        cpy CF_STR_LEN
        beq @done
        lda CF_STR_BUF,y
        cmp #$C1
        bcc @ascii_case
        cmp #$DB
        bcs @ascii_case
        sec
        sbc #$60
        jmp @store
@ascii_case:
        cmp #'A'
        bcc :+
        cmp #'Z' + 1
        bcs :+
        clc
        adc #$20
@store:
:       sta RF_STR_BUF,y
        iny
        jmp @loop
@done:
        rts
cmd_lower_low_end:

cmd_zsumnumarray_low:
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
cmd_zsumnumarray_low_end:

cmd_zrangenumarray_low:
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
cmd_zrangenumarray_low_end:

cmd_bufnew_low:
        jsr rb_handle_alloc
        rts
cmd_bufnew_low_end:

cmd_buffill_low:
        jsr rb_handle_fill
        rts
cmd_buffill_low_end:

cmd_buffree_low:
        jsr rb_handle_free
        rts
cmd_buffree_low_end:

cmd_ztempscratch_low:
        jsr rb_temp_alloc
        rts
cmd_ztempscratch_low_end:

cmd_zfail_low:
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
cmd_zfail_low_end:

cmd_freemem_low:
        jsr rb_print_live_free
        lda #13
        jsr K_CHROUT
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts
cmd_freemem_low_end:

cmd_scrcap_low:
        jsr rb_screen_handle_alloc
        lda RF_STATUS
        beq :+
        rts
:       jsr rb_screen_save_text
        jsr rb_screen_save_color
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        ldx rb_handle_index
        txa
        clc
        adc #1
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_scrcap_low_end:

cmd_scrput_low:
        jsr rb_screen_handle_validate
        lda RF_STATUS
        beq :+
        rts
:       jsr rb_screen_load_text
        jsr rb_screen_load_color
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts
cmd_scrput_low_end:

cmd_gfxsurf_low:
        lda #RB_GFX_SURF_PAGES
        sta rb_needed_pages
        lda #RB_HANDLE_TYPE_GFXSURF
        sta rb_handle_new_type
        jsr rb_handle_alloc_with_pages
        rts
cmd_gfxsurf_low_end:

cmd_gfxtarget_low:
        lda CF_NUM0_LO
        ora CF_NUM0_HI
        bne @handle
        sta RB_GFX_TARGET_HANDLE
        sta RB_GFX_TARGET_BANK
        sta RB_GFX_TARGET_PAGE
        sta RB_GFX_TARGET_PAGES
        jmp rb_gfx_ok_none
@handle:
        jsr rb_handle_load_arg
        bcs @bad
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_GFXSURF
        bne @wrong
        lda rb_handle_index
        clc
        adc #1
        sta RB_GFX_TARGET_HANDLE
        lda rb_handle_bank
        sta RB_GFX_TARGET_BANK
        lda rb_handle_page
        sta RB_GFX_TARGET_PAGE
        lda rb_handle_pages
        sta RB_GFX_TARGET_PAGES
        jmp rb_gfx_ok_none
@bad:
        lda #$24
        jmp rb_overlay_fail
@wrong:
        lda #$28
        jmp rb_overlay_fail
cmd_gfxtarget_low_end:

cmd_gfxblit_low:
        jsr rb_handle_load_arg
        bcs @bad
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_GFXSURF
        bne @wrong
        lda CPU_PORT
        pha
        lda #$35
        sta CPU_PORT
        jsr rb_gfx_blit_bitmap
        jsr rb_gfx_blit_screen
        jsr rb_gfx_blit_color
        pla
        sta CPU_PORT
        jmp rb_gfx_ok_none
@bad:
        lda #$24
        jmp rb_overlay_fail
@wrong:
        lda #$28
        jmp rb_overlay_fail
cmd_gfxblit_low_end:

rb_gfx_ok_none:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

rb_gfx_blit_bitmap:
        lda #<RB_GFX_BITMAP
        sta rb_reu_c64_lo
        lda #>RB_GFX_BITMAP
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #$20
        sta rb_reu_len_hi
        jmp rb_reu_fetch

rb_gfx_blit_screen:
        lda #<RB_GFX_SCREEN
        sta rb_reu_c64_lo
        lda #>RB_GFX_SCREEN
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        clc
        adc #$20
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #4
        sta rb_reu_len_hi
        jmp rb_reu_fetch

rb_gfx_blit_color:
        lda #<RB_GFX_COLOR
        sta rb_reu_c64_lo
        lda #>RB_GFX_COLOR
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        clc
        adc #$24
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #4
        sta rb_reu_len_hi
        jmp rb_reu_fetch

rb_handle_alloc:
        jsr rb_len_to_pages
        bcc :+
        jmp rb_overlay_bad_length
:       lda #RB_HANDLE_TYPE_BUFFER
        sta rb_handle_new_type
        jmp rb_handle_alloc_with_pages

rb_screen_handle_alloc:
        lda #RB_SCREEN_HANDLE_PAGES
        sta rb_needed_pages
        lda #RB_HANDLE_TYPE_SCREEN_TC
        sta rb_handle_new_type

rb_handle_alloc_with_pages:
        jsr rb_find_free_handle
        bcc @got_handle
        lda #$21
        jmp rb_overlay_fail
@got_handle:
        jsr rb_find_pages
        bcs @no_pages
        lda rb_reu_core_bank
        sta rb_handle_bank
        lda rb_found_page
        clc
        adc #RB_HEAP_PAGE_BASE
        sta rb_handle_page
        lda rb_needed_pages
        sta rb_handle_pages
        lda rb_handle_new_type
        sta rb_handle_type
        jsr rb_mark_pages_used
        jsr rb_store_heap_bitmap
        jsr rb_handle_store
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda rb_handle_index
        clc
        adc #1
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
@no_pages:
        lda #$22
        jmp rb_overlay_fail

rb_screen_handle_validate:
        jsr rb_handle_load_arg
        bcs @bad
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_SCREEN_TC
        bne @wrong_type
        clc
        rts
@wrong_type:
        lda #$28
        jmp rb_overlay_fail
@bad:
        lda #$24
        jmp rb_overlay_fail

rb_overlay_bad_length:
        lda #$23
rb_overlay_fail:
        sta RF_STATUS
        sta RF_ERROR
        rts

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

rb_handle_desc_fetch_page:
        lda rb_handle_index
        and #$3F
        asl
        asl
        sta rb_handle_desc_off
        lda #0
        sta rb_reu_off_lo
        lda #>RB_REU_HANDLE_OFF
        ldx rb_handle_index
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc :+
        clc
        adc #1
:       sta rb_reu_off_hi
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        rts

rb_handle_fetch:
        jsr rb_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda RB_PAGEBUF,y
        sta rb_handle_bank
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_page
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_pages
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_type
        rts

rb_handle_store:
        jsr rb_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda rb_handle_bank
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_page
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_pages
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_type
        sta RB_PAGEBUF,y
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_stash
        rts

rb_find_free_handle:
        lda #0
        sta rb_handle_scan_base
@page:
        lda rb_handle_scan_base
        sta rb_handle_index
        jsr rb_handle_desc_fetch_page
        ldy #0
        ldx #0
@slot:
        lda RB_PAGEBUF,y
        beq @found
        tya
        clc
        adc #RB_HANDLE_DESC_SIZE
        tay
        inx
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc @slot
        lda rb_handle_scan_base
        bne @full
        lda #RB_HANDLE_PAGE_SLOTS
        sta rb_handle_scan_base
        jmp @page
@found:
        txa
        clc
        adc rb_handle_scan_base
        sta rb_handle_index
        clc
        rts
@full:
        sec
        rts

rb_handle_load_arg:
        lda CF_NUM0_HI
        bne @bad
        lda CF_NUM0_LO
        beq @bad
        cmp #RB_HANDLE_COUNT + 1
        bcs @bad
        sec
        sbc #1
        sta rb_handle_index
        jsr rb_handle_fetch
        lda rb_handle_bank
        beq @bad
        clc
        rts
@bad:
        sec
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

rb_store_heap_bitmap:
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
        jsr rb_reu_stash
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

rb_handle_free:
        jsr rb_handle_load_arg
        bcs @bad
        lda rb_handle_page
        sec
        sbc #RB_HEAP_PAGE_BASE
        sta rb_found_page
        lda rb_handle_pages
        sta rb_needed_pages
        jsr rb_fetch_heap_bitmap
        jsr rb_mark_pages_free
        lda #0
        sta rb_handle_bank
        sta rb_handle_page
        sta rb_handle_pages
        sta rb_handle_type
        jsr rb_store_heap_bitmap
        jsr rb_handle_store
        lda #0
        sta RF_STATUS
        sta RF_TAG
        rts
@bad:
        lda #$24
        jmp rb_overlay_fail

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

rb_handle_fill:
        jsr rb_handle_load_arg
        bcs @bad
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_BUFFER
        bne @wrong_type
        lda CF_NUM1_LO
        ldx #0
@fillbuf:
        sta RB_PAGEBUF,x
        inx
        bne @fillbuf
        lda rb_handle_page
        sta rb_fill_page
        lda rb_handle_pages
        sta rb_needed_pages
@page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_fill_page
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_reu_stash
        inc rb_fill_page
        dec rb_needed_pages
        bne @page
        lda #0
        sta RF_STATUS
        sta RF_TAG
        rts
@bad:
        lda #$25
        jmp rb_overlay_fail
@wrong_type:
        lda #$28
        jmp rb_overlay_fail

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

rb_screen_save_text:
        lda #<SCREEN
        sta rb_reu_c64_lo
        lda #>SCREEN
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #<RB_SCREEN_BYTES
        sta rb_reu_len_lo
        lda #>RB_SCREEN_BYTES
        sta rb_reu_len_hi
        jsr rb_reu_stash
        rts

rb_screen_load_text:
        lda #<SCREEN
        sta rb_reu_c64_lo
        lda #>SCREEN
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #<RB_SCREEN_BYTES
        sta rb_reu_len_lo
        lda #>RB_SCREEN_BYTES
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        rts

rb_screen_save_color:
        lda #<COLOR_RAM
        sta rb_ptr_lo
        lda #>COLOR_RAM
        sta rb_ptr_hi
        lda rb_handle_page
        clc
        adc #4
        sta rb_copy_page
        lda #4
        sta rb_copy_chunks
@chunk:
        jsr rb_screen_set_copy_len
        jsr rb_copy_ptr_to_pagebuf
        jsr rb_stash_pagebuf_to_copy_page
        inc rb_ptr_hi
        inc rb_copy_page
        dec rb_copy_chunks
        bne @chunk
        rts

rb_screen_load_color:
        lda #<COLOR_RAM
        sta rb_ptr_lo
        lda #>COLOR_RAM
        sta rb_ptr_hi
        lda rb_handle_page
        clc
        adc #4
        sta rb_copy_page
        lda #4
        sta rb_copy_chunks
@chunk:
        jsr rb_screen_set_copy_len
        jsr rb_fetch_pagebuf_from_copy_page
        jsr rb_copy_pagebuf_to_ptr
        inc rb_ptr_hi
        inc rb_copy_page
        dec rb_copy_chunks
        bne @chunk
        rts

rb_screen_set_copy_len:
        lda rb_copy_chunks
        cmp #1
        beq @tail
        lda #0
        sta rb_copy_len_lo
        lda #1
        sta rb_copy_len_hi
        rts
@tail:
        lda #<RB_SCREEN_BYTES
        sta rb_copy_len_lo
        lda #0
        sta rb_copy_len_hi
        rts

rb_copy_ptr_to_pagebuf:
        lda rb_copy_len_hi
        beq @short
        ldy #0
@full:
        lda (rb_ptr_lo),y
        sta RB_PAGEBUF,y
        iny
        bne @full
        rts
@short:
        ldy #0
@short_loop:
        cpy rb_copy_len_lo
        beq @done
        lda (rb_ptr_lo),y
        sta RB_PAGEBUF,y
        iny
        jmp @short_loop
@done:
        rts

rb_copy_pagebuf_to_ptr:
        lda rb_copy_len_hi
        beq @short
        ldy #0
@full:
        lda RB_PAGEBUF,y
        sta (rb_ptr_lo),y
        iny
        bne @full
        rts
@short:
        ldy #0
@short_loop:
        cpy rb_copy_len_lo
        beq @done
        lda RB_PAGEBUF,y
        sta (rb_ptr_lo),y
        iny
        jmp @short_loop
@done:
        rts

rb_stash_pagebuf_to_copy_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_copy_page
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda rb_copy_len_lo
        sta rb_reu_len_lo
        lda rb_copy_len_hi
        sta rb_reu_len_hi
        jsr rb_reu_stash
        rts

rb_fetch_pagebuf_from_copy_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_copy_page
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda rb_copy_len_lo
        sta rb_reu_len_lo
        lda rb_copy_len_hi
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        rts

; ---------------------------------------------------------------------------
; Former hidden worker command implementation. It now lives in the same
; default command submodule as the other command workers.
; ---------------------------------------------------------------------------

cmd_zhiddenram_hidden:
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
cmd_zhiddenram_hidden_end:

        .segment "SLOTPACK1"

cmd_zslot1:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #31
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_zslot1_end:

cmd_gfxmode:
        lda CF_STR_LEN
        beq @return_current
        jsr gfx_mode_from_string
        bcc @bad
        jsr gfx_apply_mode
@return_current:
        jsr gfx_get_mode
        pha
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        pla
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
@bad:
        lda #$41
        jmp gfx_fail

cmd_gfxtext:
        jsr gfx_apply_text
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

cmd_gfxclear:
        jsr gfx_get_mode
        bne @gfx
        lda #147
        jsr K_CHROUT
        jmp gfx_ok_none
@gfx:
        sta rb_saved_plot_y
        jsr gfx_clear_bankd_screen
        lda rb_saved_plot_y
        cmp #RB_GFX_MODE_TILE
        beq @tile
        cmp #RB_GFX_MODE_MTILE
        beq @tile
        jsr gfx_clear_bitmap
@done:
        jmp gfx_ok_none
@tile:
        jsr gfx_init_tile_glyphs
        jmp @done

cmd_gfxtarget:
        jmp gfx_ok_none

cmd_gfxsync:
        lda RB_GFX_TARGET_HANDLE
        bne :+
        jmp gfx_ok_none
:
        lda CPU_PORT
        pha
        lda #$35
        sta CPU_PORT
        jsr gfx_sync_bitmap
        jsr gfx_sync_screen
        jsr gfx_sync_color
        pla
        sta CPU_PORT
        jmp gfx_ok_none

gfx_ok_none:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

gfx_fail:
        sta RF_STATUS
        sta RF_ERROR
        rts

gfx_sync_bitmap:
        lda #<RB_GFX_BITMAP
        sta rb_reu_c64_lo
        lda #>RB_GFX_BITMAP
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda RB_GFX_TARGET_PAGE
        sta rb_reu_off_hi
        lda RB_GFX_TARGET_BANK
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #$20
        sta rb_reu_len_hi
        jmp rb_reu_stash

gfx_sync_screen:
        lda #<RB_GFX_SCREEN
        sta rb_reu_c64_lo
        lda #>RB_GFX_SCREEN
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda RB_GFX_TARGET_PAGE
        clc
        adc #$20
        sta rb_reu_off_hi
        lda RB_GFX_TARGET_BANK
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #4
        sta rb_reu_len_hi
        jmp rb_reu_stash

gfx_sync_color:
        lda #<RB_GFX_COLOR
        sta rb_reu_c64_lo
        lda #>RB_GFX_COLOR
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda RB_GFX_TARGET_PAGE
        clc
        adc #$24
        sta rb_reu_off_hi
        lda RB_GFX_TARGET_BANK
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #4
        sta rb_reu_len_hi
        jmp rb_reu_stash

gfx_mode_from_string:
        lda CF_STR_LEN
        cmp #5
        beq @len5
        cmp #7
        beq @len7
        cmp #4
        bne :+
        jmp @len4
:
        clc
        rts
@len5:
        lda CF_STR_BUF
        jsr gfx_fold
        cmp #'H'
        beq @hires
        cmp #'T'
        beq @tile
        cmp #'M'
        beq @mtile5
        clc
        rts
@hires:
        lda CF_STR_BUF+1
        jsr gfx_fold
        cmp #'I'
        bne @bad_len5
        lda CF_STR_BUF+2
        jsr gfx_fold
        cmp #'R'
        bne @bad_len5
        lda CF_STR_BUF+3
        jsr gfx_fold
        cmp #'E'
        bne @bad_len5
        lda CF_STR_BUF+4
        jsr gfx_fold
        cmp #'S'
        bne @bad_len5
        lda #RB_GFX_MODE_HIRES
        sec
        rts
@tile:
        lda CF_STR_BUF+1
        jsr gfx_fold
        cmp #'I'
        bne @bad_len5
        lda CF_STR_BUF+2
        jsr gfx_fold
        cmp #'L'
        bne @bad_len5
        lda CF_STR_BUF+3
        jsr gfx_fold
        cmp #'E'
        bne @bad_len5
        lda #RB_GFX_MODE_TILE
        sec
        rts
@bad_len5:
        clc
        rts
@mtile5:
        lda #RB_GFX_MODE_MTILE
        sec
        rts
@len7:
        lda CF_STR_BUF
        jsr gfx_fold
        cmp #'M'
        bne @no
        lda CF_STR_BUF+1
        jsr gfx_fold
        cmp #'B'
        beq @mbitmap
        cmp #'T'
        beq @mtile
        bne @no
@mbitmap:
        lda #RB_GFX_MODE_MBITMAP
        sec
        rts
@mtile:
        lda #RB_GFX_MODE_MTILE
        sec
        rts
@len4:
        lda CF_STR_BUF
        jsr gfx_fold
        cmp #'T'
        bne @no
        lda CF_STR_BUF+1
        jsr gfx_fold
        cmp #'I'
        beq @tile4
        cmp #'E'
        bne @no
        lda #RB_GFX_MODE_TEXT
        sec
        rts
@tile4:
        lda #RB_GFX_MODE_TILE
        sec
        rts
@no:
        clc
        rts

gfx_fold:
        cmp #'a'
        bcc :+
        cmp #'z' + 1
        bcs :+
        sec
        sbc #$20
:       rts

gfx_apply_mode:
        cmp #RB_GFX_MODE_TEXT
        bne :+
        jmp gfx_apply_text
:
        pha
        lda CIA2_DDRA
        ora #$03
        sta CIA2_DDRA
        lda CIA2_PRA
        and #$FC
        sta CIA2_PRA
        lda #RB_GFX_VICMEM
        sta VIC_MEM
        lda VIC_CTRL1
        ora #$20
        sta rb_saved_plot_x
        pla
        pha
        tax
        lda rb_saved_plot_x
        cpx #RB_GFX_MODE_TILE
        beq @textlike
        cpx #RB_GFX_MODE_MTILE
        bne @store_d011
@textlike:
        and #$DF
@store_d011:
        sta VIC_CTRL1
        lda VIC_CTRL2
        sta rb_saved_plot_x
        pla
        tax
        lda rb_saved_plot_x
        cpx #RB_GFX_MODE_MBITMAP
        beq @multi
        cpx #RB_GFX_MODE_MTILE
        beq @multi
        and #$EF
        jmp @store_d016
@multi:
        ora #$10
@store_d016:
        sta VIC_CTRL2
        txa
        sta RB_GFX_MODE_STATE
        rts

gfx_get_mode:
        lda RB_GFX_MODE_STATE
        cmp #RB_GFX_MODE_MTILE + 1
        bcc :+
        lda #RB_GFX_MODE_TEXT
:       rts

gfx_apply_text:
        lda CIA2_DDRA
        ora #$03
        sta CIA2_DDRA
        lda CIA2_PRA
        ora #$03
        sta CIA2_PRA
        lda #VIC_MEM_LOWERCASE
        sta VIC_MEM
        lda VIC_CTRL1
        and #$DF
        sta VIC_CTRL1
        lda VIC_CTRL2
        and #$EF
        sta VIC_CTRL2
        lda #RB_GFX_MODE_TEXT
        sta RB_GFX_MODE_STATE
        rts

gfx_clear_bankd_screen:
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda CF_NUM0_LO
        and #$0F
        sta rb_saved_plot_x
        lda rb_saved_plot_y
        cmp #RB_GFX_MODE_TILE
        beq @dup_color
        cmp #RB_GFX_MODE_MTILE
        beq @dup_color
        lda rb_saved_plot_x
        ora #$10
        jmp @fill_screen
@dup_color:
        lda rb_saved_plot_x
        asl
        asl
        asl
        asl
        ora rb_saved_plot_x
@fill_screen:
        ldx #4
@page:
        ldy #0
@loop:
        sta (rb_ptr_lo),y
        iny
        bne @loop
        inc rb_ptr_hi
        dex
        bne @page
        lda #<RB_GFX_COLOR
        sta rb_ptr_lo
        lda #>RB_GFX_COLOR
        sta rb_ptr_hi
        lda CF_NUM0_LO
        and #$0F
        ldx #4
@cpage:
        ldy #0
@cloop:
        sta (rb_ptr_lo),y
        iny
        bne @cloop
        inc rb_ptr_hi
        dex
        bne @cpage
        rts

gfx_clear_bitmap:
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        lda #<RB_GFX_BITMAP
        sta rb_ptr_lo
        lda #>RB_GFX_BITMAP
        sta rb_ptr_hi
        lda #0
        ldx #32
@page:
        ldy #0
@loop:
        sta (rb_ptr_lo),y
        iny
        bne @loop
        inc rb_ptr_hi
        dex
        bne @page
        pla
        sta CPU_PORT
        rts

gfx_init_tile_glyphs:
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        lda #<($E000 + (64 * 8))
        sta rb_ptr_lo
        lda #>($E000 + (64 * 8))
        sta rb_ptr_hi
        lda #$FF
        ldy #0
@loop:
        sta (rb_ptr_lo),y
        iny
        cpy #$80
        bne @loop
        pla
        sta CPU_PORT
        rts

cmd_zmodload:
        lda CF_STR_LEN
        bne :+
        lda #240
        jmp @return_int
:
        ldx #<CF_STR_BUF
        ldy #>CF_STR_BUF
        jsr K_SETNAM
        lda #1
        ldx #8
        ldy #2
        jsr K_SETLFS
        lda MSGFLG
        sta rb_saved_msgflg
        lda #0
        sta MSGFLG
        sta rb_zmod_eof
        jsr K_OPEN
        bcs @fail_io_finish
        ldx #1
        jsr K_CHKIN
        bcs @fail_io_finish
        lda #16
        jsr rb_zmodload_read_pagebuf_bytes
        bcc @loaded
@fail_io_finish:
        lda #241
        jmp @finish
@loaded:
        lda RB_PAGEBUF
        cmp #'R'
        bne @fail_magic
        lda RB_PAGEBUF+1
        cmp #'B'
        bne @fail_magic
        lda RB_PAGEBUF+2
        cmp #'M'
        bne @fail_magic
        lda RB_PAGEBUF+3
        cmp #'!'
        bne @fail_magic
        lda RB_PAGEBUF+4
        cmp #1
        bne @fail_version
        lda RB_PAGEBUF+6
        beq @fail_count
        cmp #33
        bcs @fail_count
        sta rb_saved_count_lo
        lda RB_PAGEBUF+7
        cmp #33
        bcs @fail_count
        sta rb_saved_count_hi
        jsr rb_zmodload_stash_descriptors
        bcs @fail_bounds
        jsr rb_zmodload_stash_payloads
        bcs @fail_bounds
        jsr rb_clear_slot_residency
        lda rb_saved_count_lo
        jmp @finish
@fail_magic:
        lda #242
        jmp @finish
@fail_version:
        lda #243
        jmp @finish
@fail_count:
        lda #244
        jmp @finish
@fail_bounds:
        lda #245
@finish:
        pha
        jsr rb_zmodload_close
        pla
        jmp @return_int
@return_int:
        sta RF_VAL_LO
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        rts

rb_zmodload_stash_descriptors:
        lda rb_saved_count_lo
        sta rb_copy_len_lo
        lda #0
        sta rb_copy_len_hi
.repeat 5
        asl rb_copy_len_lo
        rol rb_copy_len_hi
.endrepeat
        lda RB_PAGEBUF+8
        sta rb_reu_off_lo
        lda RB_PAGEBUF+9
        sta rb_reu_off_hi
        cmp #>RB_REU_DESC_OFF
        bcc @bad
        cmp #>RB_REU_SLOT_STATE_OFF
        bcs @bad
        clc
        lda rb_reu_off_lo
        adc rb_copy_len_lo
        tax
        lda rb_reu_off_hi
        adc rb_copy_len_hi
        cmp #>RB_REU_SLOT_STATE_OFF
        bcc @stream
        bne @bad
        txa
        bne @bad
@stream:
        lda rb_reu_core_bank
        sta rb_reu_bank
        jmp rb_zmodload_stream_to_reu
@bad:
        sec
        rts

rb_zmodload_stash_payloads:
        lda rb_saved_count_hi
        sta rb_saved_count_hi
        beq @done
@loop:
        lda #6
        jsr rb_zmodload_read_pagebuf_bytes
        bcs @bad
        lda RB_PAGEBUF+2
        ora RB_PAGEBUF+3
        beq @bad
        lda RB_PAGEBUF
        sta rb_reu_off_lo
        lda RB_PAGEBUF+1
        sta rb_reu_off_hi
        lda RB_PAGEBUF+2
        sta rb_copy_len_lo
        lda RB_PAGEBUF+3
        sta rb_copy_len_hi
        clc
        lda rb_reu_off_lo
        adc rb_copy_len_lo
        lda rb_reu_off_hi
        adc rb_copy_len_hi
        bcs @bad
        lda rb_reu_code_bank
        sta rb_reu_bank
        jsr rb_zmodload_stream_to_reu
        bcs @bad
        dec rb_saved_count_hi
        bne @loop
@done:
        clc
        rts
@bad:
        sec
        rts

rb_zmodload_read_pagebuf_bytes:
        sta rb_copy_chunks
        lda #0
        sta rb_target_off
@loop:
        jsr rb_zmodload_read_byte
        bcs @bad
        ldy rb_target_off
        sta RB_PAGEBUF,y
        inc rb_target_off
        dec rb_copy_chunks
        bne @loop
        clc
        rts
@bad:
        sec
        rts

rb_zmodload_stream_to_reu:
@more:
        lda rb_copy_len_lo
        ora rb_copy_len_hi
        beq @done
        lda rb_copy_len_hi
        beq @last
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jsr rb_zmodload_read_reu_chunk
        bcs @bad
        jsr rb_reu_stash
        inc rb_reu_off_hi
        dec rb_copy_len_hi
        jmp @more
@last:
        lda rb_copy_len_lo
        sta rb_reu_len_lo
        lda #0
        sta rb_reu_len_hi
        jsr rb_zmodload_read_reu_chunk
        bcs @bad
        jsr rb_reu_stash
        lda #0
        sta rb_copy_len_lo
@done:
        clc
        rts
@bad:
        sec
        rts

rb_zmodload_read_reu_chunk:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_target_off
@loop:
        jsr rb_zmodload_read_byte
        bcs @bad
        ldy rb_target_off
        sta RB_PAGEBUF,y
        inc rb_target_off
        lda rb_target_off
        cmp rb_reu_len_lo
        bne @loop
        clc
        rts
@bad:
        sec
        rts

rb_zmodload_read_byte:
        lda rb_zmod_eof
        bne @bad
        jsr K_CHRIN
        sta rb_lookup_char
        jsr K_READST
        beq @ok
        cmp #$40
        beq @eof
        sec
        lda rb_lookup_char
        rts
@eof:
        lda #1
        sta rb_zmod_eof
@ok:
        lda rb_lookup_char
        clc
        rts
@bad:
        sec
        rts

rb_zmodload_close:
        lda rb_saved_msgflg
        pha
        jsr K_CLRCHN
        lda #1
        jsr K_CLOSE
        pla
        sta MSGFLG
        rts
cmd_zmodload_end:

        .segment "SLOTPACK2"

cmd_zslot2:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #32
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_zslot2_end:

cmd_plot:
        jsr gfx_plot_current
        jmp gfxprim_ok_none

cmd_point:
        jsr gfx_point_current
        rts

cmd_line:
        lda CF_NUM2_LO
        sta RF_ARRAY_BUF
        lda CF_NUM2_HI
        sta RF_ARRAY_BUF+1
        lda CF_NUM3_LO
        sta RF_ARRAY_BUF+2
        lda CF_NUM3_HI
        sta RF_ARRAY_BUF+3
        lda CF_NUM4_LO
        sta RF_ARRAY_BUF+4
        lda CF_NUM4_HI
        sta RF_ARRAY_BUF+5
@loop:
        lda RF_ARRAY_BUF+4
        sta CF_NUM2_LO
        lda RF_ARRAY_BUF+5
        sta CF_NUM2_HI
        jsr gfx_plot_current
        lda CF_NUM0_LO
        cmp RF_ARRAY_BUF
        bne @step
        lda CF_NUM0_HI
        cmp RF_ARRAY_BUF+1
        bne @step
        lda CF_NUM1_LO
        cmp RF_ARRAY_BUF+2
        beq @done
@step:
        jsr gfx_step_x_toward_saved
        jsr gfx_step_y_toward_saved
        jmp @loop
@done:
        jmp gfxprim_ok_none

cmd_rect:
        jsr gfx_save_rect_args
        jsr gfx_rect_top
        jsr gfx_rect_bottom
        jsr gfx_rect_left
        jsr gfx_rect_right
        jmp gfxprim_ok_none

cmd_frect:
        jsr gfx_save_rect_args
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
@row:
        jsr gfx_rect_row
        lda CF_NUM1_LO
        cmp RF_RECT_BUF+6
        beq @done
        inc CF_NUM1_LO
        jmp @row
@done:
        jmp gfxprim_ok_none

cmd_circle:
        jsr gfx_save_circle_args
        lda #0
        sta RF_RECT_BUF+10
        lda RF_RECT_BUF+4
        sta RF_RECT_BUF+11
        lda #3
        sta RF_RECT_BUF+12
        lda #0
        sta RF_RECT_BUF+13
        lda RF_RECT_BUF+4
        asl
        sta rb_digit_seen
        lda #0
        rol
        sta rb_digit_count
        lda RF_RECT_BUF+12
        sec
        sbc rb_digit_seen
        sta RF_RECT_BUF+12
        lda RF_RECT_BUF+13
        sbc rb_digit_count
        sta RF_RECT_BUF+13
@loop:
        lda RF_RECT_BUF+11
        cmp RF_RECT_BUF+10
        bcc @done
        jsr gfx_circle_plot8
        lda RF_RECT_BUF+13
        bmi @dneg
        lda RF_RECT_BUF+10
        asl
        asl
        clc
        adc #10
        sta RF_RECT_BUF+14
        lda #0
        adc #0
        sta RF_RECT_BUF+15
        lda RF_RECT_BUF+11
        asl
        asl
        sta rb_digit_seen
        lda #0
        rol
        sta rb_digit_count
        lda RF_RECT_BUF+14
        sec
        sbc rb_digit_seen
        sta RF_RECT_BUF+14
        lda RF_RECT_BUF+15
        sbc rb_digit_count
        sta RF_RECT_BUF+15
        jsr gfx_circle_add_delta
        dec RF_RECT_BUF+11
        jmp @incx
@dneg:
        lda RF_RECT_BUF+10
        asl
        asl
        clc
        adc #6
        sta RF_RECT_BUF+14
        lda #0
        adc #0
        sta RF_RECT_BUF+15
        jsr gfx_circle_add_delta
@incx:
        inc RF_RECT_BUF+10
        jmp @loop
@done:
        jmp gfxprim_ok_none

cmd_fcircle:
        jsr gfx_save_circle_args
        jsr gfx_circle_bbox_to_rect_args
        jmp cmd_frect

gfx_circle_add_delta:
        lda RF_RECT_BUF+12
        clc
        adc RF_RECT_BUF+14
        sta RF_RECT_BUF+12
        lda RF_RECT_BUF+13
        adc RF_RECT_BUF+15
        sta RF_RECT_BUF+13
        rts

gfx_circle_plot_point:
        lda #0
        sta CF_NUM0_HI
        sta CF_NUM1_HI
        sta CF_NUM2_HI
        lda RF_RECT_BUF+8
        sta CF_NUM2_LO
        jmp gfx_plot_current

gfx_circle_plot8:
        lda RF_RECT_BUF
        clc
        adc RF_RECT_BUF+10
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        clc
        adc RF_RECT_BUF+11
        sta CF_NUM1_LO
        jsr gfx_circle_plot_point
        lda RF_RECT_BUF
        sec
        sbc RF_RECT_BUF+10
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        clc
        adc RF_RECT_BUF+11
        sta CF_NUM1_LO
        jsr gfx_circle_plot_point
        lda RF_RECT_BUF
        clc
        adc RF_RECT_BUF+10
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        sec
        sbc RF_RECT_BUF+11
        sta CF_NUM1_LO
        jsr gfx_circle_plot_point
        lda RF_RECT_BUF
        sec
        sbc RF_RECT_BUF+10
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        sec
        sbc RF_RECT_BUF+11
        sta CF_NUM1_LO
        jsr gfx_circle_plot_point
        lda RF_RECT_BUF
        clc
        adc RF_RECT_BUF+11
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        clc
        adc RF_RECT_BUF+10
        sta CF_NUM1_LO
        jsr gfx_circle_plot_point
        lda RF_RECT_BUF
        sec
        sbc RF_RECT_BUF+11
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        clc
        adc RF_RECT_BUF+10
        sta CF_NUM1_LO
        jsr gfx_circle_plot_point
        lda RF_RECT_BUF
        clc
        adc RF_RECT_BUF+11
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        sec
        sbc RF_RECT_BUF+10
        sta CF_NUM1_LO
        jsr gfx_circle_plot_point
        lda RF_RECT_BUF
        sec
        sbc RF_RECT_BUF+11
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        sec
        sbc RF_RECT_BUF+10
        sta CF_NUM1_LO
        jmp gfx_circle_plot_point

gfx_circle_bbox_to_rect_args:
        lda RF_RECT_BUF
        sec
        sbc RF_RECT_BUF+4
        sta CF_NUM0_LO
        lda #0
        sta CF_NUM0_HI
        lda RF_RECT_BUF+2
        sec
        sbc RF_RECT_BUF+4
        sta CF_NUM1_LO
        lda RF_RECT_BUF
        clc
        adc RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda #0
        sta CF_NUM2_HI
        lda RF_RECT_BUF+2
        clc
        adc RF_RECT_BUF+4
        sta CF_NUM3_LO
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        lda #0
        sta CF_NUM1_HI
        sta CF_NUM3_HI
        sta CF_NUM4_HI
        rts

cmd_tile:
        jsr gfx_get_mode_prim
        cmp #RB_GFX_MODE_TEXT
        beq @text
        jsr gfx_calc_tile_addr
        ldy #0
        lda CF_NUM2_LO
        sta (rb_ptr_lo),y
        jsr gfx_screen_ptr_to_color
        lda CF_NUM3_LO
        and #$0F
        sta (rb_ptr_lo),y
        jmp gfxprim_ok_none
@text:
        jsr gfx_calc_tile_addr
        lda rb_ptr_hi
        sec
        sbc #>RB_GFX_SCREEN
        clc
        adc #>SCREEN
        sta rb_ptr_hi
        ldy #0
        lda CF_NUM2_LO
        sta (rb_ptr_lo),y
        jsr gfx_text_ptr_to_color
        lda CF_NUM3_LO
        and #$0F
        sta (rb_ptr_lo),y
        jmp gfxprim_ok_none

gfx_save_circle_args:
        lda CF_NUM0_LO
        sta RF_RECT_BUF
        lda CF_NUM1_LO
        sta RF_RECT_BUF+2
        lda CF_NUM2_LO
        sta RF_RECT_BUF+4
        lda CF_NUM3_LO
        sta RF_RECT_BUF+8
        rts

gfxprim_ok_none:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

gfxprim_ok_int:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        rts

gfx_save_rect_args:
        lda CF_NUM0_LO
        sta RF_RECT_BUF
        lda CF_NUM0_HI
        sta RF_RECT_BUF+1
        lda CF_NUM1_LO
        sta RF_RECT_BUF+2
        lda CF_NUM1_HI
        sta RF_RECT_BUF+3
        lda CF_NUM2_LO
        sta RF_RECT_BUF+4
        lda CF_NUM2_HI
        sta RF_RECT_BUF+5
        lda CF_NUM3_LO
        sta RF_RECT_BUF+6
        lda CF_NUM3_HI
        sta RF_RECT_BUF+7
        lda CF_NUM4_LO
        sta RF_RECT_BUF+8
        lda CF_NUM4_HI
        sta RF_RECT_BUF+9
        rts

gfx_restore_color_to_num4:
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        lda RF_RECT_BUF+9
        sta CF_NUM4_HI
        rts

gfx_rect_top:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+1
        sta CF_NUM0_HI
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda RF_RECT_BUF+5
        sta CF_NUM2_HI
        lda RF_RECT_BUF+2
        sta CF_NUM3_LO
        jsr gfx_restore_color_to_num4
        jmp cmd_line

gfx_rect_bottom:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+1
        sta CF_NUM0_HI
        lda RF_RECT_BUF+6
        sta CF_NUM1_LO
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda RF_RECT_BUF+5
        sta CF_NUM2_HI
        lda RF_RECT_BUF+6
        sta CF_NUM3_LO
        jsr gfx_restore_color_to_num4
        jmp cmd_line

gfx_rect_left:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+1
        sta CF_NUM0_HI
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
        lda RF_RECT_BUF
        sta CF_NUM2_LO
        lda RF_RECT_BUF+1
        sta CF_NUM2_HI
        lda RF_RECT_BUF+6
        sta CF_NUM3_LO
        jsr gfx_restore_color_to_num4
        jmp cmd_line

gfx_rect_right:
        lda RF_RECT_BUF+4
        sta CF_NUM0_LO
        lda RF_RECT_BUF+5
        sta CF_NUM0_HI
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda RF_RECT_BUF+5
        sta CF_NUM2_HI
        lda RF_RECT_BUF+6
        sta CF_NUM3_LO
        jsr gfx_restore_color_to_num4
        jmp cmd_line

gfx_rect_row:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+1
        sta CF_NUM0_HI
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda RF_RECT_BUF+5
        sta CF_NUM2_HI
        lda CF_NUM1_LO
        sta CF_NUM3_LO
        jsr gfx_restore_color_to_num4
        jmp cmd_line

gfx_step_x_toward_saved:
        lda CF_NUM0_HI
        cmp RF_ARRAY_BUF+1
        bcc @inc
        bne @dec
        lda CF_NUM0_LO
        cmp RF_ARRAY_BUF
        bcc @inc
        beq @done
@dec:
        lda CF_NUM0_LO
        bne :+
        dec CF_NUM0_HI
:       dec CF_NUM0_LO
        rts
@inc:
        inc CF_NUM0_LO
        bne @done
        inc CF_NUM0_HI
@done:
        rts

gfx_step_y_toward_saved:
        lda CF_NUM1_LO
        cmp RF_ARRAY_BUF+2
        bcc @inc
        beq @done
        dec CF_NUM1_LO
        rts
@inc:
        inc CF_NUM1_LO
@done:
        rts

gfx_plot_current:
        jsr gfx_get_mode_prim
        cmp #RB_GFX_MODE_TILE
        beq @tile
        cmp #RB_GFX_MODE_MTILE
        beq @tile
        cmp #RB_GFX_MODE_MBITMAP
        beq @mbitmap
        jsr gfx_calc_bitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx CF_NUM2_LO
        beq @clear
        ldx rb_saved_plot_y
        ora gfx_bit_masks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts
@clear:
        ldx rb_saved_plot_y
        and gfx_bit_unmasks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts
@tile:
        jmp gfx_plot_tile
@mbitmap:
        jmp gfx_plot_mbitmap

gfx_point_current:
        jsr gfx_get_mode_prim
        cmp #RB_GFX_MODE_TILE
        beq @tile
        cmp #RB_GFX_MODE_MTILE
        beq @tile
        cmp #RB_GFX_MODE_MBITMAP
        beq @mbitmap
        jsr gfx_calc_bitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx rb_saved_plot_y
        and gfx_bit_masks,x
        beq @zero
        lda #1
        bne @ret
@zero:
        lda #0
@ret:
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        pla
        sta CPU_PORT
        jmp gfxprim_ok_int
@mbitmap:
        jmp gfx_point_mbitmap
@tile:
        jsr gfx_calc_tile_addr
        ldy #0
        lda (rb_ptr_lo),y
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        jmp gfxprim_ok_int

gfx_plot_mbitmap:
        lda CF_NUM2_LO
        beq @clear
        jsr gfx_mbitmap_apply_attr
        jsr gfx_calc_mbitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx rb_saved_plot_y
        and gfx_mbit_unmasks,x
        sta rb_digit_seen
        lda rb_digit_count
        sec
        sbc #1
        asl
        asl
        clc
        adc rb_saved_plot_y
        tax
        lda gfx_mbit_pair_masks,x
        ora rb_digit_seen
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts
@clear:
        jsr gfx_calc_mbitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx rb_saved_plot_y
        and gfx_mbit_unmasks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts

gfx_point_mbitmap:
        jsr gfx_calc_mbitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        sta rb_digit_seen
        pla
        sta CPU_PORT
        lda rb_saved_plot_y
        clc
        adc #8
        tax
        lda rb_digit_seen
        and gfx_mbit_pair_masks,x
        cmp gfx_mbit_pair_masks,x
        beq @three
        lda rb_saved_plot_y
        clc
        adc #4
        tax
        lda rb_digit_seen
        and gfx_mbit_pair_masks,x
        bne @two
        ldx rb_saved_plot_y
        lda rb_digit_seen
        and gfx_mbit_pair_masks,x
        bne @one
        lda #0
        beq @store
@one:
        lda #1
        bne @store
@two:
        lda #2
        bne @store
@three:
        lda #3
@store:
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        jmp gfxprim_ok_int

gfx_plot_tile:
        jsr gfx_calc_tile_addr
        ldy #0
        lda CF_NUM2_LO
        and #$3F
        ora #$40
        sta (rb_ptr_lo),y
        jsr gfx_screen_ptr_to_color
        lda CF_NUM2_LO
        and #$0F
        sta (rb_ptr_lo),y
        rts

gfx_screen_ptr_to_color:
        lda rb_ptr_hi
        sec
        sbc #>RB_GFX_SCREEN
        clc
        adc #>RB_GFX_COLOR
        sta rb_ptr_hi
        rts

gfx_text_ptr_to_color:
        lda rb_ptr_hi
        sec
        sbc #>SCREEN
        clc
        adc #>COLOR_RAM
        sta rb_ptr_hi
        rts

gfx_calc_tile_addr:
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda CF_NUM1_LO
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        asl
        asl
        asl
        asl
        asl
        clc
        adc rb_saved_plot_x
        clc
        adc CF_NUM0_LO
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        adc #0
        sta rb_ptr_hi
        rts

gfx_get_mode_prim:
        lda RB_GFX_MODE_STATE
        cmp #RB_GFX_MODE_MTILE + 1
        bcc :+
        lda #RB_GFX_MODE_TEXT
:       rts

gfx_calc_bitmap_addr:
        lda CF_NUM1_LO
        and #7
        sta rb_saved_plot_x
        lda CF_NUM0_LO
        and #7
        sta rb_saved_plot_y
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        sta rb_digit_seen
        lda rb_digit_seen
        and #3
        asl
        asl
        asl
        asl
        asl
        asl
        sta rb_ptr_lo
        lda #>RB_GFX_BITMAP
        clc
        adc rb_digit_seen
        sta rb_ptr_hi
        lda rb_digit_seen
        lsr
        lsr
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda CF_NUM0_LO
        and #$F8
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       lda CF_NUM0_HI
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda rb_saved_plot_x
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       rts

gfx_calc_mbitmap_addr:
        lda CF_NUM1_LO
        and #7
        sta rb_saved_plot_x
        lda CF_NUM0_LO
        and #3
        sta rb_saved_plot_y
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        sta rb_digit_seen
        lda rb_digit_seen
        and #3
        asl
        asl
        asl
        asl
        asl
        asl
        sta rb_ptr_lo
        lda #>RB_GFX_BITMAP
        clc
        adc rb_digit_seen
        sta rb_ptr_hi
        lda rb_digit_seen
        lsr
        lsr
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda CF_NUM0_LO
        and #$FC
        asl
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       lda rb_saved_plot_x
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       rts

gfx_calc_mbitmap_cell_addr:
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        sta rb_digit_seen
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda rb_digit_seen
        asl
        asl
        asl
        asl
        asl
        clc
        adc rb_saved_plot_x
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        adc #0
        sta rb_ptr_hi
        lda CF_NUM0_LO
        lsr
        lsr
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       rts

gfx_mbitmap_apply_attr:
        lda CF_NUM2_LO
        cmp #16
        bcc @slot3
        cmp #32
        bcc @slot1
        cmp #48
        bcc @slot2
@slot3:
        lda #3
        sta rb_digit_count
        jsr gfx_calc_mbitmap_cell_addr
        jsr gfx_screen_ptr_to_color
        ldy #0
        lda CF_NUM2_LO
        and #$0F
        sta (rb_ptr_lo),y
        rts
@slot1:
        lda #1
        sta rb_digit_count
        jsr gfx_calc_mbitmap_cell_addr
        ldy #0
        lda (rb_ptr_lo),y
        and #$0F
        sta rb_digit_seen
        lda CF_NUM2_LO
        and #$0F
        asl
        asl
        asl
        asl
        ora rb_digit_seen
        sta (rb_ptr_lo),y
        rts
@slot2:
        lda #2
        sta rb_digit_count
        jsr gfx_calc_mbitmap_cell_addr
        ldy #0
        lda (rb_ptr_lo),y
        and #$F0
        sta rb_digit_seen
        lda CF_NUM2_LO
        and #$0F
        ora rb_digit_seen
        sta (rb_ptr_lo),y
        rts

gfx_bit_masks:
        .byte $80,$40,$20,$10,$08,$04,$02,$01
gfx_bit_unmasks:
        .byte $7F,$BF,$DF,$EF,$F7,$FB,$FD,$FE
gfx_mbit_pair_masks:
        .byte $40,$10,$04,$01,$80,$20,$08,$02,$C0,$30,$0C,$03
gfx_mbit_unmasks:
        .byte $3F,$CF,$F3,$FC

        .segment "SPANPACK"

cmd_zspan:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #40
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_zspan_end:

        .segment "OVL1PACK"

cmd_zovl1:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #51
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_zovl1_end:

cmd_sprset:
        lda CF_NUM0_LO
        and #7
        sta CF_NUM0_LO
        jsr gfx_sprite_pattern
        ldx CF_NUM0_LO
        lda gfx_sprite_bits,x
        ldy CF_NUM1_LO
        beq @off
        ora VIC_SPR_ENABLE
        sta VIC_SPR_ENABLE
        jmp @color
@off:
        eor #$FF
        and VIC_SPR_ENABLE
        sta VIC_SPR_ENABLE
@color:
        lda CF_NUM2_LO
        and #$0F
        ldx CF_NUM0_LO
        sta VIC_SPR_COLOR0,x
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

cmd_sprmove:
        lda CF_NUM0_LO
        and #7
        sta CF_NUM0_LO
        ldx CF_NUM0_LO
        txa
        asl
        tax
        lda CF_NUM1_LO
        sta $D000,x
        lda CF_NUM2_LO
        sta $D001,x
        ldx CF_NUM0_LO
        lda gfx_sprite_bits,x
        ldy CF_NUM1_HI
        beq @clear_msb
        ora VIC_SPR_X_MSB
        sta VIC_SPR_X_MSB
        jmp @done
@clear_msb:
        eor #$FF
        and VIC_SPR_X_MSB
        sta VIC_SPR_X_MSB
@done:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

cmd_sprcolor:
        lda CF_NUM0_LO
        and #7
        sta CF_NUM0_LO
        ldx CF_NUM0_LO
        lda CF_NUM1_LO
        and #$0F
        sta VIC_SPR_COLOR0,x
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

cmd_sprexpand:
        lda CF_NUM0_LO
        and #7
        tax
        lda gfx_sprite_bits,x
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        beq @xoff
        lda rb_saved_plot_x
        ora VIC_SPR_EXP_X
        sta VIC_SPR_EXP_X
        jmp @y
@xoff:
        lda rb_saved_plot_x
        eor #$FF
        and VIC_SPR_EXP_X
        sta VIC_SPR_EXP_X
@y:
        lda CF_NUM2_LO
        beq @yoff
        lda rb_saved_plot_x
        ora VIC_SPR_EXP_Y
        sta VIC_SPR_EXP_Y
        jmp gfx_ok_none_ovl1
@yoff:
        lda rb_saved_plot_x
        eor #$FF
        and VIC_SPR_EXP_Y
        sta VIC_SPR_EXP_Y
        jmp gfx_ok_none_ovl1

cmd_sprpri:
        lda CF_NUM0_LO
        and #7
        tax
        lda gfx_sprite_bits,x
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        beq @front
        lda rb_saved_plot_x
        ora VIC_SPR_PRIORITY
        sta VIC_SPR_PRIORITY
        jmp gfx_ok_none_ovl1
@front:
        lda rb_saved_plot_x
        eor #$FF
        and VIC_SPR_PRIORITY
        sta VIC_SPR_PRIORITY
        jmp gfx_ok_none_ovl1

cmd_sprmulti:
        lda CF_NUM0_LO
        and #7
        tax
        lda gfx_sprite_bits,x
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        beq @single
        lda rb_saved_plot_x
        ora VIC_SPR_MCOLOR
        sta VIC_SPR_MCOLOR
        jmp gfx_ok_none_ovl1
@single:
        lda rb_saved_plot_x
        eor #$FF
        and VIC_SPR_MCOLOR
        sta VIC_SPR_MCOLOR
        jmp gfx_ok_none_ovl1

cmd_sprmcolor:
        lda CF_NUM0_LO
        and #$0F
        sta VIC_SPR_MCOLOR0
        lda CF_NUM1_LO
        and #$0F
        sta VIC_SPR_MCOLOR1
        jmp gfx_ok_none_ovl1

cmd_sprrow:
        lda CF_NUM1_LO
        cmp #21
        bcc :+
        jmp gfx_ok_none_ovl1
:       lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        lda CF_NUM0_LO
        and #7
        sta CF_NUM0_LO
        ldx CF_NUM0_LO
        lda #<RB_GFX_SPRITES
        sta rb_ptr_lo
        lda #>RB_GFX_SPRITES
        sta rb_ptr_hi
        cpx #0
        beq @row_offset
@sprite_advance:
        clc
        lda rb_ptr_lo
        adc #64
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       dex
        bne @sprite_advance
@row_offset:
        lda CF_NUM1_LO
        asl
        clc
        adc CF_NUM1_LO
        tay
        lda CF_NUM2_LO
        sta (rb_ptr_lo),y
        iny
        lda CF_NUM3_LO
        sta (rb_ptr_lo),y
        iny
        lda CF_NUM4_LO
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        lda CF_NUM0_LO
        clc
        adc #<(RB_GFX_SPRITES / 64)
        ldx CF_NUM0_LO
        sta RB_GFX_SPR_PTRS,x
gfx_ok_none_ovl1:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

cmd_sprscan:
        lda VIC_SPR_COLL
        lda VIC_BG_COLL
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

cmd_sprcoll:
        lda CF_NUM0_LO
        and #7
        sta CF_NUM0_LO
        ldx CF_NUM0_LO
        lda gfx_sprite_bits,x
        and VIC_SPR_COLL
        beq @zero
        lda #1
        bne @ret
@zero:
        lda #0
@ret:
        sta RF_VAL_LO
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        rts

gfx_sprite_pattern:
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldx CF_NUM0_LO
        lda #<RB_GFX_SPRITES
        sta rb_ptr_lo
        lda #>RB_GFX_SPRITES
        sta rb_ptr_hi
        cpx #0
        beq @fill
@advance:
        clc
        lda rb_ptr_lo
        adc #64
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       dex
        bne @advance
@fill:
        lda CF_NUM3_LO
        asl
        tax
        lda gfx_sprite_patterns,x
        sta rb_saved_plot_x
        lda gfx_sprite_patterns+1,x
        sta rb_saved_plot_y
        ldy #0
@loop:
        lda rb_saved_plot_x
        sta (rb_ptr_lo),y
        iny
        lda rb_saved_plot_y
        sta (rb_ptr_lo),y
        iny
        lda #0
        sta (rb_ptr_lo),y
        iny
        cpy #63
        bcc @loop
        pla
        sta CPU_PORT
        lda CF_NUM0_LO
        clc
        adc #<(RB_GFX_SPRITES / 64)
        ldx CF_NUM0_LO
        sta RB_GFX_SPR_PTRS,x
        rts

gfx_sprite_bits:
        .byte $01,$02,$04,$08,$10,$20,$40,$80
gfx_sprite_patterns:
        .byte $FF,$FF
        .byte $81,$81
        .byte $18,$18
        .byte $3C,$3C

        .segment "OVL2PACK"

cmd_zovl2:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda #52
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts
cmd_zovl2_end:

cmd_joy:
        lda CF_NUM0_LO
        cmp #2
        beq @port2
        lda CIA1_PRA
        jmp @finish
@port2:
        lda CIA1_PRB
@finish:
        eor #$FF
        and #$1F
        sta RF_VAL_LO
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        rts

cmd_keyp:
        lda KEYD_COUNT
        beq @none
        lda KEYD_BUFFER
        bne @store
@none:
        lda #0
@store:
        sta RF_VAL_LO
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        rts

cmd_keyscan:
        jmp cmd_keyp

cmd_keylast:
        lda KEYD_BUFFER
        sta RF_VAL_LO
        lda #0
        sta RF_STATUS
        sta RF_VAL_HI
        lda #RB_VAL_INT
        sta RF_TAG
        rts

        .segment "OVL4PACK"

cmd_dlnew:
        lda CF_NUM0_HI
        bne @bad
        lda CF_NUM0_LO
        beq @bad
        cmp #RB_DL_MAX_RECORDS + 1
        bcs @bad
        lda #1
        sta rb_needed_pages
        lda #RB_HANDLE_TYPE_DLIST
        sta rb_handle_new_type
        jsr dl_handle_alloc_with_pages
        lda RF_STATUS
        bne @done
        jsr dl_handle_load_result
        jsr dl_clear_loaded_page
@done:
        rts
@bad:
        lda #$61
        jmp dl_fail

cmd_dlclr:
        jsr dl_load_dlist_handle
        bcs @bad
        jsr dl_clear_loaded_page
        jmp dl_ok
@bad:
        lda #$24
        jmp dl_fail

cmd_dlplot:
        jsr dl_load_dlist_handle
        bcs @bad
        lda #1
        jsr dl_append_begin
        bcs @full
        lda CF_NUM1_LO
        sta RB_PAGEBUF+1,y
        lda CF_NUM2_LO
        sta RB_PAGEBUF+2,y
        lda CF_NUM1_LO
        sta RB_PAGEBUF+3,y
        lda CF_NUM2_LO
        sta RB_PAGEBUF+4,y
        lda CF_NUM3_LO
        sta RB_PAGEBUF+5,y
        jsr dl_stash_handle_page
        jmp dl_ok
@bad:
        lda #$24
        jmp dl_fail
@full:
        lda #$62
        jmp dl_fail

cmd_dlline:
        lda #2
        bne dl_append_six
cmd_dlrect:
        lda #3
        bne dl_append_six
cmd_dlfrect:
        lda #4
dl_append_six:
        pha
        jsr dl_load_dlist_handle
        bcs @bad
        pla
        jsr dl_append_begin
        bcs @full
        lda CF_NUM1_LO
        sta RB_PAGEBUF+1,y
        lda CF_NUM2_LO
        sta RB_PAGEBUF+2,y
        lda CF_NUM3_LO
        sta RB_PAGEBUF+3,y
        lda CF_NUM4_LO
        sta RB_PAGEBUF+4,y
        lda #1
        sta RB_PAGEBUF+5,y
        jsr dl_stash_handle_page
        jmp dl_ok
@bad:
        pla
        lda #$24
        jmp dl_fail
@full:
        lda #$62
        jmp dl_fail

cmd_dldraw:
        jsr dl_load_dlist_handle
        bcs @bad
        jsr dl_fetch_handle_page
        lda RB_PAGEBUF
        sta rb_copy_count
        lda #0
        sta rb_copy_chunks
@loop:
        lda rb_copy_chunks
        cmp rb_copy_count
        bcs @done
        jsr dl_record_y
        lda RB_PAGEBUF,y
        cmp #1
        beq @plot
        cmp #2
        beq @line
        cmp #3
        beq @rect
        cmp #4
        beq @frect
@next:
        inc rb_copy_chunks
        jmp @loop
@plot:
        jsr dl_load_record_nums
        lda CF_NUM4_LO
        sta CF_NUM2_LO
        lda #0
        sta CF_NUM2_HI
        jsr dl_plot_current
        jmp @next
@line:
        jsr dl_load_record_nums
        jsr dl_line
        jmp @next
@rect:
        jsr dl_load_record_nums
        jsr dl_rect
        jmp @next
@frect:
        jsr dl_load_record_nums
        jsr dl_frect
        jmp @next
@done:
        jmp dl_ok
@bad:
        lda #$24
        jmp dl_fail

dl_handle_load_result:
        lda RF_VAL_LO
        sta CF_NUM0_LO
        lda RF_VAL_HI
        sta CF_NUM0_HI
        jmp dl_load_dlist_handle

dl_load_dlist_handle:
        jsr dl_handle_load_arg
        bcs @bad
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_DLIST
        bne @bad
        clc
        rts
@bad:
        sec
        rts

dl_clear_loaded_page:
        lda #0
        tay
@loop:
        sta RB_PAGEBUF,y
        iny
        bne @loop
        jmp dl_stash_handle_page

dl_append_begin:
        pha
        jsr dl_fetch_handle_page
        lda RB_PAGEBUF
        cmp #RB_DL_MAX_RECORDS
        bcs @full
        sta rb_copy_chunks
        inc RB_PAGEBUF
        jsr dl_record_y
        pla
        sta RB_PAGEBUF,y
        clc
        rts
@full:
        pla
        sec
        rts

dl_record_y:
        lda rb_copy_chunks
        asl
        asl
        asl
        clc
        adc #1
        tay
        rts

dl_load_record_nums:
        iny
        lda RB_PAGEBUF,y
        sta CF_NUM0_LO
        lda #0
        sta CF_NUM0_HI
        iny
        lda RB_PAGEBUF,y
        sta CF_NUM1_LO
        lda #0
        sta CF_NUM1_HI
        iny
        lda RB_PAGEBUF,y
        sta CF_NUM2_LO
        lda #0
        sta CF_NUM2_HI
        iny
        lda RB_PAGEBUF,y
        sta CF_NUM3_LO
        lda #0
        sta CF_NUM3_HI
        iny
        lda RB_PAGEBUF,y
        sta CF_NUM4_LO
        lda #0
        sta CF_NUM4_HI
        rts

dl_line:
        lda CF_NUM2_LO
        sta RF_ARRAY_BUF
        lda CF_NUM3_LO
        sta RF_ARRAY_BUF+2
        lda CF_NUM4_LO
        sta RF_ARRAY_BUF+4
@loop:
        lda RF_ARRAY_BUF+4
        sta CF_NUM2_LO
        jsr dl_plot_current
        lda CF_NUM0_LO
        cmp RF_ARRAY_BUF
        bne @step
        lda CF_NUM1_LO
        cmp RF_ARRAY_BUF+2
        beq @done
@step:
        jsr dl_step_x
        jsr dl_step_y
        jmp @loop
@done:
        rts

dl_rect:
        jsr dl_save_rect
        jsr dl_rect_top
        jsr dl_rect_bottom
        jsr dl_rect_left
        jmp dl_rect_right

dl_frect:
        jsr dl_save_rect
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
@row:
        jsr dl_rect_row
        lda CF_NUM1_LO
        cmp RF_RECT_BUF+6
        beq @done
        inc CF_NUM1_LO
        jmp @row
@done:
        rts

dl_save_rect:
        lda CF_NUM0_LO
        sta RF_RECT_BUF
        lda CF_NUM1_LO
        sta RF_RECT_BUF+2
        lda CF_NUM2_LO
        sta RF_RECT_BUF+4
        lda CF_NUM3_LO
        sta RF_RECT_BUF+6
        lda CF_NUM4_LO
        sta RF_RECT_BUF+8
        rts

dl_rect_top:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda RF_RECT_BUF+2
        sta CF_NUM3_LO
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        jmp dl_line
dl_rect_bottom:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+6
        sta CF_NUM1_LO
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda RF_RECT_BUF+6
        sta CF_NUM3_LO
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        jmp dl_line
dl_rect_left:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
        lda RF_RECT_BUF
        sta CF_NUM2_LO
        lda RF_RECT_BUF+6
        sta CF_NUM3_LO
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        jmp dl_line
dl_rect_right:
        lda RF_RECT_BUF+4
        sta CF_NUM0_LO
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda RF_RECT_BUF+6
        sta CF_NUM3_LO
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        jmp dl_line
dl_rect_row:
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+4
        sta CF_NUM2_LO
        lda CF_NUM1_LO
        sta CF_NUM3_LO
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        jmp dl_line

dl_step_x:
        lda CF_NUM0_LO
        cmp RF_ARRAY_BUF
        bcc @inc
        beq @done
        dec CF_NUM0_LO
        rts
@inc:
        inc CF_NUM0_LO
@done:
        rts

dl_step_y:
        lda CF_NUM1_LO
        cmp RF_ARRAY_BUF+2
        bcc @inc
        beq @done
        dec CF_NUM1_LO
        rts
@inc:
        inc CF_NUM1_LO
@done:
        rts

dl_plot_current:
        jsr dl_get_mode
        cmp #RB_GFX_MODE_TILE
        beq dl_plot_tile
        cmp #RB_GFX_MODE_MTILE
        beq dl_plot_tile
        cmp #RB_GFX_MODE_MBITMAP
        beq dl_plot_mbitmap
        jsr dl_calc_bitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx CF_NUM2_LO
        beq @clear
        ldx rb_saved_plot_y
        ora dl_bit_masks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts
@clear:
        ldx rb_saved_plot_y
        and dl_bit_unmasks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts

dl_plot_tile:
        jsr dl_calc_tile_addr
        ldy #0
        lda CF_NUM2_LO
        and #$3F
        ora #$40
        sta (rb_ptr_lo),y
        lda rb_ptr_hi
        sec
        sbc #>RB_GFX_SCREEN
        clc
        adc #>RB_GFX_COLOR
        sta rb_ptr_hi
        lda CF_NUM2_LO
        and #$0F
        sta (rb_ptr_lo),y
        rts

dl_plot_mbitmap:
        lda CF_NUM2_LO
        beq @clear
        jsr dl_mbitmap_apply_attr
        jsr dl_calc_mbitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx rb_saved_plot_y
        and dl_mbit_unmasks,x
        sta rb_digit_seen
        lda rb_digit_count
        sec
        sbc #1
        asl
        asl
        clc
        adc rb_saved_plot_y
        tax
        lda dl_mbit_pair_masks,x
        ora rb_digit_seen
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts
@clear:
        jsr dl_calc_mbitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx rb_saved_plot_y
        and dl_mbit_unmasks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts

dl_get_mode:
        lda RB_GFX_MODE_STATE
        cmp #RB_GFX_MODE_MTILE + 1
        bcc :+
        lda #RB_GFX_MODE_TEXT
:       rts

dl_calc_tile_addr:
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda CF_NUM1_LO
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        asl
        asl
        asl
        asl
        asl
        clc
        adc rb_saved_plot_x
        clc
        adc CF_NUM0_LO
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        adc #0
        sta rb_ptr_hi
        rts

dl_calc_bitmap_addr:
        lda CF_NUM1_LO
        and #7
        sta rb_saved_plot_x
        lda CF_NUM0_LO
        and #7
        sta rb_saved_plot_y
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        sta rb_digit_seen
        lda rb_digit_seen
        and #3
        asl
        asl
        asl
        asl
        asl
        asl
        sta rb_ptr_lo
        lda #>RB_GFX_BITMAP
        clc
        adc rb_digit_seen
        sta rb_ptr_hi
        lda rb_digit_seen
        lsr
        lsr
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda CF_NUM0_LO
        and #$F8
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       lda CF_NUM0_HI
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda rb_saved_plot_x
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       rts

dl_calc_mbitmap_addr:
        lda CF_NUM1_LO
        and #7
        sta rb_saved_plot_x
        lda CF_NUM0_LO
        and #3
        sta rb_saved_plot_y
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        sta rb_digit_seen
        lda rb_digit_seen
        and #3
        asl
        asl
        asl
        asl
        asl
        asl
        sta rb_ptr_lo
        lda #>RB_GFX_BITMAP
        clc
        adc rb_digit_seen
        sta rb_ptr_hi
        lda rb_digit_seen
        lsr
        lsr
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda CF_NUM0_LO
        and #$FC
        asl
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       lda rb_saved_plot_x
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       rts

dl_calc_mbitmap_cell_addr:
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        sta rb_digit_seen
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda rb_digit_seen
        asl
        asl
        asl
        asl
        asl
        clc
        adc rb_saved_plot_x
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        adc #0
        sta rb_ptr_hi
        lda CF_NUM0_LO
        lsr
        lsr
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       rts

dl_mbitmap_apply_attr:
        lda CF_NUM2_LO
        cmp #16
        bcc @slot3
        cmp #32
        bcc @slot1
        cmp #48
        bcc @slot2
@slot3:
        lda #3
        sta rb_digit_count
        jsr dl_calc_mbitmap_cell_addr
        lda rb_ptr_hi
        sec
        sbc #>RB_GFX_SCREEN
        clc
        adc #>RB_GFX_COLOR
        sta rb_ptr_hi
        ldy #0
        lda CF_NUM2_LO
        and #$0F
        sta (rb_ptr_lo),y
        rts
@slot1:
        lda #1
        sta rb_digit_count
        jsr dl_calc_mbitmap_cell_addr
        ldy #0
        lda (rb_ptr_lo),y
        and #$0F
        sta rb_digit_seen
        lda CF_NUM2_LO
        and #$0F
        asl
        asl
        asl
        asl
        ora rb_digit_seen
        sta (rb_ptr_lo),y
        rts
@slot2:
        lda #2
        sta rb_digit_count
        jsr dl_calc_mbitmap_cell_addr
        ldy #0
        lda (rb_ptr_lo),y
        and #$F0
        sta rb_digit_seen
        lda CF_NUM2_LO
        and #$0F
        ora rb_digit_seen
        sta (rb_ptr_lo),y
        rts

dl_fetch_handle_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_fetch

dl_stash_handle_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_stash

dl_handle_load_arg:
        lda CF_NUM0_HI
        bne @bad
        lda CF_NUM0_LO
        beq @bad
        cmp #RB_HANDLE_COUNT + 1
        bcs @bad
        sec
        sbc #1
        sta rb_handle_index
        jsr dl_handle_fetch
        lda rb_handle_bank
        beq @bad
        clc
        rts
@bad:
        sec
        rts

dl_handle_fetch:
        jsr dl_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda RB_PAGEBUF,y
        sta rb_handle_bank
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_page
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_pages
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_type
        rts

dl_handle_store:
        jsr dl_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda rb_handle_bank
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_page
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_pages
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_type
        sta RB_PAGEBUF,y
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_stash

dl_handle_desc_fetch_page:
        lda rb_handle_index
        and #$3F
        asl
        asl
        sta rb_handle_desc_off
        lda #0
        sta rb_reu_off_lo
        lda #>RB_REU_HANDLE_OFF
        ldx rb_handle_index
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc :+
        clc
        adc #1
:       sta rb_reu_off_hi
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_fetch

dl_handle_alloc_with_pages:
        jsr dl_find_free_handle
        bcc @got
        lda #$21
        jmp dl_fail
@got:
        jsr dl_find_pages
        bcc :+
        lda #$22
        jmp dl_fail
:       lda rb_reu_core_bank
        sta rb_handle_bank
        lda rb_found_page
        clc
        adc #RB_HEAP_PAGE_BASE
        sta rb_handle_page
        lda rb_needed_pages
        sta rb_handle_pages
        lda rb_handle_new_type
        sta rb_handle_type
        jsr dl_mark_pages_used
        jsr dl_store_heap_bitmap
        jsr dl_handle_store
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda rb_handle_index
        clc
        adc #1
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts

dl_find_free_handle:
        lda #0
        sta rb_handle_scan_base
@page:
        lda rb_handle_scan_base
        sta rb_handle_index
        jsr dl_handle_desc_fetch_page
        ldy #0
        ldx #0
@slot:
        lda RB_PAGEBUF,y
        beq @found
        tya
        clc
        adc #RB_HANDLE_DESC_SIZE
        tay
        inx
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc @slot
        lda rb_handle_scan_base
        bne @full
        lda #RB_HANDLE_PAGE_SLOTS
        sta rb_handle_scan_base
        jmp @page
@found:
        txa
        clc
        adc rb_handle_scan_base
        sta rb_handle_index
        clc
        rts
@full:
        sec
        rts

dl_fetch_heap_bitmap:
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
        jmp rb_reu_fetch

dl_store_heap_bitmap:
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
        jmp rb_reu_stash

dl_find_pages:
        jsr dl_fetch_heap_bitmap
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
        bne @next
        inx
        cpx rb_needed_pages
        bcc @inner
        clc
        rts
@next:
        inc rb_found_page
        jmp @outer

dl_mark_pages_used:
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

dl_ok:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

dl_fail:
        sta RF_STATUS
        sta RF_ERROR
        rts

dl_bit_masks:
        .byte $80,$40,$20,$10,$08,$04,$02,$01
dl_bit_unmasks:
        .byte $7F,$BF,$DF,$EF,$F7,$FB,$FD,$FE
dl_mbit_pair_masks:
        .byte $40,$10,$04,$01,$80,$20,$08,$02,$C0,$30,$0C,$03
dl_mbit_unmasks:
        .byte $3F,$CF,$F3,$FC

        .segment "OVL5PACK"

cmd_chrnew:
        lda #8
        sta rb_needed_pages
        lda #RB_HANDLE_TYPE_CHARSET
        jmp tile_alloc_resource

cmd_chrrow:
        jsr tile_load_handle_arg
        bcc :+
        jmp tile_bad_handle
:
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_CHARSET
        beq :+
        jmp tile_wrong_type
:
        lda CF_NUM2_LO
        cmp #8
        bcc :+
        jmp tile_bad_range
:
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        lsr
        lsr
        clc
        adc rb_handle_page
        sta rb_fill_page
        jsr tile_fetch_fill_page
        lda CF_NUM1_LO
        and #$1F
        asl
        asl
        asl
        clc
        adc CF_NUM2_LO
        tay
        lda CF_NUM3_LO
        sta RB_PAGEBUF,y
        jsr tile_stash_fill_page
        jmp tile_ok

cmd_chruse:
        jsr tile_load_handle_arg
        bcc :+
        jmp tile_bad_handle
:
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_CHARSET
        beq :+
        jmp tile_wrong_type
:
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        lda #<RB_GFX_BITMAP
        sta rb_reu_c64_lo
        lda #>RB_GFX_BITMAP
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #8
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        pla
        sta CPU_PORT
        jmp tile_ok

cmd_tsnew:
        lda #1
        sta rb_needed_pages
        lda #RB_HANDLE_TYPE_TILESET
        jmp tile_alloc_resource

cmd_tsset:
        jsr tile_load_handle_arg
        bcc :+
        jmp tile_bad_handle
:
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_TILESET
        beq :+
        jmp tile_wrong_type
:
        jsr tile_fetch_handle_page
        lda CF_NUM1_LO
        asl
        tay
        lda CF_NUM2_LO
        sta RB_PAGEBUF,y
        iny
        lda CF_NUM3_LO
        and #$0F
        sta RB_PAGEBUF,y
        jsr tile_stash_handle_page
        jmp tile_ok

cmd_tmnew:
        lda #4
        sta rb_needed_pages
        lda #RB_HANDLE_TYPE_TILEMAP
        jmp tile_alloc_resource

cmd_tmset:
        jsr tile_load_handle_arg
        bcc :+
        jmp tile_bad_handle
:
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_TILEMAP
        beq :+
        jmp tile_wrong_type
:
        lda CF_NUM1_HI
        cmp #4
        bcc :+
        jmp tile_bad_range
:
        clc
        adc rb_handle_page
        sta rb_fill_page
        jsr tile_fetch_fill_page
        ldy CF_NUM1_LO
        lda CF_NUM2_LO
        sta RB_PAGEBUF,y
        jsr tile_stash_fill_page
        jmp tile_ok

cmd_tmdraw:
        jsr tile_load_handle_arg
        bcc :+
        jmp tile_bad_handle
:
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_TILEMAP
        beq :+
        jmp tile_wrong_type
:
        lda rb_handle_bank
        sta rb_saved_plot_x
        lda rb_handle_page
        sta rb_saved_plot_y
        lda CF_NUM1_LO
        sta CF_NUM0_LO
        lda CF_NUM1_HI
        sta CF_NUM0_HI
        jsr tile_load_handle_arg
        bcc :+
        jmp tile_bad_handle
:
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_TILESET
        beq :+
        jmp tile_wrong_type
:
        lda #<RF_ARRAY_BUF
        sta rb_reu_c64_lo
        lda #>RF_ARRAY_BUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #$80
        sta rb_reu_len_lo
        lda #0
        sta rb_reu_len_hi
        jsr rb_reu_fetch
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda #<RB_GFX_COLOR
        sta rb_ptr2_lo
        lda #>RB_GFX_COLOR
        sta rb_ptr2_hi
        lda #$E8
        sta rb_copy_len_lo
        lda #3
        sta rb_copy_len_hi
        lda #0
        sta rb_copy_chunks
@page:
        lda rb_copy_chunks
        clc
        adc rb_saved_plot_y
        sta rb_fill_page
        lda rb_saved_plot_x
        sta rb_handle_bank
        jsr tile_fetch_fill_page
        lda #0
        sta rb_copy_count
@cell:
        lda rb_copy_len_lo
        ora rb_copy_len_hi
        beq @done
        ldx rb_copy_count
        lda RB_PAGEBUF,x
        and #$3F
        asl
        tax
        ldy #0
        lda RF_ARRAY_BUF,x
        sta (rb_ptr_lo),y
        inx
        lda RF_ARRAY_BUF,x
        sta (rb_ptr2_lo),y
        inc rb_ptr_lo
        bne :+
        inc rb_ptr_hi
:       inc rb_ptr2_lo
        bne :+
        inc rb_ptr2_hi
:       inc rb_copy_count
        bne :+
        inc rb_copy_chunks
        jmp @page
:       lda rb_copy_len_lo
        bne :+
        dec rb_copy_len_hi
:       dec rb_copy_len_lo
        jmp @cell
@done:
        jmp tile_ok

cmd_mcell:
        jsr tile_calc_cell_addr
        ldy #0
        lda CF_NUM2_LO
        and #$0F
        asl
        asl
        asl
        asl
        sta rb_digit_seen
        lda CF_NUM3_LO
        and #$0F
        ora rb_digit_seen
        sta (rb_ptr_lo),y
        jsr tile_screen_ptr_to_color
        lda CF_NUM4_LO
        and #$0F
        sta (rb_ptr_lo),y
        jmp tile_ok

cmd_mcbg:
        lda CF_NUM0_LO
        and #$0F
        sta VIC_BG
        jmp tile_ok

tile_alloc_resource:
        sta rb_handle_new_type
        jsr tile_handle_alloc_with_pages
        rts

tile_bad_handle:
        lda #$24
        jmp tile_fail
tile_wrong_type:
        lda #$28
        jmp tile_fail
tile_bad_range:
        lda #$64
        jmp tile_fail

tile_calc_cell_addr:
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda CF_NUM1_LO
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        asl
        asl
        asl
        asl
        asl
        clc
        adc rb_saved_plot_x
        clc
        adc CF_NUM0_LO
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        adc #0
        sta rb_ptr_hi
        rts

tile_screen_ptr_to_color:
        lda rb_ptr_hi
        sec
        sbc #>RB_GFX_SCREEN
        clc
        adc #>RB_GFX_COLOR
        sta rb_ptr_hi
        rts

tile_load_handle_arg:
        lda CF_NUM0_HI
        bne @bad
        lda CF_NUM0_LO
        beq @bad
        cmp #RB_HANDLE_COUNT + 1
        bcs @bad
        sec
        sbc #1
        sta rb_handle_index
        jsr tile_handle_fetch
        lda rb_handle_bank
        beq @bad
        clc
        rts
@bad:
        sec
        rts

tile_handle_fetch:
        jsr tile_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda RB_PAGEBUF,y
        sta rb_handle_bank
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_page
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_pages
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_type
        rts

tile_handle_store:
        jsr tile_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda rb_handle_bank
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_page
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_pages
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_type
        sta RB_PAGEBUF,y
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_stash

tile_handle_desc_fetch_page:
        lda rb_handle_index
        and #$3F
        asl
        asl
        sta rb_handle_desc_off
        lda #0
        sta rb_reu_off_lo
        lda #>RB_REU_HANDLE_OFF
        ldx rb_handle_index
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc :+
        clc
        adc #1
:       sta rb_reu_off_hi
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_fetch

tile_handle_alloc_with_pages:
        jsr tile_find_free_handle
        bcc @got
        lda #$21
        jmp tile_fail
@got:
        jsr tile_find_pages
        bcc :+
        lda #$22
        jmp tile_fail
:       lda rb_reu_core_bank
        sta rb_handle_bank
        lda rb_found_page
        clc
        adc #RB_HEAP_PAGE_BASE
        sta rb_handle_page
        lda rb_needed_pages
        sta rb_handle_pages
        lda rb_handle_new_type
        sta rb_handle_type
        jsr tile_mark_pages_used
        jsr tile_store_heap_bitmap
        jsr tile_handle_store
        jsr tile_clear_alloc_pages
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda rb_handle_index
        clc
        adc #1
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts

tile_clear_alloc_pages:
        lda #0
        tay
@clearbuf:
        sta RB_PAGEBUF,y
        iny
        bne @clearbuf
        lda rb_handle_page
        sta rb_fill_page
        lda rb_handle_pages
        sta rb_copy_count
@page:
        jsr tile_stash_fill_page
        inc rb_fill_page
        dec rb_copy_count
        bne @page
        rts

tile_find_free_handle:
        lda #0
        sta rb_handle_scan_base
@page:
        lda rb_handle_scan_base
        sta rb_handle_index
        jsr tile_handle_desc_fetch_page
        ldy #0
        ldx #0
@slot:
        lda RB_PAGEBUF,y
        beq @found
        tya
        clc
        adc #RB_HANDLE_DESC_SIZE
        tay
        inx
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc @slot
        lda rb_handle_scan_base
        bne @full
        lda #RB_HANDLE_PAGE_SLOTS
        sta rb_handle_scan_base
        jmp @page
@found:
        txa
        clc
        adc rb_handle_scan_base
        sta rb_handle_index
        clc
        rts
@full:
        sec
        rts

tile_fetch_heap_bitmap:
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
        jmp rb_reu_fetch

tile_store_heap_bitmap:
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
        jmp rb_reu_stash

tile_find_pages:
        jsr tile_fetch_heap_bitmap
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
        bne @next
        inx
        cpx rb_needed_pages
        bcc @inner
        clc
        rts
@next:
        inc rb_found_page
        jmp @outer

tile_mark_pages_used:
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

tile_fetch_handle_page:
        lda rb_handle_page
        sta rb_fill_page
tile_fetch_fill_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_fill_page
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_fetch

tile_stash_handle_page:
        lda rb_handle_page
        sta rb_fill_page
tile_stash_fill_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_fill_page
        sta rb_reu_off_hi
        lda rb_handle_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_stash

tile_ok:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

tile_fail:
        sta RF_STATUS
        sta RF_ERROR
        rts

        .segment "OVL3PACK"

cmd_poly:
        jsr poly_apply_command_kind
        jsr poly_prepare
        bcc :+
        rts
:       jsr poly_init_first_prev
        bcc :+
        rts
:       lda #1
        sta rb_copy_chunks
@loop:
        lda rb_copy_chunks
        cmp rb_copy_count
        bcs @close
        jsr poly_read_point_temp
        bcc :+
        rts
:       jsr poly_line_prev_to_temp
        jsr poly_temp_to_prev
        inc rb_copy_chunks
        jmp @loop
@close:
        jsr poly_first_to_temp
        jsr poly_line_prev_to_temp
        jmp poly_ok

cmd_fpoly:
        jsr poly_apply_command_kind
        jsr poly_prepare
        bcc :+
        rts
:       jsr poly_init_first_prev
        bcc :+
        rts
:       lda #1
        sta rb_copy_chunks
@loop:
        lda rb_copy_chunks
        cmp rb_copy_count
        bcs @done
        jsr poly_read_point_temp
        bcc :+
        rts
:       jsr poly_fill_edge_from_prev_to_temp
        jsr poly_temp_to_prev
        inc rb_copy_chunks
        jmp @loop
@done:
        jsr poly_first_to_temp
        jsr poly_fill_edge_from_prev_to_temp
        jmp poly_ok

cmd_pbufnew:
        lda CF_NUM0_HI
        bne @bad
        lda CF_NUM0_LO
        beq @bad
        cmp #65
        bcs @bad
        lda #1
        sta rb_needed_pages
        lda #RB_HANDLE_TYPE_POINTBUF
        sta rb_handle_new_type
        jsr poly_handle_alloc_with_pages
        rts
@bad:
        lda #$51
        jmp poly_fail

cmd_pbuffree:
        jsr poly_handle_load_arg
        bcs @bad
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_POINTBUF
        bne @wrong
        jsr poly_handle_free_loaded
        jmp poly_ok
@bad:
        lda #$24
        jmp poly_fail
@wrong:
        lda #$28
        jmp poly_fail

cmd_pbufset:
        jsr poly_handle_load_arg
        bcs @bad
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_POINTBUF
        bne @wrong
        lda CF_NUM1_HI
        bne @range
        lda CF_NUM1_LO
        cmp #64
        bcs @range
        jsr poly_fetch_handle_page
        lda CF_NUM1_LO
        asl
        asl
        tay
        lda CF_NUM2_LO
        sta RB_PAGEBUF,y
        iny
        lda CF_NUM2_HI
        sta RB_PAGEBUF,y
        iny
        lda CF_NUM3_LO
        sta RB_PAGEBUF,y
        iny
        lda CF_NUM3_HI
        sta RB_PAGEBUF,y
        jsr poly_stash_handle_page
        jmp poly_ok
@bad:
        lda #$24
        jmp poly_fail
@wrong:
        lda #$28
        jmp poly_fail
@range:
        lda #$52
        jmp poly_fail

poly_prepare:
        lda CF_NUM1_HI
        bne @bad
        lda CF_NUM1_LO
        cmp #3
        bcc @bad
        cmp #33
        bcs @bad
        lda CF_NUM2_LO
        sta RF_RECT_BUF+8
        lda CF_NUM2_HI
        sta RF_RECT_BUF+9
        lda CF_NUM1_LO
        sta rb_copy_count
        lda CF_COUNT0_HI
        beq @array
        jsr poly_handle_load_arg
        bcs @bad_handle
        lda rb_handle_type
        cmp #RB_HANDLE_TYPE_POINTBUF
        bne @wrong
@array:
        clc
        rts
@bad_handle:
        lda #$24
        jmp poly_fail_sec
@wrong:
        lda #$28
        jmp poly_fail_sec
@bad:
        lda #$51
poly_fail_sec:
        jsr poly_fail
        sec
        rts

poly_apply_command_kind:
        lda CF_CMD_ID
        cmp #CMD_POLYH
        beq @handle
        cmp #CMD_FPOLYH
        beq @handle
        lda #0
        beq @store
@handle:
        lda #1
@store:
        sta CF_COUNT0_HI
        rts

poly_init_first_prev:
        lda #0
        sta rb_copy_chunks
        jsr poly_read_point_temp
        bcc :+
        rts
:       lda RF_RECT_BUF+10
        sta RF_RECT_BUF
        sta RF_RECT_BUF+4
        lda RF_RECT_BUF+11
        sta RF_RECT_BUF+1
        sta RF_RECT_BUF+5
        lda RF_RECT_BUF+12
        sta RF_RECT_BUF+2
        sta RF_RECT_BUF+6
        lda RF_RECT_BUF+13
        sta RF_RECT_BUF+3
        sta RF_RECT_BUF+7
        clc
        rts

poly_read_point_temp:
        lda CF_COUNT0_HI
        beq poly_read_array_point_temp
        jmp poly_read_handle_point_temp

poly_read_array_point_temp:
        lda rb_copy_chunks
        asl
        asl
        tay
        lda CF_PTR0_LO
        sta rb_ptr_lo
        lda CF_PTR0_HI
        sta rb_ptr_hi
        tya
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       ldy #0
        lda (rb_ptr_lo),y
        sta RF_RECT_BUF+11
        iny
        lda (rb_ptr_lo),y
        sta RF_RECT_BUF+10
        iny
        lda (rb_ptr_lo),y
        sta RF_RECT_BUF+13
        iny
        lda (rb_ptr_lo),y
        sta RF_RECT_BUF+12
        clc
        rts

poly_read_handle_point_temp:
        lda rb_copy_chunks
        cmp #64
        bcc :+
        lda #$52
        jsr poly_fail
        sec
        rts
:       jsr poly_fetch_handle_page
        lda rb_copy_chunks
        asl
        asl
        tay
        lda RB_PAGEBUF,y
        sta RF_RECT_BUF+10
        iny
        lda RB_PAGEBUF,y
        sta RF_RECT_BUF+11
        iny
        lda RB_PAGEBUF,y
        sta RF_RECT_BUF+12
        iny
        lda RB_PAGEBUF,y
        sta RF_RECT_BUF+13
        clc
        rts

poly_line_prev_to_temp:
        lda RF_RECT_BUF+4
        sta CF_NUM0_LO
        lda RF_RECT_BUF+5
        sta CF_NUM0_HI
        lda RF_RECT_BUF+6
        sta CF_NUM1_LO
        lda RF_RECT_BUF+7
        sta CF_NUM1_HI
        lda RF_RECT_BUF+10
        sta CF_NUM2_LO
        lda RF_RECT_BUF+11
        sta CF_NUM2_HI
        lda RF_RECT_BUF+12
        sta CF_NUM3_LO
        lda RF_RECT_BUF+13
        sta CF_NUM3_HI
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        lda RF_RECT_BUF+9
        sta CF_NUM4_HI
        jmp poly_line

poly_temp_to_prev:
        lda RF_RECT_BUF+10
        sta RF_RECT_BUF+4
        lda RF_RECT_BUF+11
        sta RF_RECT_BUF+5
        lda RF_RECT_BUF+12
        sta RF_RECT_BUF+6
        lda RF_RECT_BUF+13
        sta RF_RECT_BUF+7
        rts

poly_first_to_temp:
        lda RF_RECT_BUF
        sta RF_RECT_BUF+10
        lda RF_RECT_BUF+1
        sta RF_RECT_BUF+11
        lda RF_RECT_BUF+2
        sta RF_RECT_BUF+12
        lda RF_RECT_BUF+3
        sta RF_RECT_BUF+13
        rts

poly_fill_edge_from_prev_to_temp:
        lda RF_RECT_BUF+10
        sta RF_RECT_BUF+14
        lda RF_RECT_BUF+11
        sta RF_RECT_BUF+15
        lda RF_RECT_BUF+12
        sta RF_STR_BUF
        lda RF_RECT_BUF+13
        sta RF_STR_BUF+1
        lda RF_RECT_BUF+4
        sta CF_NUM0_LO
        lda RF_RECT_BUF+5
        sta CF_NUM0_HI
        lda RF_RECT_BUF+6
        sta CF_NUM1_LO
        lda RF_RECT_BUF+7
        sta CF_NUM1_HI
        lda #0
        sta rb_copy_len_hi
@loop:
        dec rb_copy_len_hi
        bne :+
        jmp @done
:
        lda CF_NUM0_LO
        sta RF_STR_BUF+2
        lda CF_NUM0_HI
        sta RF_STR_BUF+3
        lda CF_NUM1_LO
        sta RF_STR_BUF+4
        lda CF_NUM1_HI
        sta RF_STR_BUF+5
        lda RF_RECT_BUF
        sta CF_NUM0_LO
        lda RF_RECT_BUF+1
        sta CF_NUM0_HI
        lda RF_RECT_BUF+2
        sta CF_NUM1_LO
        lda RF_RECT_BUF+3
        sta CF_NUM1_HI
        lda RF_STR_BUF+2
        sta CF_NUM2_LO
        lda RF_STR_BUF+3
        sta CF_NUM2_HI
        lda RF_STR_BUF+4
        sta CF_NUM3_LO
        lda RF_STR_BUF+5
        sta CF_NUM3_HI
        lda RF_RECT_BUF+8
        sta CF_NUM4_LO
        lda RF_RECT_BUF+9
        sta CF_NUM4_HI
        jsr poly_line
        lda RF_STR_BUF+2
        sta CF_NUM0_LO
        lda RF_STR_BUF+3
        sta CF_NUM0_HI
        lda RF_STR_BUF+4
        sta CF_NUM1_LO
        lda RF_STR_BUF+5
        sta CF_NUM1_HI
        lda CF_NUM0_LO
        cmp RF_RECT_BUF+14
        bne @step
        lda CF_NUM0_HI
        cmp RF_RECT_BUF+15
        bne @step
        lda CF_NUM1_LO
        cmp RF_STR_BUF
        beq @done
@step:
        jsr poly_step_x_toward_temp
        jsr poly_step_y_toward_temp
        jmp @loop
@done:
        rts

poly_line:
        lda CF_NUM2_LO
        sta RF_ARRAY_BUF
        lda CF_NUM2_HI
        sta RF_ARRAY_BUF+1
        lda CF_NUM3_LO
        sta RF_ARRAY_BUF+2
        lda CF_NUM3_HI
        sta RF_ARRAY_BUF+3
        lda CF_NUM4_LO
        sta RF_ARRAY_BUF+4
        lda CF_NUM4_HI
        sta RF_ARRAY_BUF+5
        lda #0
        sta rb_copy_len_lo
@loop:
        dec rb_copy_len_lo
        beq @done
        lda RF_ARRAY_BUF+4
        sta CF_NUM2_LO
        lda RF_ARRAY_BUF+5
        sta CF_NUM2_HI
        jsr poly_plot_current
        lda CF_NUM0_LO
        cmp RF_ARRAY_BUF
        bne @step
        lda CF_NUM0_HI
        cmp RF_ARRAY_BUF+1
        bne @step
        lda CF_NUM1_LO
        cmp RF_ARRAY_BUF+2
        beq @done
@step:
        jsr poly_step_x_toward_array
        jsr poly_step_y_toward_array
        jmp @loop
@done:
        rts

poly_step_x_toward_array:
        lda CF_NUM0_HI
        cmp RF_ARRAY_BUF+1
        bcc @inc
        bne @dec
        lda CF_NUM0_LO
        cmp RF_ARRAY_BUF
        bcc @inc
        beq @done
@dec:
        lda CF_NUM0_LO
        bne :+
        dec CF_NUM0_HI
:       dec CF_NUM0_LO
        rts
@inc:
        inc CF_NUM0_LO
        bne @done
        inc CF_NUM0_HI
@done:
        rts

poly_step_y_toward_array:
        lda CF_NUM1_LO
        cmp RF_ARRAY_BUF+2
        bcc @inc
        beq @done
        dec CF_NUM1_LO
        rts
@inc:
        inc CF_NUM1_LO
@done:
        rts

poly_step_x_toward_temp:
        lda CF_NUM0_HI
        cmp RF_RECT_BUF+15
        bcc @inc
        bne @dec
        lda CF_NUM0_LO
        cmp RF_RECT_BUF+14
        bcc @inc
        beq @done
@dec:
        lda CF_NUM0_LO
        bne :+
        dec CF_NUM0_HI
:       dec CF_NUM0_LO
        rts
@inc:
        inc CF_NUM0_LO
        bne @done
        inc CF_NUM0_HI
@done:
        rts

poly_step_y_toward_temp:
        lda CF_NUM1_LO
        cmp RF_STR_BUF
        bcc @inc
        beq @done
        dec CF_NUM1_LO
        rts
@inc:
        inc CF_NUM1_LO
@done:
        rts

poly_plot_current:
        jsr poly_get_mode
        cmp #RB_GFX_MODE_TILE
        beq poly_plot_tile
        cmp #RB_GFX_MODE_MTILE
        beq poly_plot_tile
        jsr poly_calc_bitmap_addr
        lda CPU_PORT
        pha
        and #RAM_UNDER_BASIC
        sta CPU_PORT
        ldy #0
        lda (rb_ptr_lo),y
        ldx CF_NUM2_LO
        beq @clear
        ldx rb_saved_plot_y
        ora poly_bit_masks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts
@clear:
        ldx rb_saved_plot_y
        and poly_bit_unmasks,x
        sta (rb_ptr_lo),y
        pla
        sta CPU_PORT
        rts

poly_plot_tile:
        jsr poly_calc_tile_addr
        ldy #0
        lda CF_NUM2_LO
        and #$3F
        ora #$40
        sta (rb_ptr_lo),y
        lda rb_ptr_hi
        sec
        sbc #>RB_GFX_SCREEN
        clc
        adc #>RB_GFX_COLOR
        sta rb_ptr_hi
        lda CF_NUM2_LO
        and #$0F
        sta (rb_ptr_lo),y
        rts

poly_get_mode:
        lda RB_GFX_MODE_STATE
        cmp #RB_GFX_MODE_MTILE + 1
        bcc :+
        lda #RB_GFX_MODE_TEXT
:       rts

poly_calc_tile_addr:
        lda #<RB_GFX_SCREEN
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        sta rb_ptr_hi
        lda CF_NUM1_LO
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        asl
        asl
        asl
        asl
        asl
        clc
        adc rb_saved_plot_x
        clc
        adc CF_NUM0_LO
        sta rb_ptr_lo
        lda #>RB_GFX_SCREEN
        adc #0
        sta rb_ptr_hi
        rts

poly_calc_bitmap_addr:
        lda CF_NUM1_LO
        and #7
        sta rb_saved_plot_x
        lda CF_NUM0_LO
        and #7
        sta rb_saved_plot_y
        lda CF_NUM1_LO
        lsr
        lsr
        lsr
        sta rb_digit_seen
        lda rb_digit_seen
        and #3
        asl
        asl
        asl
        asl
        asl
        asl
        sta rb_ptr_lo
        lda #>RB_GFX_BITMAP
        clc
        adc rb_digit_seen
        sta rb_ptr_hi
        lda rb_digit_seen
        lsr
        lsr
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda CF_NUM0_LO
        and #$F8
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       lda CF_NUM0_HI
        clc
        adc rb_ptr_hi
        sta rb_ptr_hi
        lda rb_saved_plot_x
        clc
        adc rb_ptr_lo
        sta rb_ptr_lo
        bcc :+
        inc rb_ptr_hi
:       rts

poly_handle_load_arg:
        lda CF_NUM0_HI
        bne @bad
        lda CF_NUM0_LO
        beq @bad
        cmp #RB_HANDLE_COUNT + 1
        bcs @bad
        sec
        sbc #1
        sta rb_handle_index
        jsr poly_handle_fetch
        lda rb_handle_bank
        beq @bad
        clc
        rts
@bad:
        sec
        rts

poly_handle_fetch:
        jsr poly_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda RB_PAGEBUF,y
        sta rb_handle_bank
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_page
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_pages
        iny
        lda RB_PAGEBUF,y
        sta rb_handle_type
        rts

poly_handle_store:
        jsr poly_handle_desc_fetch_page
        ldy rb_handle_desc_off
        lda rb_handle_bank
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_page
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_pages
        sta RB_PAGEBUF,y
        iny
        lda rb_handle_type
        sta RB_PAGEBUF,y
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_stash

poly_handle_desc_fetch_page:
        lda rb_handle_index
        and #$3F
        asl
        asl
        sta rb_handle_desc_off
        lda #0
        sta rb_reu_off_lo
        lda #>RB_REU_HANDLE_OFF
        ldx rb_handle_index
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc :+
        clc
        adc #1
:       sta rb_reu_off_hi
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_fetch

poly_handle_alloc_with_pages:
        jsr poly_find_free_handle
        bcc @got_handle
        lda #$21
        jmp poly_fail
@got_handle:
        jsr poly_find_pages
        bcc :+
        lda #$22
        jmp poly_fail
:       lda rb_reu_core_bank
        sta rb_handle_bank
        lda rb_found_page
        clc
        adc #RB_HEAP_PAGE_BASE
        sta rb_handle_page
        lda rb_needed_pages
        sta rb_handle_pages
        lda rb_handle_new_type
        sta rb_handle_type
        jsr poly_mark_pages_used
        jsr poly_store_heap_bitmap
        jsr poly_handle_store
        lda #0
        sta RF_STATUS
        lda #RB_VAL_INT
        sta RF_TAG
        lda rb_handle_index
        clc
        adc #1
        sta RF_VAL_LO
        lda #0
        sta RF_VAL_HI
        rts

poly_find_free_handle:
        lda #0
        sta rb_handle_scan_base
@page:
        lda rb_handle_scan_base
        sta rb_handle_index
        jsr poly_handle_desc_fetch_page
        ldy #0
        ldx #0
@slot:
        lda RB_PAGEBUF,y
        beq @found
        tya
        clc
        adc #RB_HANDLE_DESC_SIZE
        tay
        inx
        cpx #RB_HANDLE_PAGE_SLOTS
        bcc @slot
        lda rb_handle_scan_base
        bne @full
        lda #RB_HANDLE_PAGE_SLOTS
        sta rb_handle_scan_base
        jmp @page
@found:
        txa
        clc
        adc rb_handle_scan_base
        sta rb_handle_index
        clc
        rts
@full:
        sec
        rts

poly_fetch_heap_bitmap:
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
        jmp rb_reu_fetch

poly_store_heap_bitmap:
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
        jmp rb_reu_stash

poly_find_pages:
        jsr poly_fetch_heap_bitmap
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

poly_mark_pages_used:
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

poly_mark_pages_free:
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

poly_handle_free_loaded:
        lda rb_handle_page
        sec
        sbc #RB_HEAP_PAGE_BASE
        sta rb_found_page
        lda rb_handle_pages
        sta rb_needed_pages
        jsr poly_fetch_heap_bitmap
        jsr poly_mark_pages_free
        lda #0
        sta rb_handle_bank
        sta rb_handle_page
        sta rb_handle_pages
        sta rb_handle_type
        jsr poly_store_heap_bitmap
        jmp poly_handle_store

poly_fetch_handle_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_fetch

poly_stash_handle_page:
        lda #<RB_PAGEBUF
        sta rb_reu_c64_lo
        lda #>RB_PAGEBUF
        sta rb_reu_c64_hi
        lda #0
        sta rb_reu_off_lo
        lda rb_handle_page
        sta rb_reu_off_hi
        lda rb_reu_core_bank
        sta rb_reu_bank
        lda #0
        sta rb_reu_len_lo
        lda #1
        sta rb_reu_len_hi
        jmp rb_reu_stash

poly_ok:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

poly_fail:
        sta RF_STATUS
        sta RF_ERROR
        rts

poly_bit_masks:
        .byte $80,$40,$20,$10,$08,$04,$02,$01
poly_bit_unmasks:
        .byte $7F,$BF,$DF,$EF,$F7,$FB,$FD,$FE

        .segment "OVL6PACK"

cmd_sidclr:
        ldx #24
        lda #0
@loop:
        sta SID_BASE,x
        dex
        bpl @loop
        jmp sid_ok

cmd_vol:
        lda CF_NUM0_LO
        and #$0F
        sta rb_saved_plot_x
        lda SID_MODE_VOL
        and #$F0
        ora rb_saved_plot_x
        sta SID_MODE_VOL
        jmp sid_ok

cmd_freq:
        jsr sid_voice_offset
        bcc :+
        rts
:       lda CF_NUM1_LO
        sta SID_V1_FREQ_LO,x
        lda CF_NUM1_HI
        sta SID_V1_FREQ_HI,x
        jmp sid_ok

cmd_note:
        jsr sid_voice_offset
        bcc :+
        rts
:       stx rb_target_off
        lda CF_NUM1_LO
        cmp #12
        bcc :+
        lda #$64
        jmp sid_fail
:       tax
        lda sid_note_c0_lo,x
        sta rb_saved_plot_x
        lda sid_note_c0_hi,x
        sta rb_saved_plot_y
        lda CF_NUM2_LO
        and #7
        sta RF_COUNT_LO
        beq @store
@shift:
        asl rb_saved_plot_x
        rol rb_saved_plot_y
        dec RF_COUNT_LO
        bne @shift
@store:
        ldx rb_target_off
        lda rb_saved_plot_x
        sta SID_V1_FREQ_LO,x
        lda rb_saved_plot_y
        sta SID_V1_FREQ_HI,x
        jmp sid_ok

cmd_pulse:
        jsr sid_voice_offset
        bcc :+
        rts
:       lda CF_NUM1_LO
        sta SID_V1_PW_LO,x
        lda CF_NUM1_HI
        and #$0F
        sta SID_V1_PW_HI,x
        jmp sid_ok

cmd_adsr:
        jsr sid_voice_offset
        bcc :+
        rts
:       lda CF_NUM2_LO
        and #$0F
        sta rb_saved_plot_x
        lda CF_NUM1_LO
        and #$0F
        asl
        asl
        asl
        asl
        ora rb_saved_plot_x
        sta SID_V1_AD,x
        lda CF_NUM4_LO
        and #$0F
        sta rb_saved_plot_x
        lda CF_NUM3_LO
        and #$0F
        asl
        asl
        asl
        asl
        ora rb_saved_plot_x
        sta SID_V1_SR,x
        jmp sid_ok

cmd_wave:
        jsr sid_voice_offset
        bcc :+
        rts
:       lda CF_NUM1_LO
        sta SID_V1_CTRL,x
        jmp sid_ok

cmd_gate:
        jsr sid_voice_offset
        bcc :+
        rts
:       lda CF_NUM1_LO
        beq @off
        lda SID_V1_CTRL,x
        ora #$01
        sta SID_V1_CTRL,x
        jmp sid_ok
@off:
        lda SID_V1_CTRL,x
        and #$FE
        sta SID_V1_CTRL,x
        jmp sid_ok

cmd_voice:
        jsr sid_voice_offset
        bcc :+
        rts
:       lda CF_NUM1_LO
        sta SID_V1_FREQ_LO,x
        lda CF_NUM1_HI
        sta SID_V1_FREQ_HI,x
        lda CF_NUM3_LO
        sta SID_V1_AD,x
        lda CF_NUM4_LO
        sta SID_V1_SR,x
        lda CF_NUM2_LO
        sta SID_V1_CTRL,x
        jmp sid_ok

cmd_filter:
        lda CF_NUM0_LO
        and #$07
        sta SID_FILTER_LO
        lda CF_NUM0_LO
        sta rb_saved_plot_x
        lda CF_NUM0_HI
        sta rb_saved_plot_y
        lsr rb_saved_plot_y
        ror rb_saved_plot_x
        lsr rb_saved_plot_y
        ror rb_saved_plot_x
        lsr rb_saved_plot_y
        ror rb_saved_plot_x
        lda rb_saved_plot_x
        sta SID_FILTER_HI
        lda CF_NUM1_LO
        and #$0F
        asl
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda CF_NUM2_LO
        and #$0F
        ora rb_saved_plot_x
        sta SID_FILTER_RES
        lda CF_NUM3_LO
        and #$0F
        asl
        asl
        asl
        asl
        sta rb_saved_plot_x
        lda SID_MODE_VOL
        and #$0F
        ora rb_saved_plot_x
        sta SID_MODE_VOL
        jmp sid_ok

cmd_sound:
        jsr sid_voice_offset
        bcc :+
        rts
:       stx rb_target_off
        lda CF_NUM1_LO
        sta SID_V1_FREQ_LO,x
        lda CF_NUM1_HI
        sta SID_V1_FREQ_HI,x
        lda CF_NUM3_LO
        ora #$01
        sta SID_V1_CTRL,x
        lda CF_NUM2_LO
        beq @release
        sta RF_COUNT_LO
@outer:
        ldx #$20
@middle:
        ldy #$FF
@inner:
        dey
        bne @inner
        dex
        bne @middle
        dec RF_COUNT_LO
        bne @outer
@release:
        ldx rb_target_off
        lda SID_V1_CTRL,x
        and #$FE
        sta SID_V1_CTRL,x
        jmp sid_ok

sid_voice_offset:
        lda CF_NUM0_LO
        cmp #1
        bcc @bad
        cmp #4
        bcs @bad
        sec
        sbc #1
        tax
        lda sid_voice_offsets,x
        tax
        clc
        rts
@bad:
        lda #$64
        jsr sid_fail
        sec
        rts

sid_ok:
        lda #0
        sta RF_STATUS
        lda #RB_VAL_NONE
        sta RF_TAG
        rts

sid_fail:
        sta RF_STATUS
        sta RF_ERROR
        rts

sid_voice_offsets:
        .byte 0,7,14

; PAL C0-B0 SID frequency words. PITCH shifts these by octave.
sid_note_c0_lo:
        .byte $16,$2B,$41,$58,$70,$8A,$A4,$C0,$DD,$FC,$1D,$3F
sid_note_c0_hi:
        .byte $01,$01,$01,$01,$01,$01,$01,$01,$01,$01,$02,$02
