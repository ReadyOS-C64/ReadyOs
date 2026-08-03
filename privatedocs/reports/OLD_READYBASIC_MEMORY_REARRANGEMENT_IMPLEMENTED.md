# OLD ReadyBASIC Memory Rearrangement Implementation

<!-- READYOS-CURRENT-CONTRACT-2026-08-02 -->
> **Current ReadyOS contract (2026-08-02):** Physical `Skip` is the ReadyOS
> bank and `Skip+1` is the first dynamic bank. The launcher snapshot occupies
> ReadyOS `$0000-$B5FF`; schema v5 occupies `$B600-$FFFF`, including the token
> map at `$B740` and status at `$B840`. C64 app RAM is `$1000-$C5FF` (`$B600`),
> and the resident 1 KB shim owns `$C600-$C9FF` with its public ABI at `$C800`.
> Dated layouts and measurements below are retained as historical evidence.

This is an old planning/proposal document kept in `privatedocs/reports` for
history. The memory rearrangement described here has already been implemented.
Do not use this as the main current reference; use
`src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md` and
`src/apps/readybasic/READYBASIC_LIFECYCLE_AND_REU_ARCHITECTURE.md` for the
live design.

This note records the implemented memory-reclaim branch. The original proposal
was to move shared frames, command workers, and suspend/resume state out of the
BASIC workspace while preserving the ReadyOS app contract. The branch implements
that plan; the current ReadyBASIC docs are the detailed live reference.

## Result

ReadyBASIC now gives BASIC an empty workspace of `33789` bytes (33.0K):

```text
BASIC text starts at $1C01
variables start at   $1C03
BASIC top is         $A000

$A000 - $1C03 = 33789 bytes (33.0K)
```

For comparison, stock C64 BASIC V2 gives about `38911` bytes (38.0K):

```text
stock BASIC text starts at $0801
stock BASIC top is         $A000

$A000 - $0801 = 38911 bytes (38.0K)
```

ReadyBASIC is now `5122` bytes (5.0K) below stock. Before this branch,
ReadyBASIC had `26109` bytes (25.5K) free, so the implemented reclaim is
`7680` bytes (7.5K).

## Current Measured ReadyBASIC Map

This document began as the memory-reclaim proposal, but the current build has
also added the 128-slot paged command registry, typed screen handles, and a
REU-backed 128-handle / 48KB typed heap. The implemented map now measures:

| Segment | Range | Size | Current role |
|---|---:|---:|---|
| `ENTRY` | `$1000-$1102` | `$0103` (259B) | Cold/warm entry and early copies. |
| `RESIDENT` | `$1200-$1BAF` | `$09B0` (2480B) | Visible parser, hooks, ROM calls, REU DMA, result commit. |
| `CMDPACK` load image | `$2800-$3FFF` | `$1800` (6.0K reserved) | Cold-only low/hidden overlay seed copied to REU bank `$45`. |
| `HIDLOAD` | `$4000+` | load-only | Cold-only hidden helper seed copied to `$A000` and `$C280`. |
| `BRLOAD` | `$4800+` | load-only | Cold-only bridge seed copied to `$C000`. |
| `REGSEED` | `$5000-$600F` | `$1010` (4112B) | Cold-only registry header and 128 descriptors copied to REU bank `$44`. |
| `HIDDEN` | `$A000-$A376` | `$0377` (887B) | Hidden helper under BASIC ROM. |
| `HIDDENPACK` | `$A800-$A84C` | `$004D` (77B) | Hidden worker overlay. |
| `LOWPACK` | `$A900-$AEDE` | `$05DF` (1503B) | Low command overlay under BASIC ROM. |
| `BRIDGE` | `$C000-$C19A` | `$019B` (411B) | Persistent bridge/state bytes. |
| Shared frames | `$C200-$C5FF` | `$0400` (1.0K) | Call/result frames, descriptor/name/page buffers, hidden shadow. |

The 128 descriptors live in REU bank `$44` at `$1000-$1FFF`. Lookup fetches a
256-byte page into `$C500`, scans eight descriptors, and copies a match to
`$C480`. `SCRCAP` is slot 13; `SCRPUT` is slot 128; zero-filled slots are empty
fillers.

The handle directory lives in REU bank `$44` at `$0800-$09FF`, the heap bitmap
at `$0C00`, and typed data pages at `$4000-$FFFF`. BASIC still has `33789` empty
free bytes before and after this expansion.

## Normal ReadyOS Interactive Run

For manual testing, run ReadyOS normally and do not override `runappfirst`:

```sh
bash ./run.sh --skipbuild
```

The default `precog-dual-d71` profile currently has `runappfirst=` blank, so
this boots the launcher instead of autoloading ReadyBASIC. From there,
ReadyBASIC can be launched manually through the ReadyOS menu.

Do not use invalid single-app verification paths such as `run.sh readybasic`.
ReadyBASIC should be exercised through the normal ReadyOS launcher/shim flow.

## Candidate Memory Areas

| Area | Current use | Candidate use | ReadyOS safety |
|---|---|---|---|
| `$2800-$3FFF` | Command-pack load image only after cold seed. | BASIC workspace after cold boot. | Implemented; warm resume uses REU copies. |
| `$2400-$27FF` | Former call/result/descriptor/name/page frames. | BASIC workspace. | Implemented by moving frames to `$C200-$C5FF`. |
| `$1C00-$23FF` | Former low overlay execution slot. | BASIC workspace beginning at `$1C01`. | Implemented by moving low command execution to `$A900+`. |
| `$9600-$99FF` | Former runtime snapshot. | BASIC workspace up to `$9FFF`. | Implemented by saving zero page/stack in REU bank `$44` offsets `$0A00/$0B00`. |
| `$9A00-$9FFF` | Former hidden helper shadow. | BASIC workspace. | Implemented by refreshing a visible shadow at `$C280` during `!EXIT`. |
| `$A000-$BFFF` RAM under BASIC ROM | Hidden helper, hidden overlay, low command overlay. | Main command overlay arena. | Implemented; final ROM restore happens in resident low RAM, not while executing under ROM. |
| `$C000-$C5FF` | Bridge plus spare visible bytes. | Bridge, shared frames, resume buffers, refreshed hidden shadow. | Implemented below `$C600`; ReadyOS metadata remains untouched. |
| `$C600-$C7FF` | ReadyOS REU metadata. | Not available. | Reserved. |
| `$C800-$C9FF` | ReadyOS shim ABI. | Not available. | Reserved. |
| `$D000-$FFFF` | I/O, ROM, KERNAL, machine state. | Avoid for ReadyBASIC workspace. | Not part of the normal ReadyOS app RAM contract. |

## Loading-Only Or Rarely Needed Code

Some ReadyBASIC bytes are only needed during cold seed, command loading, or
resume. These are good candidates for REU-backed relocation.

| Component | Current location | Lifetime | Reuse idea |
|---|---:|---|---|
| `CMDPACK` load image | `$2800-$3FFF` | Cold seed only. | Let BASIC own this range after `LOWPACK`/`HIDDENPACK` are copied to REU bank `$45`. |
| `REGSEED` | `$5000-$600F` (`$1010`, 4112B) | Cold seed only. | 128 descriptor slots; never reread after BASIC owns memory. |
| Runtime snapshot | former `$9600-$99FF` (`$0400`, 1.0K) | Needed only across `EXIT`/warm resume. | Save zero page/stack/mode directly to REU during `EXIT`; restore from REU on warm entry. |
| Hidden shadow | former `$9A00-$9FFF` (`$0600`, 1.5K) | Needed only to restore `$A000` helper after app switch. | Refresh the visible `$C280-$C5F6` (`$0377`, 887B) shadow during `EXIT`. |
| Low overlay slot | former `$1C00-$23FF` (`$0800`, 2.0K) | Needed only while a command is executing. | Run low workers from banked RAM under BASIC ROM instead, using the hidden-overlay discipline. |
| Shared frames | former `$2400-$27FF` (`$0400`, 1.0K) | Needed during parse/execute/commit, not as BASIC storage. | Move to mostly free visible RAM below `$C600`, `$C200-$C5FF` (`$0400`, 1.0K). |

The key distinction: code/data can be reused only if it is not needed while a
BASIC program is stored in that address range. Load-only seed bytes are easy.
Runtime parser, command dispatch, and result commit code are not easy because
they must remain callable whenever BASIC dispatches a statement.

## Implemented Rearrangement Stages

### Stage 1: Move Shared Frames To `$C200-$C5FF`

Move the fixed shared frame block out of `$2400-$27FF`:

| Frame | Old address | Implemented address |
|---|---:|---:|
| Call frame | `$2400` | `$C200` |
| Result frame | `$2500` | `$C300` |
| Descriptor buffer | `$2680` | `$C480` |
| Command buffer | `$26A0` | `$C4A0` |
| Page buffer | `$2700` | `$C500` |

Keep the bridge at `$C000-$C1BD` (`$01BE`, 446B); leave a small gap before `$C200`. Stay below
`$C600`, because `$C600-$C7FF` is ReadyOS REU metadata.

This made `$2400-$2FFF` BASIC workspace after cold seed.

Expected free BASIC RAM:

```text
$9600 - $2403 = 29181 bytes (28.5K)
gain over old baseline = 3072 bytes (3.0K)
```

### Stage 2: Move Command Workers Under BASIC ROM

Low command execution now runs from `$A900+` under BASIC ROM. The resident
visible code still performs parsing and result commit, while workers avoid
BASIC ROM calls during banked execution.

This made `$1C00-$23FF` available to BASIC and moved `BASIC_START` to `$1C01`,
immediately after the resident core.

Expected free BASIC RAM while keeping the current `$9600` top:

```text
$9600 - $1C03 = 31229 bytes (30.5K)
gain over old baseline = 5120 bytes (5.0K)
```

Design constraints:

- Worker overlays must not call BASIC ROM while BASIC ROM is banked out.
- The resident visible core still owns parsing and result commit.
- Hidden worker calls need the same careful `$0000/$0001` banking discipline as
  current hidden helper calls.
- The hidden helper at `$A000` and worker arena at `$A800+` need a clear layout
  so they do not overlap.

### Stage 3: Move Resume Snapshot And Hidden Shadow Out Of BASIC RAM

The runtime zero-page and stack snapshot moved from `$9600-$97FF` to REU bank
`$44` offsets `$0A00/$0B00`. Resume metadata stayed in the bridge. The hidden
helper shadow moved from `$9A00` to visible RAM at `$C280`; it is refreshed
during `!EXIT` because command frames may reuse that visible space while
ReadyBASIC is running.

This allowed BASIC top to move from `$9600` back to `$A000`, the normal
BASIC ROM boundary.

With Stage 2 also done:

```text
$A000 - $1C03 = 33789 bytes (33.0K)
gain over old baseline = 7680 bytes (7.5K)
remaining gap below stock = 5122 bytes (5.0K)
```

Design constraints:

- `EXIT` must save zero page, stack, SP, mode, and line-chain guards directly to
  REU before yielding to ReadyOS.
- Warm entry copies the visible `$C280` helper shadow back to `$A000` before any
  `$A000` helper call.
- If REU state is missing or corrupt, warm entry must fall back to a safe cold
  BASIC workspace rather than resuming bad pointers.
- ReadyBASIC must continue to re-mark REU bank ownership on warm resume.

## BASIC RAM Scenarios

| Scenario | BASIC start | BASIC top | Empty free bytes | Gain over old baseline |
|---|---:|---:|---:|---:|
| Old baseline | `$3001` | `$9600` | `26109` (25.5K) | `0` |
| Reclaim load-only `CMDPACK` only | `$2801` | `$9600` | `28157` (27.5K) | `+2048` (2.0K) |
| Move shared frames to `$C200` | `$2401` | `$9600` | `29181` (28.5K) | `+3072` (3.0K) |
| Move all command workers under ROM | `$1C01` | `$9600` | `31229` (30.5K) | `+5120` (5.0K) |
| Move workers under ROM and resume state to REU | `$1C01` | `$A000` | `33789` (33.0K) | `+7680` (7.5K) |
| Stock C64 BASIC V2 | `$0801` | `$A000` | `38911` (38.0K) | `+12802` (12.5K) versus old baseline |

The implemented high-value target is `33789` bytes (33.0K) free. Getting beyond that
would require moving or radically shrinking the visible resident core below
`$1C00`, or moving parts of resident dispatch into a banked/trampoline model.
That is much riskier because the execute hook must be callable from normal BASIC
statement dispatch and must use BASIC ROM helpers safely.

## Command Growth Model

The REU registry remains the right scaling model.

Current incremental cost for a new command that reuses an existing
parameter/result signature:

- `0` bytes of BASIC workspace.
- Usually `0` bytes of permanent resident RAM.
- `32` bytes for one descriptor slot in REU bank `$44` at `$1000-$1FFF`.
- Command implementation bytes in a packed REU code bank.
- Matching load-image bytes, unless future tooling can build command packs
  directly into REU-backed media.

If many more commands are added, the next design step should not be lowering
BASIC top or adding resident command bodies. It should be:

1. Keep descriptors in REU and fetch them a page at a time.
2. Add more packed command-code banks when `$45` fills.
3. Add a bank id or pack id to the descriptor format.
4. Keep shared parser/commit code resident only when several commands reuse it.
5. Store large command-private state in REU and expose BASIC integer handles.

## Implemented Rearranged Layout

This is the most attractive medium-risk target:

| Region | Size | Implemented use |
|---|---:|---|
| `$1000-$1102` | `$0103` (259B) | Tiny entry/warm trampoline. Keep only what must run before resident setup. |
| `$1200-$1BAF` | `$09B0` (2480B) | Visible resident parser, vector hooks, ROM calls, REU DMA, result commit. |
| `$1C00` | 1B | BASIC sentinel byte. |
| `$1C01-$9FFF` | `33789` free bytes (33.0K) | BASIC workspace after resume state moves to REU and command workers move under ROM. |
| `$A000-$A376` | `$0377` (887B) | Hidden helper, restored from load image on cold entry and from `$C280` on warm entry. |
| `$A800-$A84C` | `$004D` (77B) | Hidden command overlay arena. |
| `$A900-$AEDE` | `$05DF` (1503B) | Banked low command overlay arena. |
| `$C000-$C19A` | `$019B` (411B) | Bridge state, saved vectors, and current handle scratch. |
| `$C200-$C5FF` | `$0400` (1.0K) | Shared frames, resume buffers, and refreshed hidden helper shadow at `$C280`. |
| `$C600-$C7FF` | `$0200` (0.5K) | ReadyOS REU metadata; do not use. |
| `$C800-$C9FF` | `$0200` (0.5K) | ReadyOS shim ABI; do not use. |

This keeps the ReadyOS app-window contract intact and does not ask BASIC to use
RAM above `$A000`. It gets ReadyBASIC back to `33789` empty BASIC bytes (33.0K) while
still supporting many more commands through REU-packed overlays.

## Risks And Proofs Run

- Prove hidden-worker-only execution for every current command, not just `HCRC`.
- Prove no worker calls BASIC ROM while BASIC ROM is banked out.
- Prove shared frames at `$C200-$C5FF` do not collide with bridge growth or
  ReadyOS metadata.
- Prove `EXIT` can save runtime state to REU and warm entry can restore it
  before BASIC resumes.
- Prove missing/corrupt REU state falls back safely.
- Rebuild VICE probes to cover direct mode, stored programs, `IF ... THEN !`,
  strings/REM/DATA safety, arrays, handles, failures, `EXIT`, and cross-app
  resume under the new layout.
