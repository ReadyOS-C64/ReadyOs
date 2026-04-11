# ReadyShell Overlay Inventory Report (v0.1.8M)

Artifact-backed report generated from the current local ReadyShell build, linker map, and dual-D71 disk image.

## Executive Summary

- Profile / disk source: `precog-dual-d71` using `readyos.d71` (disk label `readyos`, `212` blocks free).
- Resident ReadyShell PRG: `readyshell.prg` on disk as `readyshell`, `30015` bytes and `119` D71 blocks.
- Overlay execution window: `$8E00-$C5FF` for `14336` bytes, with PRG load-address bytes at `$8DFE-$8DFF`.
- Resident BSS / heap below overlays: BSS `$853D-$8727` (`491` bytes), heap `$8728-$8DFD` (`1750` bytes).
- High RAM runtime region outside the app window: `$CA00-$CFFF`.
- REU policy split:
  - overlays 1-3 are boot-loaded from disk and cached into fixed REU banks 0x40-0x42
  - overlays 4-7 are loaded from disk on demand for each command call
  - bank 0x48 is shared for command handoff scratch and the REU-backed ReadyShell value arena

## Runtime Memory Map

| Region | Range | Size | Notes |
| --- | --- | ---: | --- |
| Resident app window | `$1000-$C5FF` | `46592` | ReadyOS app-owned RAM window for ReadyShell. |
| Overlay load address bytes | `$8DFE-$8DFF` | `2` | PRG load address emitted ahead of each overlay sidecar file. |
| Overlay execution window | `$8E00-$C5FF` | `14336` | Shared live area for whichever overlay is active. |
| Resident BSS | `$853D-$8727` | `491` | Resident writable data below the overlay load address. |
| Resident heap | `$8728-$8DFD` | `1750` | cc65 heap carved below the overlay load address. |
| High-RAM runtime | `$CA00-$CFFF` | `1536` | Fixed ReadyShell runtime state outside the app snapshot window. |

## REU Layout And Loading Model

| Use | REU range | Size | How it is used |
| --- | --- | ---: | --- |
| Overlay 1 cache | `$400000-$4032CC` | `13005` | Cached at boot, paged back for parse phase. |
| Overlay 2 cache | `$410000-$4137FD` | `14334` | Cached at boot, paged back for exec phase. |
| Overlay 3 cache | `$420000-$420CFB` | `3324` | Cached at boot, paged back for script/fs phase. |
| Debug trace ring | `$43F000-$43F20F` | `528` | Overlay debug markers and verification state. |
| Command scratch | `$480000-$487FFF` | `32768` | Inter-overlay handoff area for command frames and streaming state. |
| REU heap metadata | `$488000-$4880FF` | `256` | ReadyShell REU heap header region. |
| REU heap arena | `$488100-$48FEFF` | `32256` | Persistent value payload arena for REU-backed strings/arrays/objects. |

## Overlay Inventory

| Ovl | Role | Build PRG | Disk name | PRG bytes | Disk blocks | Live bytes | Window use | REU cache | Commands |
| ---: | --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| 1 | Parser / Lexer | `readyshell.prg.1` | `rsovl1` | `13007` | `52` | `13005` | `90.7%` | $400000 | None directly; parse phase support. |
| 2 | Execution Core | `readyshell.prg.2` | `rsovl2` | `14336` | `57` | `14334` | `100.0%` | $410000 | PRT, TOP, SEL, GEN, TAP and the shared execution paths that command overlays return to. |
| 3 | Script / Filesystem | `readyshell.prg.3` | `rsovl3` | `3326` | `14` | `3324` | `23.2%` | $420000 | No direct shell command token; used for script and filesystem support. |
| 4 | Drive Info | `readyshell.prg.4` | `rsovl4` | `3479` | `14` | `3477` | `24.3%` | disk-only | DRVI |
| 5 | Directory Listing | `readyshell.prg.5` | `rsovl5` | `4227` | `17` | `4225` | `29.5%` | disk-only | LST |
| 6 | Load Value | `readyshell.prg.6` | `rsovl6` | `11148` | `44` | `11146` | `77.7%` | disk-only | LDV |
| 7 | Store Value | `readyshell.prg.7` | `rsovl7` | `8964` | `36` | `8962` | `62.5%` | disk-only | STV |

## Resident Program

- Build PRG: `readyshell.prg`
- Disk filename: `readyshell`
- Disk staging comes from the main ReadyShell build artifact, not an overlay copy.
- Resident sources: `readyshellpoc.c, rs_token.c, rs_bc.c, rs_errors.c, rs_vm_c64.c, rs_overlay_c64.c, rs_platform_c64.c, rs_screen_c64.c, tui_nav.c, reu_mgr_dma.c, resume_state_ctx.c, resume_state_core.c`
- Resident asm/runtime support: `rs_runtime_c64.s`
- Command role: Resident app shell loop plus vm/overlay runtime. Command tokens resolved here, then dispatched to overlay 2 or command overlays.

## Per-Overlay Details

### Overlay 1: Parser / Lexer

- Purpose: Lexer, parser, AST construction, and parse cleanup.
- Build PRG: `readyshell.prg.1`
- Disk staging PRG: `obj/readyshell_ovl1.prg`
- Disk filename: `rsovl1`
- Source files: `rs_lexer.c, rs_parse.c, rs_parse_support.c, rs_parse_free.c`
- Commands: None directly; parse phase support.
- Runtime bytes in overlay window: `13005` at `$8E00-$C0CC`
- Window share: `90.7%` used, `1331` bytes free
- Disk footprint: `13007` bytes, `52` D71 blocks
- REU policy: Cached in REU bank `0x40` at `$400000`.
- RAM notes: Lives entirely inside the shared overlay window while active.

### Overlay 2: Execution Core

- Purpose: Values, variables, formatting, pipes, and shared execution helpers.
- Build PRG: `readyshell.prg.2`
- Disk staging PRG: `obj/readyshell_ovl2.prg`
- Disk filename: `rsovl2`
- Source files: `rs_vars.c, rs_value.c, rs_format.c, rs_cmd.c, rs_pipe.c`
- Commands: PRT, TOP, SEL, GEN, TAP and the shared execution paths that command overlays return to.
- Runtime bytes in overlay window: `14334` at `$8E00-$C5FD`
- Window share: `100.0%` used, `2` bytes free
- Disk footprint: `14336` bytes, `57` D71 blocks
- REU policy: Cached in REU bank `0x41` at `$410000`.
- RAM notes: Includes rs_vm_fmt_buf[128] and rs_vm_line_buf[384] inside the overlay image.

### Overlay 3: Script / Filesystem

- Purpose: Script-control helpers plus C64 filesystem helpers used by runtime flows.
- Build PRG: `readyshell.prg.3`
- Disk staging PRG: `obj/readyshell_ovl3.prg`
- Disk filename: `rsovl3`
- Source files: `rs_script_ctl_c64.c, rs_fs_c64.c`
- Commands: No direct shell command token; used for script and filesystem support.
- Runtime bytes in overlay window: `3324` at `$8E00-$9AFB`
- Window share: `23.2%` used, `11012` bytes free
- Disk footprint: `3326` bytes, `14` D71 blocks
- REU policy: Cached in REU bank `0x42` at `$420000`.
- RAM notes: Uses the same shared overlay window; no extra fixed RAM carve beyond its own overlay payload.

### Overlay 4: Drive Info

- Purpose: Single-command overlay for DRVI.
- Build PRG: `readyshell.prg.4`
- Disk staging PRG: `obj/readyshell_ovl4.prg`
- Disk filename: `rsovl4`
- Source files: `rs_cmd_drvi_c64.c`
- Commands: DRVI
- Runtime bytes in overlay window: `3477` at `$8E00-$9B94`
- Window share: `24.3%` used, `10859` bytes free
- Disk footprint: `3479` bytes, `14` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Shares the inter-command REU handoff area at 0x480000-0x487FFF.

### Overlay 5: Directory Listing

- Purpose: Single-command overlay for LST.
- Build PRG: `readyshell.prg.5`
- Disk staging PRG: `obj/readyshell_ovl5.prg`
- Disk filename: `rsovl5`
- Source files: `rs_cmd_lst_c64.c`
- Commands: LST
- Runtime bytes in overlay window: `4225` at `$8E00-$9E80`
- Window share: `29.5%` used, `10111` bytes free
- Disk footprint: `4227` bytes, `17` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Shares the inter-command REU handoff area at 0x480000-0x487FFF.

### Overlay 6: Load Value

- Purpose: Single-command overlay for LDV.
- Build PRG: `readyshell.prg.6`
- Disk staging PRG: `obj/readyshell_ovl6.prg`
- Disk filename: `rsovl6`
- Source files: `rs_cmd_ldv_c64.c`
- Commands: LDV
- Runtime bytes in overlay window: `11146` at `$8E00-$B989`
- Window share: `77.7%` used, `3190` bytes free
- Disk footprint: `11148` bytes, `44` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Uses the shared handoff region plus the REU-backed value arena in bank 0x48 when hydrating pointer-backed values.

### Overlay 7: Store Value

- Purpose: Single-command overlay for STV.
- Build PRG: `readyshell.prg.7`
- Disk staging PRG: `obj/readyshell_ovl7.prg`
- Disk filename: `rsovl7`
- Source files: `rs_cmd_stv_c64.c`
- Commands: STV
- Runtime bytes in overlay window: `8962` at `$8E00-$B101`
- Window share: `62.5%` used, `5374` bytes free
- Disk footprint: `8964` bytes, `36` D71 blocks
- REU policy: Not cached in a dedicated REU overlay bank; loaded from disk on demand.
- RAM notes: Uses the shared handoff region plus the REU-backed value arena in bank 0x48 when serializing pointer-backed values.

## Observations

- Overlay 2 is effectively full: `14334` of `14336` bytes (`100.0%`).
- Overlay 1 is also large at `13005` bytes (`90.7%`).
- The resident heap below the overlay load address is only `1750` bytes, so large transient work must lean on overlays and REU-backed storage.
- Command overlays 4-7 stay smaller on disk and in RAM, but they pay the disk-load cost per command because they are not REU-cached today.
- Overlay 2 carries the shared formatting buffers, so its footprint reflects both command support code and the text-rendering scratch it owns.
