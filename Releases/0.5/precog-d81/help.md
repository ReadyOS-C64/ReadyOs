# precog (d81)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `d81`

## Why This Variant Exists

- Main full-content ReadyOS profile: one D81 holds the current app catalog, ReadyBASIC modules, and examples.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-d81.d81`
- Host-Side Boot PRG: `readyos-v0.5-d81-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-d81-boot.prg`

## Start ReadyOS

- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- Mount the boot-time disks on the devices listed above. At BASIC, type `LOAD"*",8`, press RETURN, then type `RUN`.
- In the launcher, select an app and press RETURN. Ctrl+B returns to the launcher. Save documents to disk before a reset.

## Included Apps

- **editor** (`editor`, drive 8) — text editor with clipboard.
- **readyshell (beta)** (`readyshell`, drive 8) — command shell poc scaffold (portable; own REU overlays/state).
- **simple files** (`simplefiles`, drive 8) — dual pane file manager and seq viewer.
- **clipboard** (`clipmgr`, drive 8) — manage clipboard items (portable; shared REU clipboard/history).
- **ready basic (beta)** (`readybasic`, drive 8) — beta basic v2 bridge with modules (portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate).
- **calendar 26** (`cal26`, drive 8) — calendar for 2026 with appointments.
- **task list** (`tasklist`, drive 8) — outliner with notes and search.
- **reu viewer** (`reuviewer`, drive 8) — view reu memory usage (portable; inspects shared REU allocation records).
- **system info** (`sysinfo`, drive 8) — system, reu, and ultimate status (portable diagnostics; Ultimate details available only on Ultimate).
- **quicknotes** (`quicknotes`, drive 8) — reu-backed note editor (portable; own REU note storage).
- **calc plus** (`calcplus`, drive 8) — keyboard-first expression calculator.
- **hex viewer** (`hexview`, drive 8) — browse memory in hex format.
- **simple cells (alpha)** (`simplecells`, drive 8) — spreadsheet with formulas and colors.
- **2048 game** (`game2048`, drive 8) — 2048 tile puzzle.
- **deminer** (`deminer`, drive 8) — minesweeper-style puzzle.
- **dizzy kanban** (`dizzy`, drive 8) — fizzy inspired kanban app.
- **readyirc** (`readyirc`, drive 8) — ultimate tcp irc client (Ultimate-only TCP; own REU scrollback).
- **uci tester** (`ucitest`, drive 8) — ultimate command interface lab (Ultimate-only command lab; no separate app-owned REU workspace).
- **read.me** (`readme`, drive 8) — readyos, ultimate buddy, precog poc os.

All apps use REU for suspend/resume. The notes identify additional workspace and hardware requirements. Clipboard operations use shared REU storage.
- ReadyBASIC example and module coverage is listed below from this profile's source configuration.
- ReadyBASIC's banked `rbcore`/`rbcode` resources are carried inside the `readybasic` executable rather than as separate disk files.

## ReadyBASIC examples and modules

This profile defines 44 BASIC example/test PRGs and 4 disk module packages: `rbm.sample1`, `rbm.sample2`, `rbm.sample3`, `rbm.media`.
Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers.
The disk set includes RBSND07, RBGFXSNDDEMO and RBUGFXSNDDEMO. Use `LDMOD("RBM.MEDIA",M%)` for eight on-demand music/image/sprite commands. Reserve with `MEMCAP(36864)` before strings/music; load images before starting music. The vetted PSID player is PAL-only. The standard demo avoids Ultimate speed calls; the Ultimate demo requires C64U Turbo Registers and uses 1 MHz during disk I/O.
In Orbital Echoes, Space restores the cached background, Q stops/releases music, and M keeps music playing at the text prompt. After M use `MUSDROP():CLR:MEMCAP(40960)` to release it.
Load the example from the disk that holds it. Keep its required modules and media on drive 8. See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for every example, command contracts and exact per-profile availability.

## VICE Setup

- Enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1581` with true drive enabled and attach `readyos-v0.5-d81.d81`.

### VICE Command Example

- Autostart target: `readyos-v0.5-d81-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1581 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.5-d81.d81 -autostart readyos-v0.5-d81-preboot.prg
```
