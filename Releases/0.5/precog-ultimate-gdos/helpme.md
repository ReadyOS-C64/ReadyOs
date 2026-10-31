# precog ultimate GDOS bridge experiment

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `ultimate`

## Why This Variant Exists

- Full Ultimate app set with the GDOS64 bridge, compiled REU skip 71, native return installer and setup guide. Drive 8 holds ReadyOS; the examples disk is optional while the target OS uses its own storage.

## Before You Boot

ReadyOS cannot discover which Ultimate host folder/image it was booted from: the mounted C64 drive does not provide the enclosing D81 host pathname. Ultimate DOS fast loading therefore needs the D81's absolute host path in `[launcher] c64u_image_path` in `apps.cfg`, together with `dma_loading=1`. SETUP browses Ultimate storage, validates the selected D81, and safely saves those settings inside it. Run SETUP again after moving or renaming the image. The normal disk loader remains the fallback.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-ultimate_1.d81`
- Optional drive-9 swap: `readyos-v0.5-ultimate_2.d81` - ReadyBASIC examples. Leave this disk unmounted while the native OS uses device 9.
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Ultimate GDOS64 bridge: complete setup

This dedicated SKU contains the full Ultimate app set, SETUP, Ultimate Zip and
the GDOS64 bridge as its first launcher entry. It is already compiled with
**skip 71**. Stock Ultimate instead carries disabled optional bridge copies on disk 2 and uses skip zero.

### Files and requirements

Disk 1 (`readyos-v0.5-ultimate_1.d81`) holds ReadyOS and all apps, data, modules
and media. Disk 2 (`readyos-v0.5-ultimate_2.d81`) is optional ReadyBASIC examples.
The companion `gdos64-readyos-bridge.cvt` is the native return desk accessory;
`gdos64-bridge.json` records its checksum. Supply your own GDOS system disk.

The supported fixed contract is PAL, 1 MHz, enabled **16 MiB REU**, two native
C1581 drives, **GDOS64 English 1.03 build 260511**, and **4 MiB DACC**. Exclude
external C=REU RAMNative drives, cartridges, active spooling/music and running
CIA timers. Avoid SID mappings conflicting with the REU's DF00 address range.

### Install and boot

1. Boot GDOS independently and confirm its version, DACC size and disk operation.
   Import `gdos64-readyos-bridge.cvt` with a GEOS/GDOS-compatible Convert importer
   onto its system disk. Confirm **ReadyOS Bridge** is available as a native desk
   accessory. This installs our accessory; it does not patch the GDOS kernel.
2. Copy ReadyOS disk 1 to `/usb1/GD/READYOS.D81`, matching the default DMA path
   `/usb1/gd/readyos.d81`. Mount it on drive A/device 8. If using another host
   path, run `LOAD"SETUP",8,1`, then `RUN`, and select/save the actual image path.
3. Mount your prepared GDOS system disk on drive B/device 9. Keep ReadyOS disk 2
   unmounted while GDOS needs that drive. Enable the REU, fresh-reset, then boot
   ReadyOS using `LOAD"*",8` and `RUN`.
4. Open **gdos64 bridge**, then **Boot location / settings**. Defaults are device
   9, partition 0, empty directory and `gd.boot`. Adjust to your installation;
   save and verify `gdos64.cfg` on device 8. Location changes affect the next cold
   boot, not an existing suspended GDOS session.
5. Choose Start / Resume and let the native disk load visibly finish. At 1 MHz
   a black interval during loading is not proof of failure.

### Return, resume and memory ownership

In GDOS use **GEOS → Applications → DeskAccessories → ReadyOS Bridge** and
confirm. Once back in ReadyOS, you can use another app through Ctrl+B. Reopen
the bridge and choose Start / Resume to return to the parked GDOS session.
The confirmation is part of this accessory's UI. Switch only when I/O is idle.

REU banks 0–63 belong to GDOS's 4 MiB DACC; 64–70 are reserved for the bridge;
71 holds ReadyOS control; 72–255 hold ReadyOS apps/resources. Do not change the
DACC size or replace this boot image with skip-zero/skip-39 images while using
the bridge. A different allocation needs a corresponding implementation change,
not just a catalog edit.

### Recovery and validation boundary

If a version, memory or native-driver check refuses switching, correct the
configuration and boot fresh. The bridge's boot-failure test intentionally uses
an invalid boot target; it is a diagnostic, not the normal launch action.
Keep documents backed up, and do not rely on parked RAM surviving a reset.
If normal cold boot fails, verify GDOS independently before retrying interop.

Earlier VICE tests verify live geoWrite restoration, continued input and REU
isolation, and the user reports manual hardware testing. Rebuilding this SKU
checks packaging; it does not add a new physical round-trip qualification.
Ultimate-specific SETUP/DMA acceptance needs physical hardware. The accessory
source and CVT builder are checked in with the ReadyOS bridge transport.

- In the launcher, select an app and press RETURN. Ctrl+B returns to the launcher. Save documents to disk before a reset.

## Included Apps

- **gdos64 bridge** (`gdos64`, drive 8) — experimental gdos64 interop (16 MiB REU, compiled skip 71 and GDOS64 1.03 with 4 MiB DACC).
- **editor** (`editor`, drive 8) — text editor with clipboard.
- **readyshell (beta)** (`readyshell`, drive 8) — command shell poc scaffold (portable; own REU overlays/state).
- **simple files** (`simplefiles`, drive 8) — dual pane file manager and seq viewer.
- **ultimate zip** (`uzip`, drive 8) — create and extract ultimate dos zips (Ultimate-only DOS; own REU package/workspace).
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

## Validation Target

- Validate SETUP and DMA on physical Ultimate hardware.
- SETUP and DMA acceptance run on physical C64 Ultimate hardware at 1, 16, and 64 MHz.
