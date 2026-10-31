# precog (solo d64 readybasic)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `solo-d64-readybasic`

## Why This Variant Exists

- Focused single-D64 ReadyBASIC profile with ReadyOS, ReadyBASIC, three sample packages and the original 41 examples; new media demos are on regular/Ultimate D81.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-solo-d64-readybasic.d64`
- Host-Side Boot PRG: `readyos-v0.5-solo-d64-readybasic-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-solo-d64-readybasic-boot.prg`

## Start ReadyOS

- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- Mount the boot-time disks on the devices listed above. At BASIC, type `LOAD"*",8`, press RETURN, then type `RUN`.
- In the launcher, select an app and press RETURN. Ctrl+B returns to the launcher. Save documents to disk before a reset.

## Included Apps

- **ready basic (beta)** (`readybasic`, drive 8) — basic v2 bridge, modules and examples (portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate).

All apps use REU for suspend/resume. The notes identify additional workspace and hardware requirements. Clipboard operations use shared REU storage.
- This one D64 contains ReadyBASIC, the three sample `rbm.*` packages, and the original 41-program procedure, graphics, and sound example/test set.
- ReadyBASIC's banked `rbcore`/`rbcode` resources are carried inside the `readybasic` executable; no second examples disk is required.

## ReadyBASIC examples and modules

This profile defines 41 BASIC example/test PRGs and 3 disk module packages: `rbm.sample1`, `rbm.sample2`, `rbm.sample3`.
Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers.
Use the regular D81 or an Ultimate D81 set for the Orbital Echoes demos and their media files.
Load the example from the disk that holds it. Keep its required modules and media on drive 8. See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for every example, command contracts and exact per-profile availability.

## VICE Setup

- Enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1541` with true drive enabled and attach `readyos-v0.5-solo-d64-readybasic.d64`.

### VICE Command Example

- Autostart target: `readyos-v0.5-solo-d64-readybasic-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1541 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.5-solo-d64-readybasic.d64 -autostart readyos-v0.5-solo-d64-readybasic-preboot.prg
```
