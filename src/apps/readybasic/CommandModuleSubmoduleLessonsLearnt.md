# ReadyBASIC Command Module/Submodule Lessons Learnt

## 0.5 RC2 command packaging

`LDMOD` and `PAUSE` remain built in. All scalar/array/scratch and slot/overlay
proof workers are now disk-only, with no Z prefix. Load `RBM.SAMPLE1` before
using ECHO1, ADD16, HIDDENRAM, SUMNUMARRAY, RANGENUMARRAY, TEMPSCRATCH, FAIL,
SLOT0, SLOT1, CPYRST or COPY. `RBM.SAMPLE2` supplies SLOT2, SPAN and OVL1/OVL2.
The packages register 12, 7 and 32 descriptors respectively; sample3 includes
COPY/CPYRST and the stateful S6AA–S8EB family. Sample3 replaces the other demo
entries, preserving production and media commands. See
[the complete package contract](READYBASIC_SAMPLE_MODULES.md) for names,
placement, dependencies and examples.

Cold registration has 83 built-ins and 45 empty slots. Media occupies core
`$1A40–$1B3F`; the demo area is `$1B40–$1F3F`; SCRPUT stays at `$1FE0`.
Demo code and the built-in SPANPACK have been removed from the runtime image.
PAUSE retains its CPU-dependent busy loop; its argument is not a clock tick.
The loader remains in module 2, slot 1. BASIC still starts at `$2AC1` with
30013 empty free bytes.


This log tracks implementation lessons for the ReadyBASIC command
module/submodule experiment. It is intentionally separate from the design plan:
the plan describes the target architecture, while this file records what was
proven, adjusted, or rejected during implementation.

## Baseline

- Branch: `codex/readybasic-command-modules`.
- Scope is slices 1 through 4 only.
- Baseline static gate passed with:
  `make bin/readybasic.prg && make readybasic-plugin-static-check`.
- Baseline VICE gate passed with:
  `READYBASIC_VISIBLE=1 build_support/run_readybasic_full_suite_visual_verification.sh`.
  Result was `success`, 167/167 steps, failed step `null`, no degraded steps.
  Run dir:
  `../agenticdevharness/logs/vice_auto_20260525_205922`.
- Baseline memory:
  - `BASIC_START=$2AC1`.
  - Formula BASIC free bytes: `30013 ($753D)`.
  - `RESIDENT $1200-$2AB9`, size `$18BA` / 6330B.
  - `HIDDEN $A000-$A376`, size `$0377` / 887B.
  - `HIDDENPACK $A800-$A84C`, size `$004D` / 77B.
  - `LOWPACK $A900-$AF3C`, size `$063D` / 1597B.
  - `BRIDGE $C000-$C1F3`, size `$01F4` / 500B.
  - `REGSEED $5000-$600F`, size `$1010` / 4112B.
  - `readybasic.prg` payload bytes: `$5200` / 20992B.
- Next hypothesis: descriptor fields can be repacked without changing
  descriptor size if command name bytes stay at offset 16 and signature stays at
  offset 14 for the existing lookup/parser paths during Slice 1.

## Slice 1: Descriptor Refactor

- Changed the 32-byte command descriptor semantics from low/hidden flags to a
  compact module-aware shape while preserving signature offset 14 and command
  name offset 16.
- Routed low and hidden command payloads through one generic under-ROM
  fetch/call path.
- Converted `HIDDENRAM` to that generic path without changing its public
  behavior.
- Static gate:
  `make bin/readybasic.prg && make readybasic-plugin-static-check` passed.
- VICE gate:
  `READYBASIC_VISIBLE=1 build_support/run_readybasic_full_suite_visual_verification.sh`
  passed with `success`, 167/167 steps, failed step `null`, no degraded steps.
  Run dir:
  `../agenticdevharness/logs/vice_auto_20260525_210853`.
- Memory delta versus baseline:
  - `BASIC_START` unchanged at `$2AC1`.
  - Formula BASIC free bytes unchanged at `30013 ($753D)`.
  - `RESIDENT` changed from `$18BA` / 6330B to `$1891` / 6289B, delta `-41B`.
  - `HIDDEN`, `HIDDENPACK`, `LOWPACK`, `BRIDGE`, `REGSEED`, and PRG payload
    size were unchanged.
- Lesson: unifying low and hidden dispatch actually removed resident code
  because the old two-branch loader carried duplicate copy/call setup.
- Next hypothesis: moving the payload layout to 2KB slots should be done by
  changing linker segments and descriptor copy offsets first, then adding
  residency checks as a separate step inside Slice 2.

## Slice 2: Slot 0 Layout

- Moved the packed command payload from the old `$A900` LOWPACK run address to
  the new 2KB slot 0 address at `$A800-$AFFF`.
- Removed the separate `HIDDENPACK` segment; former hidden command payload code
  now uses the same generic slot 0 path as the rest of the under-ROM commands.
- Widened the common/helper under-ROM region to `$A000-$A7FF` in the linker
  config and static checker.
- Added slot residency mirror fields in the bridge area, but deferred the
  copy-skip decision until Slice 4 proof commands can measure it explicitly.
- Static gate:
  `make bin/readybasic.prg && make readybasic-plugin-static-check` passed.
- VICE gate:
  `READYBASIC_VISIBLE=1 build_support/run_readybasic_full_suite_visual_verification.sh`
  passed with `success`, 167/167 steps, failed step `null`, no degraded steps.
  Run dir:
  `../agenticdevharness/logs/vice_auto_20260525_212413`.
- Memory delta versus baseline:
  - `BASIC_START` unchanged at `$2AC1`; formula BASIC free bytes unchanged at
    `30013 ($753D)`.
  - `RESIDENT` changed from `$18BA` / 6330B to `$18B3` / 6323B, delta `-7B`.
  - `HIDDEN` changed from `$0377` / 887B to `$0368` / 872B, delta `-15B`.
  - `LOWPACK` and `HIDDENPACK` merged from `$063D + $004D` / 1674B total into
    one slot 0 `LOWPACK` of `$068A` / 1674B.
  - `BRIDGE` changed from `$01F4` / 500B to `$01F8` / 504B, delta `+4B`.
  - `REGSEED` and PRG payload size were unchanged.
- Failed experiment: the first resident-skip implementation passed early integer
  commands but produced a syntax error on `T$=UPPER(S$)` in the visual suite.
  The slot bytes themselves were present at `$A800`, so the risky part was the
  new skip decision, not the segment move.
- Lesson: land the physical layout separately from no-recopy optimization.
  Slice 4 should add copy-count instrumentation and prove skip behavior with
  new commands instead of trusting it implicitly.
- Next hypothesis: the built-in demo module can be modeled in the descriptor
  table first, with compact proof commands using existing integer-return
  plumbing before richer copy-count and overlay behavior are added.

## Slice 3: Built-In Module Proofs

- Added slot 1 and slot 2 linker/run segments at `$B000-$B7FF` and
  `$B800-$BFFF`, backed by the same built-in command seed payload in REU bank
  `$45`.
- Added module 2 proof descriptors and tiny assembler proof commands:
  `SLOT0()`, `SLOT1()`, and `SLOT2()`.
- Kept resident growth flat by reusing the existing `SIG_SCRCAP` no-argument
  integer parser instead of adding a new parser signature.
- Reintroduced descriptor-driven runtime base selection for slot 0, slot 1,
  and slot 2 while keeping the implementation compact enough to stay under the
  resident budget.
- Static gate:
  `make bin/readybasic.prg && make readybasic-plugin-static-check` passed.
- VICE gate:
  `READYBASIC_VISIBLE=1 build_support/run_readybasic_full_suite_visual_verification.sh`
  passed with `success`, 167/167 steps, failed step `null`, no degraded steps.
  Run dir:
  `../agenticdevharness/logs/vice_auto_20260525_213645`.
- Memory delta versus baseline:
  - `BASIC_START` unchanged at `$2AC1`; formula BASIC free bytes unchanged at
    `30013 ($753D)`.
  - `RESIDENT` changed from `$18BA` / 6330B to `$18B3` / 6323B, delta `-7B`.
  - `HIDDEN` changed from `$0377` / 887B to `$0368` / 872B, delta `-15B`.
  - Slot 0 `LOWPACK` is `$069F` / 1695B; slot 1 `SLOTPACK1` is `$0015` / 21B;
    slot 2 `SLOTPACK2` is `$0015` / 21B.
  - `BRIDGE` remains `$01F8` / 504B; `REGSEED` and PRG payload size remain
    unchanged.
- Lesson: the existing no-arg integer command shape is a useful ABI proof tool.
  It lets new module/submodule payloads be exercised without spending resident
  bytes on new parser dispatch.
- Next hypothesis: Slice 4 can add multi-slot, overlay-like proof payloads and
  a measured copy counter by extending descriptor metadata and tests, not by
  adding a broad new resident framework.

## Slice 4: Span, Overlay, And Copy-Count Proofs

- Added `SPAN()` as a two-slot proof payload with slot mask 1+2, linked for
  `$B000-$BFFF` and returning `40`.
- Added `OVL1()` and `OVL2()` as overlay proof payloads that both target slot
  2 and return distinct values, `51` and `52`.
- Added `CPYRST()` and `COPY()` so the visual suite can reset and inspect a
  tiny copy counter.
- Added a compact residency skip path that records the last command/overlay
  identity and increments the copy counter only when a REU fetch actually
  happens. This is intentionally smaller than the final per-slot REU metadata
  design, but it proves the dispatch/fetch/call behavior without moving
  resident code above the ReadyBASIC boundary.
- Extended the local ReadyBASIC full visual suite runner to assert:
  `SLOT0()`, `SLOT1()`, `SLOT2()`, `SPAN()`, `OVL1()/OVL2()`, and the
  no-recopy proof `CPYRST(); SLOT1(); SLOT1(); COPY()`.
- Static gate:
  `make bin/readybasic.prg && make readybasic-plugin-static-check` passed.
- VICE gate:
  `READYBASIC_VISIBLE=1 build_support/run_readybasic_full_suite_visual_verification.sh`
  passed with `success`, 175/175 steps, failed step `null`, no degraded steps.
  Run dir:
  `../agenticdevharness/logs/vice_auto_20260525_215209`.
- Memory delta versus baseline:
  - `BASIC_START` unchanged at `$2AC1`; formula BASIC free bytes unchanged at
    `30013 ($753D)`.
  - `RESIDENT` changed from `$18BA` / 6330B to `$18BC` / 6332B, delta `+2B`.
  - `HIDDEN` changed from `$0377` / 887B to `$0365` / 869B, delta `-18B`.
  - Slot 0 `LOWPACK` is `$06C7` / 1735B; slot 1 and slot 2 single-slot proof
    payloads are `$0015` / 21B each.
  - Two-slot `SPANPACK` is `$0015` / 21B, and both overlay proof payloads are
    `$0015` / 21B.
  - `BRIDGE` changed from `$01F4` / 500B to `$01F7` / 503B, delta `+3B`.
  - `REGSEED` and PRG payload size remain unchanged.
- Lesson: the final per-slot/per-submodule metadata design still needs a richer
  representation, ideally backed by REU bank `$44`; the tiny last-command proof
  was the right compromise for this slice because it proved no-recopy behavior
  while keeping resident growth to two bytes over the original baseline.
- Scope stop: per user correction, implementation stops here at Slice 4. Slice
  5 loader stubs and Slice 6 documentation hardening are intentionally not
  implemented in this branch state yet.

## Slice 5: Disk Module Loader Proof

- Added `LDMOD(name$)` as a module 2 / slot 1 command. The loader lives in
  under-ROM module payload code and uses existing resident REU stash helpers;
  resident size and BASIC free bytes did not move.
- Added `build_support/build_readybasic_disk_modules.py` to generate
  ReadyBasicModule SEQ packages named `rbm.<name>`:
  - `rbm.sample1` registers `DM1()` as a single slot-1 disk-loaded command returning
    `61`.
  - `rbm.sample2` registers `DM2S()` as a slot 1+2 span returning `74`, plus
    `DOV1()` / `DOV2()` as slot-2 overlays returning `72` / `73`. Those two
    entries both use submodule 5 because they are two overlays of the same
    submodule family.
  - `rbm.sample3` registers `S6AA()` through `S8EB()` across multiple submodule
    families and overlays. Each command returns a stateful integer sentinel, and
    the command name encodes submodule, overlay, and entrypoint: `S6`/`S7`/`S8`
    are submodules 6/7/8, the middle `A`-`E` is overlay 1-5, and the final
    `A`/`B` is the entrypoint inside that resident overlay image. Each overlay
    carries a one-byte local counter so the visual tests can prove resident
    reuse versus fresh reload from REU.
- Added the generated module artifacts to ReadyBASIC-capable D81 and dual-D71
  profiles so the normal ReadyOS boot disk includes the sample modules.
- Extended the local ReadyBASIC visual suite runner to assert:
  `LDMOD("RBM.SAMPLE1")`, `DM1()`, `LDMOD("RBM.SAMPLE2")`, `DM2S()`,
  `DOV1()/DOV2()`, `LDMOD("RBM.SAMPLE3")`, and representative
  `S6AA()`-`S8EB()` calls.
- Static gate:
  `make bin/readybasic.prg && make readybasic-plugin-static-check` passed.
- VICE gate:
  `READYBASIC_VISIBLE=1 build_support/run_readybasic_full_suite_visual_verification.sh`
  passed with `success`, 181/181 steps, failed step `null`, no degraded steps.
  Run dir:
  `../agenticdevharness/logs/vice_auto_20260525_222934`.
- Memory delta versus Slice 4:
  - `BASIC_START` unchanged at `$2AC1`; formula BASIC free bytes unchanged at
    `30013 ($753D)`.
  - `RESIDENT` remains `$18BC` / 6332B.
  - `HIDDEN` remains `$0365` / 869B.
  - Slot 0 `LOWPACK` remains `$06C7` / 1735B.
  - Slot 1 `SLOTPACK1` changed from `$0015` / 21B to `$0239` / 569B because
    it now contains the streaming loader command.
  - Slot 2, span, and overlay proof payloads remain `$0015` / 21B each.
  - `BRIDGE` remains `$01F7` / 503B; `REGSEED` and PRG payload size remain
    unchanged.
- Lesson: BASIC-visible command names must avoid embedded tokenizable keywords.
  `LDMOD` looked natural, but the `LOAD` suffix was tokenized before
  ReadyBASIC command lookup. `LDMOD` avoids that and should be the naming
  pattern for future loader-style commands.
- Lesson: the pre-v1 module files are not PRGs. `LDMOD` uses KERNAL file I/O
  to stream SEQ bytes through `$C500`, then stashes descriptors in REU bank
  `$44` and payload records in bank `$45`. This is what lets large package
  files prove the module system without reducing BASIC bytes free.

## Documentation Hardening: Preserve Then Refresh

- First documentation refresh over-compressed the large markdown and HTML guides
  and removed useful command/lifecycle/detail material. That was not acceptable
  for these docs.
- Restored the prior long-form content and changed the update strategy to
  additive current sections: current module/submodule memory facts appear near
  the top of each guide, while older detailed examples and historical notes
  remain in place.
- Acceptance rule for this pass: do not delete content unless it is completely
  irrelevant. If older content is historically useful but no longer current,
  annotate it as historical/pre-module rather than removing it.

## Local ReadyBASIC VICE Suite Ownership

- Moved the full ReadyBASIC visual-suite script into this repo as
  `build_support/run_readybasic_full_suite_visual_verification.sh`.
- The script still uses the external `agenticdevharness` VICE task runner, but
  finds it through `../agenticdevharness` by default or through
  `VICE_TASKS_REPO` / `VICE_TASKS_ROOT`; no local user home path is embedded.
- `make readybasic-full-vice` is now the repo-owned gate for the 181-step full
  visual suite.
- Updated the repeat-label VICE probe to use the same relative/env-var harness
  discovery and to write its generated plan under this repo's `logs/`.
- Verified:
  `make readybasic-plugin-static-check` passed, and
  `READYBASIC_VISIBLE=0 make readybasic-full-vice` passed with `success`,
  181/181 steps, failed step `null`, no degraded steps.
- Also verified:
  `READYBASIC_VISIBLE=0 make readybasic-repeat-label-vice` passed with
  `success`, 29/29 steps, failed step `null`, no degraded steps.
- Latest run dir:
  `../agenticdevharness/logs/vice_auto_20260526_143236`.
- Latest repeat-label run dir:
  `../agenticdevharness/logs/vice_auto_20260526_144120`.

## All ReadyBASIC VICE Definitions Are Repo Assets

- Imported the remaining ReadyBASIC VICE probe definitions from the external
  harness working area into `build_support/`.
- The external `agenticdevharness` checkout is now only the execution engine;
  ReadyBASIC owns the suite scripts and the generated YAML plan artifacts.
- Tracked generated plans live beside the scripts as
  `build_support/readybasic_*.generated.yaml`; they should be regenerated and
  committed when suite definitions change.
- Added `READYBASIC_GENERATE_PLAN_ONLY=1` support and the
  `make readybasic-vice-plans` target so plan artifacts can be refreshed
  without launching VICE.
- Added Make targets for the imported probes plus `readybasic-vice-suites` for
  the complete repo-owned suite family.
- Verified:
  `bash -n build_support/run_readybasic*.sh build_support/vice_readybasic_repeat_label_probe.sh`,
  `make -n readybasic-vice-plans readybasic-lifecycle-vice readybasic-rbtest1-vice`,
  and `make readybasic-vice-plans` all passed.
- Runtime verification from the repo-owned definitions:
  `READYBASIC_VISIBLE=0 READYBASIC_SKIP_BUILD=1 make readybasic-rbtest1-vice`
  passed with `success`, 12/12 steps, failed step `null`, no degraded steps;
  `READYBASIC_VISIBLE=0 READYBASIC_SKIP_BUILD=1 make readybasic-lifecycle-vice`
  passed with `success`, 47/47 steps, failed step `null`, no degraded steps;
  `READYBASIC_VISIBLE=0 READYBASIC_SKIP_BUILD=1 make readybasic-full-vice`
  passed with `success`, 181/181 steps, failed step `null`, no degraded steps.
