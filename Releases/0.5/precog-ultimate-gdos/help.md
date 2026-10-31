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
- Optional drive-9 swap: `readyos-v0.5-ultimate_2.d81` - ReadyBASIC demo and test programs; their modules and media assets remain on drive 8.
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Included Apps

- Drive 8: `gdos64` - gdos64 bridge (portable; no separate app-owned REU workspace)
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

## Ultimate GDOS64 bridge: complete setup

This dedicated SKU contains the full Ultimate app set, SETUP, Ultimate Zip and
the GDOS64 bridge as its first launcher entry. It is already compiled with
**skip 71**. The stock Ultimate SKU has no bridge and uses skip zero.

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

## Experimental GDOS64 Bridge

The regular D81 includes GDOS64 Bridge in its main launcher catalog. The stock Ultimate
and other normal SKUs do not include it; `precog-ultimate-gdos` is a separate experiment. This is an experimental plain-C64 bridge for
the dedicated VICE lab, not a general hardware release.

The normal D81 skips **0 banks**, so switching is disabled. The experimental
configuration requires **16 MiB REU**, **ReadyOS skip 71**, and **GDOS64 English
1.03 build 260511 with 4 MiB DACC** on native C1581 drives. Do not use an external
C=REU RAMNative drive with this layout. Banks 0–63 belong to GDOS; 64–70 are
reserved for the bridge; bank 71 and above belong to ReadyOS. The current
prototype does not support other allocations, cartridges, active spooling,
music or running CIA timers.

Boot location defaults to device 9, current directory, `gd.boot`. Choose
**Boot location / settings** in the bridge to edit it; **S** saves and verifies
`gdos64.cfg` on device 8. Changes apply to the next cold boot. The return component
is the native **ReadyOS Bridge** desk accessory, built from `experiments/gdos64/accessory.s` and supplied as the companion
`gdos64-readyos-bridge.cvt` beside this release image. Install it with a native
GEOS Convert importer on your GDOS system disk. It does not
patch the GDOS kernel. Select it from GDOS's desk accessory menu to return.

Source checkout instructions, automation and memory details are in
`experiments/gdos64/README.md`. The private experiment disk is distinct from this
stock skip-0 release disk. Never enable switching merely by changing a runtime
catalog: ReadyOS's skip count is compiled into its boot image.
