# ReadyOS THEC64 D81 Experimental Staging

This folder is an experimental THEC64 Mini/Maxi staging copy for compatibility testing. It is not wired into the build.

Files:

- `readyos-v0.2.4-d81_M6TPRM.d81`
- `readyos-v0.2.4-d81_M6TPRM.cjm`

ReadyOS requires at least 1 MB REU; 8 MB or 16 MB is recommended where
available. This experimental staging copy deliberately requests C64, PAL, and
16 MB REU through its filename flags:

- `M6` = C64
- `TP` = PAL
- `RM` = 16 MB REU

The CJM repeats the same baseline with `X:64,pal,reu16384`, adds explicit joystick mappings so THEC64 controllers remain active, and leaves accurate disk mode off because THEC64 documents `accuratedisk` as applying to `d64`/`g64`.

This staging copy uses the normal disk loader. The C64 Ultimate DOS DMA path is
hardware-specific, opt-in at source-build time, and is not available on
THEC64/VICE.
