# ReadyShell Architecture

This document describes the current ReadyShell implementation in ReadyOS as it
exists in the local build, not an aspirational future design.

For command syntax and user-facing behavior, see
[../src/apps/readyshell/README.md](../src/apps/readyshell/README.md).
For the generated size and placement inventory, see
[./readyshell_overlay_inventory.md](./readyshell_overlay_inventory.md).

Keyboard note: on a C64 keyboard, users type `!` to enter the pipeline
operator `|`.

## 1. End-To-End Shape

ReadyShell is an overlayed C64 app with five main layers:

1. Resident shell/runtime code
2. Prompt editor overlay
3. Parse overlay
4. Execution-core overlay
5. External command overlays

At runtime, the resident app owns the shell loop, prompt state, REPL state,
resume hooks, and overlay orchestration. Parsing and most execution logic still
run through the shared overlay window. The prompt editor also lives in that
window now: overlay 9 is active only while ReadyShell is waiting for input, then
parser, VM, or command overlays replace it. The launcher/loader preloads all
nine overlays into the ReadyShell `rsovl` resource set before entering
ReadyShell.

```text
user types line
    |
    v
resident shell loop
    |
    +--> restore prompt editor overlay from REU
    |       edit/read logical command line
    |
    +--> restore parser overlay from REU if needed
    |       parse source -> AST / runtime structures
    |
    +--> restore exec overlay (rsvm) from REU
            execute stages
              |
              +--> direct exec-core commands
              |
              `--> external command overlay calls
                        |
                        +--> restore command overlay from REU
                        +--> run command
                        `--> restore rsvm from REU
```

Normal command execution no longer reloads overlays `3-8` from disk on each
call, and normal prompt entry no longer keeps the editor resident. Disk loading
is now a launcher/loader preload path. ReadyShell itself expects the shared
metadata record to contain valid assigned cache banks.

## 2. Runtime Memory Model

Current release build layout:

| Region | Range | Purpose |
| --- | --- | --- |
| ReadyOS snapshot window | `$1000-$C5FF` | ReadyShell-owned app RAM captured by the shim |
| Overlay load bytes | `$8DFE-$8DFF` | PRG load-address bytes for overlay sidecars |
| Overlay execution window | `$8E00-$C5FF` | Shared live window for whichever overlay is active |
| Resident shim expansion reserve | `$C600-$C7FF` | ReadyOS-owned capacity outside ReadyShell's proven overlay ABI |
| Resident BSS | `$7DEB-$7F94` | Resident writable state below overlays |
| Resident heap | `$7F96-$8DFD` | cc65 heap below overlay load address |
| High runtime area | `$CA00-$CFFF` | ReadyShell runtime state outside app snapshot |

Important constraints:

- resident growth directly reduces heap headroom
- overlay growth does not reduce heap headroom, but every overlay must still
  fit the `0x3800` overlay window
- full-window REU snapshots are required because overlay-local writable data
  must survive phase switches

## 3. Overlay Set

Current overlays:

| Overlay | File | Role | Current loading model |
| --- | --- | --- | --- |
| 1 | `rsparser.prg` | lexer, parser, parse support | `rsovl` preload, assigned bank 1 |
| 2 | `rsvm.prg` | values, vars, formatting, pipes, command lookup, shared execution paths | `rsovl` preload, assigned bank 1 |
| 3 | `rsdrvilst.prg` | `DRVI`, `LST` | `rsovl` preload, assigned bank 1 |
| 4 | `rsldv.prg` | `LDV` | `rsovl` preload, assigned bank 2 |
| 5 | `rsstv.prg` | `STV` | `rsovl` preload, assigned bank 1 |
| 6 | `rsfops.prg` | `DEL`, `REN`, `PUT`, `ADD` | `rsovl` preload, assigned bank 2 |
| 7 | `rscat.prg` | `CAT` | `rsovl` preload, assigned bank 2 |
| 8 | `rscopy.prg` | `COPY` | `rsovl` preload, assigned bank 2 |
| 9 | `rsedit.prg` | prompt drawing, cursor/edit keys, backtick continuation | `rsovl` preload, assigned bank 3 |

REU slot layout:

```text
assigned bank 1
  overlay 1  rsparser   +$0000-+$37FF
  overlay 2  rsvm       +$3800-+$6FFF
  overlay 3  rsdrvilst  +$7000-+$A7FF
  overlay 5  rsstv      +$A800-+$DFFF
  free tail             +$E000-+$FFFF

assigned bank 2
  overlay 4  rsldv      +$0000-+$37FF
  overlay 6  rsfops     +$3800-+$6FFF
  overlay 7  rscat      +$7000-+$A7FF
  overlay 8  rscopy     +$A800-+$DFFF
  free tail             +$E000-+$FFFF

assigned bank 3
  overlay 9  rsedit     +$0000-+$37FF
  free space            +$3800-+$FFFF
```

Each slot stores the full `0x3800` overlay window, not just the PRG payload
bytes.

## 4. Command Families

ReadyShell commands split into two families.

### 4.1 Exec-core commands

These live in `OVERLAY2`:

- `PRT`
- `MORE`
- `TOP`
- `SEL`
- `GEN`
- `TAP`

These execute directly once `rsvm` is active in the overlay window.

### 4.2 External commands

These live outside `OVERLAY2`:

- `DRVI`
- `LST`
- `LDV`
- `STV`
- `DEL`
- `REN`
- `PUT`
- `ADD`
- `CAT`
- `COPY`

These are driven through the REU-backed external-command registry plus one
generic resident execution path.

Current user-facing command notes that affect the overlay logic:

- `LST` accepts a wildcard pattern, an optional drive argument, and an optional
  comma-separated type filter. Embedded drive prefixes inside the pattern such
  as `9:*.PRG` take precedence over a separate numeric drive argument.
- `LDV` accepts either `LDV "9:snap"` or `LDV "snap", 9`.
- `STV` accepts either `STV $A, "9:snap"` or `STV $A, "snap", 9`.
- `TOP` accepts `TOP count` or `TOP count,skip`.
- `CAT`, `PUT`, `ADD`, `DEL`, and `COPY` also honor embedded drive prefixes in
  their filename arguments, while `REN` still uses the traditional explicit
  drive argument form.

The on-disk `RSV1` value format used by `STV` and `LDV` is documented in
[./readyshell_rsv1_format.md](./readyshell_rsv1_format.md).

## 5. Parse And Pipeline Flow

Parsing happens in overlay 1, but pipeline execution returns to overlay 2.

```text
source line
   |
   v
OVERLAY1: lexer + parser
   |
   v
resident regains control
   |
   v
OVERLAY2: execute pipeline stage-by-stage
   |
   +--> expr stage
   +--> filter stage
   +--> foreach stage
   +--> exec-core command
   `--> external command
```

Execution is command-protocol driven, not just command-name driven. The overlay
command ABI defines:

- `BEGIN`
- `ITEM`
- `RUN`
- `PROCESS`
- `END`

The current shipped commands use:

- direct `RUN` for most external commands
- `BEGIN` + repeated `ITEM` for `CAT`

The generic external runner in resident code uses command capability bits to
choose the active path.

## 6. External Command Registry

The external command registry lives in REU metadata space, not in resident
heap/BSS.

Current registry regions:

| Region | Range | Purpose |
| --- | --- | --- |
| Command registry header | state bank `+$8010-+$8017` | magic, version, counts |
| Command descriptor table | state bank `+$8020-+$807F` | fixed-capacity external command descriptors |
| Overlay state table | state bank `+$8080-+$80EB` | fixed-capacity state records for external overlays |

Each descriptor identifies:

- command id
- owning external overlay index
- protocol capability flags
- overlay-local handler id

Each overlay state record carries:

- overlay phase
- load-policy flags
- load/cache state
- cache bank and slot offset
- disk filename

This means new external commands no longer require one resident wrapper per
command.

## 7. External Command Call Flow

Current external call path:

```text
OVERLAY2 decides command is external
    |
    v
resident vm_cmd_external()
    |
    v
lookup descriptor in REU registry
    |
    v
prepare external overlay from overlay-state record
    |
    `--> restore full slot from loader-filled REU cache
    |
    v
call one overlay dispatcher with handler id
    |
    v
command overlay runs command body
    |
    v
resident restores OVERLAY2 from REU
```

For example, repeated `LST` calls now reuse the same cached overlay image:

```text
LST
  restore rsdrvilst from REU
  run LST
  restore rsvm from REU

LST again
  restore rsdrvilst from REU
  run LST
  restore rsvm from REU
```

So the registry now describes both dispatch metadata and the assigned cache-slot
placement for overlays `3-8`.

## 8. REU Usage

Current ReadyShell REU usage is split by purpose.

### 8.1 Overlay cache banks

```text
assigned bank 1
  overlays 1, 2, 3, 5
  free tail: 8192 bytes

assigned bank 2
  overlays 4, 6, 7, 8
  free tail: 8192 bytes

assigned bank 3
  overlay 9
  free space after slot: 51200 bytes
```

Important detail:

- these are full-window snapshots, not just PRG payload bytes
- the bank numbers are supplied by the launcher/loader through metadata at
  offset `$80F0` in the loader-assigned ReadyShell state bank, not by fixed
  ReadyShell constants
- writable overlay data survives phase switching because the whole window image
  is cached

### 8.2 State/scratch bank

```text
loader-assigned ReadyShell state bank
  command scratch     +$0000-+$7DDF
    CAT uses          +$0000-+$17FF while CAT is active
  diagnostics tail    +$7DE0-+$7FFF
    debug head        +$7DE0
    debug data        +$7DF0-+$7FEF
    REU probe byte    +$7FFF
  REU heap metadata   +$8000-+$80FF
  command registry    +$8010-+$80EB
  shared metadata     +$80F0-+$8113
  pause flag          +$8114
  reserved guard      +$8100-+$811F
  REU value arena     +$8120-+$FEFF
```

The state bank is shared but not overlaid: command scratch, CAT staging,
diagnostics/probe bytes, registry, pause state, and the REU-backed value arena
all live there at fixed offsets. The launcher owns the physical bank, mirrors it
as the ReadyShell state resource in bank `0`, and seeds ReadyShell's `$CFF2`
state-bank cache before entry so the runtime does not need a large registry
reader. CAT is safe to place in the same scratch area because command overlays
execute serially; while CAT is active, no other command owns that transient
handoff area.

The diagnostic ring has no C64-RAM mirror. The former `$C7A0-$C7DF` data and
`$C7F0` head is now resident shim-owned capacity; only small live
cursor/availability variables remain in ReadyShell BSS, while ring contents
and head are read and written in the loader-assigned state bank.

## 9. Responsibilities

- Resident code owns the shell loop, overlay boot/load/restore logic, generic
  external command dispatch, screen/platform glue, and REU coordination.
- Overlay 9 owns the interactive prompt editor while waiting for input.
- Overlay 1 owns lexing, parsing, parse support, and cleanup.
- Overlay 2 owns runtime values, variables, formatting, pipes, command lookup,
  capability classification, and shared execution helpers.
- External command overlays own command-specific disk logic, serialization
  logic, and overlay-local dispatchers.

## 10. Current Constraints

- Resident heap below the overlay load address is `3688` bytes in the current
  release map.
- `OVERLAY6` is currently the tightest overlay, with only `11` bytes left in the
  `$3800` live window.
- The assigned cache layout leaves `8192` bytes free at the tail of assigned
  bank 1, `8192` bytes free at the tail of assigned bank 2, and most of
  assigned bank 3 free after the small prompt-editor slot.
- External commands now pay a launcher/loader preload cost instead of repeated
  disk loads during each command call.
