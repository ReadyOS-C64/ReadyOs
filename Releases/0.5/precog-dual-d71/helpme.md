# precog (dual d71)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `dual-d71`

## Why This Variant Exists

- Two boot-time D71 images hold the core 1571 app set; a third optional drive-9 swap image adds lesser apps and the original 41 ReadyBASIC examples.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-dual-d71_1.d71`
- Boot-time drive 9: `readyos-v0.5-dual-d71_2.d71`
- Optional drive-9 swap: `readyos-v0.5-dual-d71_3.d71` - Optional app-data and ReadyBASIC example disk; swap it into drive 9 after ReadyOS boots.
- Host-Side Boot PRG: `readyos-v0.5-dual-d71-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-dual-d71-boot.prg`
- Host-Side Boot PRG: `readyos-v0.5-dual-d71-setd71.prg`

## Included Apps

- Drive 9: `editor` - editor (portable; no separate app-owned REU workspace)
- Drive 8: `readyshell` - readyshell (beta) (portable; own REU overlays/state)
- Drive 9: `simplefiles` - simple files (portable; no separate app-owned REU workspace)
- Drive 9: `clipmgr` - clipboard (portable; shared REU clipboard/history)
- Drive 9: `readybasic` - ready basic (beta) (portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate)
- Drive 8: `cal26` - calendar 26 (portable; no separate app-owned REU workspace)
- Drive 9: `tasklist` - task list (portable; no separate app-owned REU workspace)
- Drive 9: `reuviewer` - reu viewer (portable; inspects shared REU allocation records)
- Drive 9: `sysinfo` - system info (portable diagnostics; Ultimate details available only on Ultimate)
- Drive 8: `quicknotes` - quicknotes (portable; own REU note storage)
- Drive 9: `calcplus` - calc plus (portable; no separate app-owned REU workspace)
- Drive 9: `hexview` - hex viewer (portable; no separate app-owned REU workspace)
- Drive 9: `simplecells` - simple cells (alpha) (portable; no separate app-owned REU workspace)
- Drive 9: `game2048` - 2048 game (portable; no separate app-owned REU workspace)
- Drive 8: `dizzy` - dizzy kanban (portable; no separate app-owned REU workspace)
- Drive 9: `readyirc` - readyirc (Ultimate-only TCP; own REU scrollback)

All ReadyOS apps require the system REU for snapshots. The labels above distinguish additional app workspace from that baseline; shared clipboard operations also use REU. An Ultimate-only app can be present on portable media without becoming portable.
- The boot pair includes ReadyBASIC and the three sample `rbm.*` packages on its normal drive-9 disk; the banked `rbcore`/`rbcode` resources are carried inside `readybasic` itself.
- The optional drive-9 swap contains `app.*` manifests followed by `sidetris`, `deminer`, `ucitest`, and `readme`, then the original 41 ReadyBASIC examples.
- Those optional apps need no separate app-owned REU workspace; `ucitest` requires Ultimate services, while Sidetris, Deminer and Read.Me are portable ReadyOS apps.
- No REL-backed app is placed on the optional disk: CAL26 and Dizzy remain on the boot-time drive-8 image.

## ReadyBASIC examples and modules

This profile defines 41 BASIC example/test PRGs and 3 disk module packages: `rbm.sample1`, `rbm.sample2`, `rbm.sample3`.
Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers.
The example count above is this profile's actual configured subset. The new `rbm.media` package, tune/images and combined demos are currently packaged in regular D81 and Ultimate D81 only. They are not implied by having the ReadyBASIC runtime.
The new disk-module/resource loaders read drive 8; they do not take a device argument. See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for every example, command contracts and exact per-profile availability.

## Disk Directory Order

- Each image uses the groups it needs in this order: boot chain; configs; ordinary SEQ/USR data; main app PRGs; overlays/modules; REL data; ReadyBASIC examples.
- On bootable images, `PREBOOT` is the first directory entry, followed by any `SETD71` / `SHOWCFG`, then `BOOT` and `LAUNCHER`, so `LOAD"*",8` selects the bootstrap.
- ReadyShell overlay PRGs and ReadyBASIC `rbm.*` module packages stay in the overlay/module group even though their file types differ.
- Images that do not carry a category simply omit it without changing the relative order of the remaining categories.

## VICE Setup

- Enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1571` with true drive enabled and attach `readyos-v0.5-dual-d71_1.d71`.
- Configure drive 9 as `1571` with true drive enabled and attach `readyos-v0.5-dual-d71_2.d71`.
- After ReadyOS boots, replace the disk in drive `9` with `readyos-v0.5-dual-d71_3.d71` when you want its optional apps or ReadyBASIC examples.

### VICE Command Example

- Autostart target: `readyos-v0.5-dual-d71-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1571 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.5-dual-d71_1.d71 -drive9type 1571 -drive9truedrive -devicebackend9 0 +busdevice9 -9 readyos-v0.5-dual-d71_2.d71 -autostart readyos-v0.5-dual-d71-preboot.prg
```

## Boot

- This profile uses the dual-stage boot chain `PREBOOT -> SETD71 -> BOOT`.
- Both disks must already be attached before boot, and both drives must be configured as `1571`.
- `SETD71` is part of this variant and reasserts the dual-1571 setup before loading `BOOT`.
- In VICE, autostart `readyos-v0.5-dual-d71-preboot.prg`, or manually run `LOAD "PREBOOT",8` then `RUN`.
- Do not attach the optional app-data image until boot is complete; it replaces the normal drive-9 disk on demand.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- This profile compiles the portable launcher without Ultimate DOS DMA. Use `precog-ultimate` for the guided DMA-enabled D81, or explicitly override `LAUNCHER_DMA_LOAD=1` for development testing.
- Attach both disk images before boot and use `1571`-compatible drive assignments for the two-disk set.
- Boot with `LOAD "PREBOOT",8` then `RUN`; this variant then chains through `SETD71` before loading `BOOT`.
- After boot, swap the optional app-data image into drive `9` only when you want to use its manifests, apps, or examples.

## After PRECOG

PRECOG 0.5 is planned as the final PRECOG release: the series that established what is possible and clarified the vision. Next comes ReadyOS Ultimate, the main focus, installing/configuring real files and folders on Ultimate storage and using its hardware features; and ReadyOS Universal, continuing disk-image and REU workflows for VICE, THEC64 and original hardware. Universal effort will follow user interest and demand. These are future directions, not separate products shipped here. Standalone releases of many apps are also planned, including both apps with their own REU needs and apps without them. We remain committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; this does not promise a no-REU interpreter. Current ReadyOS app PRGs still require the ReadyOS runtime.
