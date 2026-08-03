;
; tui_readyos_alias.s - zero-frame ReadyOS-bank aliases for full REU clients.
;
; These entry points deliberately tail-jump to the canonical reu_mgr_dma
; helpers.  A tail jump preserves cc65's software-stack argument layout while
; avoiding a second copy of the REU register-programming routines.
;

.import _readyos_bank_read_byte
.import _readyos_bank_write_byte

.export _tui_readyos_read_byte
.export _tui_readyos_write_byte

.segment "CODE"

.proc _tui_readyos_read_byte
    jmp _readyos_bank_read_byte
.endproc

.proc _tui_readyos_write_byte
    jmp _readyos_bank_write_byte
.endproc
