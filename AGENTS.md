# Local Agent Notes

- REL file access debugging: do **not** use `src/apps/dizzy/dizzy.c` as a reference implementation for REL open/position/read/write behavior.
- For CAL26 REL work, use the `xrelchk` harness and proven CAL26-specific test results as the source of truth.

## C64 + cc65 Working Rules

- Prefer `unsigned char` / `unsigned int`; avoid `long` and heavy stack locals in hot paths.
- cc65 uses a software stack and reserves critical zero-page runtime addresses (`$02-$1B`); do not stomp ZP runtime pointers (especially `sp`).
- In cc65 linker configs, never start `BSS` at `__ONCE_RUN__`; use `__ONCE_RUN__ + __ONCE_SIZE__` so warm REU resume does not restore a BSS-clobbered startup block.
- Keep inline asm conservative and explicit (`__asm__` form preferred). For cc65 placeholders, use `%v` (global symbol), `%o` (stack offset), `%w` (16-bit immediate), etc, only when needed.
- When crossing C/asm boundaries, keep calling-convention assumptions explicit and minimal; avoid complex inline asm that depends on unstable stack layout.

## ReadyOS Architecture Rules

- App working region is `$1000-$C5FF` (`$B600` bytes); REU save/restore also targets this range.
- Shim jump table/data is resident at `$C800-$C9FF`; never place app data/code assumptions there unless intentionally using shim ABI.
- Assume KERNAL/disk I/O can clobber app memory in the active region; keep persistent control state in defined safe areas only.
- ReadyBASIC module packages are generated SEQ files named `rbm.<name>`; preserve/restore disk-image logic must treat names beginning with `rbm.` as build-owned artifacts, not user files to restore from an older disk image.
- ReadyBASIC refactors must follow `src/apps/readybasic/readyBASICrefactorguidelines.md`. In particular, do not replace BASIC ROM expression/assignment helpers with ReadyBASIC command evaluators unless the normal BASIC cases are proven unchanged; the module-refactor regression broke `I%=I%+1` by doing this and caused `REPEAT`/`UNTIL` failures.
- For load/switch behavior debugging, validate against launcher+shim flow, not standalone assumptions.
- Always build and run ReadyOS through plain `run.sh` / `run.ps1`, booting ReadyOS itself rather than trying to load an individual app directly.
- Never call `run.sh` with a specific app name such as `launcher`, `editor`, or any other single-app mode; those paths are not valid for normal ReadyOS verification.
- Avoid ad-hoc `make`, direct artifact launches, and single-app run modes so all generated assets and preserved D71 user files are included and restored correctly.

### ReadyOS Launch Cookbook

- `run.sh` is not guaranteed to be executable in this checkout; invoke it as `/bin/bash ./run.sh ...`, not `./run.sh ...`.
- Interactive regular D81, current built artifacts, fast disk mode: `/bin/bash ./run.sh --profile precog-d81 --vice-fast --skipbuild`.
- Interactive regular D81 with rebuild first: `/bin/bash ./run.sh --profile precog-d81 --vice-fast`.
- Interactive default profile, current artifacts: `/bin/bash ./run.sh --skipbuild`; add `--vice-fast` for fast disk mode.
- Do **not** use `kff2-fast` unless the user explicitly asks for the Kung Fu Flash 2 D81 SKU; it is not the regular D81 build.
- To leave a visible VICE window open from Codex, run the interactive command in a long-running background shell/session and do not run any VICE cleanup or harness prelaunch cleanup afterward.
- Headless ReadyBASIC regular automation: `make readybasic-vice-suites`; focused hotkey probe: `/bin/bash build_support/run_readybasic_hotkey_probe.sh`.
- Headless ReadyBASIC cartridge automation: `make easyflash-readybasic-vice-suites`.
- Harness scripts close VICE by default; only use their `KEEP_VICE`/non-headless options when deliberately debugging automation, not for normal interactive launch.
- C64 Ultimate hardware automation must be launched from a long-running/background
  bash shell. One-shot foreground shell calls from Codex may not reach the C64U
  REST/FTP endpoints even when the device is reachable.

## CAL26 REL Debugging Discipline

- Use `xrelchk` first; prove behavior in harness before porting to `cal26`.
- Instrument stages and command-channel status codes during REL operations (especially after `P` positioning).
- Change one REL transport variable at a time (channel byte, record base/indexing, byte order, position byte, command-channel init), then rerun headless probe.
- Keep `POTENTIAL_REL_LEARNINGS.MD` updated with proven vs provisional findings; only promote to proven after repeatable harness pass.
- After every harness run batch (including failures), append a short entry to `CLAUDEWORKING/CAL26_REL_PROGRESS.md`:
  - exact command
  - key stage trace
  - first failing step/code
  - next hypothesis
