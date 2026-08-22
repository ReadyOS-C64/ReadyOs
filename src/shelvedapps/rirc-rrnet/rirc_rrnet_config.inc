;
; ReadyIRC build-time connection defaults.
; Plain TCP IRC only: no TLS, one channel.
;

_rirc_rrnet_config_server:
        .asciiz "irc.libera.chat"

_rirc_rrnet_config_channel:
        .asciiz "#readyos"

_rirc_rrnet_config_nick:
        .asciiz "readyos64"

_rirc_rrnet_config_port:
        .word 6667
