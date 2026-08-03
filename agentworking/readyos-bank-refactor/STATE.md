# ReadyOS Bank Refactor State

This is the retained implementation audit for the refactor that makes physical
REU bank `Skip+1` the ReadyOS bank and removes the `$C600-$C7FF` resident
metadata mirror.

## Required end state

- `Skip+1` is the single authoritative ReadyOS metadata bank.
- Launcher state and the ReadyOS schema share that bank.
- App snapshots cover `$1000-$C7FF` (`$B800` bytes).
- The shim remains exactly `$0200` bytes at `$C800-$C9FF`.
- App/micromodule code does not retain a second allocation, loaded-app, token,
  resource, or clipboard metadata source in C64 RAM.
- ReadyBASIC's custom assembler consumes generated ABI constants and verifies
  the ReadyBASIC binary shape it depends upon.
- No tracked or untracked file that existed at the start of the refactor is
  deleted.

## Starting worktree

- 1,396 tracked files.
- The tree was already dirty with documentation, generated binaries, generated
  version files, and schema-v4 documentation work.
- Three tracked release artifacts were already deleted and must be restored,
  while their untracked `0.2.5d` replacements must also be preserved:
  - `Releases/0.2.5/precog-d81/readyos-v0.2.5y-d81-boot.prg`
  - `Releases/0.2.5/precog-d81/readyos-v0.2.5y-d81-preboot.prg`
  - `Releases/0.2.5/precog-d81/readyos-v0.2.5y-d81.d81`

## Baseline verification

- `/bin/bash ./run.sh --build-all`: passed in an isolated source copy.
- Shim verifier: passed; canonical image is exactly 512 bytes.
- Resume verifier: passed for the old `$B600/$4A00` contract.
- Schema-v4 control-bank verifier: passed.
- Dynamic-launcher verifier: passed, but its coverage of tokens above 23 is too
  weak and will be strengthened.
- All 16 regular ReadyBASIC aggregate suites ultimately passed.
- One plugin-command run stalled while boot displayed `LOADING LAUNCHER...`;
  the clean rerun passed all 119 steps.
- One 212-step cross-app run stopped at cycle 6 because the keyboard buffer did
  not drain; the clean rerun passed all 212 steps and all ten state checks.

## Known baseline-report gap

`report_app_headroom.py` currently reports only 19 maps and omits built apps
including Simple Cells, SIDetris, and UCITest. ReadyBASIC also needs a custom
segment report rather than the ordinary cc65 total. The reporter must discover
the complete built map set before post-refactor size claims are accepted.

## Final state — complete

- The schema-v5 ReadyOS bank at physical `Skip+1` is authoritative for bank
  types, explicit token-to-physical mappings, token status, app/resource
  registries, clipboard metadata, hotkeys, catalog strings, audit data, and
  runtime state. Physical `Skip` is the first dynamic allocation candidate.
- App snapshots are `$1000-$C7FF` (`$B800`); the resident shim remains exactly
  512 bytes at `$C800-$C9FF`. `$C836-$C838` are reserved retired bytes.
- Every discovered app map is present in the before/after report, including
  Simple Cells, SIDetris, UCITest, ReadyBASIC, both launchers, and both
  ReadyShell builds. ReadyBASIC's non-cc65 shape is separately enforced.
- All regular-D81 suites, all 19 EasyFlash plans, focused launcher/QuickNotes/
  ReadyShell probes, EasyFlash smoke, ReadyShell host tests, every release SKU,
  schema/resume/memory/shim verifiers, and HTML/source checks passed.
- All tracked release artifacts removed by versioned builds were restored.
  Final `git diff --diff-filter=D` is empty; no source, documentation, tracked
  generated artifact, or pre-existing untracked file was deleted.
