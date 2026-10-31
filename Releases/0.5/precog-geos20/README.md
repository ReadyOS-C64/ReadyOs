# precog GEOS 2.0 experiment

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `d81`

## Why This Variant Exists

- Main full-content ReadyOS profile: one D81 holds the current app catalog, ReadyBASIC modules, and examples.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-d81.d81`
- Host-Side Boot PRG: `readyos-v0.5-d81-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-d81-boot.prg`

## Included Apps

- Drive 8: `geos20` - geos 2.0 bridge (portable; no separate app-owned REU workspace)
- Drive 8: `editor` - editor (portable; no separate app-owned REU workspace)
- Drive 8: `readyshell` - readyshell (beta) (portable; own REU overlays/state)
- Drive 8: `simplefiles` - simple files (portable; no separate app-owned REU workspace)
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
- Drive 8: `ucitest` - uci tester (Ultimate-only command lab; no separate app-owned REU workspace)
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

## VICE Setup

- Enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1581` with true drive enabled and attach `readyos-v0.5-d81.d81`.

### VICE Command Example

- Autostart target: `readyos-v0.5-d81-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1581 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.5-d81.d81 -autostart readyos-v0.5-d81-preboot.prg
```

## Boot

- This profile uses the direct boot chain `PREBOOT -> BOOT`.
- There is no `SETD71` stage for this variant.
- Attach the single disk on drive `8`, then autostart `readyos-v0.5-d81-preboot.prg` or run `LOAD "PREBOOT",8` then `RUN`.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- This profile compiles the portable launcher without Ultimate DOS DMA. Use `precog-ultimate` for the guided DMA-enabled D81, or explicitly override `LAUNCHER_DMA_LOAD=1` for development testing.
- Attach the single disk image on drive `8`, then boot with `LOAD "PREBOOT",8` and `RUN`.
- This variant boots directly from `PREBOOT` into `BOOT` and does not use `SETD71`.

## After PRECOG

PRECOG 0.5 is planned as the final PRECOG release: the series that established what is possible and clarified the vision. Next comes ReadyOS Ultimate, the main focus, installing/configuring real files and folders on Ultimate storage and using its hardware features; and ReadyOS Universal, continuing disk-image and REU workflows for VICE, THEC64 and original hardware. Universal effort will follow user interest and demand. These are future directions, not separate products shipped here. Standalone releases of many apps are also planned, including both apps with their own REU needs and apps without them. We remain committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; this does not promise a no-REU interpreter. Current ReadyOS app PRGs still require the ReadyOS runtime.

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
