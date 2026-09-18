# readyos easyflash

- attach `readyos_easyflash.crt` as an easyflash cartridge
- mount `readyos_data.d64` on drive `8`
- enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available
- if an app snapshot preloaded from the cartridge is unloaded from REU, ReadyOS cannot load it again from the cartridge until you restart ReadyOS

## directory order

- the CRT uses its cartridge-bank layout and is not governed by floppy directory order
- the companion `readyos_data.d64` writes ordinary SEQ/USR data before REL data

## vice example

```sh
x64sc -reu -reusize 16384 -cartcrt readyos_easyflash.crt -drive8type 1541 -devicebackend8 0 +busdevice8 -8 readyos_data.d64
```

## Apps and requirements

Every app uses the ReadyOS REU snapshot system. Additional requirements:

- `editor`: portable; no separate app-owned REU workspace.
- `readyshell`: portable; own REU overlays/state.
- `simplefiles`: portable; no separate app-owned REU workspace.
- `clipmgr`: portable; shared REU clipboard/history.
- `readybasic`: portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate.
- `cal26`: portable; no separate app-owned REU workspace.
- `tasklist`: portable; no separate app-owned REU workspace.
- `reuviewer`: portable; inspects shared REU allocation records.
- `sysinfo`: portable diagnostics; Ultimate details available only on Ultimate.
- `quicknotes`: portable; own REU note storage.
- `calcplus`: portable; no separate app-owned REU workspace.
- `hexview`: portable; no separate app-owned REU workspace.
- `simplecells`: portable; no separate app-owned REU workspace.
- `game2048`: portable; no separate app-owned REU workspace.
- `sidetris`: portable; no separate app-owned REU workspace.
- `deminer`: portable; no separate app-owned REU workspace.
- `dizzy`: portable; no separate app-owned REU workspace.
- `readyirc`: Ultimate-only TCP; own REU scrollback.
- `ucitest`: Ultimate-only command lab; no separate app-owned REU workspace.
- `readme`: portable; no separate app-owned REU workspace.

ReadyBASIC's built-in commands are preloaded with the runtime. The companion data D64 does not package the disk-module/example/media collection; use the regular or Ultimate D81 for the new media demos. Cartridge preload is separate from Ultimate DOS DMA and needs no D81 host-path setup.

## After PRECOG

PRECOG 0.5 is planned as the final PRECOG release: the series that established what is possible and clarified the vision. Next comes ReadyOS Ultimate, the main focus, installing/configuring real files and folders on Ultimate storage and using its hardware features; and ReadyOS Universal, continuing disk-image and REU workflows for VICE, THEC64 and original hardware. Universal effort will follow user interest and demand. These are future directions, not separate products shipped here. Standalone releases of many apps are also planned, including both apps with their own REU needs and apps without them. We remain committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; this does not promise a no-REU interpreter. Current ReadyOS app PRGs still require the ReadyOS runtime.
