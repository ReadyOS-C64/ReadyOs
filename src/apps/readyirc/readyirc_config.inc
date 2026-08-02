;
; ReadyIRC build-time connection defaults.
; Plain TCP IRC only: no TLS, one channel.
;

_readyirc_config_server:
        .asciiz "irc.libera.chat"

_readyirc_config_channel:
        .asciiz "#readyos"

_readyirc_config_nick:
        .asciiz "enteryournick"

_readyirc_config_port:
        .word 6667
