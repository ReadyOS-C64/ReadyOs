# ReadyBASIC Lessons Learnt

This is the running lab notebook for ReadyBASIC. Keep entries small, falsifiable,
and updated when a hypothesis turns out to be wrong. The goal is to preserve the
actual C64/ReadyOS evidence trail rather than a pile of confident guesses.

## Current Model

- ReadyBASIC is a ReadyOS-native PRG host for BASIC, not a normal C64 BASIC PRG.
- The app PRG loads at `$1000` and must obey the ReadyOS app/shim window:
  `$1000-$C5FF` is app-owned, `$C600-$C9FF` is reserved metadata/shim space.
- BASIC programs are data inside the host. The scoped BASIC workspace is now
  `$2AC1-$9FFF`, with `30013` formula empty free bytes (29.3K); ReadyBASIC
  extension lines are left as readable text rather than crunched into private
  tokens.
- ReadyBASIC suspend state keeps zero-page and stack snapshots in the
  launcher-assigned ReadyBASIC core bank at offsets `$0A00/$0B00`; saved SP,
  mode, and line-chain guards live in bridge metadata.
- Hidden services live under BASIC ROM RAM at `$A000-$BFFF` and visible
  trampolines/state/mailbox live at `$C000-$C5FF`.
- A refreshed REU shadow copy of the hidden helper image lives in the
  launcher-assigned ReadyBASIC core bank at offset `$3000`. Warm entry restores
  `$A000` from that shadow before using hidden helpers.
- The plugin spine keeps visible resident code at `$1200-$2ABE`, command
  overlays under BASIC ROM, shared frames at `$C200-$C5FF`, and
  launcher-assigned ReadyBASIC core/code REU banks. Older entries below may
  mention `$44/$45`; treat those as historical fixed-bank examples, not the
  current contract.
- Module/submodule branch update: the current visible resident range is
  `$1200-$2ABE`, the bridge is `$C000-$C1FD`, and command payloads now use a
  common under-ROM helper area `$A000-$A7FF` plus three 2KB submodule slots:
  `$A800-$AFFF`, `$B000-$B7FF`, and `$B800-$BFFF`.
- Current command descriptors are module-aware 32-byte records: command id,
  module id, payload offset/size in the assigned code bank, submodule id,
  overlay id, slot mask, generation/check byte, runtime destination, entry
  offset, signature id, and name.
- Built-in payload bytes are prestashed into the assigned code bank: module 1 slot 0 at
  `$0000-$06CD`, module 2 slot 1 at `$06CE-$0908`, slot 2/span/overlay proofs
  through `$095C`, and disk sample payloads currently starting at `$3000`.
- The current registry has 128 descriptor slots in REU bank `$44` at
  `$1000-$1FFF`. Lookup fetches one 256-byte page at a time into `$C500`, scans
  eight descriptors locally, and copies a match into `$C480`.
- The current typed handle system supports 128 live handles. Handle descriptors
  live at REU `$44:$0800-$09FF`, the 192-page bitmap at `$44:$0C00`, and the
  48KB typed heap at `$44:$4000-$FFFF`; bridge RAM keeps only current-handle
  scratch.
- Cold entry prestashes `CMDPACK`, hidden helper/bridge seeds, and `REGSEED`
  into their runtime locations or REU. Warm resume must reuse those runtime/REU
  copies rather than rereading load-only addresses that BASIC may now own.

## Live Discipline Notes

### Distinguish App RAM From Shared ReadyOS Metadata

ReadyOS app snapshots own `$1000-$C5FF`. `$C600-$C7FF` is not app-private RAM,
but it is also not unused; ReadyOS uses it for shared REU metadata. The launcher
marks ReadyBASIC's assigned `rbcore` banks as `REU_RB_CORE` and `REU_RB_CODE`,
and ReadyBASIC may refresh those exact ownership tags after resolving them. It
must never treat that page as a general ReadyBasic buffer. If a future probe
sees boot or app-load instability, check whether ReadyBasic is writing more
than those ownership tags before blaming the launcher or VICE.

### Interrupted VICE Runs Still Need A Ledger Entry

When a visible run is killed before the harness times out, record it as
interrupted rather than pass/fail. Capture what stage was active, what artifacts
exist, whether any VICE/dotnet processes were left behind, and which next run
must produce a monitor dump if the same symptom repeats. The 2026-05-11
`vice_auto_20260511_201407` run reached `wait_readybasic_prompt` but was stopped
before a dump, so it proves only that the cold boot appeared stalled from the
UI, not which address or routine was stuck.

## Proven

### Hidden Helper Calls Need Their Own CPU-Port Save

Proven on 2026-06-09 while debugging the ReadyBASIC hotkey branch. The common
hidden-call trampoline may run while a LOWPACK command is already executing under
BASIC ROM. Sharing the same saved CPU-port byte used by the under-ROM payload
call path can restore the wrong `$01` value after the nested helper returns,
leaving BASIC ROM/helper visibility confused and causing later parser failures.

Current rule: shared helper trampolines such as `call_hidden_common` must use a
distinct CPU-port save byte from LOWPACK/under-ROM payload call machinery.

### ReadyBASIC Hotkeys Must Prove The BASIC-Editor Path

Proven on 2026-06-09 after `input.sequence [2]` appeared to test Ctrl+B but only
fed a byte to the BASIC editor path, where it could render as text and bypass the
actual key decode logic. ReadyBASIC runs inside the ROM editor, so hotkey tests
must assert the installed vectors and exercise the ReadyBASIC keylog/IRQ decode
or a real matrix path, then prove the internal dispatcher reaches the normal
ReadyOS suspend/switch flow without typed `EXIT`.

Current rule: do not accept a ReadyBASIC hotkey probe that only seeds
`rb_hotkey_pending`, injects `EXIT`, or feeds KERNAL keyboard-buffer bytes.

### Lean Plugin Spine Needs Seed-Only Tables Outside Resident RAM

Proven on 2026-05-11 while moving ReadyBASIC from demo `RB` commands to the
REU plugin spine. Keeping command descriptors and REU prestash code in the
visible resident core overflowed `$1200-$1BFF`. Moving descriptor seed data to a
load-only `REGSEED` segment, moving prestash into hidden helper code, and moving
handle/page allocation into the low overlay pack brought the resident core down
to `$08BF` bytes.

Current rule: resident low RAM keeps BASIC-facing parser, variable commit,
overlay loading, and ROM helper calls. Seed tables and non-BASIC worker logic
belong in hidden helpers or packed overlays whenever possible.

Verification artifact:
`make bin/readybasic.prg` produced `RESIDENT $1200-$1ABE`,
`LOWPACK $1C00-$1EEE`, `HIDDEN $A000-$A141`, `HIDDENPACK $A800-$A82F`,
and `BRIDGE $C000-$C075`.

### Raw `!COMMAND args` Preserves Listing Text

V1 deliberately does not install a private command token. The crunch hook
forwards to ROM BASIC first, and ReadyBASIC recognizes raw `!` text from the
execute vector. Regular BASIC `LIST` therefore shows the `!` prefix and command
name. The only crunch-time rewrite is the `IF ... THEN !COMMAND` edge: after ROM
tokenization, a real `THEN` token followed by `!` is changed to `THEN :!` so ROM
BASIC re-enters the normal statement dispatcher.

Current rule: do not add private token support without a matching lister and a
proved `ICRNCH` length/register contract. Keep the `THEN !` normalizer tiny and
post-ROM-crunch so quoted strings, `REM`, and `DATA` text are not scanned as
commands.

### Menu Resume Must Prove Screen And BASIC State

Proven on 2026-05-11 with the visible 47-step plugin probe:
`vice_auto_20260511_202329`. The acceptance path must launch ReadyBasic through
ReadyOS, exercise direct commands, type `EXIT`, return to the launcher, relaunch
ReadyBasic through the menu, and then prove both:

- The ReadyBasic READY screen is redrawn instead of leaving the launcher menu
  underneath `READY.`.
- BASIC variables survive the app snapshot round trip; the probe seeds
  `V%=321:VS$="OK"` before `EXIT` and checks `STATE 321 :OK` after resume.

Current rule: do not use `CTRL+3` as a substitute for menu re-entry in
ReadyBasic resume acceptance. The hotkey path can hide launcher selection and
screen redraw mistakes.

### 6502/C64 Assembly Discipline That Mattered Here

- Treat `$0000` and `$0001` as a pair. `$0001` only drives memory banking bits
  whose data-direction bits in `$0000` are configured as outputs. ReadyOS can
  leave `$0000` in a state that makes a normal-looking `$01` value ineffective.
- Do not map out KERNAL ROM while calling KERNAL routines. Hidden helper calls
  that only touch RAM can use RAM-under-BASIC-ROM mapping; helpers that call
  `SETLFS`, `OPEN`, `CHRIN`, `CHROUT`, etc. must keep KERNAL visible.
- Keep interrupt state explicit around banking changes. Save flags with `PHP`,
  `SEI` before remapping, restore `$01`, then `PLP`.
- Do not trust registers or flags after probing ROM BASIC unless the ROM
  contract says they are yours to change. Wedge fallbacks must leave `TXTPTR`,
  accumulator/flags expectations, and line length behavior intact.
- Page-zero BASIC pointers are live interpreter state, not cache. `TXTTAB`,
  `VARTAB`, `ARYTAB`, `STREND`, `FRETOP`, `MEMSIZ`, `TXTPTR`, and the BASIC
  line-link chain must agree before entering ROM BASIC.
- Global vectors in page 3 are outside the ReadyOS app snapshot. Restore owned
  vectors before yielding through the shim, and reinstall only after app memory
  is restored.

### Cold/Warm Entry Must Not Trust `$C000` First

Cold launcher loads can leave stale bytes in `$C000-$C5FF`. A cold/warm decision
based first on bridge magic at `$C000` can falsely take a resume path before the
bridge has been copied into place.

Current rule: use an entry-local cookie in the `$1000` entry segment as the
first discriminator. A disk load resets that cookie from the PRG image; a REU
resume preserves it in the app snapshot.

### `$01` Restore Must Happen Last

Hidden restore state under `$A000` is only readable while RAM is mapped under
BASIC ROM. Restoring saved `$0001` too early can make BASIC ROM visible again
mid-copy, so the rest of the restore reads ROM instead of the hidden buffer.

Current rule: keep RAM-under-ROM forced while restoring, copy stack and zero page
first, and restore `$0000/$0001` last.

### Hidden `$A000` Helpers Are Outside The ReadyOS Snapshot

Proven by the `EXIT` resume crash. The ReadyOS shim snapshots `$1000-$C5FF`
when an app returns to the launcher. ReadyBASIC's hidden helper code under
`$A000-$BFFF` is not part of that transfer, so warm entry cannot assume it is
still valid after the launcher or another app has run.

Current rule: keep the hidden helper shadow in the assigned ReadyBASIC core
bank at offset `$3000`, refresh it during cold seed and manual prompt `EXIT`,
and restore `$A000` from that REU shadow on every warm entry. BASIC top is
`$A000`; the old `$9A00-$9FFF` shadow reservation has been reclaimed.

### `$0000` DDR Is Part Of The Banking Contract

Proven on 2026-05-09 with the binary-monitor probe. ReadyOS/launcher state left
`$0000 == $12`, so writing a value such as `$36` to `$0001` did not reliably
drive the LORAM banking bit. Hidden calls intended to run from RAM under BASIC
ROM could accidentally execute BASIC ROM bytes around `$A000` instead of
ReadyBASIC's helper code.

Current rule: before every hidden-helper call, force the low three bits of
`$0000` to outputs. Non-KERNAL hidden helpers use `$01 & $FD` so HIRAM makes
RAM visible under `$A000`; KERNAL-calling file helpers use `$01 & $FE` so KERNAL
ROM stays callable.

### `CHRIN` Hooks Must Preserve Carry Semantics

BASIC checks carry after KERNAL input calls. The VICE C64 ROM disassembly shows
the BASIC/KERNAL wrapper around `CHRIN` branches to the BASIC error path if
carry is set after `$FFCF`.

Bug seen: `print "hello"` executed, then reported `?C error`. That was caused by
ReadyBASIC calling original `CHRIN`, comparing the returned byte with hotkeys,
and returning with the carry flag left by `CMP`.

Current rule: if original `CHRIN` returns carry set, return carry set unchanged;
for ordinary successful input, explicitly return carry clear.

### Gate Prompt Hooks By Current Input Device

`DFLTN` at `$99` is the current/default input device. Value `0` means keyboard.
ReadyBASIC must only treat bytes as app navigation when `DFLTN == 0`; otherwise
file/device input used by BASIC or `RB 10/RB 11` must pass through untouched.

### BASIC Prompt `CHRIN` Does Not Return Every Key

At the READY prompt, KERNAL `CHRIN` enters the screen editor and usually returns
only after Return, not after every keypress. That means a wrapper around `$0324`
can be too late to see `CTRL+B` or `F2`.

Revised rule: do not busy-wait in the `$0324` `CHRIN` vector before ROM's screen
editor runs. The screen editor owns cursor blink, logical-line editing, keyboard
buffer draining, and Return handling. Prompt-level ReadyOS navigation needs a
deeper editor-safe hook or a later explicit command, not a pre-editor blocking
loop.

### ReadyBASIC Must Restore Global Vectors Before Shim Yield

ReadyOS snapshots the app window `$1000-$C5FF`, but BASIC/KERNAL vectors in page
3 are global machine state outside that snapshot. If ReadyBASIC yields through
`$C80C/$C80F` with `$0304/$0306/$0308` or `$0324/$032A` still pointing into its
bridge, the launcher or next app can run with vectors into stale app memory.

Current rule: cold entry resets KERNAL I/O vectors with `$FF8A` and BASIC
vectors with `$E453`, saves the originals once, installs ReadyBASIC vectors only
while active, and restores the originals before every shim yield.

### Do Not Replace The BASIC Error Vector With `$A43A`

The stock `$0300` vector points at `$E38B`, not directly at `$A43A`. `$E38B`
checks for negative `X` values used by the ROM STOP/end-of-direct-command path
and returns to READY when appropriate. Pointing `$0300` directly at `$A43A`
bypasses that wrapper and can turn a successful direct command into a spurious
`?C error`.

Current rule: ReadyBASIC only owns the vectors it needs for the wedge
(`$0304/$0306/$0308` for now). `$0300/$0302/$030A` are preserved from the ROM
defaults unless a future hook deliberately wraps and preserves their contracts.

### IGONE Fallback Should Tail-Call The Saved Original Vector

The BASIC command dispatcher enters via `$0308`; the ROM path calls `CHRGET` and
then relies on the text pointer and processor flags in ways that are easy to
disturb. A wedge that probes a statement and then jumps into the middle of the
ROM dispatcher can leave direct-mode commands apparently working but followed by
spurious BASIC errors.

Current rule: probe the next non-space byte without changing `TXTPTR`. If the
statement is not ReadyBASIC's command, jump through the saved original `$0308`
vector so ROM BASIC performs its own `CHRGET`/dispatch path from an untouched
state. Only after raw `!` is proven does ReadyBASIC advance `TXTPTR`.

### Parser Scratch And Register State Are Functional ABI

Reinforced on 2026-05-22 during the 128-slot registry work. Paged descriptor
lookup, token-name handling, and parameter parsing are not independent pieces:
they share the live BASIC interpreter path. A change that only appears to touch
command lookup can still perturb parser scratch, `TXTPTR`, `Y`, processor flags,
or the conditions that ROM helpers such as `CHRGOT`, `FRMNUM`, `GETADR`, and
`PTRGET` expect.

The concrete regression was that pointer/register preservation was relaxed while
renaming and refactoring lookup. Direct commands still reached parts of the
dispatcher, but parser-sensitive commands such as `BUFNEW` and output-variable
forms could fail because the follow-on BASIC ROM helper contract was no longer
exactly intact. The fix was to restore the parser contract, including ensuring
the whitespace-skip/`CHRGOT` path leaves `Y` in the expected state before
parameter parsing continues.

Current rule: no functional regression is a hard acceptance criterion. Any
change to command lookup, name token handling, `TXTPTR` movement, shared scratch
buffers, low/hidden overlay dispatch, or BASIC ROM helper setup must rerun the
parser-sensitive probes, not just the new feature probe. At minimum that means
direct commands, stored-program commands, `IF ... THEN !`, colon chains, string
input/output, array input/output, handle commands, error clearing, and resume.
If the change touches tokenization or command names, the tokenizer/list/run
matrix is also mandatory.

### Keep Large Handle Tables Canonical In REU

Proven on 2026-05-22 while expanding from eight handles to 128. A direct bridge
RAM table would have spent hundreds of bytes below `$C200` and reduced space for
future state. The working model keeps canonical handle descriptors and the heap
bitmap in REU, pages them through `$C500`, and stores only the current
bank/page/page-count/type descriptor in bridge scratch.

This preserved `BASIC_START=$1C01`, `BASIC_LIMIT=$A000`, and the `33789` empty
BASIC free-byte count at the time. Later native `PROC`/`FUNC` support moved
`BASIC_START` to `$2101`; the handle lesson still stands because its cost moved
into fetched overlay code rather than forcing that relocation.

### ICRNCH Must Preserve The Tokenized Line Length

The BASIC line insertion path calls the crunch vector at `$0304`, then stores
`Y` as the tokenized line length. Any custom cruncher that changes the line but
returns the wrong `Y` can corrupt line insertion.

Current POC rule: do not tokenize ReadyBASIC commands yet. `ICRNCH` must call
the original ROM cruncher first, preserve its returned line length contract, and
only then apply tiny normalizers such as `THEN COMMAND(...)` to
`THEN :COMMAND(...)` and `THEN EXEC` to `THEN :EXEC`. Proper private token
support must be reintroduced only with a cruncher that preserves all required
register and buffer contracts.

2026-05-11 follow-up: a private `$CC` token experiment made stored `!` lines
compact, but the visible probe then blanked/crashed during `LIST`. The stable
branch deliberately removes that crunch/list experiment and treats private
tokenization as future work requiring a separate lister contract probe.

2026-05-11 program-mode follow-up: raw stored `!COMMAND args` is viable without
a private token as long as the BASIC line-chain and `$0308` contracts are intact.
The new program probe verifies `LIST` plus `RUN` for scalar, string, hidden,
array, handle, and error-path commands. Private token support remains separate
future work.

### PROC/FUNC Formals Are BASIC Globals

Proven on 2026-05-22 while adding native `PROC`/`FUNC`. Binding formals through
BASIC ROM `PTRGET` means those formals are ordinary C64 BASIC variables, not
locals. A function declared as `FUNC ADDI A%,B%,R%` and called as
`EXEC ADDI,4,5,A%` will clear the caller's output actual while parsing it; because
the first formal has the same BASIC variable name, the function then sees `A%=0`.

2026-05-23 expression-branch follow-up: `FUNC` definitions now avoid a dummy
output formal and return through `RET expr`, with optional `RET% expr` and
`RET$ expr`. `FUNC` is called as an expression and returns directly; there is no
caller output actual for `FUNC`. Current rule: document that V1 has no locals and choose input formal
names that do not collide with caller variables. A future locals model would
need a real variable save/restore or private frame mechanism, not just more
parser glue.

### THEN EXEC Has The Same Stored-Form Rule As THEN Commands

Proven on 2026-05-22 with `rbproc1`. Interactive ReadyBASIC entry can normalize
`IF 1 THEN EXEC SHOWI(7)` to `IF 1 THEN :EXEC SHOWI(7)`, but `petcat`
builds stored programs without running ReadyBASIC's crunch hook. Sample `.bas`
files built by `petcat` should therefore use the normalized `THEN :EXEC` form.

### Avoid BASIC Token Substrings In Command Names

Proven on 2026-05-22 while adding screen handle commands. ReadyBASIC command
names are stored as visible text, but C64 BASIC's cruncher can still tokenize
reserved words that appear inside the typed command name. Names such as
`SCRSAVE` and `SCRLOAD` looked safe as raw text but could contain `SAVE` and
`LOAD` tokens after crunching. The implemented names became `SCRCAP` and
`SCRPUT` to avoid those embedded tokens.

The same class of problem showed up in shorter examples too: `BUFNEW` can carry
an `FN` token, `FREEMEM` can carry a `FRE` token, and historical `PING` could
carry a `PI` token. ReadyBASIC currently has parser accommodations for shipped
names that still need them, but future command names should not rely on adding
more special cases.

The 2026-05-22 command rename pass made that rule practical: proof/demo
commands now live under a `Z...` namespace, `STRUP` became useful command
`UPPER`, sibling `LOWER` was added, and array demos use `NUM` instead of
`INT` so the BASIC tokenizer will not silently insert an `INT` token inside the
command name.

Current naming rule: prefer command names that do not contain BASIC keywords,
functions, operators, or pseudo-variables as substrings. Avoid obvious tokens
such as `SAVE`, `LOAD`, `RUN`, `LIST`, `NEW`, `CLR`, `REM`, `DATA`, `PRINT`,
`INPUT`, `GET`, `READ`, `RESTORE`, `GOTO`, `GOSUB`, `RETURN`, `IF`, `THEN`,
`FOR`, `NEXT`, `STEP`, `TO`, `STOP`, `END`, `ON`, `OPEN`, `CLOSE`, `CMD`,
`SYS`, `POKE`, `PEEK`, `WAIT`, `VERIFY`, `FN`, `FRE`, `POS`, `USR`, `RND`,
`ABS`, `SGN`, `INT`, `SQR`, `LOG`, `EXP`, `SIN`, `COS`, `TAN`, `ATN`, `TAB`,
`SPC`, `LEFT$`, `RIGHT$`, `MID$`, `STR$`, `VAL`, `LEN`, `CHR$`, `ASC`, and
`PI`.

Before accepting a new public command name, add it to a tokenizer probe that
stores, lists, and runs the command in direct mode, line-start program mode,
colon chains, and `IF ... THEN !COMMAND` form. If a preferred human name
conflicts, choose a short synonym instead of expanding resident parser token
exceptions.

### Relocated BASIC Needs A Zero Sentinel Byte

Proven on 2026-05-11 after moving `BASIC_START` to `$3001`, and still true after
the later move to `$1C01`. C64 BASIC's
`NEWSTT` path expects the byte immediately before `TXTTAB` to be zero; with
`TXTTAB=$1C01`, that means `$1C00` must be cleared. If `$1C00` still contains
leftover app-image bytes, `RUN` can fail with `?SYNTAX ERROR` before the `$0308`
ReadyBASIC wedge is ever entered.

Current rule: cold workspace initialization clears `BASIC_SENTINEL`, `BASIC_START`,
and `BASIC_START+1`. Any future relocation or loader change must keep
`TXTTAB-1 == 0` as a hard invariant and include a stored-program `RUN` probe.

### RB Parsing Worked; The Hidden Draw Path Was Invisible

Proven on 2026-05-09 with the binary-monitor probe. After `RB 2,0,12,"OK",1`,
ReadyBASIC state showed `rb_cmd_seen == $02`, `rb_arg_y == $0C`, and
`rb_strbuf == "OK"`, but screen RAM at row 12 did not contain the text. That
rules out the raw `!` matcher and argument parser as the cause of the invisible
manual command.

Current POC rule: visible `RB 2`/`RB 3` feedback uses KERNAL `PLOT`/`CHROUT`
from the bridge, with the cursor position saved and restored around the output.
The direct hidden screen-writer path needs separate repair before it should be
used for user-visible diagnostics again.

Verification artifacts:
`../agenticdevharness/logs/vice_auto_20260509_173427/` demonstrated the hidden
draw failure, and `../agenticdevharness/logs/vice_auto_20260509_174140/`
demonstrated visible `RB 2`, visible `RB 3`, mailbox `$C004/$C005 == $000F`,
and `EXIT` returning to the launcher.

### Prompt Hotkeys Need Direct Matrix Scanning

ReadyBASIC now supports prompt-level `CTRL+B`, `F2`, and `F4` by installing a
small CINV IRQ hook at `$0314/$0315` plus a KEYLOG hook at `$028F/$0290`. The IRQ
hook must not read `SHFLAG`/`SFDX` at the front of CINV: that samples the
previous KERNAL scan state before the ROM IRQ handler has scanned the current
physical key. A field test on 2026-06-09 showed normal BASIC and manual `EXIT`
working while physical `CTRL+B`/`F2`/`F4` did nothing, disproving that approach.

Current rule: the CINV hook performs a tiny direct CIA1 keyboard-matrix scan
for only the ReadyOS chords, restores CIA1 `$DC00/$DC02/$DC03`, queues the
ReadyOS action, and asks the editor to process a harmless `REM` line plus
Return. The KEYLOG hook catches KERNAL-decoded special keys, consumes the key
state, and preserves the CIA scan-port state. ReadyBASIC's normal execute hook
then sees the pending action and performs the state save, vector restore, and
shim return/switch.

This is deliberately boring in the good way: ordinary keys tail-call the saved
CINV/KEYLOG paths, no `CHRIN` vector hook is installed, and the ROM screen
editor still owns cursor blink, logical-line editing, and Return handling.

VICE Binary `input.sequence` writes through the KERNAL keyboard buffer rather
than driving physical keyboard matrix timing. The ReadyBASIC hotkey probe
therefore cannot be used as physical-key proof. A valid automated probe must
separate the claims: assert that CINV/KEYLOG point at ReadyBASIC hooks and that
the matrix scanner is present, then use pending-byte injection only to verify
execute-hook dispatch, state save, vector restore, and shim return/switch path.
Manual or GUI-host key testing is still needed to prove the host-to-matrix path
in VICE.

The hotkey helper only acts while `TXTPTR < BASIC_START`; running BASIC program
input is left alone. `CTRL+B` follows the existing `EXIT` yield path. `F2`/`F4`
scan the shim loaded-bank bitmap at `$C836-$C838`, write the target bank to
`$C820`, and jump through `$C80F`. If no neighbor app is loaded, the function key
is consumed so the BASIC editor does not see a stray app-navigation byte.

Multi-hop switching exposed one extra rule: the selected `F2`/`F4` target must
be copied into bridge state before ReadyBASIC prepares the shim yield, then
written to `$C820` only after save-state, `CLRCHN`, and vector restore. The
loaded-bank scan scratch bytes are not a durable handoff register. Before every
`EXIT`, `CTRL+B`, `F2`, or `F4` yield, ReadyBASIC must also clear its pending
hotkey byte, `$C6`, `$0277`, `SHFLAG`, `LSTX`, and `SFDX` so the destination app
does not inherit stale editor state.

A later F2/F4 double-switch failure proved that clearing state alone is not
enough when the physical key is still held across a ReadyOS app switch. Current
rule: on cold/warm entry, scan the CIA1 matrix and quarantine any still-held
ReadyBASIC hotkey until it is released. Before yielding for a prompt hotkey,
wait for the exact selected chord to release with a bounded jiffy-clock timeout,
then clear editor/KERNAL key state again. Keep the `REM` line as the known-safe
deferred-dispatch trigger unless a quieter path proves the same multi-hop
coverage.

`EXIT`/`exit` remains the explicit ReadyBASIC wedge command for returning to the
launcher: it restores ReadyBASIC-owned vectors, clears pending keyboard input,
marks the app ready, and jumps to the ReadyOS shim return entry.

Automation caveat: the current harness key helper sends lowercase host ASCII in
a way that does not match manual C64 lowercase/PETSCII entry. Automated `exit`
therefore uses uppercase key codes for now, while the command matcher still
accepts both byte forms.

### EXIT Resume Must Snapshot BASIC Runtime State

Revised on 2026-05-10. The late first-link repair made `EXIT`/resume appear to
work for a tiny program, but it was the wrong model. BASIC is a live runtime
image: program text, variables, arrays, string heap, `TXTPTR`, stack, and page
zero must stay coherent together.

Current rule: manual prompt `EXIT` saves BASIC zero page to REU bank `$44:$0A00`,
hardware stack page to REU bank `$44:$0B00`, SP and line-chain guards in bridge
metadata, refreshes the hidden helper shadow, restores ReadyBASIC vectors, and
then yields through `$C80C`. Warm entry restores hidden helpers first,
reinstalls ReadyBASIC vectors, restores the saved BASIC runtime state, and
returns to ROM BASIC without clearing the screen or reconstructing variable
pointers from line links.

The old unconditional `$1201/$1202` repair from saved first-link bytes was
removed. It could resurrect old program text after `NEW` or `RB 12`, which
explained reports where `NEW`/`CLR` still left distorted old listings visible
after resume.

Verification artifact:
`../agenticdevharness/logs/vice_auto_20260510_162036/` passed manual prompt
`EXIT`/resume cases for a multi-line program, `NEW` staying empty across
resume, variables/strings/arrays surviving resume, `CLR` keeping program text,
and a stored `!` line surviving resume.

Baseline lifecycle artifact:
`../agenticdevharness/logs/vice_auto_20260510_162159/` passed direct `PRINT`,
numbered line entry, `LIST`, `RUN`, direct `!2`, direct `!3`, stored `!`,
manual `EXIT`, resume, and `LIST` after resume.

Scope note: this verification deliberately covers manual prompt `EXIT`.
Program-line continuation through `EXIT` is deferred. Raw `EXIT` inside
`IF ... THEN` is not expected to work until `EXIT` is tokenized or the wedge
hooks more of BASIC's command dispatch.

### Resume Display Is A Contract, Not Cosmetic

Reproven on 2026-05-11 during the lean REU plugin rewrite. After moving
ReadyBASIC's visible/core memory contract down to `$1200-$2FFF` and
`BASIC_START=$3001` at the time, the app could return from the launcher to BASIC but leave
the launcher menu on screen with a `READY.` prompt painted over it.

The root cause was twofold:

- `cmd_exit` correctly decided that a manual prompt `EXIT` was a READY-mode
  return, but `save_basic_runtime_state` recalculated resume mode from
  `CURLIN`. In direct mode after the plugin command flow, that could save
  `RUNTIME_MODE=RB_RESUME_RUN` and resume through `BASIC_NEXT_STMT`. A follow-up
  run showed `cmd_exit` itself must not depend on `CURLIN` either; direct
  commands are more reliably identified by `TXTPTR` still pointing into the
  `$0200` input buffer rather than the `$1C01+` BASIC text area.
- The new resume path omitted baseline console restoration steps: clear
  channels, clear the screen/editor surface, restore lowercase VIC text mode,
  clear pending keyboard bytes, redraw the ReadyBasic banner, and position the
  prompt before entering `BASIC_READY`.

Current rule: memory layout changes must be reviewed against a lifecycle
contract checklist before the first "good" commit. At minimum: page-3 vectors,
hidden helper shadow, CPU port banking, BASIC memory bounds, live BASIC pointers
including `FRETOP`, runtime mode, stack/ZP restore, screen/editor state, prompt
position, key buffer, and launcher menu re-entry path.

Current `EXIT` mode rule: V1 supports manual prompt `EXIT`. It treats `TXTPTR`
below `BASIC_START` as READY-mode resume and `TXTPTR >= BASIC_START` as a
program-line resume candidate. Do not use `CURLIN` alone as the direct/program
discriminator in the wedge path. `cmd_exit` writes the chosen `RUNTIME_MODE`
itself; the hidden save helper preserves that byte rather than deriving mode
again from less-local state.

### Do Not Reset Live BASIC Pointers On Warm Restore

Reproven on 2026-05-11. It is correct to set KERNAL memory top/bottom on warm
resume, but not to run the cold workspace reset after restoring zero page.
`FRETOP`, `VARTAB`, `ARYTAB`, and `STREND` are live BASIC runtime state. Resetting
them on warm entry can silently lose variables, arrays, or string heap state even
when program text still looks valid.

Current rule: split "set KERNAL memory bounds" from "force cold BASIC workspace
pointers." Warm restore gets only the memory bounds unless the runtime snapshot
is invalid and we deliberately fall back to an empty BASIC state.

### Do Not Reset `FRETOP` During Ordinary BASIC Dispatch

Proven on 2026-05-10. `FRETOP` (`$33/$34`) is live BASIC string-heap state, not
just another fixed memory-limit pointer. ReadyBASIC was enforcing its scoped
workspace before both `ICRNCH` and `IGONE`, but that enforcement also reset
`FRETOP` to the then-current BASIC top before ordinary direct-mode commands. That made BASIC forget
where string data already lived, so a direct variable such as `A$="HELLO"` could
survive in the variable descriptor while its string bytes became eligible for
reuse by the next string allocation.

Current rule: ordinary dispatch may enforce `TXTTAB`, KERNAL memory bounds, and
`MEMSIZ`, and may reset only if `VARTAB` is outside the ReadyBASIC workspace.
It must preserve a valid `FRETOP`. Full variable/string resets belong only to
initialization, `NEW`, `CLR`, `RUN`, `RB 12`, loads, or a proven-invalid runtime
state.

Verification artifact:
`../agenticdevharness/logs/vice_auto_20260510_233650/` added a direct-mode
string case: `A$="HELLO"`, `EXIT`, resume, `PRINT A$+"!"`, then `B$="WORLD"`
and `PRINT A$+"2"`. It passed. Memory dumps showed `FRETOP == $95FB` after the
first string assignment and `FRETOP == $95F4` after resume plus the second
string allocation, proving the pointer was no longer reset to `$9600`.

Baseline lifecycle artifact:
`../agenticdevharness/logs/vice_auto_20260510_234541/` passed direct `PRINT`,
numbered line entry, `LIST`, `RUN`, direct `!2`, direct `!3`, stored `!`,
manual `EXIT`, resume, and `LIST` after resume with the preserved-`FRETOP`
build.

## Disproven Or Revised

### Hypothesis: The Headless Harness Proved Numbered Line Entry Stable

Disproven on 2026-05-09 by a visible binary-monitor run:

```sh
READYBASIC_SKIP_BUILD=1 READYBASIC_VISIBLE=1 READYBASIC_KEEP_VICE=1 \
  bash build_support/run_readybasic_lifecycle_probe.sh
```

Artifacts:
`../agenticdevharness/logs/vice_auto_20260509_142432/`.

Direct `PRINT 1` and `PRINT "HELLO"` return to `READY.` without a spurious
`?C ERROR`, but `10 PRINT 1` still shifts the visible screen eight columns to
the right and leaves `@@@@@@@@` on row 0. The previous assertion only checked
for `?`, so it missed the screen corruption and falsely reported success.
Future acceptance must assert screen layout/content, not just absence of BASIC
errors.

### Finding: Numbered Line Entry Needs Pointer Enforcement At Crunch Time

Proven on 2026-05-09. ReadyOS can leave BASIC low-memory pointers unsuitable
for ROM BASIC line insertion even after ReadyBASIC has claimed KERNAL memory
bounds. The failure signature was:

- `$0283/$0284` set to `$1200`, but `TXTTAB`/`VARTAB` low-memory state still
  aimed into `$0100-$05xx`.
- Entering `10 PRINT 1` made ROM BASIC move memory through screen/vector areas,
  leaving `@@@@@@@@` at row 0 and shifting visible output.
- `$1201` stayed empty, proving the line was not being inserted into the scoped
  BASIC workspace.

Fix: enforce ReadyBASIC's scoped BASIC pointers immediately before the
`ICRNCH` pass and before wedge execution. If `VARTAB` is outside the scoped
workspace, reset the empty-program pointers before ROM BASIC inserts the line.

Verification artifact:
`../agenticdevharness/logs/vice_auto_20260509_144003/`.

That run passed direct `PRINT 1`, direct `PRINT "HELLO"`, `10 PRINT 1`, `LIST`,
`RUN`, direct `RB 2`, direct `RB 3` mailbox result `$000F`, and a stored `20 RB
2,...` executed by `RUN`, with assertions rejecting `?` errors and `@` screen
artifacts.

### Hypothesis: The Launch Lockup Was Only A ReadyOS ABI Load-Bounds Problem

Revised. Load bounds did matter and are now verified, but later symptoms proved
there were independent BASIC/KERNAL vector contract bugs after the app loaded.

### Hypothesis: Hooking `CHRIN` After Original Return Is Enough For Prompt Hotkeys

Disproven. The ROM screen editor consumes the interactive key stream internally
and returns the completed logical line. Prompt navigation needs a pre-editor
physical key check or a deeper screen-editor hook. The working implementation
uses the CINV IRQ path plus KEYLOG for pre-editor detection, then queues a
harmless `REM` line so the normal BASIC execute hook can dispatch the pending
ReadyOS action.

### Hypothesis: Pre-Editor `CHRIN` Keyboard-Buffer Peek Is Safe

Disproven. The pre-editor peek was changed into a blocking wait for `$C6 != 0`.
That starved the ROM screen editor before it could manage cursor/input state,
matching delayed prompts, missing cursor blink, and fragile line entry. The
stabilization pass removes it and accepts that prompt-level hotkeys need a
separate, editor-safe design. The implemented design hooks CINV without blocking
before the ROM screen editor runs.

### Hypothesis: Tokenizing ReadyBASIC Commands Immediately Is The Best POC Path

Revised. It is desirable, but the first priority is a stable scoped BASIC host.
Raw command text recognized at execution time is safer until the crunch/list/execute
contract is fully proven.

## Current Verification Checklist

- Build through normal profile flow, not direct app launching:
  `bash ./run.sh --profile precog-d81 --vice-fast`
- For headless probe runs, use the VICE binary monitor harness outside this
  repo:
  `bash build_support/run_readybasic_lifecycle_probe.sh`
  The script builds the D81 with `runappfirst=readybasic`, boots `PREBOOT`, and
  stores artifacts under `../agenticdevharness/logs/vice_auto_*`.
- For the current REU plugin direct-mode and ReadyOS resume contract, use:
  `READYBASIC_VISIBLE=1 bash build_support/run_readybasic_plugin_command_probe.sh`
  This boots normal ReadyOS with `runappfirst=readybasic`, exercises direct
  `!COMMAND args` samples, returns through the launcher by `EXIT`, relaunches
  ReadyBasic by menu navigation, and verifies BASIC variable/string state plus
  registry function after resume.
- For the stored-program command contract, use:
  `READYBASIC_VISIBLE=1 bash build_support/run_readybasic_program_probe.sh`
  This interviews each command style early: first `LIST`/`RUN` for `ZECHO1`,
  then same-line continuation, strings, hidden workers, arrays, handles, and
	  failure clearing. It should fail fast at the first command family that
	  regresses.
- For native `PROC`/`FUNC` positive coverage, load `RBPROC1` from the D81 and run
  it. It covers no-param `PROC`, `%` and `$` inputs, `%` and `$` `FUNC` returns,
  colon-chain use, normalized `IF THEN :EXEC`, nested depth 2, and readable
  `LIST`.
- For native `PROC`/`FUNC` negative coverage, load `RBPROCERR` and run sections
  by line number (`RUN 100`, `RUN 200`, ... `RUN 800`). It covers unknown
  routine, wrong count/type, statement `EXEC` to `FUNC`, `PROC` extra actual, bare
  `ENDP`, and return-stack overflow.
- For manual-`EXIT` BASIC-state probes, use:
  `bash build_support/run_readybasic_state_probe.sh`
- For the larger direct-variable/string/array stress case, use:
  `bash build_support/run_readybasic_large_vars_probe.sh`
  It enters a five-line program, creates `A$` through `D$`, `DIM E(200)`, sets
  `E(199)`, exits, resumes, and verifies strings, array data, and `LIST`.
  On 2026-05-11 this passed five consecutive runs. The first draft falsely
  failed because it printed several checks and `LIST` before asserting, scrolling
  the earliest `A$` output off screen; screen assertions should check values
  while they are still visible or use memory-backed validation.
- For cross-app churn after `EXIT`, use:
  `bash build_support/run_readybasic_cross_app_resume_probe.sh`
  It builds the same larger BASIC/string/array case, then repeats:
  `EXIT -> launcher -> ReadyShell -> CTRL+B -> launcher -> ReadyBASIC -> LIST
  and one-at-a-time PRINT checks`. On 2026-05-11, 10 full cycles passed: all 206
  plan steps were `ok`; the run was marked `partial` only because the harness
  final automatic REU debug-ring fetch returned short after the successful plan.
- Confirm `readybasic.prg` load address is `$1000`.
- Confirm compact PRG load span remains below `$C600`.
- Confirm runtime `BRIDGE` remains below `$C200`, leaving shared frames intact.
- In ReadyBASIC direct mode:
  - `print "hello"` should print `hello` and return to `ready.` with no error.
  - `10 print 1` should enter without screen corruption or lockup.
  - `list` should show the line.
  - `run` should print `1` and return cleanly.
- At the ReadyBASIC prompt:
  - `exit` should restore vectors and return to the ReadyOS launcher.
- After relaunching ReadyBASIC from the launcher:
  - existing BASIC text, variables, strings, arrays, screen state, and `NEW`
    state should resume coherently after a manual prompt `exit`.
- `CTRL+B`, `F2`, and `F4` prompt interception uses the nonblocking CINV IRQ plus
  synthetic-`EXIT` path and must keep restoring page-3 vectors before every shim
  yield.

### Expression Hooks Need Narrow, Purpose-Built Paths

Proven on 2026-05-23 in the expression-style branch. Returning command results
through the BASIC eval vector is practical when the command has already produced
a scalar/string result, as with `ZADD16(a,b)` and `UPPER(s$)`. Bare statement
commands can also reuse the existing descriptor/signature path with only a small
resident parser wrapper.

Numeric `FUNC` expression returns are different: trying to call the ROM numeric
expression evaluator from inside the eval hook without preserving ReadyBASIC's
scan state produced incorrect values and left BASIC in a bad parse state. The
current branch uses `RET` as the function return marker, preserves the caller
expression state around nested function/command evaluation, and supports simple
scalar assignments before `RET`. A later lean nested-term pass also proved that
returned FAC/string descriptors must be normalized before BASIC ROM consumers
see them: `ABS(ADDI(1,6)-10)` needs the numeric return to clear carry and look
like a ROM numeric term, while `LEFT$(GREET("READY"),2)` needs the returned
string descriptor registered through BASIC's temporary-string path. This remains
deliberately narrower than a full BASIC interpreter for arbitrary statements
inside a `FUNC` body.

### Nested Expression Terms Must Preserve Both BASIC And ReadyBASIC State

Proven on 2026-05-23 in `exp/readybasic-proper-float-terms`. Getting
`ADDI(1,ADDI(2,3))` and `FADD(1.5,FADD(2.25,3.25))` working was not just a
matter of calling BASIC ROM recursively. The inner ReadyBASIC call reuses
scratch such as `CF_PARAM_COUNT`, `rb_target_off`, `rb_formal_*`,
`rb_form_next_*`, `rb_bind_expr_mode`, and the routine scan cursor. Those fields
must be saved around ROM expression evaluation or the outer call resumes with
the inner call's parser state and fails later with misleading syntax errors.

The `FADD` statement case also proved that keyword-prefixed command names need
careful execute dispatch. `FADD(...)` first passes through the leading-`F`
`FUNC` check; a failed keyword match must fall through to bare command lookup,
not directly back to BASIC. This keeps natural command names available without
reintroducing the removed `!` statement form.

### Demo Automation Should Explain Expected Failures In-Band

Added on 2026-05-24 with `build_support/run_readybasic_demo_suite.sh`. A demo
suite is different from a regression probe: it should pause long enough for a
viewer to read the screen, describe each section with visible `REM` text before
running it, and state expected outcomes before printing or intentionally causing
errors. The ReadyBASIC demo covers `FREEMEM`, app suspend/restore through the
Editor, assembler command groups, `PROC`/`FUNC`, array/float/string parameters,
REU handles, expected syntax/runtime errors, and nested expression forms. The
same script keeps `READYBASIC_DEMO_FAST=1` for tight development runs.

## Open Questions

- Deferred hypothesis: ReadyBASIC may leave KERNAL `MSGFLG` (`$009D`) nonzero
  when returning to ReadyOS, which would make later shim/ReadyShell `LOAD` calls
  print `SEARCHING FOR` / `LOADING` chatter that normally stays hidden. Prove
  later by dumping `$009D` before ReadyBASIC, after direct BASIC use, after
  `exit`, and immediately before launcher/shim/ReadyShell loads. Likely fix, if
  proven: save/restore or clear `MSGFLG` around ReadyBASIC yield, and consider
  defensive `SETMSG 0` around shim/overlay loads.
- Should the prompt hotkey handler eventually hook the screen editor more
  directly instead of peeking `KEYD_COUNT/KEYD_BUFFER`?
- What exact register/flag contract should the future `RB` token cruncher
  preserve beyond `Y` as length?
- Should `RB 3` keep its visible debug output long term, or become mailbox-only
  once there is a better inspection UI?
- How much of `$C000-$C5FF` should remain free for future app/shim state before
  moving more bridge code into hidden helpers?

## Useful References

- C64 BASIC-ROM map: https://www.c64-wiki.com/wiki/BASIC-ROM
- C64 vectors overview: https://www.c64-wiki.com/wiki/Vector
- Mapping the C64, `DFLTN` and KERNAL input: https://cx16.dk/mapping_c64.html
- C64 OS BASIC wedge discussion: https://c64os.com/post/basicwedgeprograms
