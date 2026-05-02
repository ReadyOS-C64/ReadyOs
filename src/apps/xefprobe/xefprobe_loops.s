.setcpu "6502"

.export _xefprobe_host_fail_loop
.export _xefprobe_host_after_clear_checkpoint
.export _xefprobe_host_after_verify_checkpoint
.export _xefprobe_payload_fail_loop
.export _xefprobe_payload_after_fetch_checkpoint

.segment "CODE"

_xefprobe_host_after_clear_checkpoint:
    rts

_xefprobe_host_after_verify_checkpoint:
    rts

_xefprobe_host_fail_loop:
@host:
    jmp @host

_xefprobe_payload_after_fetch_checkpoint:
    rts

_xefprobe_payload_fail_loop:
@payload:
    jmp @payload
