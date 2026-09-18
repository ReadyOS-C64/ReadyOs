# precog (solo d64 subset b)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `solo-d64-b`

## Why This Variant Exists

- Single-D64 productivity subset centered on quicknotes, clipboard, calculator, and files.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-solo-d64-b.d64`
- Host-Side Boot PRG: `readyos-v0.5-solo-d64-b-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-solo-d64-b-boot.prg`

## Included Apps

- Drive 8: `simplefiles` - simple files (portable; no separate app-owned REU workspace)
- Drive 8: `clipmgr` - clipboard (portable; shared REU clipboard/history)
- Drive 8: `quicknotes` - quicknotes (portable; own REU note storage)

All ReadyOS apps require the system REU for snapshots. The labels above distinguish additional app workspace from that baseline; shared clipboard operations also use REU. An Ultimate-only app can be present on portable media without becoming portable.

## ReadyBASIC examples and modules

This profile defines 1 BASIC example/test PRGs and 0 disk module packages: .
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
- Configure drive 8 as `1541` with true drive enabled and attach `readyos-v0.5-solo-d64-b.d64`.

### VICE Command Example

- Autostart target: `readyos-v0.5-solo-d64-b-preboot.prg`

```sh
x64sc -reu -reusize 16384 -drive8type 1541 -drive8truedrive -devicebackend8 0 +busdevice8 -8 readyos-v0.5-solo-d64-b.d64 -autostart readyos-v0.5-solo-d64-b-preboot.prg
```

## Boot

- This profile uses the direct boot chain `PREBOOT -> BOOT`.
- There is no `SETD71` stage for this variant.
- Attach the single disk on drive `8`, then autostart `readyos-v0.5-solo-d64-b-preboot.prg` or run `LOAD "PREBOOT",8` then `RUN`.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- This profile compiles the portable launcher without Ultimate DOS DMA. Use `precog-ultimate` for the guided DMA-enabled D81, or explicitly override `LAUNCHER_DMA_LOAD=1` for development testing.
- Attach the single disk image on drive `8`, then boot with `LOAD "PREBOOT",8` and `RUN`.
- This variant boots directly from `PREBOOT` into `BOOT` and does not use `SETD71`.

## After PRECOG

PRECOG 0.5 is planned as the final PRECOG release: the series that established what is possible and clarified the vision. Next comes ReadyOS Ultimate, the main focus, installing/configuring real files and folders on Ultimate storage and using its hardware features; and ReadyOS Universal, continuing disk-image and REU workflows for VICE, THEC64 and original hardware. Universal effort will follow user interest and demand. These are future directions, not separate products shipped here. Standalone releases of many apps are also planned, including both apps with their own REU needs and apps without them. We remain committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; this does not promise a no-REU interpreter. Current ReadyOS app PRGs still require the ReadyOS runtime.
