# precog (dual d64)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `dual-d64`

## Why This Variant Exists

- Reduced dual-disk profile for 1541-class environments that can mount two D64 images but not higher-capacity media.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-dual-d64_1.d64`
- Boot-time drive 9: `readyos-v0.5-dual-d64_2.d64`
- Host-Side Boot PRG: `readyos-v0.5-dual-d64-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-dual-d64-boot.prg`

## Start ReadyOS

- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- Mount the boot-time disks on the devices listed above. At BASIC, type `LOAD"*",8`, press RETURN, then type `RUN`.
- In the launcher, select an app and press RETURN. Ctrl+B returns to the launcher. Save documents to disk before a reset.

## Included Apps

- **editor** (`editor`, drive 9) — text editor with clipboard.
- **readyshell (beta)** (`readyshell`, drive 8) — command shell poc scaffold (portable; own REU overlays/state).
- **simple files** (`simplefiles`, drive 9) — dual pane file manager and seq viewer.
- **clipboard** (`clipmgr`, drive 9) — manage clipboard items (portable; shared REU clipboard/history).
- **calendar 26** (`cal26`, drive 8) — calendar for 2026 with appointments.
- **task list** (`tasklist`, drive 9) — outliner with notes and search.
- **quicknotes** (`quicknotes`, drive 8) — reu-backed note editor (portable; own REU note storage).
- **calc plus** (`calcplus`, drive 9) — keyboard-first expression calculator.

All apps use REU for suspend/resume. The notes identify additional workspace and hardware requirements. Clipboard operations use shared REU storage.

## ReadyBASIC examples and modules

This profile defines 1 BASIC example/test PRGs and 1 disk module packages: `rbm.sample1`.
Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers.
Use the regular D81 or an Ultimate D81 set for the Orbital Echoes demos and their media files.
Load the example from the disk that holds it. Keep its required modules and media on drive 8. See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for every example, command contracts and exact per-profile availability.

## VICE Setup

- Enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1541` with true drive enabled and attach `readyos-v0.5-dual-d64_1.d64`.
- Configure drive 9 as `1541` with true drive enabled and attach `readyos-v0.5-dual-d64_2.d64`.

### VICE Command Example

- Autostart target: `readyos-v0.5-dual-d64-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1541 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.5-dual-d64_1.d64 -drive9type 1541 -drive9truedrive -devicebackend9 0 +busdevice9 -9 readyos-v0.5-dual-d64_2.d64 -autostart readyos-v0.5-dual-d64-preboot.prg
```
