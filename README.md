# ReadyOS

ReadyOS lets you switch between full-screen apps on a Commodore 64 and return to
where you left off. It keeps suspended apps in a RAM Expansion Unit (REU).
PRECOG `0.5` is the current release. [Download all 19 editions](https://github.com/ReadyOS-C64/ReadyOs/releases/tag/v0.5)

## Choose your edition

1. **Regular D81** — Start here for VICE or a 1581-compatible drive. Mount the single disk on device 8. [Regular D81 guide](Releases/0.5/precog-d81/README.md)
2. **C64 Ultimate dual D81** — Mount the boot disk on device 8 and the companion on device 9. SETUP configures fast loading from the boot image. [Ultimate dual-D81 guide](Releases/0.5/precog-ultimate/README.md)
3. **EasyFlash cartridge** — Mount the companion data D64 on device 8, attach the CRT and reset. The cartridge preloads apps into the REU. [EasyFlash guide](Releases/0.5/precog-easyflash/README.md)

For 1541, 1571, Kung Fu Flash 2 and dedicated OS bridge packages, see the [full release guide](Releases/0.5/README.md).

## Getting started

1. **Enable the REU.** Allow at least 1 MiB for the portable disk editions. Use 16 MiB for the supplied EasyFlash setup. Dedicated OS bridges require 16 MiB and their matching boot image.
2. **Mount the boot disk on device 8.** On the stock Ultimate, mount disk 2 on device 9. Run `LOAD"SETUP",8,1` and `RUN` to record the boot D81's host path for DMA loading. Repeat SETUP if you move or rename that image.
3. **Boot from BASIC.** Type `LOAD"*",8`, press RETURN, then type `RUN`. For EasyFlash, mount its data D64 on device 8, attach the CRT and reset.
4. **Open an app.** Select it with the cursor keys and press RETURN. Ctrl+B returns to the launcher. F2 and F4 switch apps. Save work to disk before resetting.

| What you want to do | Where to start |
| --- | --- |
| Choose an app or check its hardware needs | [App catalog](#app-catalog) and [hardware requirements](docs/app_requirements.md) |
| Use ReadyShell commands and pipelines | [ReadyShell tutorial](src/apps/readyshell/ReadyShelltutorial.md) |
| Run a ReadyBASIC program | [Commands and examples](docs/readybasic_reference.md) |
| Build an app for ReadyOS | [Build and run](#build-and-run), [app configuration](CONTRIBUTING.md#app-config-format) and [shim interface](docs/ReadyOS_SHIM_ARCHITECTURE_0.5.md) |
| Write a ReadyBASIC module | [Sample packages](src/apps/readybasic/READYBASIC_SAMPLE_MODULES.md) and [command authoring guide](src/apps/readybasic/READYBASIC_MAKING_COMMAND_GUIDE.md) |

[Release history](CHANGELOG.md) · [Documentation index](docs/DOCUMENTATION_INDEX.md) · [Project homepage](https://readyos64.com) · [Wiki](https://readyos.notion.site/)

## Release Variants

There are 12 standard media variants, four dedicated bridge variants and three ReadyShell diagnostic builds. The table covers the standard variants. The [release guide](Releases/0.5/README.md) lists every folder.

| Profile | Media | Why It Exists | Boot Flow | App Set |
| --- | --- | --- | --- | --- |
| `precog-d81` | one `D81` image on drive `8` | recommended main ReadyOS SKU and default build/run target | `PREBOOT -> BOOT` | 19 launcher apps plus ReadyBASIC modules and the complete example set; `sidetris` is also present through `app.sidetris` |
| `precog-ultimate` | two `D81` images on drives `8` and `9` | C64 Ultimate-first SKU with DMA loading and guided image-path setup | run standalone `SETUP` once, then `PREBOOT -> BOOT` | Main apps and modules on drive 8; ReadyBASIC examples and optional C64OS/GDOS manifests on drive 9; includes Ultimate Zip and standalone `SETUP` |
| `precog-easyflash` | `CRT` cartridge plus companion `D64` on drive `8` | full cartridge cold-boot path for VICE and Ultimate-family setups that can keep a disk mounted | reset into cartridge boot | 20 preloaded apps and nine ReadyShell overlays |
| `precog-dual-d71` | two boot-time `D71` images on drives `8` and `9`, plus an optional third `D71` swapped into drive `9` | full core `1571` profile with capacity for optional apps and examples without crowding the boot pair | `PREBOOT -> SETD71 -> BOOT` | 16 core launcher apps; optional disk adds app-config versions of `sidetris`, `deminer`, `ucitest`, and `readme`, followed by the original 41 ReadyBASIC examples |
| `precog-kung-fu-flash-2-d81` | one `D81` image on drive `8` | broad Kung Fu Flash 2 disk-loading profile with `1MB` REU and no skipped REU banks | `PREBOOT -> BOOT` | same 19-app set as `precog-d81` |
| `precog-dual-d64` | two `D64` images on drives `8` and `9` | reduced profile for `1541`-compatible capacity limits | `PREBOOT -> BOOT` | curated subset of the current app catalog |
| `precog-solo-d64-readybasic` | one `D64` image on drive `8` | focused ReadyBASIC environment for `1541`-only systems | `PREBOOT -> BOOT` | ReadyOS launcher, ReadyBASIC, three sample modules and the original 41 examples; new media resources are currently D81-only |
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

The Ultimate D81 pair is separate from the portable main D81. Copy both images
to Ultimate storage. Mount the first on drive `8` and the examples disk on
drive `9`, then run
`LOAD"SETUP",8,1` followed by `RUN` before the first ReadyOS boot. Configure
the first image's path. Load ReadyBASIC demos with `LOAD"RBUGFXSNDDEMO",9`;
modules and media still load from drive 8. SETUP checks
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
physical REU bank `0`. If the 1MB REU fills up, launching another app may simply
do nothing instead of showing an error. Unload one or more apps to free REU
banks, then launch the app again.

The dual-D64 profile is intentionally smaller. Right now it keeps the eight-app
productivity path that fits on two `D64`s: `editor`, `readyshell`,
`simplefiles`, `clipmgr`, `cal26`, `tasklist`, `quicknotes`, and `calcplus`.

The dual-D71 profile now treats its first two images as the stable boot set.
ReadyBASIC and the three sample `rbm.*` packages stay on the normal drive-9 image.
Its banked `rbcore` and `rbcode` resources are contained inside the ReadyBASIC
executable, so no ReadyBASIC overlay payload is stranded on the swap disk.
The third image is an optional post-boot drive-9 swap: its `app.*` manifests
come first, followed by the four lesser apps they describe, then the complete
original 41-program ReadyBASIC example collection. The newer media package and
examples are not yet included in this profile. CAL26 and Dizzy remain on the boot-time drive-8
image because their REL files must not be separated onto the optional disk.

The ReadyBASIC-focused D64's original example collection fits on a single image,
so no second examples disk is needed for that set. Choose regular D81 or Ultimate
D81 for the new media module, music assets, and combined graphics/music demos.

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

Drive placement is profile-specific; the selected `cfg/profiles/*.ini` and its
generated `manifest.json` are authoritative.

**First things to try**

- **Edit a text file.** In Editor, F7 opens a file and F5 saves it. F6 starts a selection, F1 copies and F3 pastes. F8 opens help. The editor holds up to 100 lines of 79 characters. Longer input is truncated on load.
- **Plan a day.** In Calendar 26, select a date and press RETURN. In day view, N creates an appointment and RETURN edits it. F7 cycles views.
- **Make an outline.** In Task List, RETURN creates an item. Use ] and [ to indent or outdent, Space to mark an item done and N to add a note. F5 saves the list and F7 opens a saved list. In Dizzy, F1 creates a card, RETURN edits it, and + or − moves it between columns.
- **Browse a disk.** In Simple Files, F3 changes the drive, F5 refreshes its directory and RETURN previews a SEQ file.

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
| `uzip` | ultimate zip | Ultimate-only ZIP browser, extraction and creation using Ultimate DOS and REU resources; packaged in the Ultimate SKU |

Hardware requirements are separate from inclusion in a disk image. ReadyIRC,
UCI Tester and Ultimate Zip require Ultimate services for their principal work;
SETUP is also Ultimate-only. A portable image may contain Ultimate-only apps.
All apps running inside ReadyOS require its REU-backed runtime. Beyond that:

| App work beyond the ReadyOS snapshot | Applications |
| --- | --- |
| Uses its own REU storage or code resources | QuickNotes (notes), ReadyIRC (scrollback), ReadyShell (overlays and state), ReadyBASIC (core/code and optional buffers/surfaces), Ultimate Zip (package/workspace) |
| Depends on shared REU data as its purpose | Clipboard Manager (shared clipboard/history), REU Viewer (system allocation records) |
| No separate app-owned REU workspace found | Editor, Calc Plus, Hex Viewer, Task List, Simple Files, Simple Cells, 2048, Sidetris, Calendar 26, Dizzy, Deminer, Read.Me, UCI Tester |
| Hardware diagnostics | System Info detects REU and Ultimate capabilities; absence of Ultimate does not make the whole app Ultimate-only |

Clipboard operations in otherwise RAM-based apps still use the shared REU
clipboard. This classification describes today's implementation, not a claim
that existing PRGs can already run standalone. See the
[app requirements audit](docs/app_requirements.md) for scope and evidence.

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
  creates or replaces PETSCII text files; `ADD` appends to `SEQ` files and
  creates them when missing.
- `cal26` currently has a known regression: task reading is broken.
- `showcfg.prg` is a BASIC inspector for the generated `apps.cfg` payload on
  drive `8`.

### ReadyBASIC examples and current guides

ReadyBASIC retains 32 graphics examples and the original six immediate-SID
examples, plus three language/procedure tests. Regular and Ultimate D81 profiles
now add `RBSND07` (background PSID music), `RBGFXSNDDEMO` (standard Orbital
Echoes), and `RBUGFXSNDDEMO` (Ultimate speed-controlled Orbital Echoes): 44
packaged examples. The older `rbsnd08_neon.bas` remains as historical source,
making 45 BASIC source examples in the tree. They progress from mode setup and immediate drawing
through REU surfaces, sprites/input, polygons, display lists, tilemaps,
multicolor bitmap operations, and three-voice SID use. The Makefile lists the
canonical filenames; the regular and EasyFlash demo/probe suites exercise the
baseline generated programs; the newer media demos have dedicated tests.

Start with the [complete examples and command guide](docs/readybasic_reference.md)
([formatted HTML](docs/readybasic_reference.html)). The built-in graphics/sound
commands need no module load. Use `LDMOD("RBM.MEDIA",M%)` for its eight media
commands; reserve memory with `MEMCAP(36864)` before creating strings or loading
the certified PAL tune. `USPEED` and `UMHZ` are built-in Ultimate-specific
commands, not part of the media package. The standard demo avoids them.

- [current ReadyBASIC design and measured memory](src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md)
- [graphics/event commands and demo coverage](src/apps/readybasic/READYBASIC_GRAPHICS_COMMAND_DESIGN.md)
- [sound commands and examples](src/apps/readybasic/READYBASIC_SOUND_COMMAND_DESIGN.md)
- [lifecycle and REU architecture](src/apps/readybasic/READYBASIC_LIFECYCLE_AND_REU_ARCHITECTURE.md)
- [making module commands](src/apps/readybasic/READYBASIC_MAKING_COMMAND_GUIDE.md)

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

## Build and run

Install cc65, VICE (including c1541 and petcat), Python 3 and Bash. From the checkout, run `/bin/bash ./run.sh --profile precog-d81` to build and boot ReadyOS. For all release images with the plain version stamp, run `bash ./run.sh --build-all --for-release`.

## Notes for contributors

[Build and package an app](CONTRIBUTING.md) covers disk placement, catalog syntax, RAM boundaries, shared libraries and verification. For ReadyBASIC extensions, use the [sample module guide](src/apps/readybasic/READYBASIC_SAMPLE_MODULES.md).

## Optional C64OS and GDOS bridges

The regular single-D81 SKU excludes these bridges. Stock Ultimate disk 2 carries
optional C64OS/GDOS apps and device-9 manifests (`app.c64os`, `app.gdos64`) near
the top of its directory. Load them with the launcher's F5 browser on drive 9.
The stock boot still uses skip zero, so these apps explain the requirements and
refuse switching. Native CAR/CVT installers are companion files beside the
Ultimate images. See [optional Ultimate bridge instructions](docs/ultimate_optional_bridges.md).

Enabled switching uses the full dedicated `precog-ultimate-c64os` (skip39),
`precog-ultimate-gdos` (skip71), or `precog-ultimate-geos20` (skip71) variants.
Each includes its native return component, all Ultimate apps, and a complete
README/help setup guide. The [C64OS guide](docs/c64os/ultimate.md) covers the
1.09-only reversible bank cap, boot location and recovery; the
[GDOS guide](docs/gdos64/ultimate.md) and [GEOS 2.0 guide](docs/geos20/ultimate.md)
cover their distinct fixed allocations and native accessories. All require the
supported 16 MiB REU layout. See [bridge source/binary locations](experiments/BRIDGES.md).

<details><summary>ReadyOS screenshots</summary>

<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/2053305a-46fe-4335-8394-9cb949982788" />
<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/8bcc8e00-8b6c-4de6-a0cd-b03b740b6a11" />
<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/d403df2f-0aa8-4dd3-aa71-495bbd41a638" />
<img width="715" height="540" alt="image" src="https://github.com/user-attachments/assets/943470cc-10ef-483e-8edd-770c00407cbb" />
<img width="715" height="540" alt="image (7)" src="https://github.com/user-attachments/assets/741b1a49-8d95-43c7-aaa3-e752fb5933a6" />

</details>

## License

Copyright (c) 2026 Karl Prosser. [MIT License](LICENSE)
