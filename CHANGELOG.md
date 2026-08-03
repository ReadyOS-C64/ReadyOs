# Changelog

## 0.2.5 (development)

### ReadyBASIC Graphics/Sound, C64 Ultimate DMA, Documentation

- Expanded ReadyBASIC through five graphics phases and Sound Phase 1. The
  built-in module set now covers modes, pixels and primitives, REU surfaces,
  sprites/input, polygon buffers, display lists, tiles/tilemaps, multicolor
  bitmap operations, and immediate SID voice/filter commands, accompanied by
  32 graphics demos and 6 sound demos plus regular/EasyFlash probes.
- Added the opt-in regular-launcher C64 Ultimate DOS/UCI direct-to-REU loader.
  It obtains exact PRG payload sizes, supports packed ReadyShell resources,
  reuses a mounted D81 when possible, exposes `DMA:YES`/`DMA:ON`/`DMA:NO`
  launcher state, and falls back to KERNAL/disk loading on unavailable or
  failed DMA paths. The default `LAUNCHER_DMA_LOAD=0` build remains portable.
- Added focused UCI protocol/timing probes, C64 Ultimate launcher and
  ReadyShell hardware smoke coverage, valid/empty/bad-path acceptance fixtures,
  and preserved hardware investigation evidence.
- Reconciled current public/private documentation with dynamic REU allocation,
  ReadyOS-bank schema 5, shim token lookup, the nine-overlay ReadyShell layout,
  current ReadyBASIC memory/modules/examples, the 21-app EasyFlash layout, and
  opt-in DMA behavior. Historical release and dated evidence documents remain
  preserved as snapshots.
- Combined the launcher snapshot and authoritative operating-system metadata in
  physical `Skip+1`, now named the ReadyOS bank. Physical `Skip` becomes the
  first dynamic allocation bank, `$C600-$C7FF` becomes app-private, snapshots
  expand to `$B800`, and the 512-byte shim resolves nonzero tokens through the
  ReadyOS `$B940` map instead of arithmetic placement or a resident bitmap.
- Reduced launcher warm-resume duplication to a compact `RSM1` UI record. The
  launcher reconstructs banks, drives, hotkeys, resources, loaded state, and
  sizes from the ReadyOS `LS` settings and 64-app registry. Runtime testing
  also fixed catalog scratch-pointer invalidation during first allocation and
  delayed menu-selection validation until after registry restoration.

## 0.2.4

### ReadyBASIC, REU Control Bank, Dynamic Resources, Cartridge, Apps

- Started the `0.2.4` development line from the `0.2.3` release-candidate
  baseline and continued the PRECOG direction toward a larger REU-backed
  workspace that still preserves the fixed `$1000-$C5FF` app memory contract.
- Added `readybasic`, an alpha BASIC V2 bridge app. It keeps normal BASIC text
  readable while adding ReadyBASIC commands, direct and stored-program command
  dispatch, expression-returning commands, native `PROC` / `FUNC` / `RET`
  routines, `REPEAT` / `UNTIL`, `LABEL` / `JUMP`, error-introspection helpers,
  and a visible free-BASIC-bytes header.
- Expanded ReadyBASIC into an REU-backed command-module model: built-in command
  payloads and disk-loaded `rbm.*` module packages are packed into loader-owned
  REU banks, fetched into small under-ROM slots only when needed, and verified
  with a broad headless/visual VICE suite.
- Added ReadyBASIC REU handle commands and screen/buffer proofs, including a
  128-handle directory, typed 48 KB REU heap, screen capture/restore handles,
  and heap edge-case verification without consuming BASIC workspace for large
  buffers.
- Reworked the ReadyOS REU model around logical REU bank `0` as the ReadyOS
  control bank. It now mirrors compact bank state, a 64-entry app registry,
  app/catalog metadata, dependency/source lines, rich resource records, and the
  shim token-to-physical-bank lookup page used during app switching.
- Changed launcher app snapshots from fixed reserved slots to dynamic
  allocation. Apps are assigned banks when loaded or preloaded, and the launcher
  owns unload policy so app snapshots and app-owned resource banks can be
  returned to the free pool together.
- Added app-config and manifest dependency/resource syntax for loader-owned
  overlays and modules. ReadyShell `rsovl+` and ReadyBASIC `rbcore+` resource
  sets are now described by profile/manifest data rather than being baked into
  fixed physical bank constants.
- Made ReadyShell overlay banks dynamic. The launcher/cartridge loaders assign
  resource banks, copy the overlay PRGs to their configured bank-relative
  offsets, and publish the relationship into bank `0`; ReadyShell reads the
  small assigned-bank metadata block instead of knowing physical cache banks.
- Made ReadyShell's state/scratch/value/CAT/diagnostic arena dynamic as well.
  The former fixed debug/CAT bank was retired; CAT staging, command scratch,
  value storage, and the diagnostic/probe tail now share the loader-assigned
  ReadyShell state bank at fixed relative offsets.
- Updated REU Viewer to use the bank `0` metadata so it can show dynamic app
  snapshots, loader-owned resources, ReadyShell overlay/state ownership, and
  ReadyBASIC resource ownership rather than only a flat fixed-bank map.
- Added owner-recorded runtime REU allocation as an opt-in micromodule. Apps
  such as QuickNotes and the IRC experiments can allocate `REU_APP_ALLOC` banks
  while publishing compact bank `0` ownership/tag records, allowing REU Viewer
  to show the owner and launcher unload to reclaim those banks.
- Fixed the launcher REU-state VICE probe so its first ReadyBASIC wait matches
  the actual ReadyBASIC screen instead of the preboot `READY.` line, making the
  unload/reload stability check reliable under fast boot timing.
- Added and hardened EasyFlash cartridge support for the refactored resource
  model. The cartridge SKU now preloads launcher/app/resource payloads into the
  same logical bank `0` registry shape as disk builds, has REU-required boot
  handling, and has dedicated launcher/ReadyBASIC/ReadyShell VICE automation.
- Added `system info`, a read-only machine/status app that reports system,
  ROM/video, REU, cartridge visibility, Ultimate UCI status, network details
  where available, and drive status for devices `8` through `11`.
- Added experimental networking apps and diagnostics surfaces: `readyirc`
  (Ultimate TCP path), `rirc-rrnet` (RR-Net/IP65 path), and `ucitest`
  (Ultimate command-interface lab). The IRC apps are cataloged experiments and
  are not yet complete working IRC clients.
- Added or refreshed full VICE automation around ReadyBASIC, ReadyShell,
  cartridge/EasyFlash launcher flows, dynamic REU bank `0` registry validation,
  app/resource unload behavior, and cross-app suspend/resume after the REU
  refactor.
- Tightened build and release behavior around size and memory contracts:
  launchers are built with size optimization, launcher buffers/catalog text were
  moved into the global REU control-bank path where appropriate, and app
  BSS/heap/headroom comparisons are kept as part of the refactor verification.
- Refreshed architecture, ReadyShell, ReadyBASIC, REU-control-bank, EasyFlash,
  and generated HTML documentation to describe the dynamic allocator/resource
  model instead of the older fixed-bank assumptions.

## 0.2.3

### Release Packaging, System Info, Cartridge, THEC64 Staging

- Published the `0.2.3` release artifact set under `Releases/0.2.3/`, with
  regenerated per-profile `help.md`, `helpme.md`, manifests, boot PRGs, and
  media images for the public D81, dual-D71, dual-D64, solo-D64, debug, and
  EasyFlash profiles.
- Added the `system info` app to the full-content and relevant subset catalogs,
  giving ReadyOS an in-system hardware/status view with system, Ultimate, and
  drive tabs instead of relying only on external emulator or hardware menus.
- Expanded `system info` hardware probing for C64 Ultimate / Ultimate 64 style
  setups, including UCI detection, Ultimate model details, turbo/CPU speed
  display, SoftIEC and drive information, HTTP target visibility, network
  interface data, ROM/character-set identity, video timing, REU size, cartridge
  visibility, and drive status scanning for devices `8` through `11`.
- Added the EasyFlash cartridge profile to release-all packaging and shipped
  `precog-easyflash` with the cartridge image, raw EasyFlash binary, layout
  metadata, manifest, and runtime data disk.
- Hardened and documented the EasyFlash cartridge boot path as a
  preload-and-snapshot pipeline: the cartridge loader stages launcher, app, and
  ReadyShell overlay payloads from EasyFlash into REU, restores the launcher
  snapshot from REU bank `0`, and then runs the normal RAM/REU launcher path.
- Added an early EasyFlash REU-required guard and clearer cartridge boot
  progress signaling, including border-color stage cues for cart payload reads,
  REU snapshot work, shim setup, final launcher restore, and the explicit
  REU-missing return-to-BASIC path.
- Fixed the EasyFlash app table/release artifact drift that could leave the
  cartridge SKU out of sync with the generated launcher catalog.
- Added experimental THEC64 Mini/Maxi staging folders built from the `0.2.3`
  D81 and EasyFlash artifacts, with `M6TPRM` filenames and matching CJM files
  that request C64 PAL mode plus 16 MB REU for compatibility testing outside
  the main build integration path.

## 0.2

### ReadyShell, Apps, Documentation, Release Sync

- Promoted the public release line to `0.2` and rebuilt the full profile
  matrix, including regenerated `help.md`, `helpme.md`, manifests, boot-chain
  artifacts, and release media under `Releases/0.2/`.
- Expanded ReadyShell’s documented command set to include the new file
  commands `CAT`, `PUT`, `ADD`, `DEL`, `REN`, and `COPY`, alongside the
  already-shipping `MORE`, `TOP`, and `SEL` pipeline helpers.
- Added the main ReadyShell runtime work that underpins that command set:
  REU-backed values for `LDV` / `STV`, dedicated `CAT` and `COPY` overlays,
  startup preload of all eight overlays into REU, and the rewritten
  `PUT` / `ADD` text-file write flow.
- Expanded ReadyShell’s day-to-day shell behavior with paging, projection, and
  filtering work: `MORE`, `TOP`, `SEL`, wildcard-aware `LST`, file-type
  filters, aligned drive syntax, reclaimed output space, the `rshelp` quick
  reference, and documented `RSV1` value-file format coverage.
- Updated the ReadyShell user guide and tutorial to the current prompt and
  runtime reality: prompt `>`, eight-overlay layout, current BSS/heap figures,
  and example-driven coverage for file commands, value commands, and pipeline
  usage.
- Refreshed the ReadyShell architecture docs and generated overlay inventory so
  overlays `6` and `7` are documented as `rsfops` (`DEL`, `REN`, `PUT`,
  `ADD`) and `rscat` (`CAT`), with overlay `8` documented as `rscopy`
  (`COPY`) instead of the earlier split-command model.
- Renamed and refreshed the private shim architecture report to the `0.2`
  versioned artifact, then synced the public report copy and root README links
  to that updated filename.
- Regenerated the Read.me app content from its markdown-lite source so the
  in-system documentation now shows the `0.2` release line and the expanded
  ReadyShell overlay/command set.
- Fixed stale verification expectations that still referenced the older
  `rsdrvi` / `rslst` split overlays, so the current merged ReadyShell overlay
  layout verifies cleanly again.
- Added `sidetris` alpha to the `0.2` app catalog and release media, then
  followed it with control, redraw, HUD, layout, speed, and documentation
  polish.
- Polished `game2048` with a reworked header plus cleaner pause and game-over
  flow.

## 0.1.8

### ReadyShell, Shim, REU, Documentation

- Updated ReadyShell to the current shared-cache-bank architecture: overlays
  `1` and `2` now share bank `$40` as full-window cache slots, while
  command-only overlays remain disk-loaded on demand and restore `rsvm` from
  REU after each external command call.
- Added the REU-backed external-command registry in bank `$48` metadata space,
  so external command growth no longer requires one resident wrapper per
  command and the resident heap/BSS contract stays explicit.
- Corrected current ReadyShell memory reporting to the live overlay build:
  resident `BSS` is `$877B-$8971` (`503` bytes), resident heap below the
  overlay load address is `1164` bytes, and the active overlay window is
  `$8E00-$C5FF` (`0x3800` release, `0x3B00` debug).
- Refreshed the ReadyShell overlay inventory and end-to-end architecture docs
  under `docs/`, including the current REU registry layout, command-overlay
  loading model, and per-overlay headroom figures.
- Reconciled the ReadyOS shim architecture report and supporting private docs
  with the current verifier/map reality: app headroom tables, ReadyShell
  resident/overlay numbers, and the shared ReadyShell REU-bank model now match
  the live build instead of earlier fixed-bank assumptions.
- Marked archived ReadyShell and shim investigation reports as historical where
  they preserve superseded measurements, so the live docs remain the current
  source of truth without losing the older forensic snapshots.

## 0.1.7

### System, Architecture, Shim, REU, Micromodules

- Solidified the core PRECOG runtime model: resident shim, REU-backed app
  switching, boot/preboot flow, launcher-led transitions, shared
  clipboard/resume libraries, and the fixed app runtime window at
  `$1000-$C5FF`.
- Hardened suspend/resume so switching away from an app and coming back restores
  the live working state instead of treating every return as a fresh launch.
- Added shared drive `8` / drive `9` handling so boot helpers and multi-disk
  apps no longer depend on a single hard-coded storage path.
- Expanded REU/runtime capacity when the catalog grew, including app-slot
  headroom and coordinated updates to the shim registry, REU manager, boot
  path, and syscall glue.
- Tightened input handling for turbo use by disabling default key repeat where
  needed and normalizing shared hotkeys across the launcher and app catalog, so
  typing and navigation stay controlled at higher clock speeds instead of
  repeating uncontrollably.
- Pushed ReadyShell further into an REU-first overlay model: overlays became
  REU-only, warm returns stopped reloading overlays unnecessarily, and parser
  compaction work landed while the app remained a fragile demo path.
- Added config-driven launcher/runtime settings, generated boot variant title
  text, and warm-return catalog caching so profile identity survives cleanly
  through the boot path and launcher flow.
- Extended the object-level micromodule approach so apps only link the TUI,
  REU, clipboard, resume, storage, and directory helpers they actually use,
  with shared units such as `storage_device`, `file_browser`, `dir_page`, and
  `tui_hotkeys`.

### New Apps

- `quicknotes`: REU-backed fast note capture with split-pane editing and
  return-to-work flow.
- `simple files`: dual-pane file browser with directory metadata, copy/rename
  flow, and SEQ viewing.
- `simple cells`: spreadsheet-style app with formulas, formatting, colors, and
  sheet save/load.
- `deminer`: keyboard-driven minesweeper-style puzzle built to participate in
  suspend/resume and app switching.

### App Improvements

- `launcher`: fixed app-list redraw issues, adopted shared hotkeys/defaults,
  added config-driven behavior, loaded the generated `apps.cfg` catalog cleanly
  from the selected build, showed boot variant identity, and cached the catalog
  on warm return.
- `readyshell`: updated banner/help text, documented pipeline shorthand,
  shifted overlays to REU-only handling, skipped overlay reload on warm resume,
  and continued parser-compaction work while the third-entry crash remained a
  known demo limitation.
- `simple files`: improved resume/refresh behavior, polished browser UX,
  refined directory metadata and detail view, and fixed block/free-space
  parsing in both the app and the supporting `xfilechk` harness.
- `simple cells`: added paged sheet-file browsing, fixed the formula grid
  display, and absorbed runtime/catalog changes required by the larger app set.
- `quicknotes`: improved pane UX, stopped preserving PRGs in rebuild flows, and
  started remembering cursor position per note.
- `clipmgr`: added bundle import/export and documented the bundle file format.
- `editor` and `tasklist`: added paged file-list handling; `editor` also gained
  selection mode and shipped help assets.
- `deminer`: followed its initial landing with PETSCII-correct flag rendering
  and shared hotkey normalization.
- `cal26`: remained part of the core catalog, with the task-reading regression
  documented clearly in release-facing docs.

### Build System

- Kept `run.sh`, `run.ps1`, generated version assets, disk rebuild helpers, and
  memory-map/resume checks as the center of the build flow.
- Added the generated app catalog/config loader path around `apps.cfg`, built
  from profile definitions and carried onto disk as the launcher-visible app
  configuration for the selected variant.
- Updated rebuild tooling to preserve managed user content instead of replacing
  it blindly, then tightened that further for user `SEQ` files.
- Added build-side support for newer apps and probes, including the `xfilechk`
  runner, simple-files smoke coverage, and a dedicated simple-files build
  config with reserved stack headroom.
- Expanded generated assets beyond version text to include boot variant text so
  the selected profile identity reaches boot-time messaging.
- Shifted packaging to profile-driven release builds with
  `cfg/profiles/*.ini` and `cfg/profiles/*.json`, and emitted versioned outputs
  under `Releases/<version>/<profile>/` instead of assuming a single dual-d71
  artifact layout.
- Moved editable shipped `SEQ` and `REL` payloads into `cfg/authoritative/`
  and taught the profile builder to preserve and place them consistently.
- Extended `run.sh` and `run.ps1` with profile selection, profile listing,
  skip-build flow, and build-all support.

### Verification, Harnesses, Audits

- Kept memory-map and resume-contract verification as first-class checks from
  the start, so suspend/resume correctness stayed tied to the normal build
  path.
- Verified that apps implement the warm-resume contract so returning to an app
  restores in-progress state instead of silently dropping back to a cold start.
- Expanded host smoke coverage as the app set grew, including tasklist,
  editor, and simplefiles.
- Turned `xfilechk` into a more formal verification surface with a dedicated
  runner, boot helper, parser, and headless VICE probe flow.
- Continued evolving resume-contract checks alongside ReadyShell and catalog
  growth so app-slot expansion and boot/runtime changes did not drift away from
  verification.
- Added release-asset auditing so packaged `SEQ` and `REL` payloads plus the
  generated `apps.cfg` can be checked against the authoritative source set.

### Release Variants

- Shipped the first explicit release snapshot in this repo as `v0.1.7`,
  including checked-in dual-d71 release disks under `Releases/v0-1-7/`.
- Added profile-driven release packaging for:
  - `precog-dual-d71`: default full-content profile using
    `PREBOOT -> SETD71 -> BOOT` for dual-1571 setups.
  - `precog-d81`: full-content single-disk profile using `PREBOOT -> BOOT` for
    1581/D81 setups.
  - `precog-dual-d64`: reduced-content dual-1541 profile for tighter disk
    capacity.
- Added per-profile release help and manifest files so each build ships its own
  media names, boot instructions, drive assignments, and autostart targets.
- Expanded the variant matrix further with single-disk `solo-d64` subsets:
  - `precog-solo-d64-a`
  - `precog-solo-d64-b`
  - `precog-solo-d64-c`
  - `precog-solo-d64-d`
- Used those `solo-d64` profiles to split the current catalog into smaller
  one-disk packs for environments that can only mount a single `D64` cleanly
  at a time, while keeping the same `PREBOOT -> BOOT` style flow and
  per-profile help/manifest packaging.

## 0.1.5

### Baseline

- Established the original PRECOG environment around REU-backed app switching,
  a resident shim, launcher-driven flow, shared clipboard/history, and fast
  suspend/resume between apps so work could be resumed in place rather than
  restarted from scratch.
- Shipped the earlier core app set before the later catalog expansion:
  - `editor`
  - `calcplus`
  - `hexview`
  - `clipmgr`
  - `reuviewer`
  - `tasklist`
  - `game2048`
  - `cal26`
  - `dizzy`
  - `readme`
  - `readyshell`
- Provided the earlier boot and dual-disk ReadyOS packaging path that later
  profile-based builds expanded into the broader variant matrix.
