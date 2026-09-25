## ReadyOS PRECOG 0.5

The goal is **fast app switching** for Commodore 64 Ultimate use,
tuned and hardware-tested from 1MHz through 64MHz while remaining
C64-class practical.

Current snapshot:
- Current development version: **0.5**
- Audited production baseline: **0.2.5**
- Runtime target: clean behavior from stock-speed C64 use up through
  Ultimate turbo workflows
- Media layout: profile-based release media under `Releases/<version>/<profile>/` with external `helpme.md`
  instructions for the selected build
- Development artifacts use the plain **0.5** version stamp.
  Local development builds may still include an internal trailing letter.
- New since 0.1.5: **quicknotes**, **simple files**, **simple cells**,
  **sidetris**, **deminer**, **system info**, **ReadyBASIC**,
  **ReadyIRC**, **UCI Tester**, and **EasyFlash**
- New in 0.5: SETUP, Ultimate Zip, persistent DMA state,
  ReadyBASIC media modules and music/graphics demos.
- New in 0.2.5: ReadyBASIC graphics/sound, resilient asynchronous UCI,
  a fuller ReadyIRC client, and opt-in launcher DMA loading
- Ultimate SKU enables direct-to-REU loading with disk fallback.
  Other SKUs use portable disk loading.

---

## Ready Philosophy

What if a Commodore 64 could feel ready, not just nostalgic?

ReadyOS treats waiting as the enemy: responsive C64 workflows,
immediate interaction, and a "READY." that means ready.

*State continuity* is a core value, not a nice-to-have.

---

## Quick Pitch

REU-first C64 workflows: instant app switching, suspend/resume,
shared clipboard/history, and deep links between apps.

Commodore 64 Ultimate comes first, with keyboard-first operation
from 1MHz to 64MHz. Classic C64 + REU and emulators remain supported.
Other Ultimate-family devices are welcome, but not primary targets.

---

## Core Attributes

An operating environment, not just a launcher.

How it feels:
- Alt-Tab-style switching, not save-quit-reload.
- Suspend/resume keeps your place.
- Shared conventions make navigation predictable.

Design direction (unchanged goals):
- **Speed first** for app transitions and keyboard navigation.
- *Reliability first* for memory boundaries and transport paths.
- Strong verification before feature expansion.

Key attributes:
- REU-backed fast app switching.
- Small, evolving shim and shared libraries.
- Shared clipboard/history and deep links to app views/contexts.
- Stable UI/input at slow and fast CPU speeds.
- One runtime, with D81, dual-D71, general D64 and ReadyBASIC D64 profiles.
- Optional UltimateBuddy launch/control integration.

---

## UltimateBuddy Vision

UltimateBuddy is an optional C64 Ultimate companion for automation,
integration and network bridging; ReadyOS remains the core environment.

Concrete examples:
- REST-only actions beyond UCI: settings, disk mount/load.
- Faster REU app/data transfers and computer-to-C64U file sync.
- WAP-style PETSCII proxy for keyboard-driven modern web apps.
- Freeze C64 apps into recoverable ReadyOS quick-switch states.
- Remote compilation: C64 IDE requests, external build results.
- C64-friendly access to tweets, search and other internet data.

Contract:
- ReadyOS works without UltimateBuddy; integration is optional.

---

## PRECOG to Future Direction

PRECOG proves the base mechanics:
- Fast, reliable app transitions.
- Stable state restore across context switches.
- A practical daily workflow on C64-class systems.

0.5 is the last PRECOG. Next: ReadyOS Ultimate focuses on real storage
installation/configuration. Universal keeps disk images/REU for VICE,
THEC64 and original hardware, with effort based on demand. Standalone
apps are planned with and without REU needs; ReadyBASIC will be independent
of Ultimate and ReadyOS, not necessarily REU-free.

UltimateBuddy can add speed and convenience without becoming a dependency.

Future-facing themes:
- Better cross-app information flow.
- Stronger deep-link journeys between tasks.
- Richer UX while staying keyboard-first and fast.

---

## App Catalog

Apps and media profiles are listed below.

## Global App Hotkeys

ReadyOS supports nine direct app hotkey slots.

- `CTRL+1` through `CTRL+9` launch or switch to the app bound to that slot.
- `CTRL+SHIFT+1` through `CTRL+SHIFT+9` bind the current app to that slot at
  runtime in apps that use the shared hotkey handler.
- The same slot numbers can be assigned at boot in `apps.cfg` by using the
  optional fourth field in an app record such as
  `8:readyshell:readyshell (beta):2`.
- Some apps with custom raw-input loops do not currently support runtime
  rebinding, but launcher-configured default slots still work for them.

Real C64 versus emulator keys:

- On a real C64, launch is `CTRL` plus the number key.
- On a real C64, bind is `CTRL+SHIFT` plus the number key, using the same keys
  that also print `! " # $ % & ' ( )`.
- In emulators, host keyboard maps vary: you may need `CTRL+SHIFT+<digit>` or
  the shifted-symbol form such as `CTRL+!` or `CTRL+"`.
- ReadyOS accepts both forms in apps that support runtime rebinding.

Recent additions:
- Quicknotes for fast REU-backed notes.
- Simple Files for dual-pane file browsing and SEQ viewing.
- Simple Cells for spreadsheet-style work.
- Deminer for a fast puzzle/game app that still respects ReadyOS
  suspend/resume flow.
- System Info for checking machine, REU, Ultimate, and drive status.
- ReadyBASIC for BASIC V2 plus modular graphics, REU, and sound commands.
- ReadyIRC for Ultimate TCP IRC with channels, help, editing, reconnect,
  and REU-backed scrollback.
- UCI Tester for raw and decoded Ultimate command workflows.

Disk layout:
- The exact disk image names depend on the selected build profile.
- Drive assignments are documented in the external profile `helpme.md`.
- The main release variants are:
  - `precog-d81`: recommended main SKU; 19 catalog apps plus Sidetris
    on demand, four modules and 44 BASIC examples on one `1581` disk
  - `precog-ultimate`: two D81s plus standalone `SETUP`; all apps and REL
    data on drive 8, BASIC examples on drive 9; DMA with disk fallback
  - `precog-dual-d71`: 16 core apps on two boot-time `1571` disks;
    a third optional drive-9 swap adds four apps and the original 41 examples
  - `precog-kung-fu-flash-2-d81`: the D81 set adapted for 1MB-REU KFF2
  - `precog-dual-d64`: reduced 8-app set on two `1541` disks
  - `precog-solo-d64-readybasic`: ReadyOS, ReadyBASIC, three sample modules,
    and the original 41 examples; new media examples need D81
  - `precog-easyflash`: EasyFlash `CRT` plus companion `D64` on drive `8`

Disk directory order is deliberate. Bootable images put `PREBOOT` first,
followed by any boot helpers, `BOOT`, and `LAUNCHER`, so `LOAD"*",8` works.
The remaining groups are configs, ordinary SEQ/USR data, app PRGs,
overlays/modules, REL data, and finally any ReadyBASIC examples. Each image
omits groups it does not contain. The EasyFlash CRT uses a separate bank layout.

Ultimate D81 first run:
- C64U cannot tell ReadyOS its boot D81's host path. Fast loading needs
  `c64u_image_path` and `dma_loading=1` in `apps.cfg`; SETUP records them.
- Copy and mount the image on drive 8.
- Run `LOAD"SETUP",8,1`, then `RUN` before booting ReadyOS.
- SETUP checks REU, UCI, and Ultimate DOS, then browses Ultimate storage for
  the D81 and safely records its exact host path in that image's `apps.cfg`.
- F7 can apply a saved path or accept an absolute D81 path directly; config
  replacement is verified and preserves `apps.cfg` as a SEQ file.
- SETUP is standalone and reuses focused ReadyOS TUI micromodules; it is not
  a launcher app and uses no ReadyOS or ReadyFS overlays.
- This Ultimate-only workflow is accepted on physical hardware at 1, 16, and
  64 MHz. VICE does not provide meaningful UCI/Ultimate DOS acceptance here.

### Cartridge Variant

The EasyFlash cartridge build still expects a companion data disk on
drive `8`.

Cold boot notes:
- Mount `readyos_data.d64` on drive `8`, attach `readyos_easyflash.crt`,
  then reset.
- The long preload is normal. The cartridge is filling launcher, app,
  and overlay snapshots into the REU before the launcher appears.
- If an app snapshot preloaded from the cartridge is unloaded from REU,
  ReadyOS cannot load it again from the cartridge until you restart ReadyOS.
- Yellow border means the current launcher, app, or overlay payload is
  being copied from cartridge into RAM.
- Brown or orange border means that staged RAM image is being copied
  into the REU.
- When yellow and brown/orange alternate back and forth, one app or
  overlay is being processed as a pair: cartridge to RAM first, then
  RAM to REU.

### Boot-Side Apps

These are the launcher, boot chain, and utility-side apps that live on
the primary ReadyOS disk for multi-disk profiles.

### Quicknotes

REU-backed note editor for fast capture and return-to-work flow.

### Deminer

Minesweeper-style puzzle tuned for keyboard play and quick resume.

### Launcher

Entry point for launching, switching, and managing app flow.

### Calendar 26

Year-focused planner for appointments and day-to-day planning.

### Dizzy Kanban

Kanban workflow board tuned for fast keyboard organization.

### Ready Shell

ReadyShell is an overlay-based shell with expressions, pipelines,
filters, foreach stages, wildcard directory queries, disk commands,
and value serialization.

On the C64 keyboard, type `!` to enter the pipeline operator `|`.

For shipped command help, run `help`, then follow the hint:
`cat "rshelp" | more`.

It is split into a resident program plus overlay files:
- `readyshell`
- `rsparser`
- `rsvm`
- `rsdrvilst`
- `rsldv`
- `rsstv`
- `rsfops`
- `rscat`
- `rscopy`

Key shell commands:
- `PRT`, `GEN`, `TAP`, `MORE`
- `TOP`, `SEL`
- `DRVI`, `LST`
- `LDV`, `STV`
- `CAT`, `PUT`, `ADD`
- `DEL`, `REN`, `COPY`

Current file-command behavior:
- `LST` can filter by wildcard pattern, drive, and file type tokens such as
  `PRG`, `SEQ`, `USR`, and `REL`.
- `SEL "NAME"` emits the property value itself, such as a plain filename
  string.
- `SEL "NAME", "TYPE"` emits a new object containing only those named
  properties.
- `TOP count, skip` keeps `count` items after skipping the first `skip` items.
- `CAT` emits one string per text line from a PETSCII `SEQ` file.
- `PUT <expr>, <file>` creates or replaces a text file from a string or array
  of strings.
- `ADD <expr>, <file>` appends strings to an existing `SEQ` file, or creates
  it if missing.
- `LDV` and `STV` accept either embedded drive syntax like `"9:snap"` or a
  trailing drive argument such as `"snap", 9`.
- `DEL`, `REN`, and `COPY` cover scratch, rename, and copy workflows.

Common examples:
- `LST "9:t*"`
- `LST "t?", 9`
- `LST "t*", 9, "SEQ,PRG"`
- `LST | TOP 3, 1 | SEL "NAME"` skips one row, then keeps three
- `LST | SEL "NAME"` returns plain names
- `LST | SEL "NAME", "TYPE"` returns objects with `NAME` and `TYPE`
- `STV $A, "snap", 9` or `STV $A, "9:snap"`
- `$A = LDV "snap", 9` or `$A = LDV "9:snap"`

For a larger walkthrough with worked examples, see the ReadyShell
tutorial markdown in `src/apps/readyshell/ReadyShelltutorial.md`.
The ReadyShell serialized value file format used by `STV` and `LDV` is
documented in `docs/readyshell_rsv1_format.md`.

### Secondary-Disk Apps

These are the larger application-side programs for multi-disk
profiles. Single-disk profiles place them on the main release disk.

### Editor

Text editing with save/load, selection support, and system clipboard
integration.

### Calc Plus

Expression calculator with history, modes, and function tools.

### Hex View

Memory browser with PETSCII and raw screen-code views.

### Clipboard Manager

Multi-item clipboard history with preview, import, and clear.

### REU Viewer

Live map view for REU usage and allocation categories.

### System Info

Read-only hardware status for the current machine, REU, cartridge
visibility, Ultimate UCI details, and drives `8` through `11`.

### ReadyBASIC (Beta)

Beta BASIC V2 bridge with native control-flow helpers, disk-loadable command
modules, REU buffers and surfaces, graphics, sprites, input, polygons,
display lists, tilemaps, multicolor drawing, and immediate SID sound.

New: MEMCAP, BORDER, USPEED/UMHZ (Ultimate speed). LDMOD("RBM.MEDIA",M%)
adds eight media commands. D81: RBSND07, RBGFXSNDDEMO, RBUGFXSNDDEMO
(Ultimate). Music: vetted PAL PSID; reserve before strings; load at 1MHz.
Full guide: docs/readybasic_reference.html.

### ReadyIRC

Ultimate TCP IRC client with validated setup, connect/reconnect, channel
switching, common IRC event handling, help, editable input, and REU-backed
scrollback.

### UCI Tester

Interactive Ultimate Command Interface lab with raw and structured calls,
decoded results, selectable examples, and protocol-safe queue handling.

### Ultimate Zip

Ultimate-only ZIP create/extract. ReadyIRC and UCI Tester also need Ultimate.
All apps use ReadyOS REU. Own REU: QuickNotes, ReadyIRC, ReadyShell,
ReadyBASIC, Ultimate Zip. Clipboard/REU Viewer use shared records;
other app cores need no extra workspace. Clipboard still uses shared REU.

### Task List

Hierarchical outliner with notes, search, and persistence.

### Simple Files

Dual-pane file manager and SEQ viewer for disk-oriented workflows.

### Simple Cells

Spreadsheet-style grid with formulas, colors, and file persistence.

### 2048

Keyboard-first PETSCII game with quick pause/resume flow.

### Sidetris

Sideways PETSCII block-drop game with suspend/resume flow.

### Read.me

In-system project overview generated from markdown-lite content.

---

## Architecture and Approach

ReadyOS keeps the core small and focused.

Core approach:
- A tiny resident shim coordinates app lifecycle transitions.
- Apps are restored quickly from REU-backed state.
- Shared libraries provide consistent UI and behavior patterns.

Immediate workflows without a monolithic always-resident OS.

---

## Verification Process

Quality is enforced with an in-depth verification process:
- Binary analysis to confirm layout and integrity constraints.
- Dedicated test apps to probe critical behavior paths.
- Emulator sessions with remote monitor instrumentation.
- Regression checks focused on switching, restore, and stability.

READY should mean ready: responsive, reliable, and repeatable.
