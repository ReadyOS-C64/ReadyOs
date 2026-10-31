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
- Optional drive-9 swap: `readyos-v0.5-ultimate_2.d81` - ReadyBASIC demo and test programs; their modules and media assets remain on drive 8.
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Included Apps

- Drive 8: `c64os` - c64os bridge (portable; no separate app-owned REU workspace)
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
