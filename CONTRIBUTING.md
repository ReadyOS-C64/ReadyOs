# Building ReadyOS apps

1. Pick an app with similar needs. [Editor](src/apps/editor) demonstrates shared TUI and clipboard use. [Read.Me](src/apps/readme) is a small example of page navigation and returning to the launcher.
2. Build inside the app window (`$1000–$C5FF`). Use the shim and shared libraries for switching, storage, hotkeys and REU ownership. The [shim ABI](docs/ReadyOS_SHIM_ARCHITECTURE_0.5.md) describes the resident region and which state survives a switch.
3. Add the app build rule to the Makefile, its launcher entry to the intended profile catalog and its payload to the matching disk in the profile JSON. Check that the catalog device matches the disk. For an on-demand app, include an `app.*` manifest.
4. Boot the profile through `/bin/bash ./run.sh --profile precog-d81` (substitute your profile). Launch from the catalog, change some state, switch away and return. Check input, saved files and resume behavior. Check release directory order and free disk blocks before shipping.

For a ReadyBASIC module, start with the [sample package sources and loading example](src/apps/readybasic/READYBASIC_SAMPLE_MODULES.md), then follow the [command authoring checklist](src/apps/readybasic/READYBASIC_MAKING_COMMAND_GUIDE.md#checklist-for-adding-a-command).

## Build And Run

Requirements:

- `cc65` toolchain: `cl65`, `ca65`, `ld65`
- VICE tools, especially `x64sc`, `c1541`, and `petcat`
- `python3` for building and bash
- The release build is verified on macOS using Bash. Windows users should check the options supported by `run.ps1`.

Main entry points:

- `bash ./run.sh`
  rebuild the default release profile and launch ReadyOS in VICE
- `pwsh -File ./run.ps1`
  PowerShell entry point for the same workflow
- `bash ./run.sh --profile precog-d81`
  build and launch the regular D81 profile
- `bash ./run.sh --list-profiles`
  print the known release profile ids
- `bash ./run.sh --build-all`
  build every release profile and exit
- `bash ./run.sh --build-all --for-release`
  rebuild every disk and EasyFlash SKU with the plain production version and
  no rolling letter suffix in artifact filenames
- `bash ./run.sh --profile precog-d81 --build-only`
  build and package only the selected profile, then exit without starting VICE;
  this is useful for hardware-test artifacts and custom `--config` builds
- `bash ./run.sh --force-artifacts-from-d71 --build-all`
  promote non-excluded `SEQ` files and current `REL` files from the latest
  built `precog-dual-d71` images into `cfg/authoritative/`, then rebuild all
  profiles from that updated authoritative set
- `LAUNCHER_DMA_LOAD=1 bash ./run.sh --profile precog-d81`
  build the opt-in C64 Ultimate UCI/Ultimate DOS direct-to-REU launcher path;
  disk fallback remains available
- `make seed-cal26`
  seed CAL26 REL data into the latest built `precog-dual-d71` drive `8` image
- `bash ./run.sh --vice-fast`
  launch with VICE drive traps enabled, true drive emulation disabled, and the
  emulator starting in warp mode
- `bash ./run.sh --profile precog-d81 --vice-fast`
  build and launch the full D81 profile interactively in the same VICE fast-disk
  mode
- `bash ./run.sh --skipbuild`
  launch using the latest built artifacts for the selected profile
- `make audit-release-assets`
  extract packaged `SEQ`/`REL` files from every built image and compare them
  against the source of truth
- `make verify`
  run the repo verification, app host smoke checks, and the full
  `readyshell-host-tests` suite
- `make readyshell-host-tests`
  run the full host-side ReadyShell parser, VM, overlay-command, and REU tests
- `make readyshell-parse-smoke-host`
  run the fast host-side ReadyShell parser smoke checks
- `make readyshell-vm-smoke-host`
  run the host-side ReadyShell VM smoke checks, including the overlay-aware
  C64-flavored harness build and mocked file-command coverage
- `make readyshell-reu-tests-host`
  run the host-side ReadyShell REU heap/value and RSV1 serialization tests

Notes:

- `run.sh` is the preferred local workflow because it rebuilds disks, updates
  generated assets, and preserves managed disk state correctly.
- `--force-artifacts-from-d71` is opt-in. Without it, ordinary builds keep the
  existing repo-authoritative behavior.
- Disk-seeding and authoritative sync helpers should resolve their donor or
  destination `D71` from the latest built `precog-dual-d71` release manifest.
  Root-level `readyos.d71` / `readyos_2.d71` files are not the source of truth.
- `--vice-fast` only changes the VICE launch configuration. It does not affect
  build outputs or the packaged release images.
- Manual launch should match the setup in the [quick start](README.md#getting-started).
- `make verify` now runs the full `readyshell-host-tests` aggregate target,
  which includes the parser, VM/overlay, and REU ReadyShell host checks.
- Windows support is still less exercised than the Unix shell path.

## App Config Format

Each profile file in `cfg/profiles/` becomes the generated `apps.cfg` on drive
`8`. The real file format has three sections in this order: `[system]`,
`[launcher]`, and `[apps]`. Here is a minimal catalog for a two-drive setup:

```ini
[system]
variant_name=precog dual d71
variant_boot_name=precog dual d71
reu_bank_skip=0

[launcher]
load_all_to_reu=0
runappfirst=

[apps]
9:editor:editor:1
text editor with clipboard

8:quicknotes:quicknotes
reu-backed note editor

8:cal26:calendar 26
calendar for 2026 with appointments
```

How it works:

- `[system]` carries launcher-visible variant text. `variant_name` is the
  general profile name, and `variant_boot_name` is the boot/display variant
  string when that needs to differ.
- `reu_bank_skip` is a build-time boot contract, not a runtime launcher
  setting. It is compiled into the disk and EasyFlash boot/shim images as the
  number of physical 64K REU banks skipped before ReadyOS starts using REU
  space. The generated `apps.cfg` still records the profile source value for
  auditability, but the runtime launcher does not read it as configuration.
  Standard SKUs use `0`. C64OS bridge boots use `39`. GDOS64 and GEOS 2.0 bridge boots use `71`.
- `load_all_to_reu` controls whether the launcher tries to preload all app
  payloads into the REU at startup.
- `runappfirst` optionally names an app token to auto-launch after boot.
- Each app record uses `drive:program:label[:slot][:resource]` on one line, followed by a
  human-readable description on the next line.
- In `9:editor:editor:1`, `9` is the source drive, `editor` is the PRG token,
  the second `editor` is the launcher label, and the trailing `1` is the
  default hotkey slot. The parser accepts slot values `1..9`; omitting the
  slot means no default slot binding.
- A catalog entry such as `8:readyshell:readyshell (beta):2` assigns ReadyShell
  to launcher hotkey slot `2` at boot, even though ReadyShell does not currently
  support runtime rebinding from inside the shell.
- `drive` must be numeric and in the range `8..11`.
- `program` must be lowercase, must not include `.prg`, must not include a
  Commodore file-type suffix such as `,p`, and must be `12` characters or
  fewer.
- `label` is the launcher-visible app name and is limited to `31` characters.
- The description line is limited to `38` characters.
- Source text is expected to be lowercase. The build step writes the final
  `apps.cfg` as a lowercase-PETSCII `SEQ` payload, and the launcher reads that
  generated file from drive `8`.
- Blank lines and comment lines are allowed in the source profile. App records
  with a resource token ending in `+` also take a dependency line after the description. See `cfg/profiles/precog-d81.ini` for ReadyShell’s overlay list and ReadyBASIC’s `rbcore,rbcode` example.

## Architecture Snapshot

Runtime memory layout:

- app runtime window: `$1000-$C5FF` (`$B600` bytes)
- resident shim: `$C600-$C9FF` (1 KB); `$C600-$C7FF` is reserved expansion
  capacity and the public ABI remains at `$C800-$C9FF`
- ReadyShell/ReadyBASIC high-RAM workspace: `$CA00-$CFFF` when owned by the
  active app; it is outside the app snapshot and must be managed by that app
- hardware I/O region: `$D000-$DFFF`

Spatially, the core RAM contract is:

```text
$1000                                                         $C5FF $C600             $C9FF $CA00   $CFFF $D000
|<--------------- active app snapshot: $B600 bytes -------------->|<-- 1 KB shim ------>|<- app high RAM ->|<-- I/O
```

Runtime model:

- the active app owns `$1000-$C5FF`
- the shim stays resident outside that window
- app switching works by stashing and fetching the app window through the REU
- apps can return to the launcher or switch directly to another app without
  treating each transition as a fresh process launch

REU layout:

- physical `Skip`: the ReadyOS bank; launcher snapshot at `$0000-$B5FF`,
  followed by schema-v5 system state at `$B600-$FFFF`
- physical `Skip+1` and above: dynamic allocation pool for app snapshots and resources
- app snapshots: allocated on demand by the launcher, then resolved by token
  through the ReadyOS bank's explicit `$B740` mapping table
- clipboard payload banks: allocated from the dynamic pool
- app-owned runtime banks: allocated from the dynamic pool by apps that opt into
  the owned-allocation micromodule, recorded in the ReadyOS bank, and reclaimed by
  launcher unload
- ReadyShell overlay banks: loader-assigned resources described by the profile
  overlay list and published as ReadyOS-bank rich resource records
- ReadyShell state/scratch/value/CAT/diagnostic bank: one loader-assigned
  resource bank with fixed relative subranges inside that bank
- ReadyBASIC core/code banks: loader-assigned resources, not fixed `$44/$45`
  physical banks
- unavailable physical tail banks: detected by the launcher once at startup and
  marked as `REU_UNAVAIL` in the ReadyOS-bank table/header, so REU
  Viewer reports correct totals on smaller REU sizes
- remaining banks: dynamic allocation pool

The shim's logical token is not a fixed physical app-bank number. Token `0`
resolves directly to the ReadyOS bank at physical `Skip`; nonzero app tokens
resolve through its `$B740` lookup page. Loaded/resumable status is authoritative
at `$B840`, not in the retired `$C836-$C838` bitmap. This lets snapshots and
resources move while preserving the 512-byte public shim ABI inside the full
1 KB resident shim region.
The current shim has 129 verifier-checked executable-padding bytes (largest
contiguous run: 42 bytes) plus 11 ABI/data bytes that remain reserved rather
than general feature space; the exact accounting is in the current shim
architecture report.

The ReadyOS bank also contains the active launcher catalog-shape settings at
`$B9D9-$B9FF` and a 128-byte validated launcher runtime envelope at
`$FC40-$FCBF`. The latter stores only compact UI state; the launcher restores
its working registry arrays from the ReadyOS app records after every return.
The separate DMA service record at `$FCC0-$FD3F` owns the bounded image path
and validation status. Its byte layout and lifecycle are documented with the
other system records in [the shim architecture](docs/ReadyOS_SHIM_ARCHITECTURE_0.5.md#dma-service-record).

Disk layout:

- media shape depends on the selected release profile
- d81 is the default local run/test target and recommended main SKU
- d81 and dual-d64 profiles reuse the same runtime with different media maps

## Generated Assets And Public Docs

Release manifests store **repository-relative** paths with forward slashes.
Consumers that need host filenames use `readyos_profiles.load_disk_manifest`;
never copy workstation paths from a test run into a published manifest.
Before packaging, run `python3 build_support/verify_release_portability.py`.
It checks the Git inventory as well as files on disk, so an ignored file on
the build machine cannot accidentally satisfy a release dependency.

Keep release binaries, source fixtures and plan generators tracked. Checked-in
VICE plans should name shipped media; run the appropriate generator for tests
requiring a special startup app, private catalog, or diagnostic build.
Generated UCI probe disks, readbacks and instrumented assembly stay local in
`build/uci_dma_probe/` and `build/uci_timing/`. The original probe recipes
remain in `probes/`; the frozen August 2026 timing measurements are under
`docs/reports/uci-timing-2026-08/`. The dated hand-authored launcher/SETUP
reproductions in `build/` are historical references, not release inputs.

Private notes stay under the ignored `privatedocs/` tree. Public documentation
checks must pass without that tree; known local reports are checked when
present. The public EasyFlash boot-flow guide is in `docs/reports/`.

Generated build-owned assets include:

- `apps.cfg` from the selected `cfg/profiles/*.ini` source
- `src/generated/readme_pages.c` and `src/generated/readme_pages.h` from
  `src/apps/readme/readme_lite.md`
- `src/generated/build_version.h` and `src/generated/msg_version.inc` from the
  local run/build flow

Authoritative editable support payloads now live in `cfg/authoritative/`.
That includes the shipped SEQ and REL support files such as `editor help`,
`example tasks`, `myquicknotes`, `clipset1`, `clipset3`, `sheet2`,
`cal26.rel`, `cal26cfg.rel`, `dizzy.rel`, and `dizzycfg.rel`.

`cfg/authoritative/sync_inventory.json` tracks the sync-managed support set
that may be promoted from the latest built `precog-dual-d71` images when
`bash ./run.sh --force-artifacts-from-d71 ...` is used. Build-owned exclusions
such as generated `apps.cfg` and generated support payloads still come from the
normal code/build pipeline rather than from disk extraction.

Forced sync is exact for non-excluded `SEQ` files and discovered `REL` files:
new files are extracted into `cfg/authoritative/`, changed files replace the
repo copies, and previously authoritative sync-managed files that are no longer
present on the dual-D71 source images are removed so later rebuilds stop
reinserting them.

Normal profile rebuilds preserve non-managed user files from the prior
profile build while replacing build-owned artifacts in `Releases/<version>/<profile>/`.

The packaged release audit extracts those internal `SEQ` and `REL` files from
the built images and checks them byte-for-byte against `cfg/authoritative/`
plus the generated `apps.cfg`.

## Repository Layout

- `src/boot`: bootloader and BASIC boot helpers
- `src/apps`: launcher and user-facing apps
- `src/lib`: shared TUI, REU, clipboard, storage, and resume libraries
- `src/shim`: resident shim and switch/runtime support
- `cfg`: linker configs and catalog inputs
- `build_support`: local build-chain support, verification, and disk helpers
- `docs`: public format notes and related documentation
