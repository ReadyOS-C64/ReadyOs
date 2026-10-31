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
- Optional drive-9 swap: `readyos-v0.5-ultimate_2.d81` - ReadyBASIC demo and test programs; their modules and media assets remain on drive 8.
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Included Apps

- Drive 8: `geos20` - geos 2.0 bridge (portable; no separate app-owned REU workspace)
- Drive 8: `editor` - editor (portable; no separate app-owned REU workspace)
- Drive 8: `readyshell` - readyshell (beta) (portable; own REU overlays/state)
- Drive 8: `simplefiles` - simple files (portable; no separate app-owned REU workspace)
- Drive 8: `uzip` - ultimate zip (Ultimate-only DOS; own REU package/workspace)
- Drive 8: `clipmgr` - clipboard (portable; shared REU clipboard/history)
- Drive 8: `readybasic` - ready basic (beta) (portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate)
- Drive 8: `cal26` - calendar 26 (portable; no separate app-owned REU workspace)
- Drive 8: `tasklist` - task list (portable; no separate app-owned REU workspace)
- Drive 8: `reuviewer` - reu viewer (portable; inspects shared REU allocation records)
- Drive 8: `sysinfo` - system info (portable diagnostics; Ultimate details available only on Ultimate)
- Drive 8: `quicknotes` - quicknotes (portable; own REU note storage)
- Drive 8: `calcplus` - calc plus (portable; no separate app-owned REU workspace)
- Drive 8: `hexview` - hex viewer (portable; no separate app-owned REU workspace)
- Drive 8: `simplecells` - simple cells (alpha) (portable; no separate app-owned REU workspace)
- Drive 8: `game2048` - 2048 game (portable; no separate app-owned REU workspace)
- Drive 8: `deminer` - deminer (portable; no separate app-owned REU workspace)
- Drive 8: `dizzy` - dizzy kanban (portable; no separate app-owned REU workspace)
- Drive 8: `readyirc` - readyirc (Ultimate-only TCP; own REU scrollback)
- Drive 8: `readme` - read.me (portable; no separate app-owned REU workspace)

All ReadyOS apps require the system REU for snapshots. The labels above distinguish additional app workspace from that baseline; shared clipboard operations also use REU. An Ultimate-only app can be present on portable media without becoming portable.
- ReadyBASIC example and module coverage is listed below from this profile's source configuration.
- ReadyBASIC's banked `rbcore`/`rbcode` resources are carried inside the `readybasic` executable rather than as separate disk files.

## ReadyBASIC examples and modules

This profile defines 44 BASIC example/test PRGs and 4 disk module packages: `rbm.sample1`, `rbm.sample2`, `rbm.sample3`, `rbm.media`.
Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers.
The new set includes RBSND07, RBGFXSNDDEMO and RBUGFXSNDDEMO. Use `LDMOD("RBM.MEDIA",M%)` for eight on-demand music/image/sprite commands. Reserve with `MEMCAP(36864)` before strings/music; load images before starting music. The vetted PSID player is PAL-only. The standard demo avoids Ultimate speed calls; the Ultimate demo requires C64U Turbo Registers and uses 1 MHz during disk I/O.
In Orbital Echoes, Space restores the cached background, Q stops/releases music, and M keeps music playing at the text prompt. After M use `MUSDROP():CLR:MEMCAP(40960)` to release it.
The new disk-module/resource loaders read drive 8; they do not take a device argument. See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for every example, command contracts and exact per-profile availability.

## Disk Directory Order

- Each image uses the groups it needs in this order: boot chain; configs; ordinary SEQ/USR data; main app PRGs; overlays/modules; REL data; ReadyBASIC examples.
- On bootable images, `PREBOOT` is the first directory entry, followed by any `SETD71` / `SHOWCFG`, then `BOOT` and `LAUNCHER`, so `LOAD"*",8` selects the bootstrap.
- ReadyShell overlay PRGs and ReadyBASIC `rbm.*` module packages stay in the overlay/module group even though their file types differ.
- Images that do not carry a category simply omit it without changing the relative order of the remaining categories.

## Validation Target

- This is an Ultimate-only SKU. VICE does not provide the Ultimate UCI/Ultimate DOS services SETUP requires, so VICE testing has no acceptance value for this variant.
- SETUP and DMA acceptance run on physical C64 Ultimate hardware at 1, 16, and 64 MHz.

## Boot

- This profile uses the direct boot chain `PREBOOT -> BOOT`.
- There is no `SETD71` stage for this variant.
- Attach the single disk on drive `8`, then autostart `readyos-v0.5-ultimate-preboot.prg` or run `LOAD "PREBOOT",8` then `RUN`.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable a `16MB` REU. This bridge's fixed memory layout requires the full 256 banks.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- ReadyOS cannot discover which Ultimate host folder/image it was booted from: the mounted C64 drive does not provide the enclosing D81 host pathname. Ultimate DOS fast loading therefore needs the D81's absolute host path in `[launcher] c64u_image_path` in `apps.cfg`, together with `dma_loading=1`. SETUP browses Ultimate storage, validates the selected D81, and safely saves those settings inside it. Run SETUP again after moving or renaming the image. The normal disk loader remains the fallback.
- This SKU compiles the regular launcher with Ultimate DOS DMA support and ships `apps.cfg` with `dma_loading=1`; disk fallback remains active whenever DMA is unavailable.
- Mount the first D81 on drive `8`. Leave the examples D81 unmounted while the native OS uses drive `9`; swap it in only after ending that bridge session. All ReadyOS apps and runtime assets remain on drive 8.
- Before the first ReadyOS boot, run `LOAD"SETUP",8,1`, then `RUN`, and configure the first (drive-8) D81 path.
- In ReadyBASIC, load examples from drive 9, for example `LOAD"RBUGFXSNDDEMO",9` then `RUN`. LDMOD and media commands continue reading their files from drive 8.
- SETUP is a standalone utility built from focused ReadyOS TUI micromodules. It checks REU, UCI, and Ultimate DOS, browses active Ultimate storage volumes/folders for D81 images, mounts the selection, validates its `apps.cfg`, and stages the exact host path into that image.
- SETUP uses F1/F3 for pages, cursor keys for selection, RETURN to enter/select, LEFT or DELETE to go to the parent, F5 to retest prerequisites, and F7 to apply a saved path or enter an absolute D81 path when none is available.
- After SETUP reports `CONFIGURED`, exit with RUN/STOP and reset or boot `PREBOOT`. Do not rename or move the D81 afterward without running SETUP again.
- Attach the single disk image on drive `8`, then boot with `LOAD "PREBOOT",8` and `RUN`.
- This variant boots directly from `PREBOOT` into `BOOT` and does not use `SETD71`.

## After PRECOG

PRECOG 0.5 is planned as the final PRECOG release: the series that established what is possible and clarified the vision. Next comes ReadyOS Ultimate, the main focus, installing/configuring real files and folders on Ultimate storage and using its hardware features; and ReadyOS Universal, continuing disk-image and REU workflows for VICE, THEC64 and original hardware. Universal effort will follow user interest and demand. These are future directions, not separate products shipped here. Standalone releases of many apps are also planned, including both apps with their own REU needs and apps without them. We remain committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; this does not promise a no-REU interpreter. Current ReadyOS app PRGs still require the ReadyOS runtime.

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

## Experimental GEOS 2.0 Bridge

The `precog-geos20` and `precog-ultimate-geos20` profiles are dedicated bridge
experiments. They use 16 MiB REU, ReadyOS skip 71, native C1581 drives, and stock
GEOS 2.0 with its eight-bank/512 KiB allocation. They are separate from stock
skip-zero releases. Banks 0–63 are reserved for GEOS, 64–70 for the bridge,
71 for ReadyOS control and 72–255 for ReadyOS apps. Other GEOS REU expansions
are not supported by this fixed layout.

The ReadyOS app is `geos20`. Boot location defaults to `GEOboot` on device 9;
its settings screen edits and saves `geos20.cfg` on device 8. Changes affect the
next cold boot. Install the companion `geos20-readyos-bridge.cvt` on your GEOS
system disk with a native Convert importer; it becomes the ReadyOS Bridge desk
accessory. Confirm its dialog to return to ReadyOS, then Start / Resume to return
to the suspended GEOS application. Native accessory sources are checked in at
`experiments/geos20/accessory.s`. GEOS kernel binaries are not patched.

The regular D81-derived GEOS experiment also carries the optional C64OS app and
its CAR, inherited from the base profile. Its compiled skip 71 does not satisfy
the C64OS bridge's skip-39 requirement, so C64OS switching is disabled there.
Use the matching compiled layout for the bridge you intend to run; one fixed
experimental image is not a universal multi-OS allocator.
