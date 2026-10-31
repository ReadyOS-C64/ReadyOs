# precog (kung fu flash 2 d81)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `kung-fu-flash-2-d81`

## Why This Variant Exists

- Full-content single-D81 profile tuned for Kung Fu Flash 2 disk loading with a 1MB REU and no skipped REU banks.

## Compatibility Warning

- **This SKU may not currently work on Kung Fu Flash 2 hardware. The latest version has not yet been tested on KFF2; treat the current artifacts as unverified until hardware testing is completed.**

## Artifacts

- Boot-time drive 8: `readyos-v0.5-kung-fu-flash-2-d81.d81`
- Host-Side Boot PRG: `readyos-v0.5-kung-fu-flash-2-d81-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-kung-fu-flash-2-d81-boot.prg`

## Start ReadyOS

- Enable the REU with at least `1MB`; this SKU targets KFF2's 1MB REU mode.
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

This profile defines 41 BASIC example/test PRGs and 3 disk module packages: `rbm.sample1`, `rbm.sample2`, `rbm.sample3`.
Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers.
Use the regular D81 or an Ultimate D81 set for the Orbital Echoes demos and their media files.
Load the example from the disk that holds it. Keep its required modules and media on drive 8. See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for every example, command contracts and exact per-profile availability.

## VICE Setup

- Enable REU with at least `1MB`; this SKU targets KFF2's 1MB REU mode.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1581` and attach `readyos-v0.5-kung-fu-flash-2-d81.d81`.

### VICE Command Example

- Autostart target: `readyos-v0.5-kung-fu-flash-2-d81-preboot.prg`

```sh
x64sc -reu -reusize 1024 -drive8type 1581 -devicebackend8 0 +busdevice8 -8 readyos-v0.5-kung-fu-flash-2-d81.d81 -autostart readyos-v0.5-kung-fu-flash-2-d81-preboot.prg
```

## 1MB REU Budget

- This SKU is intentionally limited to `1MB` REU, which is `16` physical `64KB` REU banks.
- KFF2 uses `reu_bank_skip=0`: bank 0 holds ReadyOS, and banks 1 through 15 are available for app snapshots and resources.
- Fresh launcher state uses `1` bank by default: bank `0` is the combined ReadyOS bank, holding both the launcher snapshot and schema-v5 system state. That leaves `15` banks for suspended apps and app resources.
- Each suspended app normally costs `1` additional bank.
- ReadyShell costs `5` additional banks when loaded: `1` app snapshot bank, `3` overlay cache banks, and `1` state/scratch bank. With only ReadyShell loaded, expect about `6/16` banks in use including the ReadyOS bank.
- ReadyBasic costs `3` additional banks when loaded: `1` app snapshot bank plus `2` ReadyBasic core/code resource banks. With only ReadyBasic loaded, expect about `4/16` banks in use including the ReadyOS bank.
- ReadyShell and ReadyBasic loaded at the same time can use about `9/16` banks including the ReadyOS bank, before any other suspended apps are counted.
- When REU Viewer or the launcher shows the 1MB REU getting close to full, unload suspended apps before launching more. Unloading frees their app snapshot and resource banks.
- If all REU banks are full, launching another app may simply do nothing instead of showing an error. Unload one or more apps to make room, then launch the app again.
