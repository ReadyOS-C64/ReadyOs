# precog ultimate (dual d81)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `ultimate`

## Why This Variant Exists

- Full Ultimate main app set on drive 8; ReadyBASIC examples and optional C64OS/GDOS apps/manifests on drive 9. Stock skip zero disables OS switching; dedicated bridge variants provide matched reservations.

## Before You Boot

ReadyOS cannot discover which Ultimate host folder/image it was booted from: the mounted C64 drive does not provide the enclosing D81 host pathname. Ultimate DOS fast loading therefore needs the D81's absolute host path in `[launcher] c64u_image_path` in `apps.cfg`, together with `dma_loading=1`. SETUP browses Ultimate storage, validates the selected D81, and safely saves those settings inside it. Run SETUP again after moving or renaming the image. The normal disk loader remains the fallback.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-ultimate_1.d81`
- Boot-time drive 9: `readyos-v0.5-ultimate_2.d81`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Included Apps

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
- Attach all listed disks before boot, then autostart `readyos-v0.5-ultimate-preboot.prg` or run `LOAD "PREBOOT",8` then `RUN`.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- ReadyOS cannot discover which Ultimate host folder/image it was booted from: the mounted C64 drive does not provide the enclosing D81 host pathname. Ultimate DOS fast loading therefore needs the D81's absolute host path in `[launcher] c64u_image_path` in `apps.cfg`, together with `dma_loading=1`. SETUP browses Ultimate storage, validates the selected D81, and safely saves those settings inside it. Run SETUP again after moving or renaming the image. The normal disk loader remains the fallback.
- This SKU compiles the regular launcher with Ultimate DOS DMA support and ships `apps.cfg` with `dma_loading=1`; disk fallback remains active whenever DMA is unavailable.
- Mount the first D81 on drive `8` and the examples/optional-apps D81 on drive `9`. Main-catalog apps, games, REL data, RBM packages and media assets remain on drive 8; optional bridge manifests load their PRGs from drive 9.
- Before the first ReadyOS boot, run `LOAD"SETUP",8,1`, then `RUN`, and configure the first (drive-8) D81 path.
- In ReadyBASIC, load examples from drive 9, for example `LOAD"RBUGFXSNDDEMO",9` then `RUN`. LDMOD and media commands continue reading their files from drive 8.
- SETUP is a standalone utility built from focused ReadyOS TUI micromodules. It checks REU, UCI, and Ultimate DOS, browses active Ultimate storage volumes/folders for D81 images, mounts the selection, validates its `apps.cfg`, and stages the exact host path into that image.
- SETUP uses F1/F3 for pages, cursor keys for selection, RETURN to enter/select, LEFT or DELETE to go to the parent, F5 to retest prerequisites, and F7 to apply a saved path or enter an absolute D81 path when none is available.
- After SETUP reports `CONFIGURED`, exit with RUN/STOP and reset or boot `PREBOOT`. Do not rename or move the D81 afterward without running SETUP again.
- Attach all listed disk images to their matching drives before boot, then run `LOAD "PREBOOT",8` and `RUN`.
- This variant boots directly from `PREBOOT` into `BOOT` and does not use `SETD71`.

## After PRECOG

PRECOG 0.5 is planned as the final PRECOG release: the series that established what is possible and clarified the vision. Next comes ReadyOS Ultimate, the main focus, installing/configuring real files and folders on Ultimate storage and using its hardware features; and ReadyOS Universal, continuing disk-image and REU workflows for VICE, THEC64 and original hardware. Universal effort will follow user interest and demand. These are future directions, not separate products shipped here. Standalone releases of many apps are also planned, including both apps with their own REU needs and apps without them. We remain committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; this does not promise a no-REU interpreter. Current ReadyOS app PRGs still require the ReadyOS runtime.

## Optional bridges on Ultimate disk 2

The stock Ultimate's second D81 contains ReadyBASIC examples plus optional
`c64os` and `gdos64` bridge PRGs. Their `app.c64os` and `app.gdos64` SEQ manifests
are placed near the top of that disk's directory, using the standard build
ordering rules. Each manifest selects its app on **device 9**. The regular
single-D81 SKU contains neither bridge nor their manifests/installer companions.

Keep stock Ultimate disk 1 on device 8 and disk 2 on device 9. In the launcher,
use **F5 (Browse)**, browse device 9, and select `app.c64os` or `app.gdos64`.
The added entry loads the corresponding PRG from device 9. Location settings
still save on device 8 as `c64os.cfg` / `gdos64.cfg`.

**This stock SKU still compiles skip zero. Both apps therefore display their
setup-required guard and refuse OS switching.** Loading an optional manifest
does not repartition the REU. Use the dedicated `precog-ultimate-c64os` (skip39)
or `precog-ultimate-gdos` (skip71) image for enabled switching, with its matching
native component and OS requirements. These are different fixed layouts, not a
single setting that enables both bridges together.

The stock release folder also supplies `bridge.car` (C64OS Setup/return utility)
and `gdos64-readyos-bridge.cvt` (GDOS return accessory), with checksum metadata.
They are companion host files, outside the D81 images. Follow the C64OS/GDOS
requirements and installation sections below before using the dedicated images.

Native OS storage may itself need device 9. The dedicated bridge SKUs keep their
active bridge on disk 1 and their examples disk optional; they do not inherit
these extra drive-9 bridge copies/manifests. Do not replace a native OS disk
with the examples disk while its suspended session still depends on that drive.

# Experimental C64OS bridge

The regular single-D81 SKU excludes this bridge. Stock Ultimate supplies the
optional `c64os` PRG and `app.c64os` manifest on its second D81/device 9, with
`bridge.car` beside the images. Use the launcher's **F5 (Browse)** on device 9
to select that manifest. Stock Ultimate still uses skip zero and the app refuses
switching until booted under a matching compiled reservation.

The dedicated `precog-ultimate-c64os` SKU puts the bridge directly in the main
catalog on device 8 and already compiles skip 39. Installing an app or editing
`apps.cfg` alone does not partition the REU.

## Requirements and matched settings

- Your own licensed **C64OS 1.09**, on a device with the required directory support.
- A **16 MB REU** for the current bridge implementation.
- ReadyOS built with **`reu_bank_skip=39`**, followed by a fresh machine boot.
- Bridge Setup applied to that C64OS installation, selecting **8–32 banks**.
- The ReadyOS return utility from the same `bridge.car` package.

Each bank is 64 KB (65,536 bytes); 16 banks are 1 MB (1,048,576 bytes).
The selector shows the exact binary MB value and bank count.

| C64OS cap | C64OS physical banks | Unused gap | Bridge snapshot/reserve banks | ReadyOS skip |
| --- | --- | --- | --- | --- |
| 8 banks / 0.5 MB | 0–7 | 8–31 | 32–38 | **39** |
| 16 banks / 1 MB | 0–15 | 16–31 | 32–38 | **39** |
| 24 banks / 1.5 MB | 0–23 | 24–31 | 32–38 | **39** |
| 32 banks / 2 MB | 0–31 | None | 32–38 | **39** |

All integer caps from 8 through 32 follow the same rule: C64OS gets banks
0 through cap-minus-one; banks from the cap through 31 remain unused.
**Do not set ReadyOS skip equal to the C64OS cap.** The existing bridge uses
fixed physical snapshot banks; reducing the cap does not relocate them.
ReadyOS owns bank 39 for control state and banks 40–255 for apps/resources.

## Install and configure C64OS first

1. Boot C64OS independently, before starting ReadyOS or creating bridge contexts.
2. Copy the release folder's `bridge.car` to storage C64OS can read. Open it in
   File Manager and use the normal Installer. This one CAR installs the
   **Bridge Setup** application and **ReadyOS** return utility. Installer exits
   C64OS; boot C64OS again.
3. Open **Bridge Setup** from Applications. It requires the system's
   `settings/version.t` to say exactly `1.09` and checks the complete REU library
   against the known original or this tool's patch. Unknown files are refused.
4. Select the bank limit using the native cycle control (Shift-click reverses).
   Choose **Apply / Update**, then **Confirm**. The tool keeps the original
   `library/bridge.rec`, writes the selected six-byte override into `rec.lib.o`,
   and reads the result back. No licensed library is distributed in the CAR.
5. **Restart C64OS after applying or updating.** The running allocator is not
   resized. Then configure and boot the matching ReadyOS build below.

The CAR does not replace your Utilities menu. Open the installed ReadyOS utility
through C64OS's Utilities browser, or add it using the native Menu Editor.
Only switch after disk operations have finished.

## Configure ReadyOS

For the dedicated Ultimate SKU, follow its included Ultimate C64OS setup guide;
the memory reservation is already compiled in. Open the bridge's location screen
before the first cold boot and select your C64OS installation. On an Ultimate
without IDE64, the unsaved app default is device 8, which normally holds ReadyOS;
use device 9 if that is where you mounted C64OS, then save.

For a custom regular D81, `reu_bank_skip` is compiled into the boot/shim image. Editing the disk's
`apps.cfg` alone cannot change it. Copy `experiments/c64os/apps.ini` (which includes the active bridge catalog
entry and skip 39), then build through the normal runner:

```sh
/bin/bash ./run.sh --profile precog-d81 --config /absolute/path/bridge-readyos.ini --reu-size 16384 --build-only
```

Boot that custom D81 fresh and select its main-catalog C64OS bridge entry. The bridge checks the actual
booted skip and REU capacity, and explains the required settings if unsuitable.
The normal release D81 remains skip zero. Our experiment's separate
`experiments/c64os/apps.ini` supplies skip 39 for automated bridge testing.

Inside the bridge choose **Boot location / settings (L)**. Edit the device
(8–30), partition (0 leaves it unchanged), directory (empty leaves it unchanged),
and boot filename. Save writes and verifies `c64os.cfg` on **device 8**. This is
separate from `app.c64os`, which describes the launcher entry.

Defaults are device 12, partition 1, `//os`, `booter` when IDE64 is detected;
otherwise device 8, current partition/directory, `booter`. The target may be a
different device from the device-8 configuration file. A changed location applies
to the next cold C64OS load; a saved context resumes its original installation.
Prepare the target using Bridge Setup before attempting the first bridge load.

## Undo and recovery

Choose **Undo patch**, then **Confirm**, to restore the original 1.09 detector.
**The next C64OS boot must be independent of ReadyOS.** Its original probe can
access all physical REU banks and would damage a suspended ReadyOS context.
To use the bridge again, apply a safe cap in a standalone C64OS session first.

Writes are verified and a detected failure attempts to restore the prior image.
If power is lost during a library write, restore `library/bridge.rec` as
`library/rec.lib.o` before booting C64OS. Keep the original installation media.

## Build provenance and limits

Bridge Setup source and the CAR builder are checked in at
`experiments/c64os/setup`. The ReadyOS return utility uses this repository's
shared bridge core. The applicable D81/Ultimate packager includes the CAR as a companion file in the release
folder. The archive contains only our app, utility and help/metadata.

The switching core retains its existing ABI and fixed layout. IDE64 state is
saved only when a supported IDE64 is detected; ordinary IEC skips that path.
VICE IDE64 tests do not certify physical CMD/SD2IEC hardware. Custom interrupt,
SID playback and active-transfer suspension remain outside the supported scope.

## Experimental GDOS64 Bridge

The regular single-D81 SKU excludes this bridge. Stock Ultimate carries its
optional `gdos64` PRG and `app.gdos64` manifest on disk 2/device 9; use F5 Browse
on that device to load the entry. Stock Ultimate skips zero banks and therefore
refuses switching. `precog-ultimate-gdos` is the dedicated enabled variant.

The supported experimental configuration requires **16 MiB REU**, **ReadyOS skip 71**, and **GDOS64 English
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
stock Ultimate skip-zero release disks. Never enable switching merely by changing a runtime
catalog: ReadyOS's skip count is compiled into its boot image.
