# ReadyBASIC Refactor Guidelines

This file is the discipline checklist for ReadyBASIC refactors. It complements
`READYBASIC_CURRENT_DESIGN.md`, `READYBASIC_LIFECYCLE_AND_REU_ARCHITECTURE.md`,
`READYBASIC_PLUGIN_ARCH.md`, and `REadyBASICCommandModuleAndSubmodulePlan.MD`.
Use it before changing `readybasic.s`, module generation, disk-module loading,
or ReadyBASIC VICE tests.

## Recent Regression Lesson

The module refactor accidentally changed ordinary BASIC numeric assignment
evaluation. The assignment path used to evaluate the right-hand side through
the BASIC ROM numeric expression helper. The refactor routed that through the
ReadyBASIC command evaluator instead.

That looked local to modules, but it broke plain BASIC:

```basic
10 I%=0
20 REPEAT
30 I%=I%+1
40 UNTIL I%=3
```

`I%=I%+1` stopped updating reliably, so `UNTIL` never became true and the repeat
stack overflowed. The fix was to restore numeric assignment RHS handling to the
ROM `FRMNUM` path in `rb_expr_exec_assignment`, while preserving the existing
ReadyBASIC expression-command hooks around it. This was a ReadyBASIC language
runtime bug, not a module command bug.

The rule: if a change touches BASIC expression parsing, assignment, statement
dispatch, string descriptors, `TXTPTR`, or native routine return state, prove
ordinary BASIC still works before diagnosing module or PETSCII side effects.

## Current Fixed Contract

- Empty BASIC free bytes remain `30013`.
- `BASIC_START` remains `$2AC1`; the sentinel immediately before it is `$2AC0`.
- `bin/readybasic.prg` was still `20994` bytes after the assignment fix.
- `I%=I%+1`, numeric assignment from expressions, and `REPEAT`/`UNTIL` pass the
  ReadyBASIC repeat-label VICE probe.
- The module loader and RBM3 overlay/caching proof pass after the assignment
  fix; they were not the root cause of the assignment regression.

## Memory Contracts

ReadyBASIC lives inside the ReadyOS app contract and also hosts a relocated
BASIC workspace. Treat these as hard boundaries:

| Range | Contract |
| --- | --- |
| `$1000-$C7FF` | ReadyOS app working region. REU app save/restore targets this `$B800` span. |
| `$1000-$11FF` | ReadyBASIC entry/cold-warm handoff area. Keep small. |
| `$1200-$2AC0` | ReadyBASIC resident code plus sentinel. Visible resident code owns BASIC-facing parser and commit work. |
| `$2AC1-$9FFF` | User BASIC program, variables, arrays, strings, and reclaimed cold-load seed space. Must remain the steady-state BASIC workspace. |
| `$A000-$A7FF` | Common under-ROM helper area. Banking must be explicit and restored. |
| `$A800-$AFFF` | Submodule slot 0. |
| `$B000-$B7FF` | Submodule slot 1. |
| `$B800-$BFFF` | Submodule slot 2 and overlay target. |
| `$C000-$C1FF` | ReadyBASIC bridge/state. Keep persistent control state here only when it is part of the defined bridge contract. |
| `$C200-$C5FF` | Shared frames and buffers, including call/result frames and the `$C500` disk-module page buffer. |
| `$C600-$C7FF` | App-private snapshot room, deliberately unused by the custom ReadyBASIC image today. |
| `$C800-$C9FF` | ReadyOS shim ABI. Do not place ReadyBASIC assumptions here unless intentionally using the shim ABI. |

Cold-load seed bytes may appear inside what later becomes the BASIC workspace.
After cold setup, those bytes must be considered gone from C64 RAM and copied to
REU. Warm resume must not reread seed tables from BASIC-owned memory.

## REU Contracts

ReadyBASIC uses launcher-assigned REU resource banks. The launcher marks those
physical banks in the ReadyOS bank table at `$B840` with `REU_RB_CORE` and `REU_RB_CODE`;
ReadyBASIC resolves the bank ids at startup and must not assume fixed `$44/$45`
addresses.

`readybasic.s` and `cfg/ready_app_readybasic.cfg` form a coupled ca65/ld65
load/run ABI. Do not substitute the generic app config; run
`verify_readybasic_plugin.py` after changing either side.

| REU bank/area | Contract |
| --- | --- |
| Assigned core bank | Runtime metadata: command descriptors, registry data, handles, snapshots, state tables, and small persistent proof state. |
| Assigned code bank | Payload bytes for built-in and disk-loaded command modules. |
| Core bank `$0A00-$0BFF` | Zero-page and hardware stack snapshots. |
| Core bank `$2000-$3FFF` | Reserved for richer module/submodule residency catalog work. |

Keep REU writes typed and bounded. Any loader change must reject bad magic,
version mismatch, short reads, invalid counts, descriptor overflow, and payload
overflow. After failure, restore KERNAL channels, close files, restore message
state, and avoid leaving stale residency that claims a payload is loaded.

## Module Package Contracts

`RBM` means `ReadyBasicModule`.

ReadyBASIC module packages are disk SEQ files named `rbm.<name>`, for example:

- `rbm.sample1`
- `rbm.sample2`
- `rbm.sample3`

They are not PRG files and must not carry a two-byte PRG load address. The file
contents still begin with the module package header magic `RBM!` version `1`.
That is a package-format header, not a C64 PRG header.

The disk loader command `ZMODLD(name$)` opens the SEQ file and streams it
through the existing `$C500` page buffer into the allocated REU descriptor and
payload areas. It must not load the module package into the BASIC workspace or
consume BASIC free bytes.

Generated disk-image preservation logic must treat `rbm.*` files as build-owned
artifacts. Do not restore old `rbm.*` files from a previous disk image over the
freshly built packages.

## Module, Submodule, And Overlay Vocabulary

Use these terms consistently:

- Module: logical command family identified by module id.
- Module package/container: the disk SEQ file that carries descriptors and
  payload records.
- Submodule: a runtime payload family linked for one or more fixed under-ROM
  slots.
- Overlay: a swappable payload image for a submodule.

One module package may contain many command descriptors, multiple submodules,
and multiple overlays for the same submodule. Seeing the same submodule number
more than once is valid when each entry has a different overlay id. For example,
RBM2 mentioning submodule `5` twice means the same submodule family has two
different overlays; it does not mean duplicate conflicting numbering.

## Residency And Copy-Skip Contracts

Command dispatch must be based on descriptor metadata:

1. Look up command name.
2. Read module id, submodule id, overlay id, slot mask, payload location, runtime
   destination, entry offset, and signature id.
3. Compare required module/submodule/overlay/generation against residency.
4. If the required image is already resident, skip copying from REU.
5. If it is not resident, copy the required payload from REU to the fixed
   under-ROM slot range and update residency.
6. Call the under-ROM entry.
7. Return to visible resident code for result commit.

Multi-slot submodules must update all claimed slot records together. Overlay
loads replace only the relevant overlay image and must not silently corrupt
sibling slots. Tests should prove both behaviors:

- repeated calls to commands in the same resident submodule/overlay do not
  increase the copy count;
- switching to a different overlay or submodule does increase the copy count
  and resets or changes overlay-local state as expected.

RBM3 exists to prove this behavior. Its command naming and return patterns
should keep module, submodule, overlay, and state/copy-count behavior visible to
humans reading the VICE trace.

## BASIC Language Contracts

ReadyBASIC is not a private tokenized BASIC. Stored lines remain readable BASIC
text, and the extensions are recognized through statement/eval hooks.

Do not break these ordinary BASIC contracts:

- numeric assignment such as `I%=I%+1`;
- string assignment and string heap descriptors;
- `FOR`/`NEXT`;
- `IF ... THEN` with normal BASIC statements;
- `PRINT`, `LIST`, `RUN`, `NEW`, `CLR`, `LOAD`, and direct mode;
- colon-separated statements;
- strings, `REM`, and `DATA` staying ordinary text.

ReadyBASIC extensions currently include:

- bare command statements: `ZECHO1(P%)`;
- expression commands: `PRINT ZADD16(2,3)`;
- `REPEAT` / `UNTIL`;
- `LABEL` / `JUMP`;
- `PROC` / `EXEC` / `ENDP`;
- `FUNC` / `RET`, including `%`, `$`, and floating return paths.

The crunch hook only normalizes narrow `THEN EXEC ...` and `THEN JUMP ...`
forms. Descriptor-backed command statements after `THEN` should use an explicit
colon, such as:

```basic
IF 1 THEN :ZECHO1(P%)
```

## Parameter And Result Contracts

BASIC-facing parse and commit work belongs in visible resident code. Under-ROM
workers and module payloads should not mutate BASIC interpreter state directly.

Resident parsing may use BASIC ROM helpers such as:

- `CHKCOM`
- `FRMNUM`
- `GETADR`
- `PTRGET`

Command outputs use the call/result frame convention. Output variables are
captured and cleared before execution so failures do not expose stale output.
Workers write compact results; resident code commits successful results back to
BASIC variables or expression return state.

Current supported patterns include:

- integer input/output;
- numeric BASIC float input/output through the tested float signatures;
- string input/output with the current ReadyBASIC string cap;
- array-reading helper commands;
- explicit output-variable statement commands;
- expression-safe scalar/string/float commands;
- hidden/under-ROM worker commands that return through result frames.

When adding a signature, add direct-mode, stored-program, expression, failure,
and resume tests appropriate to the blast radius.

## Native PROC/FUNC Contracts

`PROC` and `FUNC` definitions are BASIC program text, not module payloads.
Keep definitions after the main program `END` in v1-style samples.

Rules:

- `EXEC` calls `PROC`.
- `FUNC` is called as an expression, not with `EXEC`.
- `ENDP` returns from a `PROC`.
- `RET expr`, `RET% expr`, and `RET$ expr` return from a `FUNC`.
- `%`, `$`, and plain numeric formals are supported.
- Arrays, local scopes, by-reference parameters, and multiple outputs are out of
  scope unless deliberately designed and tested.
- Nested `EXEC` depth is finite; preserve the current return-stack contract.

`FUNC` bodies are not a complete BASIC subinterpreter. They support the tested
scalar assignment forms before `RET`, including nested ReadyBASIC command or
function expression calls. Any expansion here must include tests for string
descriptor preservation, nested ROM expression state, and ordinary BASIC
assignment behavior.

## Test Discipline

Before blaming PETSCII, VICE, modules, or the shim, compare against a known good
branch and inspect the exact failing screen/memory state. For ReadyBASIC
regressions, prefer deterministic probes over visual impressions.

Minimum focused tests after ReadyBASIC runtime changes:

```sh
make bin/readybasic.prg readybasic-plugin-static-check
READYBASIC_SKIP_BUILD=1 READYBASIC_VISIBLE=0 make readybasic-vice-suites
```

The aggregate contains all 26 official regular ReadyBASIC targets, including
graphics/sprite/sound examples and the minimum-resume, screen/REU temporary,
loaded-app, lifecycle, hotkey, cross-app, and full visual probes. Keep
`READYBASIC_VICE_SCRIPTS` and the aggregate dependency list synchronized when
adding a new official probe.

Important targeted probes:

- `build_support/vice_readybasic_repeat_label_probe.sh`
- `build_support/run_readybasic_lifecycle_probe.sh`
- `build_support/run_readybasic_state_probe.sh`
- `build_support/run_readybasic_large_vars_probe.sh`
- `build_support/run_readybasic_cross_app_resume_probe.sh`
- `build_support/run_readybasic_second_entry_editor_probe.sh`
- `build_support/run_readybasic_module_overlay_probe.sh`
- `build_support/run_readybasic_full_suite_visual_verification.sh`

The ReadyBASIC second-entry/editor probe should stay scoped to ReadyBASIC plus
Editor. ReadyShell and Calc Plus have distinct REU-overlay behavior and should
be covered by their own focused probes, not used as evidence for a ReadyBASIC
runtime regression.

Any assertion that checks screen text must account for C64 screen scrolling.
If a proof emits many lines, split it into smaller screen-clear blocks so the
asserted text remains visible.

## Refactor Checklist

Before editing:

- Identify whether the change touches language runtime, command descriptors,
  module loader, REU layout, under-ROM payloads, or tests only.
- Read the current map/report when memory layout matters:
  `obj/readybasic.map` and `docs/readybasic_memory_diagrams.html`.
- Preserve `BASIC_START=$2AC1` and empty free bytes `30013` unless the change is
  explicitly a memory-layout change.

During editing:

- Keep BASIC ROM calls in visible resident code.
- Keep under-ROM banking explicit and restored.
- Keep KERNAL/disk I/O cleanup paths complete.
- Keep disk module packages as SEQ `rbm.*`, not PRG.
- Keep module/submodule/overlay ids meaningful in tests and documentation.
- Do not use string parsing hacks where structured descriptor/package parsing is
  already available.

After editing:

- Rebuild `readybasic.prg`.
- Regenerate generated ReadyBASIC YAML plans if scripts changed.
- Run focused VICE probes first, then `make readybasic-vice-suites`.
- Confirm no unexpected `?SYNTAX ERROR` or `?RB ERROR` appears except tests that
  deliberately assert failure behavior.
- Confirm BASIC free bytes and module loader BASIC-free assertions still pass.
- Record any changed command/module behavior in both Markdown docs and generated
  HTML/docs when applicable.
