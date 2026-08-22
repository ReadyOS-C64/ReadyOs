# Local Agent Notes

- C64 Ultimate REST/FTP access from Codex must be run from a separate
  Terminal-owned/background bash, for example via `osascript` opening Terminal
  and writing logs/status files. Plain foreground `exec_command` curls can fail
  with false "no route"/connection errors even when the C64U is reachable.
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
- The full resident shim region is `$C600-$C9FF` (1 KB). `$C600-$C7FF` is reserved expansion space and the public jump-table/data ABI remains at `$C800-$C9FF`; never place app data/code assumptions anywhere in this region.
- Assume KERNAL/disk I/O can clobber app memory in the active region; keep persistent control state in defined safe areas only.
- ReadyBASIC module packages are generated SEQ files named `rbm.<name>`; preserve/restore disk-image logic must treat names beginning with `rbm.` as build-owned artifacts, not user files to restore from an older disk image.
- Preserve the release disk-directory ordering contract in `build_support/readyos_profiles.py`: boot-chain files (`PREBOOT` first, then any `SETD71`/`SHOWCFG`, `BOOT`, and `LAUNCHER`) precede configs, ordinary SEQ/USR data, main app PRGs, overlays/modules, REL files, and finally ReadyBASIC examples. This makes `LOAD"*",8` select `PREBOOT`. New unusual payloads must use a valid `directory_group`, and release changes must pass `build_support/verify_release_directory_order.py`. The EasyFlash CRT bank layout is separate, but its companion D64 follows the applicable data-file ordering.
- The Ultimate SKU's `SETUP` utility is standalone, not a ReadyOS app. It may link the focused ReadyOS TUI micromodules (`tui`, `tui_window`, `tui_menu`, and `tui_misc`) but must not use the ReadyOS shim, overlays, or ReadyFS architecture. ReadyFS is reference material only for proven Ultimate DOS/UCI behavior.
- C64 Ultimate SETUP automation may create and mutate only uniquely named test folders and images under its owned `READYOS_SETUP_TEST` roots on `usb1` or the SD card. It must never delete, rename, or overwrite unrelated Ultimate storage content; config replacement inside a test D81 uses Ultimate DOS names `rdyset.seq`/`rdyset.bak.seq` and rollback. Ultimate DOS must address the final C64 `apps.cfg` SEQ entry as `apps.cfg.seq`; omitting the suffix changes it to PRG.
- ReadyBASIC refactors must follow `src/apps/readybasic/readyBASICrefactorguidelines.md`. In particular, do not replace BASIC ROM expression/assignment helpers with ReadyBASIC command evaluators unless the normal BASIC cases are proven unchanged; the module-refactor regression broke `I%=I%+1` by doing this and caused `REPEAT`/`UNTIL` failures.
- For load/switch behavior debugging, validate against launcher+shim flow, not standalone assumptions.
- Always build and run ReadyOS through plain `run.sh` / `run.ps1`, booting ReadyOS itself rather than trying to load an individual app directly.
- Never call `run.sh` with a specific app name such as `launcher`, `editor`, or any other single-app mode; those paths are not valid for normal ReadyOS verification.
- Avoid ad-hoc `make`, direct artifact launches, and single-app run modes so all generated assets and preserved D71 user files are included and restored correctly.

## Ultimate Command Interface (UCI) Protocol Discipline

- Treat UCI as an asynchronous state machine, not as a timing-sensitive byte
  port. Use the official Ultimate UCI documentation as the protocol authority.
- Before writing a command, synchronize to `STATE=IDLE` with `CMD_BUSY`,
  `DATA_ACC`, and `ABORT_P` clear. Clear stale `ERROR` only as part of explicit
  recovery; an `ERROR` raised during a command is a transport failure, not the
  target's status response.
- Write every command byte while idle, then issue `PUSH_CMD` exactly once.
  `PUSH_CMD` is asynchronous: an immediate post-push `STATE=IDLE` sample means
  the Ultimate has not observed the push yet. It is never command completion.
- After `PUSH_CMD`, wait for `STATE=DATA_LAST` or `STATE=DATA_MORE`. Do not use
  fixed delays or quiet-loop counts to decide that a response is ready.
- In each data state, drain the response register while `DATA_AV` is set and
  the status register while `STAT_AV` is set. Only when both queue flags are
  clear may the C64 issue `DATA_ACC`.
- `DATA_ACC` is asynchronous too. After accepting `DATA_MORE`, do not treat an
  immediate unchanged `DATA_MORE` sample as the next block. The documented
  transition must leave `DATA_MORE` for `COMMAND_BUSY` before software waits
  for the next `DATA_LAST`/`DATA_MORE` block. After accepting `DATA_LAST`, wait
  for idle with all pending control bits clear before starting another command.
- `ABORT` is asynchronous. After requesting it, keep servicing/clearing the
  interface as needed and wait for a fully quiescent idle state. `ABORT_P`
  means a request is already pending; poll/service it rather than re-issuing
  `ABORT` on every status sample.
- Poll-count limits are failure bounds only; they must not provide protocol
  pacing, and servicing a flag must not reset the limit indefinitely. Bound
  each queue-drain loop too, with a limit above the documented queue capacity,
  so a stuck availability bit cannot hang the caller. A state-wait bound must
  also remain long enough for legitimate slow commands at the fastest tested
  CPU speed; a 16-bit instruction-count loop can become less than one second
  at 16 MHz and four times shorter again at 64 MHz. Validate UCI transports on
  physical Ultimate hardware at 1 MHz, 16 MHz, and the configured top end such
  as 64 MHz so accidental instruction delays cannot hide races. VICE is not
  evidence for Ultimate-specific UCI behavior.
- Respect the hardware queue capacities: 896 command bytes, 896 response-data
  bytes per queue block, and 256 status bytes. Always drain oversized replies
  even when the caller's capture buffer truncates them.
- Every app-level UCI call site and every standalone UCI probe must carry a
  nearby comment naming the state-machine contract it relies on: the transport
  owns synchronization, asynchronous PUSH/ABORT handling, complete queue
  draining, DATA_ACC, and the final quiet-idle wait. Run
  `python3 build_support/verify_uci_protocol_contract.py` after UCI changes;
  never copy an older transaction loop without bringing it under this check.

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
