# precog (dual d71)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `dual-d71`

## Why This Variant Exists

- Two boot-time D71 images hold the core 1571 app set; a third optional drive-9 swap image adds lesser apps and the original 41 ReadyBASIC examples.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-dual-d71_1.d71`
- Boot-time drive 9: `readyos-v0.5-dual-d71_2.d71`
- Optional drive-9 swap: `readyos-v0.5-dual-d71_3.d71` - Optional app-data and ReadyBASIC example disk; swap it into drive 9 after ReadyOS boots.
- Host-Side Boot PRG: `readyos-v0.5-dual-d71-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-dual-d71-boot.prg`
- Host-Side Boot PRG: `readyos-v0.5-dual-d71-setd71.prg`

## Start ReadyOS

- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- Mount the boot-time disks on the devices listed above. At BASIC, type `LOAD"*",8`, press RETURN, then type `RUN`.
- In the launcher, select an app and press RETURN. Ctrl+B returns to the launcher. Save documents to disk before a reset.

## Included Apps

- **editor** (`editor`, drive 9) — text editor with clipboard.
- **readyshell (beta)** (`readyshell`, drive 8) — command shell poc scaffold (portable; own REU overlays/state).
- **simple files** (`simplefiles`, drive 9) — dual pane file manager and seq viewer.
- **clipboard** (`clipmgr`, drive 9) — manage clipboard items (portable; shared REU clipboard/history).
- **ready basic (beta)** (`readybasic`, drive 9) — beta basic v2 bridge with modules (portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate).
- **calendar 26** (`cal26`, drive 8) — calendar for 2026 with appointments.
- **task list** (`tasklist`, drive 9) — outliner with notes and search.
- **reu viewer** (`reuviewer`, drive 9) — view reu memory usage (portable; inspects shared REU allocation records).
- **system info** (`sysinfo`, drive 9) — system, reu, and ultimate status (portable diagnostics; Ultimate details available only on Ultimate).
- **quicknotes** (`quicknotes`, drive 8) — reu-backed note editor (portable; own REU note storage).
- **calc plus** (`calcplus`, drive 9) — keyboard-first expression calculator.
- **hex viewer** (`hexview`, drive 9) — browse memory in hex format.
- **simple cells (alpha)** (`simplecells`, drive 9) — spreadsheet with formulas and colors.
- **2048 game** (`game2048`, drive 9) — 2048 tile puzzle.
- **dizzy kanban** (`dizzy`, drive 8) — fizzy inspired kanban app.
- **readyirc** (`readyirc`, drive 9) — ultimate tcp irc client (Ultimate-only TCP; own REU scrollback).

All apps use REU for suspend/resume. The notes identify additional workspace and hardware requirements. Clipboard operations use shared REU storage.
- The boot pair includes ReadyBASIC and the three sample `rbm.*` packages on its normal drive-9 disk; the banked `rbcore`/`rbcode` resources are carried inside `readybasic` itself.
- The optional drive-9 swap contains `app.*` manifests followed by `sidetris`, `deminer`, `ucitest`, and `readme`, then the original 41 ReadyBASIC examples.
- Those optional apps need no separate app-owned REU workspace; `ucitest` requires Ultimate services, while Sidetris, Deminer and Read.Me are portable ReadyOS apps.
- No REL-backed app is placed on the optional disk: CAL26 and Dizzy remain on the boot-time drive-8 image.

## ReadyBASIC examples and modules

This profile defines 41 BASIC example/test PRGs and 3 disk module packages: `rbm.sample1`, `rbm.sample2`, `rbm.sample3`.
Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers.
Use the regular D81 or an Ultimate D81 set for the Orbital Echoes demos and their media files.
Load the example from the disk that holds it. Keep its required modules and media on drive 8. See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for every example, command contracts and exact per-profile availability.

## VICE Setup

- Enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1571` with true drive enabled and attach `readyos-v0.5-dual-d71_1.d71`.
- Configure drive 9 as `1571` with true drive enabled and attach `readyos-v0.5-dual-d71_2.d71`.
- After ReadyOS boots, replace the disk in drive `9` with `readyos-v0.5-dual-d71_3.d71` when you want its optional apps or ReadyBASIC examples.

### VICE Command Example

- Autostart target: `readyos-v0.5-dual-d71-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1571 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.5-dual-d71_1.d71 -drive9type 1571 -drive9truedrive -devicebackend9 0 +busdevice9 -9 readyos-v0.5-dual-d71_2.d71 -autostart readyos-v0.5-dual-d71-preboot.prg
```
