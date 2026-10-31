# precog ultimate GEOS 2.0 experiment

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `ultimate`

## Why This Variant Exists

- Full Ultimate app set with the GEOS 2.0 bridge, compiled REU skip 71, native return installer and setup guide. Drive 8 holds ReadyOS; the examples disk is optional while the target OS uses its own storage.

## Before You Boot

ReadyOS cannot discover which Ultimate host folder/image it was booted from: the mounted C64 drive does not provide the enclosing D81 host pathname. Ultimate DOS fast loading therefore needs the D81's absolute host path in `[launcher] c64u_image_path` in `apps.cfg`, together with `dma_loading=1`. SETUP browses Ultimate storage, validates the selected D81, and safely saves those settings inside it. Run SETUP again after moving or renaming the image. The normal disk loader remains the fallback.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-ultimate_1.d81`
- Optional drive-9 swap: `readyos-v0.5-ultimate_2.d81` - ReadyBASIC examples and optional C64OS/GDOS bridge apps/manifests; stock skip zero disables switching.
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Ultimate GEOS 2.0 bridge: complete setup

This dedicated SKU contains the full Ultimate app set, SETUP, Ultimate Zip and
**geos 2.0 bridge** as its first launcher entry. ReadyOS is already compiled with
**skip 71**. Stock Ultimate remains a separate skip-zero SKU without enabled GEOS switching.

### Files and requirements

`readyos-v0.5-ultimate_1.d81` holds ReadyOS and all apps/data/modules/media.
`readyos-v0.5-ultimate_2.d81` is optional ReadyBASIC examples. Install the companion
`geos20-readyos-bridge.cvt` on your GEOS system disk; `geos20-bridge.json` records
its checksum. The licensed operating system and core apps such as geoWrite are
not distributed in this package; use your own installation.

Use PAL, 1 MHz, an enabled **16 MiB REU**, two native C1581 drives, and **stock
GEOS 2.0** with its **512 KiB/eight-bank** REU allocation. Extended REU drivers
and other GEOS versions are outside this bridge contract. The low 4 MiB
reservation below is conservative protection, not a GEOS expansion patch.

### Install and boot

1. Boot GEOS independently and verify its disk/input operation. Use a native
   Convert importer to install `geos20-readyos-bridge.cvt` onto the system disk,
   where it becomes the **ReadyOS Bridge** desk accessory. Use the GEOS2 CVT,
   not the GDOS CVT: their version checks and session signatures differ.
2. Copy ReadyOS disk 1 to `/usb1/GEOS20/READYOS.D81`, matching this SKU's default
   DMA path `/usb1/geos20/readyos.d81`. Mount it on drive A/device 8. For another
   host path, run `LOAD"SETUP",8,1`, then `RUN`, and select/save the actual image.
3. Mount the prepared GEOS system disk on drive B/device 9. Do not mount the
   optional ReadyOS examples disk there while GEOS needs it. Enable the REU,
   fresh-reset, then `LOAD"*",8` and `RUN` to boot ReadyOS.
4. Open **geos 2.0 bridge**, then its boot-location settings. Defaults are device
   9, partition 0, empty directory and `geoboot`. Adjust them to your installation
   and save/verify `geos20.cfg` on device 8. The CBMFiles GEOS2 system image used
   in the lab has this GEOboot loader; other installations must provide the
   compatible native loader expected by this bridge.
5. Start / Resume GEOS and allow loading to visibly finish before interacting.

### Return, resume and memory ownership

Open **ReadyOS Bridge** from the GEOS desk-accessory menu and confirm the return.
It uses native accessory save/restore to preserve a displaced application such
as geoWrite. In ReadyOS use Ctrl+B for another app, then reopen the bridge and
Start / Resume to continue GEOS. Saved location changes apply to the next cold
boot, not to an already parked session. Switch only after disk I/O finishes.

REU banks 0–63 are reserved for GEOS, including native swap/reboot storage; 64–70
are bridge reserve/snapshots; 71 is ReadyOS control; 72–255 are ReadyOS apps and
resources. Do not mix this image with stock skip-zero or C64OS skip-39 boots.
Do not install a larger GEOS REU driver and assume it respects this reservation.

### Recovery and validation boundary

If the accessory rejects the version, allocation or driver setup, use the stock
supported setup and reboot. To diagnose a failed cold boot, first prove GEOS can
boot independently. Reset discards parked contexts; save documents to disk before
resetting. No GEOS kernel patch is installed by this accessory.

The prior 13-check VICE run includes cold/warm switching, live geoWrite text,
continued typing and reciprocal REU preservation. The user reports manual
hardware testing. These rebuilt packages have not received a new physical
round-trip run; Ultimate SETUP/DMA requires physical Ultimate acceptance. All
native accessory, transport and CVT build sources are included in this repo.

- In the launcher, select an app and press RETURN. Ctrl+B returns to the launcher. Save documents to disk before a reset.

## Included Apps

- **geos 2.0 bridge** (`geos20`, drive 8) — experimental geos 2.0 interop (16 MiB REU, compiled skip 71 and stock GEOS 2.0).
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
