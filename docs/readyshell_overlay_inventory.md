# ReadyShell Overlay Inventory Report (v0.5)

Artifact-backed report generated from the current local ReadyShell build, linker map, and D71 disk image.

## Executive Summary

- Profile / disk source: `precog-dual-d71` using `Releases/0.5/precog-dual-d71/readyos-v0.5-dual-d71_1.d71` (disk label `readyos`, `24` blocks free).
- Resident ReadyShell PRG: `readyshell.prg` on disk as `readyshell`, `28141` bytes and `111` D71 blocks.
- Overlay execution window: `$8E00-$C5FF` for `14336` bytes, with PRG load-address bytes at `$8DFE-$8DFF`.
- Resident BSS / heap below overlays: BSS `$7DEB-$7F94` (`426` bytes), heap `$7F96-$8DFD` (`3688` bytes).
- High RAM runtime region outside the app window: `$CA00-$CFFF`.
- REU policy split:
  - overlays 1-9 are boot-loaded during shell startup and cached into fixed full-window REU slots
  - bank `0x36` holds overlays `1`, `2`, `3`, and `5`; bank `0x37` holds overlays `4`, `6`, `7`, and `8`; bank `0x38` holds overlay `9`
  - the ReadyShell state bank (current generated EasyFlash state bank 0x39) holds the external-command registry, overlay metadata, pause state, command handoff scratch, and REU-backed value arena

## Runtime Memory Map

| Region | Range | Size | Notes |
| --- | --- | ---: | --- |
| ReadyOS snapshot window | `$1000-$C5FF` | `46592` | Full app-owned RAM captured by the shim; the resident 1 KB shim begins at `$C600`. |
| Overlay load address bytes | `$8DFE-$8DFF` | `2` | PRG load address emitted ahead of each overlay sidecar file. |
| Overlay execution window | `$8E00-$C5FF` | `14336` | Shared live area for whichever overlay is active. |
| Resident BSS | `$7DEB-$7F94` | `426` | Resident writable data below the overlay load address. |
| Resident heap | `$7F96-$8DFD` | `3688` | cc65 heap carved below the overlay load address. |
| High-RAM runtime | `$CA00-$CFFF` | `1536` | Fixed ReadyShell runtime state outside the app snapshot window. |

## REU Layout And Loading Model

| Use | REU range | Size | How it is used |
| --- | --- | ---: | --- |
| Shared cache bank 1 | `$360000-$36FFFF` | `65536` | Loader-assigned ReadyShell resource bank holding overlays 1, 2, 3, and 5 (current generated EasyFlash assignment). |
| Overlay 1 parse slot | `$360000-$3637FF` | `14336` | Full overlay-window snapshot for overlay 1. |
| Overlay 2 exec slot | `$363800-$366FFF` | `14336` | Full overlay-window snapshot for overlay 2. |
| Overlay 3 command slot | `$367000-$36A7FF` | `14336` | Full overlay-window snapshot for overlay 3. |
| Overlay 5 command slot | `$36A800-$36DFFF` | `14336` | Full overlay-window snapshot for overlay 5. |
| Cache bank 1 free tail | `$36E000-$36FFFF` | `8192` | Unused tail after the four cache slots in assigned bank 1. |
| Shared cache bank 2 | `$370000-$37FFFF` | `65536` | Loader-assigned ReadyShell resource bank holding overlays 4, 6, 7, and 8 (current generated EasyFlash assignment). |
| Overlay 4 command slot | `$370000-$3737FF` | `14336` | Full overlay-window snapshot for overlay 4. |
| Overlay 6 command slot | `$373800-$376FFF` | `14336` | Full overlay-window snapshot for overlay 6. |
| Overlay 7 command slot | `$377000-$37A7FF` | `14336` | Full overlay-window snapshot for overlay 7. |
| Overlay 8 command slot | `$37A800-$37DFFF` | `14336` | Full overlay-window snapshot for overlay 8. |
| Cache bank 2 free tail | `$37E000-$37FFFF` | `8192` | Unused tail after the four cache slots in assigned bank 2. |
| Shared cache bank 3 | `$380000-$38FFFF` | `65536` | Loader-assigned ReadyShell resource bank holding overlay 9, the prompt editor (current generated EasyFlash assignment). |
| Overlay 9 editor slot | `$380000-$3837FF` | `14336` | Full overlay-window snapshot for overlay 9. |
| Cache bank 3 free tail | `$383800-$38FFFF` | `51200` | Unused tail after the editor slot in assigned bank 3. |
| ReadyShell state bank | `$390000-$39FFFF` | `65536` | Loader-assigned ReadyShell resource bank for command scratch, registry metadata, pause state, and the value arena (current generated EasyFlash state bank 0x39). |
| Debug trace ring | `$397DE0-$397FEF` | `528` | Overlay debug markers and verification state. |
| Command scratch | `$390000-$397DDF` | `32224` | Inter-overlay handoff area for command frames and streaming state. |
| Command registry header | `$398010-$398017` | `8` | REU-backed external-command registry header. |
| Command descriptor table | `$398020-$39807F` | `96` | Fixed-capacity external-command descriptor table in REU metadata. |
| Overlay state table | `$398080-$3980EB` | `108` | Fixed-capacity overlay load/cache state table for external command overlays. |
| Shared ReadyShell metadata | `$3980F0-$398113` | `36` | Shared core-overlay cache metadata record. |
| Pause flag | `$398114` | `1` | Shared output-pause bit used by resident output and `MORE`. |
| REU heap metadata | `$398000-$3980FF` | `256` | ReadyShell REU heap header region, including shared metadata bytes. |
| Reserved metadata guard | `$398100-$39811F` | `32` | Reserved gap keeping shared overlay metadata and pause state clear of the value arena. |
| REU heap arena | `$398120-$39FEFF` | `32224` | Persistent value payload arena for REU-backed strings/arrays/objects. |

## Shared Overlay Cache Visual

```text
REU bank 0x36

+----------------------------------------+ $360000
| overlay 1 parse slot                   |
| full overlay-window image: 0x3800      |
| active file: rsparser.prg              |
+----------------------------------------+ $363800
| overlay 2 exec slot                    |
| full overlay-window image: 0x3800      |
| active file: rsvm.prg                  |
+----------------------------------------+ $367000
| overlay 3 command slot                 |
| full overlay-window image: 0x3800      |
| active file: rsdrvilst.prg             |
+----------------------------------------+ $36A800
| overlay 5 command slot                 |
| full overlay-window image: 0x3800      |
| active file: rsstv.prg                 |
+----------------------------------------+ $36E000
| free tail                              |
| 0x2000 bytes                           |
+----------------------------------------+ $36FFFF

REU bank 0x37

+----------------------------------------+ $370000
| overlay 4 command slot                 |
| full overlay-window image: 0x3800      |
| active file: rsldv.prg                 |
+----------------------------------------+ $373800
| overlay 6 command slot                 |
| full overlay-window image: 0x3800      |
| active file: rsfops.prg                |
+----------------------------------------+ $377000
| overlay 7 command slot                 |
| full overlay-window image: 0x3800      |
| active file: rscat.prg                 |
+----------------------------------------+ $37A800
| overlay 8 command slot                 |
| full overlay-window image: 0x3800      |
| active file: rscopy.prg                |
+----------------------------------------+ $37E000
| free tail                              |
| 0x2000 bytes                           |
+----------------------------------------+ $37FFFF

REU bank 0x38

+----------------------------------------+ $380000
| overlay 9 prompt editor slot           |
| full overlay-window image: 0x3800      |
| active file: rsedit.prg                |
+----------------------------------------+ $383800
| free tail                              |
| 0xc800 bytes                           |
+----------------------------------------+ $38FFFF
```

## Command Scratch And Value Arena Usage

| Commands | Overlay | Command scratch | Value arena | How the REU region is used |
| --- | --- | --- | --- | --- |
| PRT, MORE, TOP, SEL, GEN, TAP | `2 / rsvm` | No direct use | Indirect only | Run inside the shared execution core. They do not stage command-local data in `$390000-$397DDF`; any REU-backed values are handled through the normal overlay-2 value/runtime paths. |
| DRVI | `3 / rsdrvilst` | No | No | Reads drive header/status data and builds its output object in transient overlay-local RAM. |
| LST | `3 / rsdrvilst` | Yes | No | Spools 28-byte directory records through `$390000-$397DDF` so `BEGIN`/`ITEM` can walk the listing without keeping the directory channel open. |
| LDV | `4 / rsldv` | Yes | Yes, writes persistent values | Reads the RSV1 file into `$390000-$397DDF`, validates its header, then materializes strings, arrays, and objects into the REU heap arena `$398120-$39FEFF`. |
| STV | `5 / rsstv` | Yes | Yes, reads existing pointer values | Uses `$390000-$3900FF` for session metadata and `$390100-$397DDF` for the outgoing RSV1 payload. When serializing pointer-backed values, it dereferences them from the persistent REU heap arena before flattening them into scratch. |
| DEL, REN | `6 / rsfops` | No | No | Issue DOS scratch/rename commands directly through command-channel I/O with no REU staging. |
| PUT, ADD | `6 / rsfops` | Yes | No | Use `$390000-$39001F` for session metadata and `$390020-$397DDF` as a text spool for new/appended SEQ content before writing it back to disk. |
| CAT | `7 / rscat` | Yes | No | Uses `$390000-$3907FF` as a line-record table and `$390800-$397DDF` as the line-data spool so `ITEM` can replay file lines after the initial read pass. |
| COPY | `8 / rscopy` | No | No | Uses its overlay-local 128-byte transfer buffer plus direct DOS copy/streamed file I/O. It does not use the shared command scratch or the persistent value arena. |

- The command scratch window is shared, not partitioned per overlay. Only one command overlay owns it at a time even though all external overlays are preloaded into REU, because they still run serially through the shared overlay window.
- The value arena is persistent session state in the loader-assigned ReadyShell state bank. `LDV` populates it explicitly, while `STV` can serialize values already living there.

## Static Audit Checks

- Registry capacity check: `rs_cmd_registry.c` seeds `10` external command descriptors into `16` reserved descriptor slots and `6` overlay-state rows into `6` reserved state slots.
- Metadata packing check: ReadyShell control metadata starts in `$398000-$3980FF`. Header uses `$398010-$398017`, descriptor rows reserve `$398020-$39807F` with live rows ending at `$39805B`, state rows reserve `$398080-$3980EB` with live rows ending at `$3980EB`, shared metadata uses `$3980F0-$398113`, and the pause flag sits at `$398114` before the value arena at `$398120`.
- Non-overlap check: command scratch ends at `$397DDF` and REU heap metadata begins at `$398000`; the state table ends at `$3980EB` and shared metadata begins at `$3980F0`; shared metadata ends at `$398113` and the pause flag is `$398114`; the value arena begins at `$398120`.
- Cache-slot audit: ReadyShell caches overlays 1-9. Bank `0x36` carries overlays `1`, `2`, `3`, and `5`; bank `0x37` carries overlays `4`, `6`, `7`, and `8`; bank `0x38` carries overlay `9`. Every slot is a full `14336`-byte overlay-window snapshot.
- Command-source audit: `DRVI` builds output only in overlay-local RAM; `LST` writes 28-byte directory records into shared scratch; `LDV` streams RSV1 payloads into scratch and materializes persistent values into the REU heap arena; `STV` serializes into scratch and dereferences pointer-backed values from the arena; `DEL` and `REN` issue direct DOS commands without REU staging; `PUT` and `ADD` use scratch metadata plus a text spool; `CAT` uses a scratch record table plus line-data spool; `COPY` stays overlay-local with `g_copy_buf[128]`.

## Overlay Inventory

| Ovl | Role | Build PRG | Disk name | PRG bytes | Disk blocks | Live bytes | Window use | REU cache | Commands |
| ---: | --- | --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| 1 | Parser / Lexer | `rsparser.prg` | `rsparser` | `13007` | `52` | `13005` | `90.7%` | bank `0x36` slot `$360000-$3637FF` | None directly; parse phase support. |
| 2 | Execution Core | `rsvm.prg` | `rsvm` | `14030` | `56` | `14028` | `97.9%` | bank `0x36` slot `$363800-$366FFF` | PRT, MORE, TOP, SEL, GEN, TAP and the shared execution paths that command overlays return to. |
| 3 | Drive Info + Directory Listing | `rsdrvilst.prg` | `rsdrvilst` | `11260` | `45` | `11258` | `78.5%` | bank `0x36` slot `$367000-$36A7FF` | DRVI, LST |
| 4 | Load Value | `rsldv.prg` | `rsldv` | `11896` | `47` | `11894` | `83.0%` | bank `0x37` slot `$370000-$3737FF` | LDV |
| 5 | Store Value | `rsstv.prg` | `rsstv` | `10251` | `41` | `10249` | `71.5%` | bank `0x36` slot `$36A800-$36DFFF` | STV |
| 6 | File Delete / Rename / Write | `rsfops.prg` | `rsfops` | `14327` | `57` | `14325` | `99.9%` | bank `0x37` slot `$373800-$376FFF` | DEL, REN, PUT, ADD |
| 7 | File Read | `rscat.prg` | `rscat` | `8083` | `32` | `8081` | `56.4%` | bank `0x37` slot `$377000-$37A7FF` | CAT |
| 8 | File Copy | `rscopy.prg` | `rscopy` | `6601` | `26` | `6599` | `46.0%` | bank `0x37` slot `$37A800-$37DFFF` | COPY |
| 9 | Prompt Editor | `rsedit.prg` | `rsedit` | `2004` | `8` | `2002` | `14.0%` | bank `0x38` slot `$380000-$3837FF` | None directly; prompt/input phase support. |

## Command Topology

```text
Resident ReadyShell dispatcher
  |
  +-- Overlay 2  rsvm       [shared execution core]
  |      commands: PRT | MORE | TOP | SEL | GEN | TAP
  |      note: shared execution paths that command overlays return to
  |
  +-- Overlay 3  rsdrvilst  [shared command overlay]
  |      commands: DRVI | LST
  |      note: multiple commands share one disk sidecar and one RAM image
  |
  +-- Overlay 4  rsldv      [single-command overlay]
  |      commands: LDV
  |
  +-- Overlay 5  rsstv      [single-command overlay]
  |      commands: STV
  |
  +-- Overlay 6  rsfops     [shared command overlay]
  |      commands: DEL | REN | PUT | ADD
  |      note: multiple commands share one disk sidecar and one RAM image
  |
  +-- Overlay 7  rscat      [single-command overlay]
  |      commands: CAT
  |
  `-- Overlay 8  rscopy     [single-command overlay]
         commands: COPY
```

- `DRVI` and `LST` co-reside in `rsdrvilst`, so both commands restore the same cached overlay image.
- All overlays 1-9 are REU-cached today; overlays `3-9` are no longer reloaded from disk on repeat command or prompt calls inside the same session.

## Resident Program

- Build PRG: `readyshell.prg`
- Disk filename: `readyshell`
- Disk staging comes from the main ReadyShell build artifact, not an overlay copy.
- Resident sources: `readyshell.c, rs_token.c, rs_bc.c, rs_errors.c, rs_cmd_registry.c, rs_vm_c64.c, rs_overlay_c64.c, rs_platform_c64.c, tui_nav.c, reu_mgr_dma.c, resume_state_ctx.c, resume_state_core.c`
- Resident asm/runtime support: `rs_runtime_c64.s, $(TUI_READYOS_SRC)`
- Command role: Resident app shell loop plus vm/overlay runtime. Command tokens resolved here, then dispatched to overlay 2 or command overlays.
- Current linker-visible resident footprint:
  - `CODE` `0x692C`
  - `RODATA` `0x03F2`
  - `DATA` `0x0046`
  - `INIT` `0x001C`
  - `ONCE` `0x0038`
  - `BSS` `0x01AA`

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
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x36` slot `$360000-$3637FF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Lives entirely inside the shared overlay window while active.

### Overlay 2: Execution Core

- Purpose: Values, variables, formatting, pipes, and shared execution helpers.
- Build PRG: `rsvm.prg`
- Disk staging PRG: `obj/rsvm.prg`
- Disk filename: `rsvm`
- Source files: `rs_vars.c, rs_value.c, rs_format.c, rs_cmd.c, rs_pipe.c`
- Commands: PRT, MORE, TOP, SEL, GEN, TAP and the shared execution paths that command overlays return to.
- Runtime bytes in overlay window: `14028` at `$8E00-$C4CB`
- Window share: `97.9%` used, `308` bytes free
- Disk footprint: `14030` bytes, `56` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x36` slot `$363800-$366FFF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Includes rs_vm_fmt_buf[128] and rs_vm_line_buf[384] inside the overlay image.

### Overlay 3: Drive Info + Directory Listing

- Purpose: Shared command overlay for DRVI and LST.
- Build PRG: `rsdrvilst.prg`
- Disk staging PRG: `obj/rsdrvilst.prg`
- Disk filename: `rsdrvilst`
- Source files: `rs_cmd_lst_c64.c, rs_cmd_drvi_c64.c`
- Commands: DRVI, LST
- Runtime bytes in overlay window: `11258` at `$8E00-$B9F9`
- Window share: `78.5%` used, `3078` bytes free
- Disk footprint: `11260` bytes, `45` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x36` slot `$367000-$36A7FF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Shares the inter-command REU handoff area at offset $0000-$7FFF in the loader-assigned ReadyShell state bank.

### Overlay 4: Load Value

- Purpose: Single-command overlay for LDV.
- Build PRG: `rsldv.prg`
- Disk staging PRG: `obj/rsldv.prg`
- Disk filename: `rsldv`
- Source files: `rs_cmd_ldv_c64.c`
- Commands: LDV
- Runtime bytes in overlay window: `11894` at `$8E00-$BC75`
- Window share: `83.0%` used, `2442` bytes free
- Disk footprint: `11896` bytes, `47` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x37` slot `$370000-$3737FF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Uses the shared handoff region plus the REU-backed value arena in the loader-assigned ReadyShell state bank when hydrating pointer-backed values.

### Overlay 5: Store Value

- Purpose: Single-command overlay for STV.
- Build PRG: `rsstv.prg`
- Disk staging PRG: `obj/rsstv.prg`
- Disk filename: `rsstv`
- Source files: `rs_cmd_stv_c64.c`
- Commands: STV
- Runtime bytes in overlay window: `10249` at `$8E00-$B608`
- Window share: `71.5%` used, `4087` bytes free
- Disk footprint: `10251` bytes, `41` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x36` slot `$36A800-$36DFFF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Uses the shared handoff region plus the REU-backed value arena in the loader-assigned ReadyShell state bank when serializing pointer-backed values.

### Overlay 6: File Delete / Rename / Write

- Purpose: Shared command overlay for DEL, REN, PUT, and ADD.
- Build PRG: `rsfops.prg`
- Disk staging PRG: `obj/rsfops.prg`
- Disk filename: `rsfops`
- Source files: `rs_cmd_delren_c64.c, rs_cmd_putadd_c64.c`
- Commands: DEL, REN, PUT, ADD
- Runtime bytes in overlay window: `14325` at `$8E00-$C5F4`
- Window share: `99.9%` used, `11` bytes free
- Disk footprint: `14327` bytes, `57` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x37` slot `$373800-$376FFF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Keeps file-operation staging and transient command state in overlay-local code plus the shared REU scratch region.

### Overlay 7: File Read

- Purpose: Single-command overlay for CAT.
- Build PRG: `rscat.prg`
- Disk staging PRG: `obj/rscat.prg`
- Disk filename: `rscat`
- Source files: `rs_cmd_cat_c64.c`
- Commands: CAT
- Runtime bytes in overlay window: `8081` at `$8E00-$AD90`
- Window share: `56.4%` used, `6255` bytes free
- Disk footprint: `8083` bytes, `32` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x37` slot `$377000-$37A7FF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Uses overlay-local file I/O logic plus shared REU scratch when line staging is needed.

### Overlay 8: File Copy

- Purpose: Single-command overlay for COPY.
- Build PRG: `rscopy.prg`
- Disk staging PRG: `obj/rscopy.prg`
- Disk filename: `rscopy`
- Source files: `rs_cmd_copy_c64.c`
- Commands: COPY
- Runtime bytes in overlay window: `6599` at `$8E00-$A7C6`
- Window share: `46.0%` used, `7737` bytes free
- Disk footprint: `6601` bytes, `26` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x37` slot `$37A800-$37DFFF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Uses an overlay-local 128-byte transfer buffer plus direct DOS copy or streamed file I/O. It does not use the shared REU scratch or value arena.

### Overlay 9: Prompt Editor

- Purpose: Interactive prompt input, cursor editing, key normalization, and backtick logical-line continuation.
- Build PRG: `rsedit.prg`
- Disk staging PRG: `obj/rsedit.prg`
- Disk filename: `rsedit`
- Source files: `rs_edit_c64.c`
- Commands: None directly; prompt/input phase support.
- Runtime bytes in overlay window: `2002` at `$8E00-$95D1`
- Window share: `14.0%` used, `12334` bytes free
- Disk footprint: `2004` bytes, `8` D71 blocks
- REU policy: Boot-loaded from disk during shell startup, then restored from bank `0x38` slot `$380000-$3837FF` as a full `0x3800`-byte overlay-window snapshot.
- RAM notes: Keeps the editable physical-line buffer overlay-local; the final logical command line remains resident in g_line.

## Observations

- Overlay 2 is effectively full: `14028` of `14336` bytes (`97.9%`).
- Overlay 1 is also large at `13005` bytes (`90.7%`).
- The resident heap below the overlay load address is only `3688` bytes, so large transient work must lean on overlays and REU-backed storage.
- ReadyShell now uses three loader-assigned REU resource banks (current generated EasyFlash assignment): `0x36` for overlays `1`, `2`, `3`, and `5`; `0x37` for overlays `4`, `6`, `7`, and `8`; and `0x38` for overlay `9` in this artifact.
- Assigned bank 1 leaves `8192` bytes free; assigned bank 2 leaves `8192` bytes free; assigned bank 3 leaves `51200` bytes free.
- External commands now pay a launcher/loader preload cost instead of a repeated disk-load cost during each command call.
- Overlay 2 carries the shared formatting buffers, so its footprint reflects both command support code and the text-rendering scratch it owns.
