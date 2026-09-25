# precog (kung fu flash 2 d81)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `kung-fu-flash-2-d81`

## Why This Variant Exists

- Full-content single-D81 profile tuned for Kung Fu Flash 2 disk loading with a 1MB REU and no skipped REU banks.

## Compatibility Warning

- **This SKU may not currently work on Kung Fu Flash 2 hardware. The latest version has not yet been tested on KFF2; treat the current artifacts as unverified until hardware testing is completed.**

## Artifacts

- Boot-time drive 8: `readyos-v0.5-kung-fu-flash-2-d81.d81`
- Host-Side Boot PRG: `readyos-v0.5-kung-fu-flash-2-d81-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-kung-fu-flash-2-d81-boot.prg`

## Included Apps

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

- Enable REU with at least `1MB`; this SKU targets KFF2's 1MB REU mode.
- The host-side boot PRGs are convenience autostart files. The disk copy of `PREBOOT` is still the normal disk-side bootstrap.
- Configure drive 8 as `1581` and attach `readyos-v0.5-kung-fu-flash-2-d81.d81`.

### VICE Command Example

- Autostart target: `readyos-v0.5-kung-fu-flash-2-d81-preboot.prg`

```sh
x64sc -reu -reusize 1024 -drive8type 1581 -devicebackend8 0 +busdevice8 -8 readyos-v0.5-kung-fu-flash-2-d81.d81 -autostart readyos-v0.5-kung-fu-flash-2-d81-preboot.prg
```

## 1MB REU Budget

- This SKU is intentionally limited to `1MB` REU, which is `16` physical `64KB` REU banks.
- Like every current SKU, it uses `reu_bank_skip=0`: bank 0 holds ReadyOS, and banks 1 through 15 are available for app snapshots and resources.
- Fresh launcher state uses `1` bank by default: bank `0` is the combined ReadyOS bank, holding both the launcher snapshot and schema-v5 system state. That leaves `15` banks for suspended apps and app resources.
- Each suspended app normally costs `1` additional bank.
- ReadyShell costs `5` additional banks when loaded: `1` app snapshot bank, `3` overlay cache banks, and `1` state/scratch bank. With only ReadyShell loaded, expect about `6/16` banks in use including the ReadyOS bank.
- ReadyBasic costs `3` additional banks when loaded: `1` app snapshot bank plus `2` ReadyBasic core/code resource banks. With only ReadyBasic loaded, expect about `4/16` banks in use including the ReadyOS bank.
- ReadyShell and ReadyBasic loaded at the same time can use about `9/16` banks including the ReadyOS bank, before any other suspended apps are counted.
- When REU Viewer or the launcher shows the 1MB REU getting close to full, unload suspended apps before launching more. Unloading frees their app snapshot and resource banks.
- If all REU banks are full, launching another app may simply do nothing instead of showing an error. Unload one or more apps to make room, then launch the app again.

## Boot

- This profile uses the direct boot chain `PREBOOT -> BOOT`.
- There is no `SETD71` stage for this variant.
- Attach the single disk on drive `8`, then autostart `readyos-v0.5-kung-fu-flash-2-d81-preboot.prg` or run `LOAD "PREBOOT",8` then `RUN`.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable the REU with at least `1MB`; this SKU targets KFF2's 1MB REU mode.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- This profile compiles the portable launcher without Ultimate DOS DMA. Use `precog-ultimate` for the guided DMA-enabled D81, or explicitly override `LAUNCHER_DMA_LOAD=1` for development testing.
- Attach the single disk image on drive `8`, then boot with `LOAD "PREBOOT",8` and `RUN`.
- This variant boots directly from `PREBOOT` into `BOOT` and does not use `SETD71`.

## After PRECOG

PRECOG 0.5 is planned as the final PRECOG release: the series that established what is possible and clarified the vision. Next comes ReadyOS Ultimate, the main focus, installing/configuring real files and folders on Ultimate storage and using its hardware features; and ReadyOS Universal, continuing disk-image and REU workflows for VICE, THEC64 and original hardware. Universal effort will follow user interest and demand. These are future directions, not separate products shipped here. Standalone releases of many apps are also planned, including both apps with their own REU needs and apps without them. We remain committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; this does not promise a no-REU interpreter. Current ReadyOS app PRGs still require the ReadyOS runtime.
