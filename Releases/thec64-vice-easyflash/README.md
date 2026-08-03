# ReadyOS THEC64 EasyFlash Experimental Staging

This folder is an experimental THEC64 Mini/Maxi staging copy for compatibility testing. It is not wired into the build.

Files:

- `readyos_easyflash_M6TPRM.crt`
- `readyos_easyflash_M6TPRM.cjm`
- `readyos_data.d64`

ReadyOS requires at least 1 MB REU; 8 MB or 16 MB is recommended where
available. This experimental staging copy deliberately requests C64, PAL, and
16 MB REU through its filename flags:

- `M6` = C64
- `TP` = PAL
- `RM` = 16 MB REU

The CJM repeats the same baseline with `X:64,pal,reu16384` and adds explicit joystick mappings so THEC64 controllers remain active. `readyos_data.d64` is included beside the cartridge for runtime app data access; the CJM cannot auto-mount it.

If an app snapshot preloaded from the cartridge is unloaded from REU, ReadyOS cannot load it again from the cartridge until you restart ReadyOS.

EasyFlash uses cartridge-to-RAM-to-REU cold preload and never links the regular
launcher's experimental Ultimate DOS DMA module. THEC64/VICE also does not
provide the required C64 Ultimate UCI service.
