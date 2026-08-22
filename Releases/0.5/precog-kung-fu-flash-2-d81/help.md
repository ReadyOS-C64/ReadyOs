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

- Drive 8: `editor` - editor
- Drive 8: `readyshell` - readyshell (beta)
- Drive 8: `simplefiles` - simple files
- Drive 8: `clipmgr` - clipboard
- Drive 8: `readybasic` - ready basic (beta)
- Drive 8: `cal26` - calendar 26
- Drive 8: `tasklist` - task list
- Drive 8: `reuviewer` - reu viewer
- Drive 8: `sysinfo` - system info
- Drive 8: `quicknotes` - quicknotes
- Drive 8: `calcplus` - calc plus
- Drive 8: `hexview` - hex viewer
- Drive 8: `simplecells` - simple cells (alpha)
- Drive 8: `game2048` - 2048 game
- Drive 8: `deminer` - deminer
- Drive 8: `dizzy` - dizzy kanban
- Drive 8: `readyirc` - readyirc
- Drive 8: `ucitest` - uci tester
- Drive 8: `readme` - read.me
- ReadyBASIC is accompanied by all three external `rbm.*` module packages and the complete 41-program procedure, graphics, and sound example/test set.
- ReadyBASIC's banked `rbcore`/`rbcode` resources are carried inside the `readybasic` executable rather than as separate disk files.

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
- It uses `reu_bank_skip=0`, so ReadyOS can use all 16 physical banks instead of skipping the lower bank range used by the normal test profiles.
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
- Choosing a disk SKU does not enable the experimental Ultimate DOS DMA launcher. Normal release artifacts use the portable disk loader; DMA requires an explicit `LAUNCHER_DMA_LOAD=1` source build and retains disk fallback.
- Attach the single disk image on drive `8`, then boot with `LOAD "PREBOOT",8` and `RUN`.
- This variant boots directly from `PREBOOT` into `BOOT` and does not use `SETD71`.
