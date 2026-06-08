# precog (dual d71)

- Release Line: `0.2.5`
- Artifact Build: `0.2.5`
- Kind: `dual-d71`

## Why This Variant Exists

- Default full-content profile for two 1571-class drives and the main local verification target.

## Artifacts

- Drive 8: `readyos-v0.2.5-dual-d71_1.d71`
- Drive 9: `readyos-v0.2.5-dual-d71_2.d71`
- Host-Side Boot PRG: `readyos-v0.2.5-dual-d71-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.2.5-dual-d71-boot.prg`
- Host-Side Boot PRG: `readyos-v0.2.5-dual-d71-setd71.prg`

## Included Apps

- Drive 9: `editor` - editor
- Drive 8: `readyshell` - readyshell (beta)
- Drive 9: `simplefiles` - simple files
- Drive 9: `clipmgr` - clipboard
- Drive 9: `readybasic` - ready basic (alpha)
- Drive 8: `cal26` - calendar 26
- Drive 9: `tasklist` - task list
- Drive 9: `reuviewer` - reu viewer
- Drive 9: `sysinfo` - system info
- Drive 8: `quicknotes` - quicknotes
- Drive 9: `calcplus` - calc plus
- Drive 9: `hexview` - hex viewer
- Drive 9: `simplecells` - simple cells (alpha)
- Drive 9: `game2048` - 2048 game
- Drive 8: `dizzy` - dizzy kanban
- Drive 9: `readyirc` - readyirc
- Drive 9: `ucitest` - uci tester
- `readme` is intentionally omitted from the dual-D71 variants to preserve disk space for app growth.

## VICE Setup

- Enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1571` with true drive enabled and attach `readyos-v0.2.5-dual-d71_1.d71`.
- Configure drive 9 as `1571` with true drive enabled and attach `readyos-v0.2.5-dual-d71_2.d71`.

### VICE Command Example

- Autostart target: `readyos-v0.2.5-dual-d71-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1571 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.2.5-dual-d71_1.d71 -drive9type 1571 -drive9truedrive -devicebackend9 0 +busdevice9 -9 readyos-v0.2.5-dual-d71_2.d71 -autostart readyos-v0.2.5-dual-d71-preboot.prg
```

## Boot

- This profile uses the dual-stage boot chain `PREBOOT -> SETD71 -> BOOT`.
- Both disks must already be attached before boot, and both drives must be configured as `1571`.
- `SETD71` is part of this variant and reasserts the dual-1571 setup before loading `BOOT`.
- In VICE, autostart `readyos-v0.2.5-dual-d71-preboot.prg`, or manually run `LOAD "PREBOOT",8` then `RUN`.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- Attach both disk images before boot and use `1571`-compatible drive assignments for the two-disk set.
- Boot with `LOAD "PREBOOT",8` then `RUN`; this variant then chains through `SETD71` before loading `BOOT`.

