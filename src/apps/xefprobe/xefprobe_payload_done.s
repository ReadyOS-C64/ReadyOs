.setcpu "6502"

.export _xefprobe_payload_done

.segment "CODE"

_xefprobe_payload_done:
@loop:
    jmp @loop
