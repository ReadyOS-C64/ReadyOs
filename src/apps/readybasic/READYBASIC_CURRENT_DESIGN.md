# ReadyBASIC Current Design

This is the current ReadyBASIC design as implemented by
`src/apps/readybasic/readybasic.s`, linked by `cfg/ready_app_readybasic.cfg`,
and verified against the current `obj/readybasic.map`.

ReadyBASIC is a ReadyOS app that hosts a relocated C64 BASIC V2 workspace and
adds a lean command spine for bare `COMMAND(...)` statements and selected
`COMMAND(...)` expressions, plus native bare `PROC`/`FUNC` reusable BASIC
routines. It is not a private BASIC token system. Stored program lines remain
readable, `LIST` shows visible `PROC`, `FUNC`, `EXEC`, `ENDP`, and command text,
and the execute/eval hooks recognize extensions when BASIC dispatches a
statement or expression.

For the most visual current memory explanation, regenerate and open
`docs/readybasic_memory_diagrams.html` with:

```sh
make readybasic-memory-report
```

That special report is generated from the current ReadyBASIC map/linker/source
facts and shows the proportional C64 RAM, cold-load seed, steady-state BASIC
workspace, under-ROM slot, and assigned ReadyBASIC core/code bank pictures.

## Current Module/Submodule Snapshot

This section is the current source of truth for ReadyBASIC's module/submodule
layout. Older implementation names such as `LOWPACK` remain in a few labels and
tables because they are still used by the linker and source, but the current
runtime model is the module-aware layout described here.

Measured from the current `obj/readybasic.map`:

| Item | Current value |
|---|---:|
| `BASIC_START` | `$2AC1` |
| Empty BASIC free bytes | `30013` |
| `ENTRY` | `$1000-$11FF`, `$0200` / 512B |
| `RESIDENT` | `$1200-$2ABA`, `$18BB` / 6331B |
| BASIC sentinel | `$2AC0` |
| Common under-ROM helper | `$A000-$A790`, `$0791` / 1937B |
| Slot 0 / module 1 | `$A800-$AEFC`, `$06FD` / 1789B |
| Slot 1 / module 2/GFXCORE | `$B000-$B474`, `$0475` / 1141B |
| Slot 2 / GFXPRIM | `$B800-$BB62`, `$0363` / 867B |
| Slot 2 overlay 1 / GFXSPR | `$BB63-$BD24`, `$01C2` / 450B |
| Slot 2 overlay 2 / INPUTEV | `$BD25-$BD91`, `$006D` / 109B |
| `BRIDGE` | `$C000-$C1FE`, `$01FF` / 511B |
| Shared frames/buffers | `$C200-$C5FF` |
| `REGSEED` load-only registry | `$5000-$600F`, `$1010` / 4112B |

After cold initialization, BASIC owns `$2AC1-$9FFF`. The bytes loaded there as
module payload seed or registry seed are not persistent C64 RAM; they have been
copied into the loader-assigned ReadyBASIC core and code banks.

ReadyBASIC now treats `$A000-$BFFF` as a common under-ROM helper area plus three
2KB command submodule slots:

| Area | Range | Current role |
|---|---:|---|
| Common under-ROM | `$A000-$A7FF` | Helper code and future resident-code relief. |
| Slot 0 | `$A800-$AFFF` | Default/system module; existing core commands run here. |
| Slot 1 | `$B000-$B7FF` | Built-in proof module and `ZMODLD` disk-module loader. |
| Slot 2 | `$B800-$BFFF` | Proof submodule and overlay target. |

Command descriptors are still 32 bytes, but they are module-aware records now:
command id, module id, payload offset/size in the assigned code bank, submodule id,
overlay id, slot mask, generation/check byte, runtime destination, entry
offset, signature id, and command name. The special hidden-command dispatch
category is gone; all descriptor-backed machine-code commands execute from RAM
hidden behind BASIC ROM.

The current proof commands added by the module work are `ZSLOT0`, `ZSLOT1`,
`ZSLOT2`, `ZSPAN`, `ZOVL1`, `ZOVL2`, `ZCPYRST`, and `ZCOPY`. The disk-module
loader proof is `ZMODLD(name$)` in module 2/slot 1. It opens ReadyBasicModule
SEQ packages named `rbm.<name>` and streams them through the `$C500` page buffer
into REU, rather than loading them as PRG files. Current sample packages are
`rbm.sample1`, `rbm.sample2`, and `rbm.sample3`.

Phase 1 graphics commands are also built in and prestashed to the assigned code
bank; they do not require `ZMODLD`. They use module id `3`: `GFXCORE`
submodule `16` in slot 1, `GFXPRIM` submodule `17` in slot 2, `GFXSPR`
submodule `18` as slot-2 overlay 1, and `INPUTEV` submodule `19` as slot-2
overlay 2. The surface handle entry points `GFXSURF` and `GFXBLIT` use the
existing system slot allocator path so typed REU graphics handles can share the
same handle directory as `BUFNEW` and `SCRCAP`.

The graphics Bank D layout intentionally avoids ReadyBASIC/ReadyOS state:
screen RAM is `$CC00-$CFFF`, sprite data is `$CA00-$CBFF`, bitmap/charset RAM
is `$E000-$FFFF`, and color RAM is `$D800-$DBE7`. `$C000-$C9FF` remains owned
by ReadyBASIC bridge/shared frames, ReadyOS REU metadata, and shim ABI.

The naming nuance matters. A module is the logical command family identified by
module id. A module package/container is the disk SEQ file that carries
descriptors and payload records. A submodule is the runtime payload family that
claims one or more 2K command slots. An overlay is one swappable image of that
submodule. That is why the RBM2 sample can mention submodule `5` twice: both
entries are the same submodule family, but they are different overlays.

The assigned core bank is the authoritative registry/runtime bank: header,
call/result scratch, handle directory, zero-page/stack snapshots, command
descriptors, and the typed heap. The assigned code bank is the payload bank:
built-in module bytes start at `$0000`, and the current disk samples use
`$3000`, `$3200`, `$3300`, and `$3400`. Future richer per-slot residency
metadata is reserved for assigned core-bank offsets `$2000-$3FFF`; the current proof keeps only a tiny last-command/copy-count
record in bridge RAM.

## Current Syntax And Statement Behavior

ReadyBASIC commands now prefer ordinary-looking parenthesized BASIC syntax:

```basic
COMMAND(arg,arg,out%)
PRINT COMMAND(arg,arg)
```

Statement commands keep the existing output-variable convention. Expression
commands return the scalar, string, or float result directly. Older
non-parenthesized command syntax is no longer part of the current design;
current examples and tests use bare `COMMAND(...)`.

Bare commands are recognized only where BASIC is about to dispatch a statement
or evaluate an expression:

| Context | Supported | Notes |
|---|---:|---|
| Immediate mode statement | Yes | Example: `ZECHO1(P%)`. |
| Stored program line start | Yes | Raw command text survives `LIST` and runs through `$0308`. |
| After `:` | Yes | Example: `PRINT "A":ZECHO1(P%)`. |
| Inside `FOR/NEXT` body | Yes | Use it as a statement in the loop body. |
| After `IF ... THEN` | Limited | `EXEC` and `JUMP` are normalized by the crunch hook to `THEN :EXEC` and `THEN :JUMP`. Bare command statements should use an explicit colon, such as `IF 1 THEN :ZECHO1(P%)`. Ordinary BASIC assignments like `IF 1 THEN A%=ZADD16(1,2)` work through BASIC's normal assignment path. |
| Inside `PRINT`, assignments, or larger expressions | Selected commands only | `ZADD16(2,3)+7`, `ABS(ADDI(1,6)-10)`, `ABS(FADD(1.2,2.3)-3)`, `LEFT$(GREET("READY")+"!",3)`, `UPPER(S$)`, and numeric/string/float `FUNC` expression returns are supported. |
| Inside strings, `REM`, or `DATA` | Ordinary text | These are not rewritten or dispatched. |
| After `ELSE` | No native support | BASIC V2 has no `ELSE`; ReadyBASIC does not add it. |

`IF 1 THEN EXEC SHOW(7)` and `IF I%>0 THEN JUMP LOOP` work when typed
interactively, but their stored/listed forms include the inserted colon after
`THEN`. For descriptor-backed command statements, write the colon explicitly:
`IF 1 THEN :ZECHO1(P%)`. This keeps BASIC's existing statement dispatcher in
charge without adding a larger custom IF parser.

Native routines use ordinary BASIC program text:

```basic
1000 PROC SHOW(P%,M$)
1010 PRINT P%;M$
1020 ENDP

1100 FUNC ADDI(X%,Y%)
1110 RET X%+Y%
1120 ENDP

10 EXEC SHOW(3,"READY")
20 A%=ADDI(4,5)
30 PRINT ADDI(6,7)
```

`EXEC` calls a `PROC`. `PROC` has input formals only. `FUNC` also declares input
formals only, but is called as a BASIC expression and returns with a `RET`
statement. `EXEC FUNC(...)` is rejected; write `A%=ADDI(4,5)` or
`PRINT ADDI(6,7)` instead. `ENDP` returns from `PROC` without a value.
Version 1 supports scalar `%` integer, `$` string, and plain C64 BASIC floating
formals/variables. Arrays, locals, by-reference parameters, and multiple
outputs remain out of scope. String inputs and returns use the same 64-byte
ReadyBASIC string cap as command results. Nested `EXEC` has a four-entry return
stack; a fifth active call reports `?RB ERROR 33`.

`RET expr` returns from a `FUNC`. `RET% expr` and `RET$ expr` force integer or
string return handling. Untyped `RET expr` evaluates through BASIC ROM and
returns the expression's natural type: string for string expressions and plain
C64 BASIC float for numeric expressions. Assigning that float result to a `%`
target still works through BASIC's normal integer coercion. A `FUNC` expression
scans and runs simple scalar assignment statements before `RET`, so later
assignment then return works:

```basic
2000 FUNC ADDLATE(X%,Y%)
2010 R%=X%+Y%
2020 RET R%
2030 ENDP

10 A%=ADDLATE(4,5)
```

This is still intentionally smaller than a general BASIC subinterpreter: V1
`FUNC` bodies support scalar `%`/`$`/plain numeric assignments before `RET`,
including nested command/`FUNC` calls in the tested assignment forms. Other
statements inside a `FUNC` body remain invalid. The current implementation
preserves enough BASIC ROM expression/string-descriptor state for nested ROM
consumers and ReadyBASIC actuals such as `ABS(ADDI(1,6)-10)`,
`ABS(FADD(1.2,2.3)-3)`, `LEFT$(GREET("READY")+"!",3)`,
`ADDI(1,ADDI(2,3))`, and `FADD(1.5,FADD(2.25,3.25))`.

Routine definitions are normal BASIC lines and are not command overlays,
descriptors, or `LOWPACK` entries. Put definitions after `END` in V1. Reaching a
`PROC` or `FUNC` definition by ordinary fall-through is invalid and produces a
BASIC syntax error; this keeps the resident implementation small. Like C64 BASIC
variables generally, formal variables are global by name. Avoid reusing a
function's input formal name as an important caller variable unless you intend
the call to overwrite that global BASIC variable.

`IF 1 THEN A%=ADDI(1,2)` works through BASIC's normal assignment path. `IF 1
THEN EXEC SHOW(7)` works when typed into ReadyBASIC and is normalized by
the crunch hook to `IF 1 THEN :EXEC SHOW(7)`. `petcat`-built stored examples
should use the already-normalized `THEN :EXEC` form because they bypass the
interactive crunch hook.

ReadyBASIC recognizes manual prompt `EXIT` plus prompt-level `CTRL+B`, `F2`,
and `F4` as ReadyOS navigation. Prompt hotkeys are detected by the KEYLOG
preprocessing vector at `$028F/$0290` only after ReadyBASIC's IMAIN hook at
`$0302/$0303` has marked the direct prompt active, input is coming from the
keyboard, and BASIC is not executing a program statement. The execute hook clears
that prompt-active flag before direct commands and stored program lines run. A
legacy `TXTPTR < BASIC_START` check remains as a fallback for partially typed
prompt lines, but it is not the sole proof of prompt state because `CURLIN` and
`TXTPTR` can still point into the just-run program after `RUN` returns to
`READY.`. KEYLOG records the requested ReadyOS action, consumes the decoded key
state, and queues only Return. The CHRIN hook at `$0324/$0325` then discards the
editor-returned character and dispatches the pending action before BASIC can
store or execute any partial line. This keeps ordinary key repeat, cursor blink,
line editing, and space handling owned by the ROM editor; ReadyBASIC no longer
installs a CINV/IRQ scanner for prompt hotkeys and no longer injects visible
`REM` text.
ReadyBASIC also quarantines any still-held ReadyOS hotkey on cold/warm entry and
waits, with a jiffy-bounded timeout, for the selected physical chord to be
released before yielding through `CTRL+B`, `F2`, or `F4`. That release wait is
what prevents one held function key from being seen by both the outgoing and
incoming app.
Program-line `EXIT` resume and running-program hotkeys remain future work; the
proven paths are direct prompt `EXIT` and prompt-level navigation keys.

## Phase 1 Graphics Commands

The current command catalog includes the command-only Phase 1 graphics surface:

| Command | Form | Notes |
|---|---|---|
| `GFXMODE` | `GFXMODE("HIRES")`, `M%=GFXMODE()` | Supports `TEXT`, `HIRES`, `MBITMAP`, `TILE`, and `MTILE`. |
| `GFXTEXT` | `GFXTEXT()` | Restores ordinary text mode. |
| `GFXCLEAR` | `GFXCLEAR(C)` | Clears Bank D screen/color RAM and bitmap RAM for bitmap modes. |
| `GFXSURF` | `H%=GFXSURF("HIRES")` | Allocates typed graphics-surface handle `3` in the REU heap. |
| `GFXTARGET` | `GFXTARGET(0)` | Phase 1 visible-target selector; REU target drawing is future work. |
| `GFXBLIT` | `GFXBLIT(H%)` | Phase 1 handle validation for typed graphics surfaces. |
| `GFXSYNC` | `GFXSYNC()` | No pending dirty-rect queue in Phase 1. |
| `PLOT` / `POINT` / `PNT` | `PLOT(X,Y,C)`, `PNT(X,Y,P%)` | Bitmap bit plot/read or tile cell write/read, depending on mode; `POINT` remains registered as the long form. |
| `LINE` / `RECT` / `FRECT` | five numeric args | Immediate primitive workers in `GFXPRIM`. |
| `SPRSET` / `SPRMOVE` / `SPRCOLOR` / `SPRROW` | sprite config/move/color/pixels | Uses eight 64-byte Bank D sprite definitions at `$CA00`; `SPRROW` writes explicit 24-bit sprite rows. |
| `SPRSCAN` / `SPRCOLL` | `SPRSCAN()`, `SPRCOLL(N,C%)` | Polls VIC collision latches; no IRQ sampler. |
| `JOY` / `KEYP` / `KEYSCAN` / `KEYLAST` | output/polling forms | Polling only, no resident input event queue. |

See `READYBASIC_GRAPHICS_COMMAND_DESIGN.md` for mode tradeoffs, memory layout,
and Phase 1 limits.

## Native Flow Control And Error Introspection

ReadyBASIC now also adds a small visible-text flow-control layer. These are
language features in the resident parser, not descriptor-backed command
overlays:

```basic
10 I%=0
20 REPEAT
30 I%=I%+1
40 UNTIL I%=3
50 PRINT "DONE";I%

100 LABEL LOOP
110 PRINT I%
120 I%=I%-1
130 IF I%>0 THEN JUMP LOOP
```

`REPEAT` records the current BASIC text pointer and line number. `UNTIL expr`
evaluates the expression through BASIC ROM; false loops back and true continues
after the `UNTIL`. Loops can be nested four deep. A fifth active `REPEAT`
reports `?RB ERROR 35`; `UNTIL` without a matching active `REPEAT` reports
`?RB ERROR 36`.

`LABEL name` is a no-op marker in stored BASIC text. `JUMP name` scans the
stored program for the matching label, sets the current execution line, and
continues there. It works forward, backward, after colons, and after normalized
`IF ... THEN`. Missing labels report `?RB ERROR 39`. Numeric `GOTO` remains the
ordinary BASIC ROM statement.

The last ReadyBASIC runtime error can be read back with `ERRCODE` and
`ERRLINE`, either as expression functions or statement output commands:

```basic
10 ZFAIL(6,X%)
20 PRINT ERRCODE();ERRLINE()
30 ERRCODE(E%):ERRLINE(L%):PRINT E%;L%
```

After a ReadyBASIC runtime error, `ERRCODE()` returns the `?RB ERROR` code and
`ERRLINE()` returns the BASIC line number. Direct-mode errors report line `0`.

## Command Families

The current commands are examples of command shapes that the spine needs to
support, not the final product command catalog.

| Category | Commands | Purpose |
|---|---|---|
| Scalar Outputs | `ZECHO1`, `ZADD16` | Prove integer output variables, numeric expression parsing, and scalar result commit. |
| String Transfer/Transform | `UPPER`, `LOWER` | Prove string input capture and resident-owned BASIC string output allocation. |
| Under-ROM Worker | `ZHIDDENRAM` | Prove worker code can run behind BASIC ROM in the shared slot-0 module payload. |
| Integer Array Transfer | `ZSUMNUMARRAY`, `ZRANGENUMARRAY` | Prove array input/output via explicit base element plus count. |
| Persistent REU Handles | `BUFNEW`, `BUFFILL`, `BUFFREE`, `SCRCAP`, `SCRPUT` | Prove stable BASIC-visible handles for persistent REU-backed data, including typed screen text+color resources. |
| Temporary REU Workspace | `ZTEMPSCRATCH` | Prove temporary page allocation and cleanup. |
| Error/Failure Contract | `ZFAIL` | Prove outputs are cleared before execution and stale results are not committed. |
| Timing/Delay | `ZPAUSE` | Prove a small timing command can wait for a number of jiffies without command overlay growth elsewhere. |
| Runtime Introspection | `FREEMEM`, `ERRCODE`, `ERRLINE` | Prints live BASIC free memory and exposes the last ReadyBASIC runtime error. |
| Module/Submodule Proofs | `ZSLOT0`, `ZSLOT1`, `ZSLOT2`, `ZSPAN`, `ZOVL1`, `ZOVL2`, `ZCPYRST`, `ZCOPY`, `ZMODLD`, `ZDM1`, `ZDM2S`, `ZDOV1`, `ZDOV2`, `ZSAA`-`ZUEB` | Prove slot dispatch, multi-slot span loading, overlay replacement, no-recopy behavior, SEQ package streaming, and disk-loaded module registration. |
| ReadyOS Yield | `EXIT` | Save BASIC runtime state, restore vectors, and return through the ReadyOS shim. |

### Command Inventory

| Command | Code placement | Parameters | Result behavior |
|---|---|---|---|
| `ZECHO1(OUT%)` / `ZECHO1()` | Resident-precomputed result; a legacy low stub remains in `LOWPACK` | output integer, or expression integer | Returns `1` without fetching an overlay in the current runtime. |
| `ZADD16(A,B,OUT%)` / `ZADD16(A,B)` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | two numeric expressions, output integer or expression integer | Returns 16-bit sum. |
| `FADD(A,B,OUT)` / `FADD(A,B)` | Resident-computed float demo command; descriptor slot 16 has a tiny slot-0 stub | two plain numeric expressions, output plain numeric variable or expression float | Uses BASIC ROM floating addition. Statement output must be a plain numeric variable, not `%`. |
| `ZPAUSE(TICKS)` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | tick count | Waits for the requested jiffy count. |
| `UPPER(S$,OUT$)` / `UPPER(S$)` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | string variable or quoted literal, output string or expression string | Uppercases staged bytes. |
| `LOWER(S$,OUT$)` / `LOWER(S$)` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | string variable or quoted literal, output string or expression string | Lowercases string byte values. On the default C64 screen this is verified by `ASC()` values, because display case is charset-dependent. |
| `ZHIDDENRAM(S$,OUT%)` / `ZHIDDENRAM(S$)` | Module 1 slot 0 at `$A800+`, unified under-ROM dispatch | string variable or quoted literal, output integer or expression integer | Returns a simple uppercase-byte checksum. |
| `ZSUMNUMARRAY(A%(0),COUNT,OUT%)` / `ZSUMNUMARRAY(A%(0),COUNT)` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | integer array base, count, output integer or expression integer | Sums integer array elements. |
| `ZRANGENUMARRAY(START,COUNT,A%(0))` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | start value, count, output array base | Stages consecutive integers, then resident code writes them to the array. |
| `BUFNEW(LEN,H%)` / `BUFNEW(LEN)` | Module 1 slot 0, copies full `$06CE` slot-0 payload | byte length, output handle or expression handle | Allocates buffer pages in the assigned core bank and returns a one-based handle. |
| `BUFFILL(H%,BYTE)` | Module 1 slot 0, copies full `$06CE` slot-0 payload | buffer handle, fill byte | Fills buffer handles through the `$C500` page buffer and rejects non-buffer handles. |
| `BUFFREE(H%)` | Module 1 slot 0, copies full `$06CE` slot-0 payload | handle | Frees any valid handle type and clears metadata/page bitmap state. |
| `ZTEMPSCRATCH(LEN,OUT%)` / `ZTEMPSCRATCH(LEN)` | Module 1 slot 0, copies full `$06CE` slot-0 payload | byte length, output integer or expression integer | Allocates and frees temporary pages, returning page count. |
| `ZFAIL(CODE,OUT%)` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | error code, output integer | Clears output first, then reports `?RB ERROR code`. |
| `FREEMEM()` | Module 1 slot 0 at `$A800+`, descriptor-backed slice | none | Prints current live BASIC free bytes. |
| `SCRCAP(H%)` / `SCRCAP()` | Slot 14 descriptor; module 1 slot 0, copies full `$06CE` slot-0 payload | output handle or expression handle | Captures screen text `$0400-$07E7` and color RAM `$D800-$DBE7` into a typed screen handle. |
| `ERRCODE(OUT%)` / `ERRCODE()` | Resident-precomputed result; legacy low stub remains in `LOWPACK` | output integer, or expression integer | Returns the last ReadyBASIC runtime error code. |
| `ERRLINE(OUT%)` / `ERRLINE()` | Resident-precomputed result; legacy low stub remains in `LOWPACK` | output integer, or expression integer | Returns the line number of the last ReadyBASIC runtime error, or `0` for direct mode. |
| `SCRPUT(H%)` | Slot 128 descriptor; module 1 slot 0, copies full `$06CE` slot-0 payload | screen handle | Validates the screen handle type and restores text plus color RAM. |
| `ZSLOT0()` / `ZSLOT1()` / `ZSLOT2()` | Built-in module proof payloads in slot 0, slot 1, and slot 2 | none | Prove each 2K submodule slot can dispatch independently. |
| `ZSPAN()` | Built-in module 2 two-slot span payload for slots 1+2 | none | Proves a payload can claim adjacent slots. |
| `ZOVL1()` / `ZOVL2()` | Built-in module 2 slot-2 overlay proof payloads | none | Prove overlay replacement in slot 2. |
| `ZCPYRST()` / `ZCOPY()` | Built-in module 1 slot-0 copy-count proof helpers | none | Reset and inspect the tiny no-recopy proof counter. |
| `ZMODLD(NAME$)` | Built-in module 2 slot-1 disk-module loader | module package filename string | Opens generated SEQ packages such as `RBM.SAMPLE1`, `RBM.SAMPLE2`, and `RBM.SAMPLE3`, registering their descriptors in the assigned core bank and payloads in the assigned code bank. |
| `ZDM1()` / `ZDM2S()` / `ZDOV1()` / `ZDOV2()` | Disk-loaded sample module payloads in the assigned code bank | none | Prove disk module, span, and overlay registration after `ZMODLD`. |
| `ZSAA()`-`ZUEB()` | `rbm.sample3` disk-loaded proof payloads in the assigned code bank | none | Return stateful integer sentinels; the command name encodes submodule, overlay, and entrypoint while copy-count tests prove residency. `ZS/ZT/ZU` mean submodules 6/7/8, `A`-`E` mean overlays 1-5, and the final `A/B` is the entrypoint. Each overlay image has one local counter byte, so reuse increments while reload resets. |

The handle-oriented commands copy the full slot-0 payload because their wrappers
share allocator helper routines that currently live in that module payload.
That keeps the resident core lean at the cost of copying more under-ROM bytes
for these sample commands.

`SCRCAP`/`SCRPUT` were named to avoid C64 BASIC tokenizer conflicts with
embedded `SAVE`/`LOAD` tokens. They are the implemented forms of the original
screen save/load concept.

Historical proof names such as `PING`, `ADD16`, `STRUP`, `HCRC`, `SUMAI`,
`RANGEAI`, `TEMPSCRATCH`, and `FAIL` are no longer runtime command aliases.
Their current demo/proof forms use the `Z...` namespace, and array demo names
use `NUM` rather than `INT` to avoid the BASIC `INT` token.

## Command Overlay Loading And Files Involved

There is one ReadyBASIC app executable: `bin/readybasic.prg`. There is not one
PRG or executable file per command. The command overlays are linker segments
inside that one PRG load image:

| File or artifact | Role |
|---|---|
| `src/apps/readybasic/readybasic.s` | All current ReadyBASIC code, command descriptors, module/submodule payloads, overlay proof payloads, and hidden helpers. |
| `cfg/ready_app_readybasic.cfg` | Defines the load/run split: resident code, `CMDPACK`, `REGSEED`, hidden helper load image, bridge load image, and runtime overlay slots. |
| `Makefile` | Assembles `readybasic.s`, links it with `ready_app_readybasic.cfg`, and writes `bin/readybasic.prg` plus `obj/readybasic.map`. |
| `obj/readybasic.map` | Current source of truth for segment ranges and sizes. |
| `bin/readybasic.prg` | The single ReadyOS app executable that contains resident code plus cold-load seed images. |
| `src/apps/readybasic/rbtest1.bas` / `obj/rbtest1.prg` | Legacy sample BASIC program only; not part of the command overlay mechanism. |
| `src/apps/readybasic/rbproc1.bas` / `obj/rbproc1.prg` | Positive `PROC`/`FUNC` sample: no-param PROC, `%`, `$`, explicit `RET%`/`RET$`, colon chain, normalized `IF THEN :EXEC`, nested depth 2, int/string/float command and FUNC expression returns, nested ReadyBASIC actuals, statement `FADD` output, string concatenation through ROM functions, and readable `LIST`. |
| `src/apps/readybasic/rbprocerr.bas` / `obj/rbprocerr.prg` | Negative `PROC`/`FUNC` sample; run sections by line number to exercise unknown routine, wrong count/type, statement `EXEC` to `FUNC`, PROC extra actual, bare `ENDP`, return-stack overflow, malformed nested actuals, and float/string/numeric context errors. |
| `build_support/verify_readybasic_plugin.py` | Static guardrail checker for the ReadyBASIC layout and REU constants. |
| `READYBASIC_MAKING_COMMAND_GUIDE.md` / `readybasic_making_command_guide.html` | Walkthrough for adding commands using the current demo, string, array, hidden, and REU-handle examples. |

The linker puts packed command bytes in the PRG load image at `CMDPACK`
`$2B00-$3FFF`, but their runtime addresses are different:

| Segment | Size | Load/source role | Runtime role |
|---|---:|---|---|
| `LOWPACK` | `$06CE` (1.7K, 1742 exact bytes) | Historical segment name for the built-in module 1 slot-0 payload loaded from `CMDPACK` and prestashed to assigned code-bank offset `$0000`. | Fetched on demand into `$A800-$AECD`. |
| `SLOTPACK1` | `$023B` (571B) | Built-in module 2 proof and streaming `ZMODLD` loader payload, prestashed to assigned code-bank offset `$06CE`. | Fetched on demand into `$B000-$B23A`. |
| `SLOTPACK2` / `SPANPACK` / `OVL1PACK` / `OVL2PACK` | `$0015` each (21B) | Built-in module 2 slot, span, and overlay proof payloads, prestashed after `SLOTPACK1`. | Fetched into slot 2, slots 1+2, or slot-2 overlay target addresses. |
| `HIDLOAD` | `$06EA` (1.7K, 1770 exact bytes) | Load-only hidden helper seed starting at `$4000`. | Copied on cold boot into `$A000-$A6E9` and stashed to the assigned core-bank hidden shadow at `$3000`. |
| `BRLOAD` | `$01FF` (511B) | Load-only bridge seed starting at `$4800`. | Copied on cold boot into `$C000-$C1FE`. |
| `REGSEED` | `$1010` (4.0K, 4112 exact bytes) | Load-only registry header and 128 command descriptors at `$5000-$600F`. | Copied on cold boot into assigned core-bank offsets `$0000` and `$1000`. |

Cold boot is the only time the load-image command pack and `REGSEED` are trusted.
The hidden helper copies the registry/header to the assigned core bank and
copies the built-in module/submodule payloads from `CMDPACK` to the assigned code bank. After
that, BASIC may own the former load-image addresses, so warm resume reuses the
REU copies and does not reseed from `$2B00+`, `$4000+`, `$4800+`, or `$5000+`.

At command execution time, the descriptor tells the resident loader which bytes
to fetch:

1. Descriptor bytes `2-5` name the payload offset and size in the assigned code bank.
2. Descriptor bytes `6-9` name the submodule id, overlay id, slot mask, and
   generation/check byte.
3. Descriptor bytes `10-13` name the runtime destination offset from `$A000`
   and the entry offset inside the loaded payload.
4. ReadyBASIC maps RAM under BASIC ROM, fetches the payload into `$A800`,
   `$B000`, `$B800`, or a multi-slot span, calls the entry, then returns to
   visible resident code for result commit.
5. Commands can fetch a small slice or a full slot payload. The current handle
   and screen-handle commands fetch the whole `$06CE` slot-0 payload because
   their shared allocator and screen-copy helpers live there.

## C64 RAM Layout

ReadyBASIC runs inside the ReadyOS app working region `$1000-$C5FF`. ReadyOS
metadata and shim space above that are not general ReadyBASIC scratch.

| Region | Current range | Size | Owner and role |
|---|---:|---:|---|
| `ENTRY` | `$1000-$11FF` | `$0200` (512B) | Tiny entry, cold/warm discriminator, early hidden/bridge copies, and prompt hotkey helpers. |
| `RESIDENT` | `$1200-$2ABD` | `$18BE` (6.2K, 6334 exact bytes) | Visible parser, vector hooks, BASIC ROM calls, REU DMA wrappers, result commit, bare command dispatch, expression hook, native `PROC`/`FUNC`/`RET`, `REPEAT`/`UNTIL`, `LABEL`/`JUMP`, error introspection, proper nested term state, float helpers, and prompt hotkey dispatch. |
| BASIC sentinel | `$2AC0` | 1 byte | Must stay zero before stored-program `RUN`. |
| BASIC workspace | `$2AC1-$9FFF` | `$753F` region, `30013` formula free bytes (29.3K) | Program text, variables, arrays, string heap. |
| Command pack load image | `$2B00-$3FFF` | `$1500` (5.25K) file range | Built-in module/submodule payload seed bytes before cold prestash. |
| Hidden helper load image | `$4000+` | `$06EA` (1.7K, 1770 exact bytes) load-only | Hidden helper seed copied to `$A000` and stashed to assigned core-bank offset `$3000`. |
| Bridge load image | `$4800+` | `$01FF` (511B) load-only | Bridge seed copied to `$C000`. |
| Registry seed load image | `$5000-$600F` | `$1010` (4.0K, 4112 exact bytes) load-only | Header and 128 descriptors copied to the assigned core bank. |
| Runtime snapshot | Assigned core bank offsets `$0A00-$0BFF` | `$0200` (0.5K) plus bridge metadata | Saved zero page, stack page, SP, resume mode, line-chain guards. |
| Common under-ROM helper | `$A000-$A6E9` | `$06EA` (1770B) | Helper code run with RAM mapped under BASIC ROM. |
| Slot 0 module payload | `$A800-$AECD` | `$06CE` (1742B) | Module 1 system/default payload fetched from the assigned code bank. |
| Slot 1 module payload | `$B000-$B23A` | `$023B` (571B) | Module 2 proof and streaming `ZMODLD` loader payload. |
| Slot 2 proof/overlays | `$B800-$B83E` | `$003F` (63B) | Current slot-2 base and overlay proof slices. |
| `BRIDGE` | `$C000-$C1FE` | `$01FF` (511B) | Persistent bridge state, saved vectors, overlay variables, current handle scratch, debug bytes, native routine return stack, and flow-control scratch. |
| Shared frames | `$C200-$C5FF` | `$0400` (1.0K) | Call frame, result frame, descriptor buffer, command-name buffer, page/runtime buffers. |
| Hidden helper shadow | Assigned core bank `$3000+` | `$06EA` (1770B) | REU source for restoring `$A000` helper on warm resume; refreshed during `EXIT` and cold seed. |
| ReadyOS REU metadata | `$C600-$C7FF` | `$0200` (0.5K) shared | ReadyBASIC only marks REU bank ownership here. |
| ReadyOS shim ABI | `$C800-$C9FF` | `$0200` (0.5K) shared | ReadyOS jump table and data; not ReadyBASIC RAM. |

The PRG load image is larger than the live resident core. On cold entry,
ReadyBASIC copies the hidden helper seed from the load image to `$A000`, stashes
the helper shadow in the assigned core bank at `$3000`, copies the bridge seed to `$C000`, and prestashes
registry/code seed data into REU. After that, BASIC owns `$2AC1-$9FFF`. Warm resume must therefore
not reread load-only seed tables at `$4000+`, because that address range may now
be BASIC program or variable storage.

### Stage-Specific Memory Use

Some map entries intentionally overlap the eventual BASIC workspace. That is
not a contradiction; it is a time-of-use distinction.

| Stage | C64 RAM ownership | BASIC-visible effect |
|---|---|---|
| PRG load / cold seed | `CMDPACK` is loaded at `$2B00-$3FFF`, `HIDLOAD` at `$4000+`, `BRLOAD` at `$4800+`, and `REGSEED` at `$5000-$600F`. These ranges are inside the future BASIC workspace but BASIC is not live there yet. | No user BASIC program or variables exist yet, so the load image can safely occupy this space temporarily. |
| End of cold seed | Built-in module/submodule payloads have been copied from `CMDPACK` to the assigned code bank; the registry has been copied to the assigned core bank; hidden and bridge live copies are in their runtime homes. | `$2AC1-$9FFF` becomes the BASIC workspace. The former load-image bytes are now disposable. |
| Ready prompt / running BASIC | BASIC owns `$2AC1-$9FFF`, including the old `$2B00-$600F` load ranges. Command code is fetched from REU into `$A800`, `$B000`, and/or `$B800` under BASIC ROM only while a command runs. | Empty BASIC free space is `30013` formula bytes. Warm resume never trusts the old load-image addresses. |
| Future command growth | The current `CMDPACK` reservation is `$1500` (5.25K). Today it carries `$095D` / 2.3K of built-in module/submodule payloads. | The remaining reserved `CMDPACK` capacity can absorb `$0BA3` / 2.9K more built-in payload seed bytes without reducing steady-state BASIC free bytes. Growing beyond the reserved load-only area may increase PRG size or require another cold-only seed range, but it should still be reclaimed before BASIC owns the workspace. |

The visual way to read this: `CMDPACK` looks like it overlaps BASIC RAM in the
link/load map because it really does during cold loading. It does not reduce the
steady-state BASIC workspace because its live copy is in REU before the user can
store a BASIC program.

`CMDPACK` is only the current C64 cold-load seed window. It is not the total
command-code capacity of the architecture. The current descriptor format points
into the assigned code bank with 16-bit offsets and sizes, so the current single code
bank can hold up to `$10000` bytes (64.0K) of packed command bodies. The built-in
payloads currently use `$1119` (4.3K, 4377 exact bytes), leaving `$EEE7`
(59.7K, 61159 exact bytes) available in the assigned code bank. To actually seed beyond the current 5.25K
`CMDPACK` linker window, the cold-load layout would need a larger or additional
load-only seed range, copied to REU before BASIC owns `$2AC1-$9FFF`. Going
beyond one 64K code bank would require a descriptor/loader extension for
additional command-code banks.

## BASIC Free RAM Compared With Stock C64 BASIC

Stock C64 BASIC V2 starts at `$0801` and normally has memory top at `$A000`,
which gives about `38911` bytes free on an empty machine.

ReadyBASIC relocates BASIC to `$2AC1` and uses `$A000` as the BASIC memory top.
On an empty ReadyBASIC workspace, variables begin at `$2AC3`, so the practical
empty BASIC free space is:

```text
$A000 - $2AC3 = 30013 bytes (29.3K)
```

That is `8898` bytes (8.7K) less than stock C64 BASIC, or about `77.1%` of the
stock empty BASIC free space. The latest extra resident growth pays for
`REPEAT`/`UNTIL`, `LABEL`/`JUMP`, and error introspection while keeping the
resident segment below the measured `$2ABF` ceiling.

| Environment | BASIC text start | BASIC top | Empty free bytes |
|---|---:|---:|---:|
| Stock C64 BASIC V2 | `$0801` | `$A000` | `38911` (38.0K) |
| ReadyBASIC current layout | `$2AC1` | `$A000` | `30013` (29.3K) |
| Difference | - | - | `-8898` (-8.7K) |

Strategies to maximize BASIC RAM while adding many more commands:

- Keep the resident core below the current BASIC page boundary; every resident byte is permanent C64 RAM
  pressure.
- Put command implementation code in packed REU code banks and copy it into the
  existing overlay slots only when invoked.
- Reuse existing signatures and commit paths where possible. A new command with
  an existing parameter/result shape should not need new resident parser code.
- Keep descriptors in the REU registry, not in resident RAM.
- Move command-private persistent data to dynamic REU banks and expose small
  BASIC integer handles instead of storing large data in BASIC RAM.
- Split or group overlay helper routines only when it reduces total copied bytes
  without bloating resident RAM.
- If the current assigned code bank becomes crowded, extend the registry model to
  support additional packed code banks rather than lowering BASIC's top or
  adding permanent C64-resident command code.
- Consider a compact REU-backed signature/parameter table if many future
  commands would otherwise require one resident parser case each.

Current per-command overhead:

| New command kind | Permanent C64 RAM overhead | REU/load-image overhead |
|---|---:|---|
| New command reusing an existing signature and submodule slot | Usually `0` bytes of BASIC workspace and `0` bytes of resident RAM. | One 32-byte descriptor in the assigned core bank, command code bytes in the assigned code bank, and matching load-image bytes in the appropriate module/submodule payload. |
| New command needing a new parameter/result signature | No BASIC workspace cost, but resident parser/commit code grows by the new shared support. | One 32-byte descriptor plus command code bytes. |
| New command needing persistent data | No BASIC workspace cost if represented as a handle. | Descriptor/code bytes plus REU data-bank allocation and handle metadata. |
| New command needing a larger fixed C64 buffer | Permanent C64 RAM cost only if it expands `$C200-$C5FF`, the overlay slots, resident RAM, or bridge state. | Depends on whether the data can be moved to REU instead. |

## Page-3 Vector Ownership

ReadyBASIC saves the original BASIC vectors, then installs:

| Vector | Address | ReadyBASIC role |
|---|---:|---|
| Crunch | `$0304/$0305` | Calls ROM crunch first, then normalizes tokenized `THEN EXEC ...` and `THEN JUMP ...` into colon-prefixed statements. |
| Execute | `$0308/$0309` | Peeks for `EXIT`, `PROC`, `FUNC`, `EXEC`, `ENDP`, or a bare descriptor command; otherwise tail-calls the original execute vector without advancing `TXTPTR`. |
| Eval | `$030A/$030B` | Recognizes selected `COMMAND(...)` and `FUNC(...)` expression returns, then falls back to ROM expression evaluation. |
| IMAIN | `$0302/$0303` | Marks the BASIC direct prompt active when ROM BASIC returns to `READY.`. |
| KEYLOG | `$028F/$0290` | Catches KERNAL-decoded prompt hotkeys only while ReadyBASIC owns the direct prompt, records the pending ReadyOS action, clears the consumed key state, and queues Return. |
| CHRIN | `$0324/$0325` | Wraps the original CHRIN, preserves normal return semantics for ordinary input, and abort-dispatches pending prompt hotkeys before BASIC stores or executes the partial line. |
| List | `$0306/$0307` | Saved/restored, but V1 leaves normal ROM listing behavior. |

Page-3 vectors are global machine state. `EXIT` restores the original vectors
before jumping back through the ReadyOS shim so the launcher or another app
cannot accidentally dispatch through stale ReadyBASIC code.

## Command Dispatch Pipeline

1. BASIC dispatches a statement through the execute vector.
2. ReadyBASIC peeks at the next non-space byte without mutating `TXTPTR`.
3. If the statement is neither `EXIT`, a native routine keyword, nor a known
   bare descriptor command followed by `(`, ReadyBASIC tail-calls the
   saved ROM execute vector.
4. For a bare command, ReadyBASIC parses and normalizes the command name into
   `$C4A0`, requires `(`, and reuses the descriptor lookup and signature parser.
5. Command lookup linearly scans up to 128 fixed 32-byte descriptors in the
   assigned core bank at `$1000-$1FFF`. It fetches one 256-byte page into `$C500`,
   scans eight descriptors locally, and copies a match into `$C480`. Zero-filled
   filler descriptors are empty command slots.
6. The resident parser dispatches by signature id and uses BASIC ROM helpers to
   parse parameters and capture output references.
7. Output variables are cleared before command execution.
8. The call frame at `$C200` is mirrored to assigned core-bank offset `$0400`.
9. Command code is fetched from the assigned code bank into one or more under-ROM
   submodule slots at `$A800`, `$B000`, and `$B800`.
10. The worker writes a compact result frame at `$C300`.
11. The result frame is mirrored to assigned core-bank offset `$0400`.
12. Resident code checks status, prints `?RB ERROR n` on failure, or commits
    integer, string, or array results to the captured BASIC output reference.

The command-name parser accepts normal letters/digits, folds lowercase ASCII to
uppercase, maps shifted uppercase/PETSCII-like `$C1-$DA` bytes back to `A-Z`,
and handles a small number of tokenization edge cases that can appear inside
names.

## Frames And ABI Surfaces

| Surface | Address or offset | Role |
|---|---:|---|
| Call frame | `$C200` | Command id, parameter count, numeric slots, pointer/count slots, string input buffer. |
| Result frame | `$C300` | Status, error number, value tag, scalar value, string output buffer, array output buffer. |
| Descriptor buffer | `$C480` | One descriptor fetched from the assigned core bank. |
| Command buffer | `$C4A0` | Uppercase normalized command name. |
| Page buffer | `$C500` | 256-byte staging page for descriptor scans, REU handle operations, heap bitmap scans, and warm-resume stack buffer. |
| REU call snapshot | Assigned core bank `$0400` | Copy of the current call frame. |
| REU result snapshot | Assigned core bank `$0400` | Copy of the current result frame. |
| Runtime zero-page snapshot | Assigned core bank `$0A00` | Saved zero page, restored through the temporary buffer at `$C400`. |
| Runtime stack snapshot | Assigned core bank `$0B00` | Saved stack page, restored through the temporary buffer at `$C500`. |

The descriptor ABI is fixed-size and compact:

| Descriptor offset | Field |
|---:|---|
| `0` | Command id. |
| `1` | Module id. |
| `2-3` | Payload offset in the assigned code bank. |
| `4-5` | Payload size. |
| `6` | Submodule id. |
| `7` | Overlay id, or `0` for a base submodule. |
| `8` | Slot mask: bit 0 = `$A800`, bit 1 = `$B000`, bit 2 = `$B800`. |
| `9` | Generation/check byte. |
| `10-11` | Runtime destination offset from `$A000`. |
| `12-13` | Entry offset inside the loaded payload. |
| `14` | Signature id. |
| `15` | Uppercase command-name length. |
| `16-31` | Uppercase command-name bytes, zero padded. |

Supported result tags are:

| Tag | Meaning |
|---:|---|
| `0` | No result. |
| `1` | Integer result. |
| `2` | String result. |
| `3` | Integer array result. |

String output is committed only by visible resident code. Workers stage string
bytes in the result frame; resident code allocates BASIC string heap space by
lowering `FRETOP` and updates the output string descriptor. Hidden and low
workers do not directly mutate the BASIC string heap.

## REU Layout

ReadyBASIC uses two launcher-assigned REU resource banks:

| Resource | ReadyOS type | Purpose |
|---:|---:|---|
| core bank | `14` | ReadyBASIC core/system storage. |
| code bank | `15` | Packed ReadyBASIC command-code storage. |

The launcher marks the assigned physical banks in the ReadyOS REU allocation
table at `$C600-$C6FF`. ReadyBASIC scans that table at startup and does not use
`$C600-$C7FF` as private scratch.

### Assigned Core Bank: Core/System Storage

| Offset | Region |
|---:|---|
| `$0000` | Header: `RBPL`, version, descriptor count, descriptor size, and frame offsets. |
| `$0400` | Current call-frame snapshot. |
| `$0400` | Current result-frame snapshot. |
| `$0600` | Reserved debug ring area. |
| `$0800-$09FF` | REU-backed handle directory: 128 descriptors, 4 bytes each. |
| `$0A00` | Saved zero page for ReadyOS suspend/resume. |
| `$0B00` | Saved stack page for ReadyOS suspend/resume. |
| `$0C00-$0CFF` | 192-byte heap page bitmap plus reserved bytes. |
| `$1000-$1FFF` | 128 command descriptor slots, 32 bytes each. Slot 14 is `SCRCAP`; slot 128 is `SCRPUT`; zero-filled slots are unused fillers. |
| `$2000-$3FFF` | Reserved common/system space for future ReadyBASIC metadata. |
| `$4000-$FFFF` | Typed handle heap: 192 pages / 48KB. |

The descriptor table intentionally leaves a large filler span between the two
screen commands:

| Descriptor range | REU offset | Role | Slots | Size |
|---|---:|---|---:|---:|
| Slots 1-14 | `$1000-$11BF` | Current front commands from `ZECHO1` through `SCRCAP`. | 14 | `$01C0` / 448B |
| Slots 15-127 | `$11C0-$1FDF` | Zero-filled filler descriptors available for future commands. | 113 | `$0E20` / 3.5K / 3616 exact bytes |
| Slot 128 | `$1FE0-$1FFF` | `SCRPUT`, deliberately placed at the end to prove full-table lookup. | 1 | `$0020` / 32B |

The persistent handle model supports 128 live handles. Each handle is
represented to BASIC as a small integer from `1` to `128`, while canonical
metadata lives in REU and bridge RAM keeps only the current descriptor scratch.
Type `1` is a byte buffer, and type `2` is a screen text+color buffer.
`BUFFILL` accepts only buffer handles; `BUFFREE` frees any valid handle;
`SCRPUT` accepts only screen handles. The typed heap uses assigned core-bank pages
`$40-$FF`; future large or long-lived objects should allocate additional REU
banks and keep the same small handle model.

### Assigned Code Bank: Packed Command Code

| Offset | Region |
|---:|---|
| `$0000-$06CD` | Built-in module 1 slot-0 payload copied into `$A800-$AECD` (`$06CE`, 1742B). |
| `$06CE-$0908` | Built-in module 2 slot-1 proof and streaming `ZMODLD` loader payload copied into `$B000-$B23A` (`$023B`, 571B). |
| `$0909-$095C` | Built-in slot-2, span, and overlay proof slices (`$0054`, 84B total). |
| `$095D-$14FF` | Free gap before current disk-module descriptor proof offsets (`$0BA3`, 2979B). |
| `$1500-$151F` | `rbm.sample1` descriptor for `ZDM1`. |
| `$1600-$165F` | `rbm.sample2` descriptors for `ZDM2S`, `ZDOV1`, and `ZDOV2`. |
| `$1700-$1ABF` | `rbm.sample3` descriptors for `ZSAA`-`ZUEB`. |
| `$3000-$3014`, `$3200-$3214`, `$3300-$3314`, `$3400-$3414` | Small sample disk-loaded payload proofs. |
| `$3800-$463C` | `rbm.sample3` payload records for `ZSAA`-`ZUEB`, stored on `$100`-byte strides. |

Descriptors point into these packed bytes with payload offset, payload size,
slot mask, runtime destination, and entry offset. Heap and screen-handle
commands currently fetch the whole `$06CE` slot-0 payload because shared
allocator and screen-copy helpers live there.

## Cold Boot Lifecycle

1. ReadyOS loads `readybasic.prg` at `$1000`.
2. Entry code checks its local warm/cold magic.
3. On cold path, it maps RAM under BASIC ROM long enough to copy the hidden
   helper seed to `$A000`, then stashes the helper shadow to assigned core-bank
   offset `$3000`.
4. It copies bridge seed bytes from the load image to `$C000`.
5. The resident core resets KERNAL/BASIC vectors, saves originals, and installs
   ReadyBASIC crunch/execute/eval, KEYLOG, and CHRIN hooks.
6. BASIC is relocated to `TXTTAB=$2AC1` with top at `$A000`.
7. `$2AC0`, `$2AC1`, and `$2AC2` are cleared. `$2AC0` is the sentinel byte
   required by BASIC `RUN`.
8. The assigned core bank receives the header and descriptors from `REGSEED`.
9. The assigned code bank receives the packed low and hidden command code.
10. ReadyBASIC clears/redraws the screen, prints its banner, and enters
    `BASIC_READY`.

## EXIT, Suspend, And Warm Resume

Manual prompt `EXIT` and prompt-level `CTRL+B` share the launcher-return path.
Prompt-level `F2`/`F4` use the same state-save/vector-restore setup, then write
the selected loaded app bank to `$C820` and jump through `$C80F`. The selected
bank is first copied into bridge state before yield preparation, then replayed
after the hidden save/vector/channel cleanup has run; scratch bytes used by the
loaded-bank scan are not trusted across the yield path. The keyboard path
records a pending action, queues only Return, then lets the CHRIN hook dispatch
before BASIC stores or executes the editor line. READY-mode hotkey yields force
the saved resume stack to a clean BASIC-ready value instead of preserving the
transient CHRIN/editor call depth.
Before a prompt hotkey yield, ReadyBASIC waits for the exact physical chord that
selected the action to be released, then clears editor/KERNAL key state again.
Cold and warm entry also scan the physical matrix and suppress the same
still-held ReadyOS hotkey until it has been released. This keeps one F2/F4 press
from double-switching through the next app and keeps the resumed BASIC editor
keyboard-live.

On `EXIT`, ReadyBASIC:

1. Identifies direct-prompt return by checking that `TXTPTR` is below
   `BASIC_START`.
2. Stores bridge and entry magic for READY-mode resume.
3. Calls hidden save-state code.
4. Saves zero page `$0000-$00FF` to assigned core-bank offset `$0A00`.
5. Saves stack page `$0100-$01FF` to assigned core-bank offset `$0B00`.
6. Saves SP, mode, runtime magic, and line-chain guards in bridge metadata.
7. Refreshes the assigned-core-bank hidden-helper shadow at `$3000`.
8. Clears pending ReadyBASIC/KERNAL keyboard state and calls `CLRCHN`.
9. Restores the original page-3 BASIC/KERNAL vectors.
10. Jumps to the ReadyOS shim return entry at `$C80C`.

For `F2`/`F4`, ReadyBASIC scans the shim loaded-bank bitmap `$C836-$C838` from
the current bank `$C834`. If a neighbor app is found, it performs the same
READY-mode save/restore setup, persists the target bank in bridge state, clears
editor hotkey state, restores channels/vectors, writes the saved target to
`$C820`, and jumps to the shim switch entry `$C80F`. If no neighbor is loaded,
the key is consumed and the BASIC editor continues waiting.

On warm resume, ReadyOS restores the app window and jumps back to `$1000`.
ReadyBASIC:

1. Sees entry-local warm magic.
2. Restores the hidden helper at `$A000` from the preserved assigned-core-bank `$3000` shadow.
3. Reinstalls ReadyBASIC-owned vectors.
4. Re-marks REU bank ownership for the assigned ReadyBASIC core/code banks.
5. Does not reread cold-only `REGSEED` or command-pack load images.
6. Restores zero page and stack from the assigned core bank through `$C400/$C500`, and
   restores SP/mode metadata from the bridge.
7. Preserves live BASIC pointers such as `FRETOP`, `VARTAB`, `ARYTAB`, and
   `STREND`.
8. In READY-mode resume, clears the launcher surface, redraws the ReadyBASIC
   banner, positions the prompt, and enters `BASIC_READY`.

## Hidden Code And Banking Contract

ReadyBASIC uses hidden code in two places:

| Code | Range | Purpose |
|---|---:|---|
| Hidden helper | `$A000-$A6C7` | REU prestash, save/restore helpers, bank-sensitive work. |
| Command slots | `$A800-$BFFF` | Module/submodule payload slots fetched from the assigned code bank. |

Before calling hidden code, ReadyBASIC saves flags, disables interrupts, forces
the low CPU data-direction bits in `$0000` to outputs, saves `$0001`, maps RAM
under BASIC ROM while keeping KERNAL visible, performs the copy or call, then
restores `$0001` and flags. This keeps BASIC ROM/RAM banking explicit and keeps
KERNAL-visible calls safe where needed.

## Invariants

- ReadyBASIC is verified through normal ReadyOS run/profile flows, not by
  loading an individual app directly.
- `BASIC_START` is `$2AC1`.
- `$2AC0` must remain zero before stored-program `RUN`.
- `RESIDENT` must stay below `$2AC0`.
- `BRIDGE` must stay below `$C200`, leaving `$C200-$C5FF` for relocated frames.
- `$C600-$C7FF` is shared ReadyOS REU metadata, not ReadyBASIC scratch.
- `$C800-$C9FF` is ReadyOS shim ABI, not app RAM.
- Warm resume restores `$A000` from the assigned-core-bank hidden-helper shadow before hidden helper calls.
- Warm resume must not cold-reset `FRETOP`, `VARTAB`, `ARYTAB`, or `STREND`.
- ReadyBASIC-owned vectors are restored before yielding to ReadyOS.
- Non-ReadyBASIC statements tail-call the original execute vector without
  mutating `TXTPTR`.
- Output variables are cleared before command execution.
- String heap writes happen only in visible resident code.
- Acceptance re-entry uses launcher menu navigation, not direct app-bank
  hotkeys.

## Current Verification Evidence

Static guardrails:

```sh
make readybasic-plugin-static-check
```

Current static layout:

| Segment | Range | Size |
|---|---:|---:|
| `ENTRY` | `$1000-$11FF` | `$0200` (512B) |
| `RESIDENT` | `$1200-$2ABD` | `$18BE` (6.2K, 6334 exact bytes) |
| `REGSEED` | `$5000-$600F` | `$1010` (4.0K, 4112 exact bytes) |
| `HIDDEN` | `$A000-$A6E9` | `$06EA` (1770B) |
| `LOWPACK` / slot 0 payload | `$A800-$AECD` | `$06CE` (1742B) |
| `SLOTPACK1` / slot 1 payload | `$B000-$B23A` | `$023B` (571B) |
| slot 2 proof/overlays | `$B800-$B814` | `$0015` (21B) |
| `BRIDGE` | `$C000-$C1FE` | `$01FF` (511B) |

Current measured guardrails:

| Measure | Current value |
|---|---:|
| `BASIC_START` | `$2AC1` |
| Empty BASIC free bytes | `30013` |
| `bin/readybasic.prg` size | `20994` |
| `RESIDENT` | `$18BE` / 6334B |
| `LOWPACK` | `$06CE` / 1742B |
| `HIDDEN` | `$06EA` / 1770B |
| `BRIDGE` | `$01FF` / 511B |
| `REGSEED` | `$1010` / 4112B |

Recent VICE coverage includes:

The broad external command/program/lifecycle/state wrappers have been refreshed
for bare parenthesized syntax. The demo suite is intentionally viewer-paced;
the regression probes stay shorter and more assertion-heavy.

| Probe | Coverage |
|---|---|
| Full expression probe | Direct bare command statements, command expressions, parenthesized `EXEC PROC`, `FUNC` with later assignment plus `RET`, numeric `FUNC` expression return/assignment, string `FUNC` expression return, and readable `LIST`. |
| Plugin command probe | Direct command statements, `IF ... THEN` assignment/expression coverage, `UPPER`/`LOWER`, old-name rejection, string/REM safety, leading-comma rejection, `SCRCAP`/`SCRPUT`, slot-128 lookup, 128-handle edge, 48KB heap edge, screen heap exhaustion, wrong-handle-type rejection, screen-handle free, resume. |
| Program probe | Stored line start, colon chains, true/false `IF ... THEN` assignment/expression coverage, `FOR/NEXT`, strings, REM, DATA, arrays, hidden worker, handles, failure clearing. |
| `rbproc1` probe | Stored positive `PROC`/`FUNC`: no-param PROC, `%`, `$`, explicit `RET%`/`RET$`, colon chain, normalized `IF THEN :EXEC`, nested depth 2, int/string/float command and FUNC returns, `FADD` expression and statement forms, nested ReadyBASIC actuals, string concatenation, and readable `LIST`. |
| `rbprocerr` probe | Stored negative `PROC`/`FUNC`: unknown routine, wrong count/type, statement `EXEC` to `FUNC`, PROC extra actual, `ENDP` without `EXEC`, return-stack overflow, malformed nested actuals, and return/context type errors. |
| Full visual verification | Human-watchable command, program, screen-handle, handle/heap edge, resume, and error coverage. |
| Lifecycle probe | Cold entry, `EXIT`, launcher re-entry, READY-mode redraw. |
| State probe | BASIC variable/string survival and command availability after resume. |
| `rbtest1` probe | Sample program assembled at the relocated BASIC workspace. |
| Large-vars probe | BASIC workspace and variable behavior under heavier state. |
| Cross-app resume stress | ReadyBASIC survives repeated app switches. |
| Second-entry/editor stress | ReadyBASIC survives editor/launcher round trips and later re-entry. |
| Demo automation suite | Viewer-paced walkthrough covering `FREEMEM`, editor round trip, assembler commands, `PROC`/`FUNC`, parameter groups, expected errors, REU handles, and nested expression forms. |

Some harness wrappers can report a process-level `partial` status even when
every step is `ok` and `FailedStep` is `null`; for ReadyBASIC these were treated
as harness shutdown-status quirks, not command failures.

## Current Bare Commands And Expressions

Parenthesized calls are the preferred syntax. Selected commands can return
values as BASIC expressions while keeping the resident implementation tight.

Supported command expressions:

```basic
ZECHO1(P%)
ZADD16(4,5,A%)
PRINT ZADD16(5,10)
A=ZADD16(8,9)
T$=UPPER("ready")
PRINT ZHIDDENRAM("A")
```

Supported native routine forms:

```basic
100 PROC SHOWI(P%)
110 PRINT P%
120 ENDP

200 FUNC GREET(N$)
210 RET "HI "+N$
220 ENDP

10 EXEC SHOWI(7)
20 T$=GREET("READY"):PRINT T$
```

`PROC`/`FUNC` definitions and `EXEC` calls accept parentheses for non-empty
argument lists. Zero-argument routines still use `EXEC NAME`; `EXEC NAME()` was
cut to save resident bytes. `FUNC` uses `RET expr`, with optional `RET% expr`
or `RET$ expr` type markers. `EXEC` runs `PROC` bodies only; `FUNC` returns
the value as the expression result. `FUNC` calls scan the body, execute
simple scalar assignments, and evaluate the `RET` expression; arbitrary earlier
BASIC statements remain outside V1.

Numeric actuals for command and `FUNC` calls can be ordinary numeric
expressions in the flat forms tested by `rbproc1`, such as `ADDI(1,2+4)`.
ReadyBASIC also accepts a single wrapper pair around numeric actual
expressions, including `ADDI(1,(2+4))`, `ZADD16(1,(2+4))`, and
`ADDI((1+2),(3+4))`. String actuals remain string variables or quoted literals.
Command and `FUNC` returns can be assigned or printed directly; command numeric
returns work in `ABS(ZADD16(1,6)-10)`, and `FUNC` returns now work in the tested
ROM consumer forms `ABS(ADDI(1,6)-10)` and `LEFT$(GREET("READY"),2)`. Fully
recursive ReadyBASIC terms inside other ReadyBASIC actual lists remain future
work.

## Current Float-Term Support

Selected ReadyBASIC calls behave like real BASIC expression terms in the tested
nested contexts. Plain C64 BASIC float values are supported on command and
`FUNC` paths.

Supported examples:

```basic
A=ABS(FADD(1.2,2.3)-3)
A%=ABS(ADDI(1,6)-10)
A$=LEFT$(GREET("READY")+"!",3)
PRINT ADDI(1,ADDI(2,3))
PRINT FADD(1.5,FADD(2.25,3.25))
PRINT LEFT$(UPPER(GREET("ready")),2)
FUNC SCALE(X)
RET X*1.5
ENDP
```

`FADD(A,B)` is the float demo command. It is resident-computed because the low
overlay runs with BASIC ROM hidden and cannot safely call ROM float helpers.
The descriptor still exists so registry lookup and syntax are exercised; the low
code is only a one-byte `RTS` stub. `FADD(A,B,Q)` is the statement form and
requires a plain numeric output variable. `FADD(A,B,A%)` is rejected.

Current verification includes `rbproc1` lines for `FADD`, nested `FADD`, float
`FUNC` input/return, nested `ADDI`, `ABS(FADD(...)-3)`, statement-form float
output, string concatenation with a `FUNC` return, and
`LEFT$(UPPER(GREET(...)),2)`. `rbprocerr` adds negative sections for malformed
nested actuals, float output to `%`, string return in numeric context, and
numeric return in string context.
