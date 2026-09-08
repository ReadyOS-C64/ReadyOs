# ReadyOS

ReadyOS PRECOG is an experimental REU-first environment for a modern
Commodore 64 setup. Its long-term center of gravity is the new Commodore 64
Ultimate and related Ultimate-family hardware, but it is intended to support a
wide range of C64 setups that have a reasonably large REU. That includes VICE,
Ultimate-family hardware, and other practical REU-capable modern paths. PRECOG
`0.5` is the current development line.

The current `0.5` development line is still comparatively generic rather
than being explicitly tailored to the new C64 Ultimate. This cycle is expected
to push further in that Ultimate-first direction while still trying to stay
usable on other REU-capable C64 setups.

The current tree also contains a C64 Ultimate DOS DMA launcher path. It loads
disk PRGs directly into loader-assigned REU destinations. The dedicated
`precog-ultimate` D81 compiles and enables it by default and includes the
standalone `SETUP` utility; portable SKUs leave it disabled. Every enabled
build falls back to the established disk loader whenever UCI, image mounting,
file lookup, or transfer verification is unavailable. See
[`docs/ultimate_dos_dma_loading.md`](docs/ultimate_dos_dma_loading.md).

## The Concept

What if a Commodore 64 could feel ready, not just nostalgic? ReadyOS treats
waiting as the enemy. It is a keyboard-first, full-screen terminal-style
environment built around instant app switching, suspend/resume, shared
clipboard and history, and deeper links between apps. The goal is a C64
workflow where READY means responsive, reliable, and repeatable.

At a glance:

- requires an REU-backed modern C64 path; `1MB` is the minimum REU target, with `8MB` or `16MB` recommended where available
- main product direction: new Commodore 64 Ultimate and Ultimate-family workflows
- practical secondary path today: VICE with REU enabled
- still intended to support other C64 setups with a decent-sized REU
- tuned and hardware-tested from `1MHz` through `64MHz` Ultimate turbo operation
- ships multiple release SKUs so the runtime can fit `D64`, `D71`, `D81`, and EasyFlash cartridge workflows
- emphasizes "instant" app switching with apps suspended in the REU

Project links:

- GitHub: https://github.com/ReadyOS-C64/ReadyOs
- Homepage: https://readyos64.com
- Wiki: https://readyos.notion.site/

<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/2053305a-46fe-4335-8394-9cb949982788" />
<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/8bcc8e00-8b6c-4de6-a0cd-b03b740b6a11" />
<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/d403df2f-0aa8-4dd3-aa71-495bbd41a638" />
<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/943470cc-10ef-483e-8edd-770c00407cbb" />
<img width="715" height="540" alt="image (7)" src="https://github.com/user-attachments/assets/741b1a49-8d95-43c7-aaa3-e752fb5933a6" />

## Getting Started

The canonical release layout is:

- `Releases/<version>/precog-d81/`
- `Releases/<version>/precog-ultimate/`
- `Releases/<version>/precog-easyflash/`
- `Releases/<version>/precog-dual-d71/`
- `Releases/<version>/precog-kung-fu-flash-2-d81/`
- `Releases/<version>/precog-dual-d64/`
- `Releases/<version>/precog-solo-d64-readybasic/`
- `Releases/<version>/precog-solo-d64-a/`
- `Releases/<version>/precog-solo-d64-b/`
- `Releases/<version>/precog-solo-d64-c/`
- `Releases/<version>/precog-solo-d64-d/`
- `Releases/<version>/precog-solo-d64-e/`

Targets:

- VICE
- THEC64 Mini / Maxi
- Commodore 64 Ultimate-family hardware such as C64 Ultimate, Ultimate 64, or
  Ultimate Cart

So far tested in VICE and the Commodore 64 Ultimate.

Recommended baseline:

- enable the REU
- set REU size to at least `1MB`; use `8MB` or `16MB` where available
- follow the `helpme.md` inside the selected `Releases/<version>/<profile>/` directory

Boot note:

- disk SKUs boot with the documented `PREBOOT` chain for that profile
- `precog-easyflash` boots by mounting `readyos_data.d64` on drive `8`,
  attaching `readyos_easyflash.crt`, and resetting into the cartridge
- during that boot, the on-screen cartridge label reads `precog cartridge (beta)`

## Current Status

- Current development version: `0.5`
- Audited production baseline: `0.2.5`
- Local builds use the existing rolling suffix flow for artifact filenames only
- Builds release media per profile
- Full-content profiles currently include about `19` to `20` launcher-visible entries
  depending on media capacity; the catalog table below is the current app-token
  union across profiles.
- App snapshots and app-owned resource banks are allocated on demand and
  recorded in the schema-v5 ReadyOS bank at physical `Skip`; ReadyOS does
  not reserve a fixed app-slot gap or keep an allocation mirror in C64 RAM.

## What's New In 0.5

The `0.5` line starts from the audited, production-stamped `0.2.5` tree. This
initial branch/version transition deliberately changes no runtime contract;
additional release work will be recorded here as it lands.

### 0.2.5 foundation carried into 0.5

- ReadyBASIC grew through five graphics phases and Sound Phase 1. Its built-in
  modules now cover text/hires/multicolor modes, drawing primitives, REU-backed
  surfaces, sprites and input, polygon buffers, retained display lists,
  tiles/tilemaps, multicolor bitmap operations, and immediate SID voice/filter
  commands. The tree includes 32 graphics demonstrations and 6 sound
  demonstrations, with regular-disk and EasyFlash automation.
- The regular launcher gained a gated C64 Ultimate DOS/UCI direct-to-REU
  loader. It validates exact PRG sizes, handles packed ReadyShell resources,
  reuses a suitable mounted image, reports `DMA:YES` / `DMA:ON` / `DMA:NO`, and
  falls back to the portable KERNAL/disk path on any unavailable or failed DMA
  operation. Portable profiles keep `LAUNCHER_DMA_LOAD=0`; the dedicated
  Ultimate profile compiles and enables it and adds a guided standalone setup
  utility.
- UCI callers in the launcher, ReadyIRC, UCITest, and SysInfo now share the
  asynchronous state-machine discipline proven on physical Ultimate hardware:
  quiet-idle synchronization, asynchronous PUSH/ABORT handling, complete data
  and status draining, explicit DATA_ACC transitions, and bounded waits that do
  not depend on CPU-speed timing delays.
- ReadyIRC now provides a validated setup form, connect/disconnect and reconnect
  flow, channel switching, `/join` and `/names`, NAMES/JOIN/PART handling,
  editable input, help, and REU-backed scrollback. Fixture-backed C64 Ultimate
  automation covers its UCI network path.
- UCITest now has a larger structured/raw command catalog, decoded dword,
  socket, HTTP, and IEC results, remembered handles, selectable examples, and a
  dedicated user guide.
- The launcher and cartridge loaders use the physical `Skip` ReadyOS bank as
  the source of truth for the launcher snapshot, explicit app-token mapping,
  loaded/resumable status, clipboard metadata, hotkeys, resource relationships,
  app catalog metadata, and REU Viewer ownership details. ReadyShell overlays,
  ReadyShell state/scratch, and ReadyBASIC core/code banks are loader-assigned
  resources rather than fixed physical banks.
- Launcher return state is intentionally small: an `RSM1` UI record at
  ReadyOS `$FC40` keeps selection/scroll/one-shot state, while the `LS` record
  at `$B9D9` and app registry at `$BA00` reconstruct the app-bank, drive,
  hotkey, resource, loaded-state, and size arrays. Those arrays are not kept as
  a second authoritative copy in the launcher resume payload.
- Ultimate DMA configuration and validation are first-class system state in
  the ReadyOS `$FCC0` `DM` v1 service record. It owns the image path, enable/
  availability/use flags, and last error. Ctrl-B restores this state without
  repeating the startup probe or remounting the D81.
- Apps can opt into owner-recorded runtime REU allocation without growing the
  primitive allocator or shim. QuickNotes note banks and the IRC scrollback
  banks now publish compact ownership records in the ReadyOS bank, so REU
  Viewer can identify the owner and launcher unload can free those banks with
  the app.
- The 0.2.4 EasyFlash, Kung Fu Flash 2, SysInfo, dynamic-resource, ReadyBASIC,
  ReadyIRC, and UCITest foundations remain part of 0.2.5. Their build
  products, boot diagnostics, REU-required behavior, and cartridge verification
  are retained rather than being reintroduced as new 0.2.5 features.

## Release Variants

ReadyOS now ships the same runtime in `12` public media variants because the
target drive types, disk capacities, and cartridge support are different.

| Profile | Media | Why It Exists | Boot Flow | App Set |
| --- | --- | --- | --- | --- |
| `precog-d81` | one `D81` image on drive `8` | recommended main ReadyOS SKU and default build/run target | `PREBOOT -> BOOT` | 19 launcher apps plus ReadyBASIC modules and the complete example set; `sidetris` is also present through `app.sidetris` |
| `precog-ultimate` | one `D81` image on drive `8` | C64 Ultimate-first full-content SKU with DMA loading enabled and guided image-path setup | run standalone `SETUP` once, then `PREBOOT -> BOOT` | same full app/module/example set as `precog-d81`, plus non-catalog `SETUP` utility |
| `precog-easyflash` | `CRT` cartridge plus companion `D64` on drive `8` | full cartridge cold-boot path for VICE and Ultimate-family setups that can keep a disk mounted | reset into cartridge boot | full current app catalog |
| `precog-dual-d71` | two boot-time `D71` images on drives `8` and `9`, plus an optional third `D71` swapped into drive `9` | full core `1571` profile with capacity for optional apps and examples without crowding the boot pair | `PREBOOT -> SETD71 -> BOOT` | 16 core launcher apps; optional disk adds app-config versions of `sidetris`, `deminer`, `ucitest`, and `readme`, followed by all ReadyBASIC examples |
| `precog-kung-fu-flash-2-d81` | one `D81` image on drive `8` | broad Kung Fu Flash 2 disk-loading profile with `1MB` REU and no skipped REU banks | `PREBOOT -> BOOT` | same 19-app set as `precog-d81` |
| `precog-dual-d64` | two `D64` images on drives `8` and `9` | reduced profile for `1541`-compatible capacity limits | `PREBOOT -> BOOT` | curated subset of the current app catalog |
| `precog-solo-d64-readybasic` | one `D64` image on drive `8` | complete ReadyBASIC environment for `1541`-only systems | `PREBOOT -> BOOT` | ReadyOS launcher, ReadyBASIC, all three module packages, and every procedure, graphics, and sound example |
| `precog-solo-d64-a` | one `D64` image on drive `8` | standalone single-disk subset with editor, reference, and dizzy | `PREBOOT -> BOOT` | `editor`, `hexview`, `readme`, `dizzy` |
| `precog-solo-d64-b` | one `D64` image on drive `8` | standalone single-disk notes/files subset | `PREBOOT -> BOOT` | `simplefiles`, `clipmgr`, `quicknotes` |
| `precog-solo-d64-c` | one `D64` image on drive `8` | standalone single-disk planning/system subset | `PREBOOT -> BOOT` | `cal26`, `tasklist`, `reuviewer`, `sysinfo` |
| `precog-solo-d64-d` | one `D64` image on drive `8` | standalone single-disk experimental subset with simple cells, calculator, 2048, and deminer | `PREBOOT -> BOOT` | `simplecells`, `calcplus`, `game2048`, `deminer` |
| `precog-solo-d64-e` | one `D64` image on drive `8` | standalone single-disk ReadyShell-focused subset for one-disk-only environments | `PREBOOT -> BOOT` | `readyshell` and its shell-focused subset |

The cartridge SKU has one important nuance: `readyos_data.d64` is still part of
the expected runtime. The cartridge contains the EasyFlash boot code and the
preloaded payloads, while drive `8` remains the normal disk-backed place for
runtime files, help content, and app data.

Those cartridge-preloaded app snapshots are a cold-boot preload only. If one is
unloaded from REU, ReadyOS cannot load it again from the cartridge until you
restart ReadyOS.

The Ultimate D81 is deliberately separate from the portable main D81. Copy it
to Ultimate storage, mount it on drive `8`, and run
`LOAD"SETUP",8,1` followed by `RUN` before the first ReadyOS boot. SETUP checks
the REU, UCI, and Ultimate DOS; browses active storage volumes, folders, and
D81 files; mounts and validates the selected image; and updates that image's
`apps.cfg` with `dma_loading=1` and its exact host path. It uses staged
`rdyset.seq`/`rdyset.bak.seq` replacement with verification and rollback. It
addresses the final C64 `apps.cfg` SEQ entry as `apps.cfg.seq`, as required by
Ultimate DOS's mounted-image filename mapping. SETUP is
not a launcher app: it is a standalone program that reuses only focused
ReadyOS TUI micromodules and has no ReadyOS shim or overlay dependency.
Its physical acceptance matrix passed at 1, 16, and 64 MHz, including folder
navigation, invalid-path rejection, staged commit/readback, and disabled
REU/UCI guidance. VICE has no acceptance value for this Ultimate-only SKU.

The cartridge SKU also now performs an explicit early REU check. If REU is not
present, the boot loader shows a clear error, waits for a keypress, and returns
to BASIC cold start instead of trying to continue.

The Kung Fu Flash 2 D81 SKU is intentionally separate from the EasyFlash CRT
SKU. It is a disk-image path for KFF2 setups where CRT cartridge mode would
disable KFF2 REU emulation. It targets KFF2's `1MB` REU mode and starts at
physical REU bank `0` instead of skipping the lower bank range used by the
normal test profiles. If the 1MB REU fills up, launching another app may simply
do nothing instead of showing an error. Unload one or more apps to free REU
banks, then launch the app again.

The dual-D64 profile is intentionally smaller. Right now it keeps the eight-app
productivity path that fits on two `D64`s: `editor`, `readyshell`,
`simplefiles`, `clipmgr`, `cal26`, `tasklist`, `quicknotes`, and `calcplus`.

The dual-D71 profile now treats its first two images as the stable boot set.
ReadyBASIC and all `rbm.*` module packages stay on the normal drive-9 image.
Its banked `rbcore` and `rbcode` resources are contained inside the ReadyBASIC
executable, so no ReadyBASIC overlay payload is stranded on the swap disk.
The third image is an optional post-boot drive-9 swap: its `app.*` manifests
come first, followed by the four lesser apps they describe, then the complete
ReadyBASIC example collection. CAL26 and Dizzy remain on the boot-time drive-8
image because their REL files must not be separated onto the optional disk.

The ReadyBASIC-focused D64 fits on a single image, so no second examples disk is
needed. It is the most direct `1541`-compatible way to try the complete
ReadyBASIC beta environment rather than a general-purpose ReadyOS app subset.

### Disk Directory Order

Every generated D64, D71, and D81 uses one semantic directory order, adjusted
to the files that are actually present on that particular image:

1. Boot-chain PRGs: `PREBOOT` first, then any `SETD71` / `SHOWCFG`, `BOOT`, and
   `LAUNCHER`. Keeping `PREBOOT` as the first directory entry makes
   `LOAD"*",8` select the correct bootstrap.
2. Launcher/app configuration files such as `apps.cfg` and `app.*`.
3. Ordinary SEQ and USR data files.
4. Complete ReadyOS application PRGs.
5. Runtime overlays and modules, including ReadyShell overlays and `rbm.*`.
6. REL data files.
7. ReadyBASIC example/test PRGs, when that image includes them.

An image simply omits groups it does not need, while retaining the relative
group order. The EasyFlash CRT has its own cartridge-bank layout; its companion
data D64 follows the applicable data-file portion of this contract. The build
enforces the policy and `build_support/verify_release_directory_order.py`
audits the finished images.

The solo-D64 variants exist for environments that can mount only one `D64`
at a time, such as some web emulators and simplified media loaders. The split
is intentional.

### EasyFlash Boot Colors

The `precog-easyflash` cold boot now uses border colors so the long preload is
visibly doing work.

- light blue border: loader setup and general control flow
- green border: shim install and shared-state setup
- yellow border: cartridge-to-RAM copy
- orange border: RAM-to-REU stash or REU restore
- light green border: final launcher handoff
- red border: REU missing, waiting for keypress to return to BASIC

The blue background remains constant. Long yellow or orange phases are expected
and mean the machine is still preloading launcher, app, and overlay snapshots.

## App Catalog

The launcher-visible catalog lives in `cfg/profiles/*.ini` and is generated
to `apps.cfg` on drive `8` for the selected profile.

## App Config Format

Each profile file in `cfg/profiles/` becomes the generated `apps.cfg` on drive
`8`. The real file format has three sections in this order: `[system]`,
`[launcher]`, and `[apps]`. The dual-`d71` profile begins like this:

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
- `load_all_to_reu` controls whether the launcher tries to preload all app
  payloads into the REU at startup.
- `runappfirst` optionally names an app token to auto-launch after boot.
- Each app record uses `drive:program:label[:slot]` on one line, followed by a
  human-readable description on the next line. Used a lot for automated app testing.
- In `9:editor:editor:1`, `9` is the source drive, `editor` is the PRG token,
  the second `editor` is the launcher label, and the trailing `1` is the
  default hotkey slot. The parser accepts slot values `1..9`; omitting the
  fourth field means no default slot binding.
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
  `apps.cfg` as a lowercase-PETASCII `SEQ` payload, and the launcher reads that
  generated file from drive `8`.
- Blank lines and comment lines are allowed in the source profile. App records
  still follow the same alternating entry-line / description-line structure.

## Global App Hotkeys

ReadyOS supports up to nine direct app hotkey slots.

- `CTRL+1` through `CTRL+9` launch or switch to the app bound to that slot.
- `CTRL+SHIFT+1` through `CTRL+SHIFT+9` bind the current app to that slot at
  runtime in apps that use the shared hotkey handler.
- The same slot numbers can be seeded at boot from `cfg/profiles/*.ini` by
  adding the optional fourth `:slot` field in the `[apps]` section.
- Apps with their own raw keyboard input loops may not support runtime rebinding
  even though launcher-configured default slots still work.

Real C64 versus emulator key forms:

- On a real C64, launch uses `CTRL` plus the number key.
- On a real C64, bind uses `CTRL+SHIFT` plus the number key. Those are the same
  physical keys that print shifted digit symbols: `! " # $ % & ' ( )`.
- In emulators, host keymaps vary. Some map cleanly to `CTRL+SHIFT+<digit>`,
  while others deliver `CTRL+!`, `CTRL+"`, `CTRL+#`, and so on.
- ReadyOS accepts both forms in apps that support runtime rebinding, so use
  whichever chord your emulator keymap produces.

Drive placement is profile-specific; the selected `cfg/profiles/*.ini` and its
generated `manifest.json` are authoritative.

| Program | Display Name | Current Role |
| --- | --- | --- |
| `editor` | editor | Text editor with selection abilities, clipboard, find, and disk save/open |
| `quicknotes` | quicknotes | Split-pane REU-backed notes with save/open and search |
| `calcplus` | calc plus | Expression calculator with history, modes, variables, and clipboard |
| `hexview` | hex viewer | Memory browser with PETSCII and screen-code views |
| `clipmgr` | clipboard | Multi-item clipboard manager with preview and file import/export |
| `reuviewer` | reu viewer | Visual 256-bank REU map |
| `sysinfo` | system info | Read-only machine, REU, Ultimate, cartridge, and drive status; on C64 Ultimate, also shows Ultimate machine details including networking information and IP addresses |
| `tasklist` | task list | Hierarchical outliner with notes, search, and file persistence |
| `simplefiles` | simple files | Dual-pane file manager with copy, rename, delete, and SEQ previewing |
| `simplecells` | simple cells (alpha) | Single-sheet spreadsheet with formulas, formatting, and save/load |
| `game2048` | 2048 game | 2048 puzzle game with resume/app switching |
| `sidetris` | sidetris | Sideways block-drop game with suspend/resume |
| `cal26` | calendar 26 | 2026 calendar with month, week, day, upcoming, and REL-backed appointments |
| `dizzy` | dizzy kanban | Kanban board with REL-backed persistence, search, and reorder |
| `readme` | read.me | In-system ReadyOS guide viewer |
| `readyshell` | ready shell | A C64 command language with file commands and an object-pipeline programming model, including wildcard directory queries and `cat`, `put`, `add`, `del`, `ren`, and `copy` |
| `deminer` | deminer | Minesweeper-style puzzle with suspend/resume |
| `readybasic` | ready basic (beta) | Beta BASIC V2 bridge with ReadyBASIC commands, native `PROC`/`FUNC`, REU-backed command modules, graphics/sound, and suspend/resume |
| `readyirc` | readyirc | Ultimate TCP IRC client with setup, reconnect, channels, common IRC event parsing, help, editing, and REU-backed scrollback |
| `ucitest` | uci tester | Ultimate command-interface lab with decoded responses, protocol guidance, and selectable examples ([guide](docs/uci_tester.md)) |

Notes:

- ReadyShell guide: [src/apps/readyshell/README.md](src/apps/readyshell/README.md)
- ReadyShell tutorial: [src/apps/readyshell/ReadyShelltutorial.md](src/apps/readyshell/ReadyShelltutorial.md)
- ReadyShell architecture: [docs/ReadyShellArchitecture.md](docs/ReadyShellArchitecture.md)
- ReadyShell overlay inventory: [docs/readyshell_overlay_inventory.md](docs/readyshell_overlay_inventory.md)
- ReadyShell now ships nine overlays allowing language and command
  functionality that otherwise could not fit into C64 memory: `rsparser`,
  `rsvm`, `rsdrvilst`, `rsldv`, `rsstv`, `rsfops`, `rscat`, `rscopy`, and
  `rsedit`.
- ReadyShell overlays are loader-assigned resources. The profile
  `rsovl+` line records which overlay PRGs are packed into each resource bank
  and at which bank-relative offset, while the schema-v5 ReadyOS bank at
  physical `Skip` records the runtime owner/resource relationship for REU
  Viewer, unload, and diagnostics.
- Current ReadyShell command set: `PRT`, `MORE`, `TOP`, `SEL`, `GEN`, `TAP`,
  `DRVI`, `LST`, `LDV`, `STV`, `CAT`, `PUT`, `ADD`, `DEL`, `REN`, and `COPY`.
- `LST` accepts wildcard patterns, optional drive selection, and optional
  comma-separated file-type filters such as `PRG`, `SEQ`, `USR`, and `REL`.
- `LDV` and `STV` accept either embedded drive syntax like `"9:snap"` or a
  trailing drive argument like `"snap", 9`.
- `PUT` and `ADD` use direct `COMMAND <expr>, <filename>` syntax. `PUT`
  creates or replaces PETASCII text files; `ADD` appends to `SEQ` files and
  creates them when missing.
- `cal26` currently has a known regression: task reading is broken.
- `showcfg.prg` is a BASIC inspector for the generated `apps.cfg` payload on
  drive `8`.

### ReadyBASIC examples and current guides

ReadyBASIC now builds 32 graphics example PRGs (`rbgfx01_modes` through
`rbgfx32_convex_poly`) and 6 sound examples (`rbsnd01_sid_basics` through
`rbsnd06_three_voice`). They progress from mode setup and immediate drawing
through REU surfaces, sprites/input, polygons, display lists, tilemaps,
multicolor bitmap operations, and three-voice SID use. The Makefile lists the
canonical filenames; the regular and EasyFlash demo/probe suites exercise the
same generated programs.

- [current ReadyBASIC design and measured memory](src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md)
- [graphics/event commands and demo coverage](src/apps/readybasic/READYBASIC_GRAPHICS_COMMAND_DESIGN.md)
- [sound commands and examples](src/apps/readybasic/READYBASIC_SOUND_COMMAND_DESIGN.md)
- [lifecycle and REU architecture](src/apps/readybasic/READYBASIC_LIFECYCLE_AND_REU_ARCHITECTURE.md)
- [making module commands](src/apps/readybasic/READYBASIC_MAKING_COMMAND_GUIDE.md)

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

## Build And Run

Requirements:

- `cc65` toolchain: `cl65`, `ca65`, `ld65`
- VICE tools, especially `x64sc`, `c1541`, and `petcat`
- `python3` for building and bash
- only tested "on my machine" on macOS. The PowerShell build script is likely obsolete.

Main entry points:

- `bash ./run.sh`
  rebuild the default release profile and launch ReadyOS in VICE
- `pwsh -File ./run.ps1`
  PowerShell entry point for the same workflow
- `bash ./run.sh --profile precog-d81`
  build and launch a non-default profile
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
- `make`
  build the default profile release package
- `make release-all`
  build all release profiles with one version stamp
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
- Manual launch should match the setup in the Getting Started section.
- `make verify` now runs the full `readyshell-host-tests` aggregate target,
  which includes the parser, VM/overlay, and REU ReadyShell host checks.
- Windows support is still less exercised than the Unix shell path.

## Generated Assets And Public Docs

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

Public supporting docs in `docs/` currently include:

- [documentation index and freshness policy](docs/DOCUMENTATION_INDEX.md)
- [C64 Ultimate DOS DMA loading](docs/ultimate_dos_dma_loading.md)
- [UCI Tester user guide](docs/uci_tester.md)
- [current 0.5 shim and ReadyOS-bank architecture](docs/ReadyOS_SHIM_ARCHITECTURE_0.5.md)
- `docs/clipboard_bundle_seq_format.md`
- `docs/quicknotes_seq_format.md`
- `docs/simplecells_seq_format.md`
- [docs/readyshell_overlay_inventory.md](docs/readyshell_overlay_inventory.md)
- [docs/ReadyShellArchitecture.md](docs/ReadyShellArchitecture.md)

Rendered documentation exports in `docs/` currently include:

- [Documentation Index (HTML)](docs/DOCUMENTATION_INDEX.html)
- [C64 Ultimate DOS DMA Loading (HTML)](docs/ultimate_dos_dma_loading.html)
- [UCI Tester User Guide (HTML)](docs/uci_tester.html)
- [Current 0.5 SHIM Architecture Report (HTML)](docs/ReadyOS_SHIM_ARCHITECTURE_0.5.html)
- [Preserved 0.2.4 SHIM Architecture Report (HTML)](docs/ReadyOS%20SHIM%20Architecture%20Report%20%280.2%29.html)
- [ReadyShell Overlay Inventory (HTML)](docs/readyshell_overlay_inventory.html)
- [ReadyShell Architecture (HTML)](docs/ReadyShellArchitecture.html)

## Repository Layout

- `src/boot`: bootloader and BASIC boot helpers
- `src/apps`: launcher and user-facing apps
- `src/lib`: shared TUI, REU, clipboard, storage, and resume libraries
- `src/shim`: resident shim and switch/runtime support
- `cfg`: linker configs and catalog inputs
- `build_support`: local build-chain support, verification, and disk helpers
- `docs`: public format notes and related documentation

## Notes For Contributors

- Treat the launcher + shim path as the real execution model for ReadyOS.
- Respect the fixed app window and resident shim/system regions.
- Use `run.sh`, `run.ps1`, `make`, and `make verify` for normal public
  build-and-check workflows.
- look at existing apps to see the "micromodule layout"
- Apps should always be compiled with the full OS for now (so we don't have to support backwards compatibility with the ABI, and REU patterns change)

## License

ReadyOS is licensed under the MIT License. See `LICENSE` for the full text.
Copyright (c) 2026 Karl Prosser.
