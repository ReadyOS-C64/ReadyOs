# ReadyBASIC Command Module And Submodule Plan

This document is the design note for replacing ReadyBASIC's current
`LOWPACK` / `HIDDENPACK` command-code model with assembler-only command
modules, fixed under-ROM submodule slots, REU-backed residency tracking, and a
disk-loadable module format.

This document began as a plan, and now also records the first implemented proof
shape. The early implementation refactored the current command model while
preserving behavior, then added a deliberately small disk-module loader proof
after the internal module model was stable.

## Goals

- Make command placement and memory use consistent across the ReadyBASIC command
  catalog.
- Remove the special "hidden command" category. All machine-code commands run
  from RAM hidden behind BASIC ROM, so command descriptors should describe
  module/submodule residency rather than "low" versus "hidden" execution.
- Keep visible resident ReadyBASIC code growth extremely low. Prefer metadata,
  module payloads, and command helpers in REU-backed under-ROM code.
- Keep ReadyBASIC compatible with the ReadyOS app memory contract:
  - app-owned RAM is `$1000-$C5FF`;
  - `$C600-$C7FF` is ReadyOS resident shim expansion capacity and must not be used as scratch;
  - `$C800-$C9FF` is the ReadyOS shim ABI area and must not be used by
    ReadyBASIC modules.
- Keep built-in ReadyBASIC modules bundled into `readybasic.prg` and prestashed
  into REU on cold entry, so they do not cost steady-state BASIC workspace.
- Future-proof the architecture for disk-loaded modules and module-owned
  overlays without implementing disk loading in the first slices.

## Runtime Memory Model

ReadyBASIC continues to use the existing app layout discipline:

| Range | Owner | Plan |
| --- | --- | --- |
| `$1000-$11FF` | ReadyBASIC entry | Keep as tiny entry and cold/warm discriminator. |
| `$1200-BASIC_START-1` | Visible resident core | Keep small; only resident code should call BASIC ROM helpers. |
| `BASIC_START-$9FFF` | BASIC workspace | Former cold-load seed bytes are reclaimed after cold prestash. |
| `$A000-$BFFF` | RAM under BASIC ROM | New common area plus three 2KB submodule slots. |
| `$C000-$C1FF` | ReadyBASIC bridge | Persistent bridge/state only; must stay below shared frames. |
| `$C200-$C5FF` | ReadyBASIC shared frames | Call/result/descriptor/name/page buffers and hidden helper shadow. |
| `$C600-$C7FF` | ReadyOS shim expansion reserve | Do not use as ReadyBASIC scratch. |
| `$C800-$C9FF` | ReadyOS shim ABI | Do not place ReadyBASIC module code/data here. |

The under-BASIC-ROM area is divided into four 2KB regions:

| Region | Range | Role |
| --- | --- | --- |
| Common under-ROM | `$A000-$A7FF` | Existing hidden helper use and future resident-code relief. Not a normal pageable command slot. |
| Submodule slot 0 | `$A800-$AFFF` | Preferred default/system command slot. |
| Submodule slot 1 | `$B000-$B7FF` | Pageable command slot. |
| Submodule slot 2 | `$B800-$BFFF` | Pageable command slot and overlay target. |

Submodules are linked for fixed runtime addresses. They are not relocatable
binaries. A submodule may claim:

- one slot: `$A800`, `$B000`, or `$B800`;
- two adjacent slots: `$A800-$B7FF` or `$B000-$BFFF`;
- all three slots: `$A800-$BFFF`.

An exclusive 6KB submodule cannot assume the default/system slot 0 helpers are
present. It may call only resident ABI services and any code it owns or has
explicitly loaded.

## REU Layout

The first implementation keeps the current fixed ReadyBASIC REU bank split:

| Bank | Type | Role |
| --- | --- | --- |
| `$44` | `REU_RB_CORE` | Authoritative ReadyBASIC runtime metadata, command catalog, module catalog, residency table, handles, snapshots, and common/system data. |
| `$45` | `REU_RB_CODE` | Built-in command module and submodule payload bytes prestashed from `readybasic.prg`. |

Future disk-loaded modules should first use free space in bank `$45`. If there
is not enough space, the loader command may allocate another REU bank and extend
the module metadata to point at that bank. Resident startup does not allocate
extra module banks; dynamic bank allocation belongs to the future loader command.

Warm resume must reuse REU bank `$44` and `$45` contents and must not reread
load-only seed data from BASIC-owned RAM.

## Module Model

A module is a named command package. A module owns one or more submodules and
may also own overlay payloads that are copied into a submodule slot on demand.

The default built-in system module should use submodule slot 0 for commands and
helpers that should usually remain resident:

- REU helper commands;
- shared command dispatch helpers;
- module-loading helper command stubs;
- small command helpers that other commands may call.

Other built-in modules may use slot 1 and slot 2, or may claim larger adjacent
slot ranges if the command family benefits from shared local helpers.

The first built-in demo shape should prove at least two internal modules:

- a default/system module with slot 0 populated;
- a second module that uses slot 1 and slot 2, including at least one command
  that demonstrates non-slot-0 dispatch.

Later demo work should prove multi-slot and overlay behavior.

## Submodule Residency

ReadyBASIC should track what is currently loaded in each submodule slot. The
authoritative table lives in REU bank `$44`; resident RAM should keep only the
minimum scratch needed for dispatch.

Each live slot record should model:

| Field | Meaning |
| --- | --- |
| Slot id | `0`, `1`, or `2`. |
| Module id | Module currently occupying the slot. |
| Submodule id | Submodule currently occupying the slot. |
| Overlay id | `0` for the base submodule, nonzero for a loaded overlay. |
| Generation/checksum | Small validation value to reject stale residency. |
| Slot mask | Which slots are occupied by this loaded payload. |

Dispatch flow:

1. Lookup command name in the command catalog.
2. Read module id, submodule id, optional overlay id, slot mask, entry offset,
   and payload location from metadata.
3. Check the residency table.
4. If the required payload is already resident with matching module,
   submodule, overlay, and generation/checksum, skip REU fetch.
5. Otherwise fetch the payload from REU into the fixed slot range and update
   the residency table.
6. Call the entry point under BASIC ROM RAM.
7. Return to visible resident code for result commit.

For a multi-slot submodule, all claimed slot records must be updated together.
For overlays, the overlay target slot record replaces the previous overlay
record, while sibling stable submodule slots remain resident.

## Command Descriptor Direction

The current descriptor split between `LOW` and `HIDDEN` should be replaced with
module-aware fields. The exact byte packing can change during implementation,
but the descriptor must carry these concepts:

| Concept | Purpose |
| --- | --- |
| Command id | Stable internal command id. |
| Signature id | Resident parser/result contract. |
| Name length and name bytes | Visible ReadyBASIC command text. |
| Module id | Which module owns the command. |
| Submodule id | Which submodule must be resident before call. |
| Overlay id | Optional overlay required before call. |
| Slot mask | Required under-ROM slot footprint. |
| Entry offset | Offset from the submodule runtime base. |
| Flags | Expression-safe, statement-only, internal/helper, exclusive, or future loader flags. |

Parser signatures remain resident and conservative. BASIC-facing parsing,
BASIC ROM calls, output variable clearing, and result commit continue to happen
in visible resident code. Worker submodules read the call frame and write the
result frame.

## Module File Format

ReadyBASIC can use its own disk format for future module files. The format
should be assembler-friendly and streamable from disk into REU.

All multi-byte integer fields are little-endian.

### Header

| Field | Size | Meaning |
| --- | ---: | --- |
| Magic | 4 | ASCII module magic, proposed `RBMD`. |
| Format version | 1 | Initial version is `1`. |
| Required API version | 1 | Minimum ReadyBASIC module ABI version. |
| Header size | 2 | Byte size of the fixed header. |
| Module id | 1 | Stable id inside ReadyBASIC runtime. |
| Module flags | 1 | Built-in, disk-loaded, exclusive-capable, has overlays, etc. |
| Submodule count | 1 | Number of submodule directory records. |
| Overlay count | 1 | Number of overlay directory records. |
| Command count | 1 | Number of command directory records. |
| Reserved | 1 | Must be zero in v1. |
| Total payload size | 2 | Total payload bytes following metadata. |
| Required slot mask | 1 | Slots needed for the module's minimum useful command set. |
| Max live slot mask | 1 | Maximum simultaneous slot footprint. |
| Directory offset | 2 | Offset to the first directory record. |
| Payload offset | 2 | Offset to payload blob area. |
| Header checksum | 2 | Optional; zero means not checked in v1. |

### Submodule Directory Record

| Field | Size | Meaning |
| --- | ---: | --- |
| Submodule id | 1 | Stable id within module. |
| Slot mask | 1 | Slot or adjacent slots claimed at runtime. |
| Runtime base | 2 | `$A800`, `$B000`, or `$B800`. |
| Payload bank hint | 1 | Initial built-ins use `$45`; future disk modules may use another REU bank. |
| Payload offset | 2 | Offset inside the payload REU bank. |
| Payload size | 2 | Bytes to fetch into the slot range. |
| Entry table offset | 2 | Offset to optional local entry table; zero if unused. |
| Generation/checksum | 1 | Small residency validation value. |
| Flags | 1 | Shared, exclusive, callable-helper, default-sticky, etc. |

### Overlay Directory Record

| Field | Size | Meaning |
| --- | ---: | --- |
| Parent submodule id | 1 | Stable submodule that manages this overlay. |
| Overlay id | 1 | Overlay id within parent submodule. |
| Target slot | 1 | Slot receiving the overlay payload. |
| Slot mask | 1 | Usually one slot; may be more if explicitly supported. |
| Payload bank hint | 1 | Initial built-ins use `$45`; future disk modules may use another bank. |
| Payload offset | 2 | Offset inside the payload REU bank. |
| Payload size | 2 | Bytes to fetch into the target slot. |
| Entry offset | 2 | Entry offset from target slot runtime base. |
| Generation/checksum | 1 | Small residency validation value. |
| Flags | 1 | Internal, callable, destructive, etc. |

### Command Directory Record

| Field | Size | Meaning |
| --- | ---: | --- |
| Command id | 1 | Stable internal command id. |
| Signature id | 1 | Resident parser/result contract. |
| Module id | 1 | Owning module. |
| Submodule id | 1 | Required submodule. |
| Overlay id | 1 | Required overlay, or `0` for base submodule. |
| Slot mask | 1 | Required live footprint. |
| Entry offset | 2 | Entry offset from runtime base. |
| Flags | 1 | Expression-safe, statement-only, helper/internal, etc. |
| Name length | 1 | Length of uppercase visible command name. |
| Name bytes | 16 | Uppercase command name, zero padded. |

The exact record sizes can be tuned during implementation, but the v1 format
must preserve these semantics. A build-time packer may emit this format from
assembler symbols rather than requiring hand-maintained byte offsets.

## Overlay Behavior

Some modules will use more code than can be live at once. A module may keep a
stable submodule resident and rotate overlays through another slot.

Example file-oriented module:

- Slot 1: stable file-command dispatcher and shared file helpers.
- Slot 2: overlay target.
- Module file payload:
  - one 2KB slot-1 submodule;
  - five 2KB slot-2 overlays.
- Total module payload in REU: 12KB.
- Maximum live footprint: 4KB.

The stable slot-1 code is responsible for loading the correct slot-2 overlay
before calling overlay-local entry points. This is more tedious for module
authors, but it lets ReadyBASIC run much more code without expanding resident
RAM or requiring every command family to occupy all three slots at once.

## Built-In Module Prestash

Built-in modules are bundled into `readybasic.prg` as cold-load seed bytes.
During cold ReadyBASIC entry:

1. Hidden/setup code marks `$44` as `REU_RB_CORE` and `$45` as `REU_RB_CODE`.
2. Module metadata is copied into bank `$44`.
3. Built-in module payload bytes are copied into bank `$45`.
4. The module catalog records each payload's REU bank and offset.
5. BASIC workspace is initialized and the seed ranges are treated as reclaimed.

During warm resume:

1. Re-mark `$44` and `$45` ownership.
2. Restore hidden helper/common code as the current implementation already does.
3. Reuse module metadata and payloads from REU.
4. Do not reread built-in module seed bytes from C64 RAM.

The first pass should continue to use the current fixed banks. Extra REU bank
allocation is deferred to the future disk module loader command.

## Disk Module Loader

The first disk module loader proof is implemented as a command in the second
built-in module, not as resident startup code. The BASIC-visible command name is
`ZMODLD(name$)`, intentionally avoiding the tokenized `LOAD` keyword inside the
command text. The loader command lives in slot 1/module 2 and uses the resident
REU stash ABI to register descriptors and payload chunks.

`ZMODLD()` returns the number of command descriptors registered. Failure returns
a nonzero diagnostic value in the `240` range for the current proof build.

The implemented proof loader:

1. Opens a ReadyBasicModule SEQ package named `rbm.<name>`.
2. Streams the file through the existing `$C500` page buffer.
3. Validates magic, format version, descriptor counts, and payload bounds.
4. Copies ready-made command descriptors into REU bank `$44`.
5. Copies payload chunks into REU bank `$45`.
6. Invalidates the tiny current residency proof state.
7. Leaves resident startup and BASIC free bytes unchanged.

The proof format is intentionally smaller than the long-term `RBMD` format
above, but it no longer depends on a one-page PRG load. It is built by
`build_support/build_readybasic_disk_modules.py`.

### Implemented Proof File Format

The generated files are SEQ module packages, not PRGs. They do not have a load
address. The byte stream is:

| Offset | Size | Meaning |
| --- | ---: | --- |
| `0` | 4 | Magic `RBM!`. |
| `4` | 1 | Format version, currently `1`. |
| `5` | 1 | Module id. |
| `6` | 1 | Command descriptor count, maximum `32` in this proof. |
| `7` | 1 | Payload chunk count, maximum `32` in this proof. |
| `8` | 2 | Destination offset in REU bank `$44` for descriptors. |
| `10` | 6 | Reserved, zero. |

The descriptor blob follows the 16-byte header directly. Payload records follow
the descriptor blob; each record has a six-byte header immediately followed by
its payload bytes:

| Field | Size | Meaning |
| --- | ---: | --- |
| REU offset | 2 | Destination offset in REU bank `$45`. |
| Size | 2 | Payload size in bytes. |
| Flags/reserved | 2 | Reserved for future proof work. |

Command descriptors use the normal 32-byte ReadyBASIC descriptor layout. Payload
chunks are assembler-language command workers linked/generated for fixed slot
addresses.

### Sample Disk Modules

The current build generates and places these files on ReadyBASIC-capable disk
profiles:

| Disk file | Module | Commands | Layout proved |
| --- | --- | --- | --- |
| `rbm.sample1` | Module 3 | `ZDM1()` returns `61` | Single slot-1 submodule. |
| `rbm.sample2` | Module 4 | `ZDM2S()` returns `74`; `ZDOV1()`/`ZDOV2()` return `72`/`73` | Two-slot slot 1+2 span plus two slot-2 overlays. `ZDOV1` and `ZDOV2` both use submodule 5 because they are different overlays of the same submodule family. |
| `rbm.sample3` | Module 5 | `ZSAA()`-`ZUEB()` return stateful integer sentinels | Larger package with 30 descriptors. The command names encode submodule, overlay, and entrypoint: `ZSAA` is submodule 6, overlay 1, entry A; `ZSAB` is the second entrypoint in that same resident overlay image. Each overlay image carries one byte of local state so tests can prove when a resident overlay is reused versus freshly reloaded. `ZT*` and `ZU*` cover submodules 7 and 8. |

The long-term loader should still:

1. Open a module file from disk.
2. Validate magic, format version, API version, sizes, and checksums.
3. Find free space in ReadyBASIC's module-code REU area.
4. Copy the module payload to bank `$45` when space allows.
5. Allocate an additional REU bank only when `$45` cannot hold the payload.
6. Register module/submodule/overlay/command metadata in bank `$44`.
7. Reject command-name collisions unless a later explicit replace policy is
   designed.

Resident ReadyBASIC startup should not implement this logic. The loader is a
command/module feature so the resident core stays lean.

## Implementation Slices

### Slice 1: Descriptor Refactor

- Replace `LOW` / `HIDDEN` command categories with module/submodule descriptor
  fields.
- Preserve all current command behavior.
- Keep parser signatures, call frame, result frame, and commit behavior stable.
- Convert `ZHIDDENRAM` from a special hidden command into a normal submodule
  command that happens to run under BASIC ROM RAM.

### Slice 2: 2KB Submodule Slots

- Split command payloads into fixed 2KB-aligned slot images.
- Add slot residency checks before REU fetch.
- Fetch only the required slot or adjacent slot range.
- Update the static checker so slot boundaries are guarded.

### Slice 3: Two Built-In Modules

- Build two internal ReadyBASIC modules into the cold-load PRG seed image.
- Prestash their payloads into bank `$45`.
- Register module/submodule/command metadata in bank `$44`.
- Keep the default/system module's slot 0 helpers normally resident.

### Slice 4: Demo Coverage

- Convert current demo commands so tests prove:
  - slot 0 command dispatch;
  - slot 1 command dispatch;
  - slot 2 command dispatch;
  - a two-slot submodule;
  - a full three-slot exclusive submodule or a documented equivalent if code
    size is not yet available;
  - overlay rotation through one target slot.

### Slice 5: Disk Loader Proof

- Add a small loader command in module 2/slot 1, not resident startup.
- Generate two sample disk module PRGs and place them on ReadyBASIC-capable
  disk profiles.
- Register disk-loaded descriptors in REU bank `$44` and payloads in `$45`.
- Prove a single-slot disk module, a two-slot disk module, and overlay-style
  disk module commands.
- Keep extra-bank allocation behind the future full loader boundary.

### Slice 6: Guardrails And Docs

- Update `build_support/verify_readybasic_plugin.py` to guard new slot ranges,
  metadata offsets, and bank ownership assumptions.
- Update `READYBASIC_MICROMODULE_SYNC.md` with new module/submodule constants.
- Update current design and command-making docs after implementation lands.
- Extend the ReadyBASIC demo suite with residency and overlay probes.

## Test Plan

Static checks:

- Visible resident code still fits below `BASIC_START`.
- Bridge remains below `$C200`.
- Shared frames remain `$C200-$C5FF`.
- No ReadyBASIC scratch or code enters `$C600-$C9FF`.
- `$A000-$A7FF`, `$A800-$AFFF`, `$B000-$B7FF`, and `$B800-$BFFF` are
  non-overlapping and 2KB-aligned.
- Built-in module payload metadata points at valid REU offsets and valid slot
  masks.
- Warm-resume paths do not read load-only seed ranges.

Functional probes:

- Existing ReadyBASIC demo suite still passes.
- A command in an already-loaded slot does not recopy from REU.
- Calling a command in another submodule copies only the needed slot range.
- Multi-slot submodules load and execute correctly.
- Overlay submodules can rotate several payloads through the same target slot.
- Warm resume reuses REU metadata/payloads and preserves BASIC program state.
- Failure paths leave output variables cleared and report ReadyBASIC errors as
  before.

Verification must use normal ReadyOS startup through plain `run.sh` / `run.ps1`
and boot ReadyOS itself. Do not verify ReadyBASIC by directly launching a
single-app PRG path.

## Graphics Note

Graphics commands are not part of this first module/submodule implementation.
When graphics modes are added, the goal is to use VIC bank D and memory behind
KERNAL ROM carefully, while remaining compatible with ReadyOS shim usage. In
particular, graphics code must avoid `$C800-$C9FF` unless it is intentionally
using the shim ABI, and bitmap/screen/charset placement must be designed around
ReadyOS's existing resident areas.

## Defaults And Assumptions

- Metadata is authoritative in REU bank `$44`.
- Built-in module and submodule payload bytes live in REU bank `$45`.
- Disk-loaded modules first try to use free space in `$45`; only the future
  loader command may allocate additional REU banks.
- All module code is assembler and linked for fixed runtime slot addresses.
- The first shipped implementation should prioritize internal module/submodule
  correctness over a general disk loader.
- Resident code growth is treated as a regression unless it removes more code
  from visible RAM than it adds.
