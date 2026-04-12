# ReadyShell Overlay Inventory Report (v0.1.8X)

Artifact-backed report generated from the current local ReadyShell build, linker map, and D71 disk image.

## Executive Summary

- Profile / disk source: `precog-dual-d71` using `releases/0.1.8/precog-dual-d71/readyos-v0.1.8x-dual-d71_1.d71` (disk label `readyos`, `218` blocks free).
- Resident ReadyShell PRG: `readyshell.prg` on disk as `readyshell`, `30325` bytes and `120` D71 blocks.
- Overlay execution window: `$8E00-$C5FF` for `14336` bytes, with PRG load-address bytes at `$8DFE-$8DFF`.
- Resident BSS / heap below overlays: BSS `$8673-$8869` (`503` bytes), heap `$886A-$8DFD` (`1428` bytes).
- High RAM runtime region outside the app window: `$CA00-$CFFF`.
- REU policy split:
  - overlays 1-2 are boot-loaded from disk and cached into one shared REU bank using fixed full-window slots
  - overlays 3-6 are loaded from disk on demand for each command call
  - bank 0x48 is shared for overlay metadata, pause state, command handoff scratch, and the REU-backed ReadyShell value arena

## Runtime Memory Map

| Region | Range | Size | Notes |
| --- | --- | ---: | --- |
| Resident app window | `$1000-$C5FF` | `46592` | ReadyOS app-owned RAM window for ReadyShell. |
| Overlay load address bytes | `$8DFE-$8DFF` | `2` | PRG load address emitted ahead of each overlay sidecar file. |
| Overlay execution window | `$8E00-$C5FF` | `14336` | Shared live area for whichever overlay is active. |
| Resident BSS | `$8673-$8869` | `503` | Resident writable data below the overlay load address. |
| Resident heap | `$886A-$8DFD` | `1428` | cc65 heap carved below the overlay load address. |
| High-RAM runtime | `$CA00-$CFFF` | `1536` | Fixed ReadyShell runtime state outside the app snapshot window. |

## REU Layout And Loading Model

| Use | REU range | Size | How it is used |
| --- | --- | ---: | --- |
| Shared cache bank | `$400000-$40FFFF` | `65536` | One bank reserved for both core overlays and their free tail. |
| Overlay 1 parse slot | `$400000-$4037FF` | `14336` | Full overlay-window snapshot for overlay 1. |
| Overlay 2 exec slot | `$403800-$406FFF` | `14336` | Full overlay-window snapshot for overlay 2. |
| Shared bank free tail | `$407000-$40FFFF` | `36864` | Currently unused tail after the two core overlay slots. |
| Debug trace ring | `$43F000-$43F20F` | `528` | Overlay debug markers and verification state. |
| Command scratch | `$480000-$487FFF` | `32768` | Inter-overlay handoff area for command frames and streaming state. |
| Shared ReadyShell metadata | `$4880E0-$4880EB` | `12` | Shared core-overlay cache metadata record. |
| Pause flag | `$4880F0` | `1` | Shared output-pause bit used by resident output and `MORE`. |
| REU heap metadata | `$488000-$4880FF` | `256` | ReadyShell REU heap header region, including shared metadata bytes. |
| REU heap arena | `$488100-$48FEFF` | `32256` | Persistent value payload arena for REU-backed strings/arrays/objects. |

## Shared Core Overlay Cache Visual

```text
REU bank 0x40

+----------------------------------------+ $400000
| overlay 1 parse slot                   |
| full overlay-window image: 0x3800      |
| active file: rsparser.prg              |
+----------------------------------------+ $403800
| overlay 2 exec slot                    |
| full overlay-window image: 0x3800      |
| active file: rsvm.prg                  |
+----------------------------------------+ $407000
| free tail                              |
| 0x9000 bytes                           |
+----------------------------------------+ $40FFFF
```

## Overlay Inventory

| Ovl | Role | Build PRG | Disk name | PRG bytes | Disk blocks | Live bytes | Window use | REU cache | Commands |
| ---: | --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| 1 | Parser / Lexer | `rsparser.prg` | `rsparser` | `13007` | `52` | `13005` | `90.7%` | bank `0x40` slot `$400000-$4037FF` | None directly; parse phase support. |
| 2 | Execution Core | `rsvm.prg` | `rsvm` | `13899` | `55` | `13897` | `96.9%` | bank `0x40` slot `$403800-$406FFF` | PRT, MORE, TOP, SEL, GEN, TAP and the shared execution paths that command overlays return to. |
| 3 | Drive Info | `rsdrvi.prg` | `rsdrvi` | `3479` | `14` | `3477` | `24.3%` | disk-only | DRVI |
| 4 | Directory Listing | `rslst.prg` | `rslst` | `4227` | `17` | `4225` | `29.5%` | disk-only | LST |
| 5 | Load Value | `rsldv.prg` | `rsldv` | `11148` | `44` | `11146` | `77.7%` | disk-only | LDV |
| 6 | Store Value | `rsstv.prg` | `rsstv` | `8964` | `36` | `8962` | `62.5%` | disk-only | STV |

## Resident Program

- Build PRG: `readyshell.prg`
- Disk filename: `readyshell`
- Disk staging comes from the main ReadyShell build artifact, not an overlay copy.
- Resident sources: `readyshellpoc.c, rs_token.c, rs_bc.c, rs_errors.c, rs_vm_c64.c, rs_overlay_c64.c, rs_platform_c64.c, rs_screen_c64.c, tui_nav.c, reu_mgr_dma.c, resume_state_ctx.c, resume_state_core.c`
- Resident asm/runtime support: `rs_runtime_c64.s`
- Command role: Resident app shell loop plus vm/overlay runtime. Command tokens resolved here, then dispatched to overlay 2 or command overlays.
- Current linker-visible resident footprint:
  - `CODE` `0x7188`
  - `RODATA` `0x0411`
  - `DATA` `0x0053`
  - `INIT` `0x001C`
  - `ONCE` `0x0038`
  - `BSS` `0x01F7`

## Per-Overlay Details

### Overlay 1: Parser / Lexer

- Purpose: Lexer, parser, AST construction, and parse cleanup.
- Build PRG: `rsparser.prg`
- Disk staging PRG: `obj/rsparser.prg`
- Disk filename: `rsparser`
- Source files: `rs_lexer.c, rs_parse.c, rs_parse_support.c, rs_parse_free.c`
- Commands: None directly; parse phase support.
- Runtime bytes in overlay window: `13005` at `$8E00-$C0CC`
- Window share: `90.7%` used, `1331` bytes free
- Disk footprint: `13007` bytes, `52` D71 blocks
- REU policy: Cached in shared bank `0x40`, slot `$400000-$4037FF`, as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Lives entirely inside the shared overlay window while active.

### Overlay 2: Execution Core

- Purpose: Values, variables, formatting, pipes, and shared execution helpers.
- Build PRG: `rsvm.prg`
- Disk staging PRG: `obj/rsvm.prg`
- Disk filename: `rsvm`
- Source files: `rs_vars.c, rs_value.c, rs_format.c, rs_cmd.c, rs_pipe.c`
- Commands: PRT, MORE, TOP, SEL, GEN, TAP and the shared execution paths that command overlays return to.
- Runtime bytes in overlay window: `13897` at `$8E00-$C448`
- Window share: `96.9%` used, `439` bytes free
- Disk footprint: `13899` bytes, `55` D71 blocks
- REU policy: Cached in shared bank `0x40`, slot `$403800-$406FFF`, as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Includes rs_vm_fmt_buf[128] and rs_vm_line_buf[384] inside the overlay image.

### Overlay 3: Drive Info

- Purpose: Single-command overlay for DRVI.
- Build PRG: `rsdrvi.prg`
- Disk staging PRG: `obj/rsdrvi.prg`
- Disk filename: `rsdrvi`
- Source files: `rs_cmd_drvi_c64.c`
- Commands: DRVI
- Runtime bytes in overlay window: `3477` at `$8E00-$9B94`
- Window share: `24.3%` used, `10859` bytes free
- Disk footprint: `3479` bytes, `14` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Shares the inter-command REU handoff area at 0x480000-0x487FFF.

### Overlay 4: Directory Listing

- Purpose: Single-command overlay for LST.
- Build PRG: `rslst.prg`
- Disk staging PRG: `obj/rslst.prg`
- Disk filename: `rslst`
- Source files: `rs_cmd_lst_c64.c`
- Commands: LST
- Runtime bytes in overlay window: `4225` at `$8E00-$9E80`
- Window share: `29.5%` used, `10111` bytes free
- Disk footprint: `4227` bytes, `17` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Shares the inter-command REU handoff area at 0x480000-0x487FFF.

### Overlay 5: Load Value

- Purpose: Single-command overlay for LDV.
- Build PRG: `rsldv.prg`
- Disk staging PRG: `obj/rsldv.prg`
- Disk filename: `rsldv`
- Source files: `rs_cmd_ldv_c64.c`
- Commands: LDV
- Runtime bytes in overlay window: `11146` at `$8E00-$B989`
- Window share: `77.7%` used, `3190` bytes free
- Disk footprint: `11148` bytes, `44` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Uses the shared handoff region plus the REU-backed value arena in bank 0x48 when hydrating pointer-backed values.

### Overlay 6: Store Value

- Purpose: Single-command overlay for STV.
- Build PRG: `rsstv.prg`
- Disk staging PRG: `obj/rsstv.prg`
- Disk filename: `rsstv`
- Source files: `rs_cmd_stv_c64.c`
- Commands: STV
- Runtime bytes in overlay window: `8962` at `$8E00-$B101`
- Window share: `62.5%` used, `5374` bytes free
- Disk footprint: `8964` bytes, `36` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Uses the shared handoff region plus the REU-backed value arena in bank 0x48 when serializing pointer-backed values.

## Observations

- Overlay 2 is effectively full: `13897` of `14336` bytes (`96.9%`).
- Overlay 1 is also large at `13005` bytes (`90.7%`).
- The resident heap below the overlay load address is only `1428` bytes, so large transient work must lean on overlays and REU-backed storage.
- Overlays 1-2 no longer consume separate REU banks; they share bank `0x40` and leave `36864` bytes free at the tail of that bank.
- Command overlays 3-6 stay smaller on disk and in RAM, but they pay the disk-load cost per command because they are not REU-cached today.
- Overlay 2 carries the shared formatting buffers, so its footprint reflects both command support code and the text-rendering scratch it owns.
