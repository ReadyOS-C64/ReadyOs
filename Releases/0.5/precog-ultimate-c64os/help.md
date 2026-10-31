# precog ultimate C64OS bridge

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `ultimate`

## Why This Variant Exists

- Full Ultimate app set with the C64OS bridge, compiled REU skip 39, native return installer and setup guide. Drive 8 holds ReadyOS; the examples disk is optional while the target OS uses its own storage.

## Before You Boot

ReadyOS cannot discover which Ultimate host folder/image it was booted from: the mounted C64 drive does not provide the enclosing D81 host pathname. Ultimate DOS fast loading therefore needs the D81's absolute host path in `[launcher] c64u_image_path` in `apps.cfg`, together with `dma_loading=1`. SETUP browses Ultimate storage, validates the selected D81, and safely saves those settings inside it. Run SETUP again after moving or renaming the image. The normal disk loader remains the fallback.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-ultimate_1.d81`
- Optional drive-9 swap: `readyos-v0.5-ultimate_2.d81` - ReadyBASIC examples and optional C64OS/GDOS bridge apps/manifests; stock skip zero disables switching.
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Ultimate C64OS bridge: complete setup

This is the full Ultimate application set plus the C64OS bridge, standalone
Ultimate SETUP and Ultimate Zip. The bridge is the first main-catalog entry.
The stock Ultimate SKU remains separate. ReadyOS is already compiled with
**skip 39**; no custom build is needed for this reservation.

### Files in this release folder

- `readyos-v0.5-ultimate_1.d81`: ReadyOS boot, all apps, data, modules and media.
- `readyos-v0.5-ultimate_2.d81`: optional ReadyBASIC examples disk.
- `bridge.car`: native C64OS Bridge Setup and ReadyOS return utility installer.
- `bridge.json`: CAR checksum and return-utility provenance.
- Host-side PREBOOT/BOOT PRGs and these README/help documents.

Supply your own licensed **C64OS 1.09 installation**; its operating-system image
is not included. Use PAL, 1 MHz and an enabled **16 MiB REU** for the current
bridge contract. C64OS must boot and operate independently on its selected
storage before attempting interop. Physical CMD/SD2IEC paths have not received
the same validation as the IDE64 test environment.

### Install and prepare C64OS

1. Start C64OS independently. Import `bridge.car` with its File Manager/Installer,
   then boot C64OS again after Installer exits.
2. Open **Bridge Setup** in Applications. It verifies version 1.09 and recognizes
   only the original or supported patched REU library.
3. Select **8–32 banks** (0.5–2 MiB); **32 banks / 2 MiB** is the largest supported
   allocation. Apply / Update, confirm, and restart C64OS. All offered values
   require the same ReadyOS skip 39. Keep its library backup for recovery.
4. Confirm the installed **ReadyOS** utility is accessible through Utilities,
   or add it using C64OS Menu Editor. The CAR does not replace the global menu.

### Mount and boot ReadyOS

1. Copy disk 1 to `/usb1/C64OS/READYOS.D81` on the Ultimate. That matches this
   SKU's default DMA path `/usb1/c64os/readyos.d81`. If using another path/name,
   mount that image on drive A/device 8 and run `LOAD"SETUP",8,1` then `RUN`;
   use SETUP to select the actual image and save its DMA configuration.
2. Keep ReadyOS disk 1 on device 8. For a native 1581 C64OS installation, mount
   its own prepared system image on drive B/device 9. Leave ReadyOS's examples
   disk unmounted while device 9 is needed by C64OS. IDE64 installations can
   remain on their configured IDE64 device instead.
3. Fresh-reset the machine, then `LOAD"*",8` and `RUN`. Enable the 16 MiB REU
   before boot; do not substitute the stock skip-zero Ultimate boot image.
4. Select **c64os bridge**, then **Boot location / settings (L)**. Set the actual
   OS device, partition, directory and boot filename; for a device-9 system disk,
   start with device 9, partition 0, empty directory and `booter`, adjusting them
   to your installation. Save and verify the settings on device 8 as `c64os.cfg`.
   With IDE64 detected, unsaved defaults are device 12, partition 1, `//os`,
   `booter`; without IDE64 they are device 8/current directory/`booter`, so the
   location must be configured before a device-9 cold boot.
5. Choose **Start / resume C64OS (C)** and allow disk activity to finish.

### Switch and resume

In C64OS, open the installed **ReadyOS** utility through Utilities. It returns
to the suspended ReadyOS bridge. Use Ctrl+B to visit the launcher and another
ReadyOS app, then reopen the bridge and choose Start / Resume to continue the
same C64OS session. Location edits affect the next cold boot, not a parked
session. The RAM and missing-boot tests in the bridge are diagnostic actions.

Physical REU banks 0–31 are reserved for C64OS (only the selected cap is used),
32–38 for bridge snapshots/reserve, 39 for ReadyOS control, and 40–255 for ReadyOS
apps/resources. GDOS/GEOS bridge SKUs use a different layout; boot each SKU fresh
rather than exchanging boot images while a context is parked.

### Recovery and validation boundary

To remove the cap, run Bridge Setup's Undo and confirm. The next C64OS boot must
be independent of ReadyOS. After undo the original detector may touch the whole
REU. If a library write is interrupted, restore `library/bridge.rec` as
`library/rec.lib.o` before rebooting C64OS. Reapply a supported cap before using
this bridge again. Keep valuable files backed up and switch only when disk I/O
has completed; custom IRQ/music and active-transfer suspension are unsupported.

This SKU's build and packaging checks do not constitute a new physical hardware
round-trip test. Existing bridge tests and the user's manual tests cover prior
configurations. Ultimate SETUP/DMA acceptance requires real Ultimate hardware;
VICE cannot validate the Ultimate command interface. The native setup source,
CAR builder and switching transport are all included in this repository.

- In the launcher, select an app and press RETURN. Ctrl+B returns to the launcher. Save documents to disk before a reset.

## Included Apps

- **c64os bridge** (`c64os`, drive 8) — experimental c64os interop (16 MiB REU, compiled skip 39 and a prepared C64OS 1.09 installation).
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
