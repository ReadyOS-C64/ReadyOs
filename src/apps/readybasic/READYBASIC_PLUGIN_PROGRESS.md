# ReadyBASIC Plugin Progress

> Chronology note: entries below record the contract that existed when each
> experiment ran. The current schema-v5 contract uses a `$1000-$C7FF` (`$B800`)
> app snapshot and a combined ReadyOS bank at physical `Skip+1`; ReadyBASIC
> leaves `$C600-$C7FF` unused to preserve its custom assembler/linker shape.

This is a chronological progress log. Older entries intentionally preserve the
layout and addresses that were current when the tests were run. For the current
memory map, use `READYBASIC_CURRENT_DESIGN.md`.

Current runtime command names use `ZECHO1`, `ZADD16`, `UPPER`, `LOWER`,
`ZHIDDENRAM`, `ZSUMNUMARRAY`, `ZRANGENUMARRAY`, `ZTEMPSCRATCH`, `ZPAUSE`,
`ZFAIL`, `MEMAVL`, `ERRCODE`, and `ERRLINE` for the core demo/proof commands.
The module/submodule branch also includes `ZSLOT0`, `ZSLOT1`, `ZSLOT2`,
`ZSPAN`, `ZOVL1`, `ZOVL2`, `ZCPYRST`, `ZCOPY`, `ZMODLD`, and disk-loaded sample
commands `ZDM1`, `ZDM2S`, `ZDOV1`, and `ZDOV2`. Native language features also include `PROC`/`FUNC`,
`REPEAT`/`UNTIL`, and `LABEL`/`JUMP`. Older entries below may mention historical names such
as `PING`, `ADD16`, `STRUP`, `HCRC`, `SUMAI`, `RANGEAI`, `TEMPSCRATCH`, and
`FAIL`; those are retained as dated notes, not current aliases.

## 2026-05-25: Repeat/Until, Label/Jump, And Error Introspection

- Branch: `codex/0.2.4-dev` after merging the ReadyBASIC repeat/label work.
- Added resident `REPEAT` / `UNTIL expr` post-test loops. The loop stack is four
  entries deep; overflow reports `?RB ERROR 35`, and `UNTIL` without a matching
  active `REPEAT` reports `?RB ERROR 36`.
- Added resident `LABEL name` / `JUMP name` stored-program transfer. `JUMP`
  scans for the named label and works forward, backward, after colons, and after
  normalized `IF ... THEN`. Missing labels report `?RB ERROR 39`; numeric
  `GOTO` remains BASIC ROM behavior.
- Added `ERRCODE` and `ERRLINE` expression/statement forms for the last
  ReadyBASIC runtime error code and line.
- Memory impact versus expression-style branch:
  - `BASIC_START`: `$2401` -> `$2AC1`.
  - Empty BASIC free bytes: `31741` -> `30013`, delta `-1728`.
  - `RESIDENT`: `$11FE` / 4606B -> `$18BA` / 6330B, delta `+1724`.
  - `BRIDGE`: `$01EA` / 490B -> `$01F4` / 500B, delta `+10`.
  - `LOWPACK`: `$061A` / 1562B -> `$063D` / 1597B, delta `+35`.
  - `HIDDEN`: unchanged at `$0377`; `HIDDENPACK`: unchanged at `$004D`.
  - `REGSEED`: unchanged at `$1010`.
  - `bin/readybasic.prg`: unchanged at `20994` bytes.
- Verification:
  - `make verify PROFILE=precog-d81`: pass.
  - `build_support/vice_readybasic_repeat_label_probe.sh`: pass, 29/29 steps.
  - Full ReadyBASIC visual verification suite: pass, 167/167 steps.
  - `make easyflash-smoke`: pass.

## 2026-05-23: Proper Nested Terms Plus Float Experiment

- Branch: `exp/readybasic-proper-float-terms`, stacked from
  `exp/readybasic-lean-nested-terms` commit `6afae5f`.
- Added proper nested ReadyBASIC expression-term support for the tested integer,
  string, and float forms. Proven examples include `ADDI(1,ADDI(2,3))`,
  `FADD(1.5,FADD(2.25,3.25))`, `ABS(FADD(1.2,2.3)-3)`,
  `LEFT$(GREET("READY")+"!",3)`, and `LEFT$(UPPER(GREET("ready")),2)`.
- Added plain C64 BASIC float support for command inputs/outputs and `FUNC`
  formals/returns. `RET expr` now returns string for string expressions and
  plain float for untyped numeric expressions; `RET%` and `RET$` force integer
  or string. Statement `FADD(A,B,Q)` writes a plain numeric variable and rejects
  `%` output variables.
- Added resident-computed `FADD` as the first float demo command. It keeps a
  descriptor and a one-byte low-overlay stub, but resident code does the actual
  ROM float add because low overlays run with BASIC ROM hidden.
- Fixed statement dispatch for commands beginning with `F`: a failed `FUNC`
  keyword match now falls through to the bare-command dispatcher, allowing
  `FADD(...)` as a statement without reviving the old `!` path.
- Memory impact versus expression-style branch:
  - `BASIC_START`: `$2401` -> `$2901`.
  - Empty BASIC free bytes: `31741` -> `30461`, delta `-1280`.
  - `RESIDENT`: `$11FE` / 4606B -> `$16FD` / 5885B, delta `+1279`.
  - `BRIDGE`: `$01EA` / 490B -> `$01EC` / 492B, delta `+2`.
  - `LOWPACK`: `$061A` / 1562B -> `$061B` / 1563B, command overlay delta `+1`.
  - `HIDDEN`: unchanged at `$0377`; `HIDDENPACK`: unchanged at `$004D`.
  - `REGSEED`: unchanged at `$1010`.
  - `bin/readybasic.prg`: unchanged at `20994` bytes.
- Verification so far:
  - `make readybasic-plugin-static-check`: pass with `$2901`/`$16FD` guardrails.
  - `make verify`: pass after rebuilding the current worktree.
  - Focused `RBPROC1` VICE probe: pass; screen output includes `FADD 3.5`,
    `NFADD 7`, `SCALE 3.375`, `NADDI 6`, `FABS .5`, `SFADD 3.5`, `CAT HI`,
    and `NGS HI`.
  - Broad external command/program/lifecycle/state wrappers were later refreshed
    to the bare parenthesized syntax on this branch.
  - Added a viewer-paced ReadyBASIC demo automation suite covering `MEMAVL`,
    editor round trips, command groups, `PROC`/`FUNC`, expected errors, REU
    handles, and nested expression forms.

## 2026-05-23: Lean Nested-Term Experiment

- Branch: `exp/readybasic-lean-nested-terms`, stacked from
  `exp/readybasic-expression-style` commit `1690035`.
- Added targeted nested return support without making ReadyBASIC a full
  recursive expression-term engine. The eval hook now stages command and `FUNC`
  results through a common result-return path, clears carry on success, and uses
  BASIC ROM string-descriptor helpers so ROM consumers can safely consume
  returned strings.
- Added shared one-wrapper numeric actual parsing for command expression
  parsing and native `FUNC` argument binding. Proven forms include
  `ADDI(1,(2+4))`, `ZADD16(1,(2+4))`, and `ADDI((1+2),(3+4))`.
- Proven nested return forms now include `ABS(ADDI(1,6)-10)` and
  `LEFT$(GREET("READY"),2)`. General ReadyBASIC terms nested inside other
  ReadyBASIC actual lists and plain floating variables remain branch-2 scope.
- Memory impact versus expression-style branch:
  - `BASIC_START`: `$2401` -> `$2501`.
  - Empty BASIC free bytes: `31741` -> `31485`, delta `-256`.
  - `RESIDENT`: `$11FE` / 4606B -> `$1289` / 4745B, delta `+139`.
  - `BRIDGE`: `$01EA` / 490B -> `$01EB` / 491B, delta `+1`.
  - `LOWPACK`: unchanged at `$061A` / 1562B; command overlay delta `0`.
  - `HIDDEN`: unchanged at `$0377`; `HIDDENPACK`: unchanged at `$004D`.
  - `REGSEED`: unchanged at `$1010`.
  - `bin/readybasic.prg`: unchanged at `20994` bytes.

## 2026-05-23: Bare Commands And Expression-Style Experiment

- Branch: `exp/readybasic-expression-style`.
- Added bare `COMMAND(...)` statement dispatch, sharing the existing descriptor
  lookup, signature parser, overlay loader, and result commit path. The legacy
  `!COMMAND args` statement form was later removed on this branch to keep the
  natural BASIC syntax lean.
- Added `$030A/$030B` eval-vector hook for selected expression returns:
  `ZECHO1()`, `ZADD16(a,b)`, `UPPER(s$)`, `LOWER(s$)`, `ZHIDDENRAM(s$)`,
  `ZSUMNUMARRAY(a%(0),n)`, `BUFMAKE(n)`, `ZTEMPSCRATCH(n)`, and `SCRCAP()`.
- Added parenthesized `PROC`/`FUNC` definitions and parenthesized non-empty
  `EXEC` actual lists. Zero-argument routines still use `EXEC NAME`; `EXEC
  NAME()` was cut for resident size.
- Reworked `FUNC` so definitions have input formals only and return with
  `RET expr`. Optional `RET% expr` and `RET$ expr` markers make the return type
  explicit. `FUNC` is now expression-only and returns directly to the expression
  evaluator; `EXEC FUNC(...)` is rejected. `FUNC` bodies support the common
  "assign later, then `RET R%`" pattern with simple scalar assignments before
  `RET`, including nested ReadyBASIC function/command calls on the RHS.
- Memory impact versus the native PROC/FUNC baseline:
  - `BASIC_START`: `$2101` -> `$2401`.
  - Empty BASIC free bytes: `32509` -> `31741`, delta `-768`.
  - `RESIDENT`: `$0EF4` / 3828B -> `$11FE` / 4606B, delta `+778`.
  - `BRIDGE`: `$01FB` / 507B -> `$01F5` / 501B, delta `-6`.
  - `LOWPACK`: unchanged at `$061A` / 1562B.
  - `bin/readybasic.prg`: unchanged at `20994` bytes.
- Verification:
  - `make readybasic-plugin-static-check`: pass.
  - Focused VICE expression probe: pass for bare command statements, command
    expressions, parenthesized `EXEC PROC`, `FUNC` with later assignment
    plus `RET`, numeric `FUNC` expression return/assignment, and string `FUNC`
    expression return.
  - `RBPROC1` stored-program probe: pass for no-param `PROC`, `%`/`$` inputs,
    `%`/`$` returns, explicit `RET%`/`RET$`, colon chain, normalized
    `IF THEN :EXEC`, nested depth 2, expression `FUNC`, command and `FUNC`
    return assignments, command return inside top-level `ABS(...)`, flat
    numeric expression actuals, string command return inside a `FUNC` body,
    and readable `LIST`.
  - Current expression limits verified during the expanded `RBPROC1` work:
    `ADDI(1,2+4)` works, but `ADDI(1,(2+4))` does not; command/FUNC returns are
    not yet general nested arguments to BASIC ROM functions such as
    `ABS(ADDI(...))` or `LEFT$(GREET(...),2)`. The later
    `exp/readybasic-lean-nested-terms` branch addresses those targeted forms.
  - Parenthesized `RBPROCERR` negative VICE probe: pass for unknown routine,
    missing/wrong args, statement `EXEC` to `FUNC`, extra `PROC` actual, bare `ENDP`,
    and stack overflow.
  - `make verify`: pass after rebuilding the normal `precog-dual-d71` profile.

## 2026-05-22: Native PROC/FUNC/EXEC/ENDP

- Implemented bare native BASIC routines:
  - `PROC NAME P%,S$ ... ENDP` for input-only reusable BASIC code.
  - `FUNC NAME P%,S$,R$ ... ENDP` with the final formal as one output.
  - `EXEC NAME,...` for both `PROC` and `FUNC`.
  - `CALL` remains reserved and unimplemented.
- V1 limits:
  - `%` and `$` formals only; no arrays, locals, plain floating formals, by-ref
    parameters, or multiple outputs.
  - Four active nested `EXEC` calls; overflow reports `?RB ERROR 33`.
  - Definitions should live after `END`; fall-through into `PROC`/`FUNC` is a
    syntax error.
  - `IF ... THEN EXEC ...` is normalized by the ReadyBASIC crunch hook to
    `IF ... THEN :EXEC ...`; `petcat` sample sources use the normalized stored
    form.
- Memory comparison against the pre-PROC baseline:
  - `BASIC_START`: `$1C01` -> `$2101`.
  - Empty BASIC free bytes: `33789` -> `32509` formula bytes (`32519` live header
    bytes).
  - `bin/readybasic.prg`: `20994` -> `20994` bytes.
  - `RESIDENT`: `$1200-$1BB3`, `$09B4` (2484B) -> `$1200-$20F3`, `$0EF4`
    (3828B), delta `+$0540` / `+1344B`.
  - `LOWPACK`: `$061A` unchanged; command overlay delta `0B`.
  - `HIDDEN`: `$0377` unchanged; `HIDDENPACK`: `$004D` unchanged.
  - `BRIDGE`: `$C000-$C19A`, `$019B` (411B) -> `$C000-$C1FA`, `$01FB` (507B),
    delta `+$0060` / `+96B`.
  - `REGSEED`: `$1010` unchanged.
- Verification:
  - `make readybasic-plugin-static-check`
  - Positive `RBPROC1` stored-program probe:
    `./logs/vice_auto_20260522_174557`
  - Negative `RBPROCERR` stored-program probe with screen clear between cases:
    `./logs/vice_auto_20260522_180449`
  - Existing `rbtest1` probe:
    `./logs/vice_auto_20260522_174629`
  - Existing direct command probe:
    `../agenticdevharness/logs/vice_auto_20260522_174650`
  - Existing lifecycle probe:
    `../agenticdevharness/logs/vice_auto_20260522_174916`
  - Existing stored-program command probe:
    `../agenticdevharness/logs/vice_auto_20260522_174948`

## 2026-05-22: Command Rename, LOWER, And Making-Command Guide

- Commands:
  - `make readybasic-plugin-static-check`
  - `make bin/reuviewer.prg`
  - `make verify`
  - ReadyBASIC direct, program, lifecycle, state, large-vars, `rbtest1`, and
    full visual VICE suites through normal ReadyOS boot paths.
- Result: static/build verification passed. All VICE concrete steps completed
  `ok`; the harness wrapper still reported process-level `partial` with
  `FailedStep: null`, matching the known wrapper behavior.
- Run dirs:
  - Direct: `../agenticdevharness/logs/vice_auto_20260522_164329`
  - Program: `../agenticdevharness/logs/vice_auto_20260522_165634`
  - Lifecycle: `../agenticdevharness/logs/vice_auto_20260522_165745`
  - State: `../agenticdevharness/logs/vice_auto_20260522_165814`
  - Large vars: `../agenticdevharness/logs/vice_auto_20260522_165907`
  - `rbtest1`: `./logs/vice_auto_20260522_170005`
  - Full visual: `../agenticdevharness/logs/vice_auto_20260522_170022`
- Current static layout:
  - `ENTRY` `$1000-$1102`, size `$0103` (259B).
  - `RESIDENT` `$1200-$1BB3`, size `$09B4` (2484B).
  - `REGSEED` `$5000-$600F`, size `$1010` (4.0K, 4112 exact bytes).
  - `HIDDEN` `$A000-$A376`, size `$0377` (887B).
  - `HIDDENPACK` `$A800-$A84C`, size `$004D` (77B).
  - `LOWPACK` `$A900-$AF19`, size `$061A` (1.5K, 1562 exact bytes).
  - `BRIDGE` `$C000-$C19A`, size `$019B` (411B).
  - Empty BASIC free bytes remain `33789`.
- Behavior covered:
  - Proof/demo names are now `ZECHO1`, `ZADD16`, `ZHIDDENRAM`,
    `ZSUMNUMARRAY`, `ZRANGENUMARRAY`, `ZTEMPSCRATCH`, and `ZFAIL`.
  - `STRUP` is replaced by `UPPER`; `LOWER` is added and verified by `ASC()`
    byte values because C64 visual case is charset-dependent.
  - Old names are rejected with the existing unknown-command error.
  - `SCRCAP` remains near the front in slot 14 and `SCRPUT` remains in slot
    128. Later built-in graphics/sound aliases reduced the filler span to 11
    descriptors; static verification now enforces that real descriptors plus
    filler remain exactly 128.

## 2026-05-22: REU-Backed 128 Handles And 48KB Typed Heap

- Commands:
  - `make readybasic-plugin-static-check`
  - `make bin/reuviewer.prg`
  - `make verify`
  - ReadyBASIC direct, program, lifecycle, state, large-vars, and full visual
    VICE suites through normal ReadyOS boot paths.
- Result: static/build verification passed. VICE harness runs completed every
  concrete assertion with `FailedStep: null`; the wrapper still reported
  `partial`, matching the current harness behavior when no concrete step fails.
- Run dirs:
  - Direct: `../agenticdevharness/logs/vice_auto_20260522_153017`
  - Program: `../agenticdevharness/logs/vice_auto_20260522_154145`
  - Lifecycle: `../agenticdevharness/logs/vice_auto_20260522_154246`
  - State: `../agenticdevharness/logs/vice_auto_20260522_154309`
  - Large vars: `../agenticdevharness/logs/vice_auto_20260522_154359`
  - Full visual: `../agenticdevharness/logs/vice_auto_20260522_154424`
- Current static layout:
  - `ENTRY` `$1000-$1102`, size `$0103` (259B).
  - `RESIDENT` `$1200-$1BAF`, size `$09B0` (2480B).
  - `REGSEED` `$5000-$600F`, size `$1010` (4112B).
  - `HIDDEN` `$A000-$A376`, size `$0377` (887B).
  - `HIDDENPACK` `$A800-$A84C`, size `$004D` (77B).
  - `LOWPACK` `$A900-$AEDE`, size `$05DF` (1503B).
  - `BRIDGE` `$C000-$C19A`, size `$019B` (411B).
- BASIC workspace:
  - `BASIC_START=$1C01`, BASIC top `$A000`.
  - Empty free space remains `33789` bytes (33.0K), a `0` byte reduction from
    the pre-change baseline.
- Handles and heap:
  - `RB_HANDLE_COUNT=128`.
  - `RB_HEAP_PAGES=192`, with the typed heap at REU bank `$44:$4000-$FFFF`.
  - The handle directory is REU-backed at `$44:$0800-$09FF`.
  - The 192-page heap bitmap is REU-backed at `$44:$0C00`.
  - Bridge RAM keeps only the current handle descriptor scratch, not a 128-entry
    resident table.
- Probe coverage added:
  - handle `1..128` allocation and handle-table-full on handle 129;
  - low-handle reuse after free;
  - 48KB buffer allocation and heap-full rejection;
  - screen text+color handles filling the 48KB heap;
  - type rejection and freeing behavior after the expanded table.

## 2026-05-22: 128-Slot Registry And Typed Screen Handles

- Commands:
  - `make readybasic-plugin-static-check`
  - `make bin/reuviewer.prg`
  - `make verify`
  - ReadyBASIC direct, program, lifecycle, state, large-vars, and full visual
    VICE suites through normal ReadyOS boot paths.
- Result: static/build verification passed. VICE harness runs completed all
  concrete steps with `FailedStep: null`; the harness process status remained
  `partial`, matching the pre-change baseline wrapper behavior.
- Current static layout:
  - `ENTRY` `$1000-$1102`, size `$0103` (259B).
  - `RESIDENT` `$1200-$1BF9`, size `$09FA` (2554B).
  - `REGSEED` `$5000-$600F`, size `$1010` (4112B).
  - `HIDDEN` `$A000-$A336`, size `$0337` (823B).
  - `HIDDENPACK` `$A800-$A84C`, size `$004D` (77B).
  - `LOWPACK` `$A900-$ADDF`, size `$04E0` (1248B).
  - `BRIDGE` `$C000-$C1C4`, size `$01C5` (453B).
  - PRG payload `$5200` (20992B).
- BASIC workspace:
  - `BASIC_START=$1C01`, BASIC top `$A000`.
  - Empty free space remains `33789` bytes (33.0K), a `0` byte reduction from
    the memory-reclaim baseline.
- Registry:
  - `RB_CMD_DESC_COUNT=128`.
  - Descriptor table is cold-seeded from `REGSEED` to REU bank `$44`
    `$1000-$1FFF`.
  - Lookup fetches 256-byte descriptor pages into `$C500`, scans eight
    descriptors locally, and copies a match to `$C480`.
  - Filler descriptors are zero-filled empty slots.
  - `SCRCAP` is near the front in slot 13; `SCRPUT` is in slot 128.
- Handles:
  - Existing live handle count remains eight.
  - Type `1` is a byte buffer; type `2` is screen text+color.
  - `BUFFILL` rejects non-buffer handles with `?RB ERROR 40`.
  - `BUFDROP` frees any valid handle type.
  - `SCRCAP H%` captures `$0400-$07E7` and `$D800-$DBE7`.
  - `SCRPUT H%` validates type `2` and restores screen text plus color RAM.
  - The originally proposed `SCRSAVE`/`SCRLOAD` names were changed to
    `SCRCAP`/`SCRPUT` to avoid C64 BASIC tokenizer conflicts.
- Probe coverage added:
  - screen capture/restore;
  - slot-128 lookup;
  - filler descriptor scanning;
  - wrong-handle-type rejection in both directions;
  - freeing a screen handle.

## 2026-05-21: Memory-Reclaim Layout

- Command: `make readybasic-plugin-static-check`
- Result: pass.
- Current static layout:
  - `ENTRY` `$1000-$1102`, size `$0103` (259B).
  - `RESIDENT` `$1200-$1BAB`, size `$09AC` (2.4K).
  - `REGSEED` `$4000-$418F`, size `$0190` (400B).
  - `HIDDEN` `$A000-$A336`, size `$0337` (0.8K).
  - `HIDDENPACK` `$A800-$A84C`, size `$004D` (77B).
  - `LOWPACK` `$A900-$ABF2`, size `$02F3` (0.7K).
  - `BRIDGE` `$C000-$C1BD`, size `$01BE` (446B).
- Current BASIC workspace:
  - `BASIC_START=$1C01`.
  - BASIC top is `$A000`.
  - Empty free space is `33789` bytes (33.0K), up from `26109` bytes (25.5K).
  - Recovered space is `7680` bytes (7.5K).
- Current suspend/resume layout:
  - Shared frames live at `$C200-$C5FF` (`$0400`, 1.0K).
  - Hidden helper shadow is refreshed at `$C280-$C5B6` (`$0337`, 0.8K).
  - Zero page and stack snapshots live in REU bank `$44` offsets `$0A00/$0B00`.
- Latest full visual verification:
  - Run dir: `../agenticdevharness/logs/vice_auto_20260521_202657`
  - Result: 98/98 steps, `FailedStep: null`, no degraded steps.

## 2026-05-11: Assembler Spine Build

- Command: `make bin/readybasic.prg`
- Result: pass.
- Key trace:
  - `ENTRY` size `$0103`.
  - `RESIDENT` `$1200-$1ABE`, size `$08BF`.
  - `LOWPACK` `$1C00-$1EEE`, size `$02EF`.
  - `HIDDEN` `$A000-$A141`, size `$0142`.
  - `HIDDENPACK` `$A800-$A82F`, size `$0030`.
  - `BRIDGE` `$C000-$C075`, size `$0076`.
- First failing step before pass: resident overflowed `$1200-$1BFF` by `$0286`, then `$0116`, then `$008E`.
- Fixes applied:
  - Moved registry seed data to `REGSEED`.
  - Moved REU prestash code to hidden helper.
  - Moved handle/page allocator implementation into the low overlay pack.
  - Added `CMD_LOW_ALL` for heap commands that need the low overlay helper cluster.
  - Prevented warm resume from rereading `REGSEED` after BASIC may own `$4000+`.
- Next hypothesis:
  - Run static guardrail target after each layout-sensitive edit.
  - Add VICE command-level probes through normal ReadyOS boot once the launcher-side automation is updated for raw `!COMMAND args` samples.
  - Existing ReadyBasic lifecycle probe still needed regeneration for the new
    plugin-spine commands before it could be treated as authoritative.

## 2026-05-11: Static Guardrail

- Command: `make readybasic-plugin-static-check`
- Result: pass.
- Checks:
  - `BASIC_START=$3001`.
  - Resident below `$1C00`.
  - Low overlay in `$1C00-$23FF`.
  - Hidden helper below `$A600`.
  - Hidden overlay in `$A000-$BFFF`.
  - Bridge below `$C600`.
  - ReadyBasic REU bank/type constants synced with `src/lib/reu_mgr.h`.

## 2026-05-11: REU Viewer Compile Check

- Command: `make bin/reuviewer.prg`
- Result: pass.
- Reason: ReadyBASIC added REU bank types `14/15`, so the C-side viewer and fixed-bank sync needed a compile check.

## 2026-05-11: Baseline Contract Review

- Command: `git show HEAD:src/apps/readybasic/readybasic.s` plus targeted `rg` over `readybasiclessonslearnt.md`.
- Still relevant and must be carried forward:
  - Cold entry must reset KERNAL and BASIC vectors before installing ReadyBASIC's vector ownership.
  - ReadyBASIC-owned vectors must be restored before jumping through the ReadyOS shim.
  - Warm entry must restore hidden helper code from `$9A00` before calling any `$A000` helper.
  - Manual prompt `EXIT` must save BASIC zero page, stack, line-chain validity, and app mode before returning to the launcher.
  - READY-mode resume must clear the screen/editor surface, clear pending key buffer bytes, restore lowercase VIC text mode, redraw ReadyBasic's banner, position the prompt, and then enter `BASIC_READY`.
  - Warm restore must preserve live BASIC pointers such as `FRETOP`, `VARTAB`, `ARYTAB`, and `STREND`; those are restored from the runtime snapshot and must not be reset to the cold empty-program layout.
  - Fallback to ROM BASIC must not leave `TXTPTR` advanced unless ReadyBASIC has proven it owns the statement.
  - `BASIC_START=$3001` changes addresses, not the lifecycle contract.
- No longer relevant or intentionally replaced:
  - Demo commands `RB 1/2/3/10/11/12`, hidden screen drawing, mailbox text drawing, and PRG save/load helpers.
  - `$1201` BASIC workspace and old `$1000-$3FFF` compact load assumptions.
  - Private `RB` token experiments. The visible probe showed the `$CC` token/list attempt can crash/blank `LIST`; V1 keeps raw direct `!COMMAND args` only.
  - Direct app-bank hotkeys in probes. Acceptance should navigate the launcher menu for ReadyBasic re-entry.
- Fixes from this review:
  - `cmd_exit` now identifies manual prompt `EXIT` by `TXTPTR < BASIC_START` instead of `CURLIN`; this matches the direct input buffer path and keeps program-line resume as a later candidate.
  - `cmd_exit` writes `RUNTIME_MODE` directly in visible RAM before the hidden save helper. The hidden helper preserves that byte instead of recomputing it from `CURLIN` or bridge state, preventing direct prompt `EXIT` from resuming via `BASIC_NEXT_STMT`.
  - READY-mode restore now uses the baseline console-reset steps (`K_CLRCHN`, lowercase VIC text mode, key buffer clear, banner redraw, prompt positioning).
  - Warm restore now sets only KERNAL memory bounds after restoring zero page; cold initialization remains responsible for resetting BASIC pointers and `FRETOP`.
- Next hypothesis:
  - Rerun visible `run_readybasic_plugin_command_probe.sh` with menu-based re-entry and add a variable/string-after-resume probe before committing the safe branch.

## 2026-05-11: Interrupted Visible Boot/Resume Run

- Command: `make readybasic-plugin-static-check && READYBASIC_VISIBLE=1 build_support/run_readybasic_plugin_command_probe.sh`
- Run dir: `../agenticdevharness/logs/vice_auto_20260511_201407`
- Result: interrupted/terminated manually after VICE appeared locked before the ReadyBasic banner.
- Key trace:
  - Static guardrail passed before launch.
  - Probe started 44-step visible plan and reached `wait_readybasic_prompt`.
  - Only `boot_initial` artifacts were captured; no memory dump was produced before termination.
  - The decoded boot screen showed the same uninitialized screen pattern seen in earlier healthy runs, but unlike earlier runs it had not advanced to the ReadyBasic banner quickly enough.
- First failing step/code:
  - Treat as `wait_readybasic_prompt` until a complete harness timeout or monitor dump proves otherwise.
- Follow-up checks:
  - Process table cleanup confirmed no lingering `x64sc`/`ViceTasks.Binary` probe processes.
  - `$C600-$C7FF` was rechecked against ReadyOS docs and source: it is reserved ReadyOS REU metadata, and `src/lib/reu_mgr.c` uses `$C600` as the allocation table. ReadyBasic may mark `$C600+$44/$45` only as shared REU ownership metadata; it must not treat this range as app-private scratch.
  - Static guardrail rerun after cleanup: `make readybasic-plugin-static-check` passed.
- Next hypothesis:
  - The persistent functional regression remains the menu-based resume path: the previous complete run reached all direct command probes, returned to the launcher, then failed `wait_readybasic_after_resume`.
  - Re-run the visible probe with an interrupt-capable terminal session. If cold boot stalls again, capture a monitor dump before killing VICE. If cold boot succeeds, focus on `RUNTIME_MODE`, bridge magic, and READY-mode screen redraw after menu resume.

## 2026-05-11: Visible Plugin Probe Passed

- Command: `READYBASIC_VISIBLE=1 build_support/run_readybasic_plugin_command_probe.sh`
- Run dir: `../agenticdevharness/logs/vice_auto_20260511_202147`
- Result: pass, 44/44 steps.
- Key trace:
  - Cold boot reached `READYBASIC REU PLUGINS`.
  - Direct command probes passed for the current proof commands, including scalar, string, hidden-worker, array, REU-buffer, temporary-scratch, failure, and unknown-command cases.
  - `EXIT` returned to the launcher.
  - ReadyBasic was relaunched by menu navigation, not `CTRL+3`.
  - READY-mode resume cleared/redrew the ReadyBasic screen and `!PING` still worked after resume.
- First failing step/code: none.
- Next hypothesis:
  - Add an explicit BASIC variable/string survival check across the same launcher round trip before committing a known-good fallback branch.

## 2026-05-11: Visible Resume-State Probe Passed

- Command: `READYBASIC_SKIP_BUILD=1 READYBASIC_VISIBLE=1 build_support/run_readybasic_plugin_command_probe.sh`
- Run dir: `../agenticdevharness/logs/vice_auto_20260511_202329`
- Result: pass, 47/47 steps.
- Key trace:
  - Reused the already-built `precog-d81` image from the previous passing run.
  - Added `V%=321:VS$="OK"` before `EXIT`.
  - Returned to launcher, relaunched ReadyBasic by menu navigation, and asserted `STATE 321 :OK` after resume.
  - Asserted `RESUME 1` from `!PING` after the state check.
  - Final bridge dump starts with `52 a6`, confirming READY resume magic after the round trip.
- First failing step/code: none.
- Next hypothesis:
  - Safe branch can be created and committed as the current fallback point.
  - Stored-line/private-token support remains out of scope for this checkpoint and needs a separate crunch/list contract probe before reintroduction.

## 2026-05-11: Fresh Build Visible Probe Passed On Branch

- Branch: `codex/readybasic-reu-plugin-spine`
- Static command: `make readybasic-plugin-static-check`
- Static result: pass.
- Automation command: `READYBASIC_VISIBLE=1 build_support/run_readybasic_plugin_command_probe.sh`
- Run dir: `../agenticdevharness/logs/vice_auto_20260511_202716`
- Result: pass, 47/47 steps after a fresh `precog-d81` profile build with `runappfirst=readybasic`.
- Key trace:
  - Cold ReadyOS boot autoloaded ReadyBasic through the generated app config.
  - All direct plugin sample commands passed.
  - `FAIL` cleared the output variable before reporting `?RB ERROR 7`.
  - Unknown command reported `?RB ERROR 1`.
  - Menu-based ReadyBasic re-entry passed; no `CTRL+3` path was used.
  - `V%=321` and `VS$="OK"` survived `EXIT` -> launcher -> menu relaunch.
  - Post-resume `!PING P%` returned `RESUME 1`.
- First failing step/code: none.
- Next hypothesis:
  - Commit the source/config/docs/static-check delta as the known-good fallback point.

## 2026-05-11: Program-Mode Probe Passed

- First failing command: `READYBASIC_SKIP_BUILD=1 READYBASIC_VISIBLE=1 bash build_support/run_readybasic_program_probe.sh`
- First failing run dir: `../agenticdevharness/logs/vice_auto_20260511_203414`
- First failing step/code: `assert_program_ping_run`, screen contained `?SYNTAX ERROR` instead of `PRPING 1`.
- Key diagnosis:
  - `LIST` showed `10 !PING`, so raw stored text survived crunch/list.
  - Bridge debug state showed `$0308` was not reached during the stored line.
  - Dumping BASIC text proved `$3000`, the byte before relocated `BASIC_START=$3001`, contained `$20`.
  - C64 BASIC `NEWSTT` expects the byte before `TXTTAB` to be zero; otherwise it reports syntax before advancing into the first stored line.
- Fixes applied:
  - Restored the older IGONE-style non-mutating peek/tail-call contract for `$0308` fallback.
  - Cold BASIC workspace initialization now clears `BASIC_SENTINEL` (`$3000`) as well as the empty line-link bytes at `$3001/$3002`.
  - Removed the now-unused saved-`TXTPTR` bridge bytes after switching back to non-mutating peek; final `BRIDGE` remains `$C000-$C075`.
- Static command: `make readybasic-plugin-static-check`
- Static result: pass.
- Passing program command: `READYBASIC_VISIBLE=1 bash build_support/run_readybasic_program_probe.sh`
- Passing program run dir: `../agenticdevharness/logs/vice_auto_20260511_204630`
- Program result: pass, 24/24 steps after a fresh `precog-d81` profile build with `runappfirst=readybasic`.
- Program trace:
  - `!PING OUT%` works from stored BASIC and survives `LIST`.
  - Same-line continuation works: `!ADD16 ...:PRINT ...`.
  - String input/output works: `!STRUP S$,T$`.
  - Hidden `$A000` worker works: `!HCRC "AB",H%`.
  - Integer array input/output works: `SUMAI` and `RANGEAI`.
  - Persistent REU handle lifecycle works: `BUFMAKE`, `BUFFILL`, `BUFDROP`.
  - Error path works: `!FAIL 7,X%` reports `?RB ERROR 7` and leaves the pre-cleared output variable at zero.
- Regression command: `READYBASIC_SKIP_BUILD=1 READYBASIC_VISIBLE=1 bash build_support/run_readybasic_plugin_command_probe.sh`
- Regression run dir: `../agenticdevharness/logs/vice_auto_20260511_204727`
- Regression result: pass, 47/47 steps.
- Next hypothesis:
  - The raw `!COMMAND args` path is now proven for both direct mode and stored program mode.
  - Future token/crunch work must keep the sentinel, IGONE fallback, and line-length contracts under explicit tests.

## 2026-05-12: Full Suite Visual Verification Passed

- Script: `build_support/run_readybasic_full_suite_visual_verification.sh`
- Generated plan id: `readybasic_full_suite_visual_verification`
- Command: `READYBASIC_VISIBLE=1 build_support/run_readybasic_full_suite_visual_verification.sh`
- Run dir: `../agenticdevharness/logs/vice_auto_20260512_002033`
- Result: pass, 84/84 steps after a fresh `precog-d81` profile build with `READYOS_CONFIG_RUN_FIRST=readybasic`.
- Visual pacing:
  - All input sections use `post_delay_s: 3.0`, so the current result or `LIST` output remains visible for three seconds before the next screen-clearing section begins.
- Key trace:
  - Cold ReadyOS boot autoloaded ReadyBasic through the generated app config.
  - Direct-mode sample commands passed across scalar, string, hidden worker, array, REU handle, temporary heap, failure, and unknown-command paths.
  - Launcher round trip used menu navigation, not `CTRL+3`; ReadyBasic redrew on return and variables plus registry state survived.
  - Stored BASIC program tests passed for scalar, same-line numeric, string, hidden-worker, array, `BUFMAKE`/`BUFFILL`/`BUFDROP`, and failure output clearing.
- First failing step/code: none.
- Next hypothesis:
  - Keep this script as the human-watchable acceptance suite while shorter direct/program probes remain better for tight edit-run loops.

## 2026-05-21: Bang Command Syntax Migration

- Syntax changed from `RB NAME,...` to `!NAME args`, with the first argument
  separated by spaces and later arguments still comma-separated.
- Parser changes:
  - `$0308` execute hook now recognizes raw `!` at BASIC statement start,
    including after `:`.
  - Command-name parsing uses raw `TXTPTR` reads so `!PING P%` does not absorb
    `P%` as part of the command name.
  - Parameter parsing allows whitespace before the first argument and commas for
    subsequent arguments.
  - A leading comma after the command name is rejected as syntax.
- `IF ... THEN !COMMAND` fix:
  - Added a tiny `$0304` crunch hook that calls ROM crunch first.
  - After ROM tokenization, only a real `THEN` token followed by `!` is rewritten
    to `THEN :!`, letting ROM BASIC reach the normal `$0308` statement dispatch.
  - Quoted strings, `REM`, and `DATA` text containing `THEN !` are not rewritten.
- Static command: `make readybasic-plugin-static-check`
- Static result: pass; `RESIDENT $1200-$1BC1` (`$09C2`), `BRIDGE $C000-$C164`
  (`$0165`).
- Fresh-build command probe:
  - Command: `READYBASIC_VISIBLE=1 bash .../run_readybasic_plugin_command_probe.sh`
  - Run dir: `../agenticdevharness/logs/vice_auto_20260521_010753`
  - Follow-up run with direct `IF 1 THEN !PING`: `../agenticdevharness/logs/vice_auto_20260521_011815`
  - All steps reported `ok`; wrapper status was `partial` with no failed step.
- Program probe:
  - Run dir: `../agenticdevharness/logs/vice_auto_20260521_012044`
  - All steps reported `ok`, including stored `IF 1 THEN !PING`, `IF 0 THEN`,
    `FOR/NEXT`, and `THEN !` inside string/`REM`/`DATA` text.
- Full visual suite:
  - Run dir: `../agenticdevharness/logs/vice_auto_20260521_012154`
  - All 98 steps reported `ok`; wrapper status was `partial` with no failed step.
- Additional probes:
  - Lifecycle: `../agenticdevharness/logs/vice_auto_20260521_012603`, all 47 steps `ok`.
  - State: `../agenticdevharness/logs/vice_auto_20260521_012627`, all 57 steps `ok`.
  - `RBTEST1`: `./logs/vice_auto_20260521_012719`, all 12 steps `ok`.
  - Large vars: `../agenticdevharness/logs/vice_auto_20260521_012739`, all 19 steps `ok`.
- Harness instability:
  - Cross-app resume reached step 200/206 before VICE exited with code 137:
    `../agenticdevharness/logs/vice_auto_20260521_012802`.
  - A rerun failed near launch with the same VICE exit code 137, and the
    second-entry editor probe also failed before reaching ReadyBASIC due VICE
    monitor/exit-137 launch failures.
