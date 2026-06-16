# ReadyBASIC Hotkey Timing Fix Plan

## Objective

Make ReadyBASIC honor `Ctrl+B`, `F2`, and `F4` immediately when pressed at the prompt, after/during `LIST`, and across app round trips. The fix must call the same suspend/yield/switch machinery used by `EXIT` without injecting the text command `EXIT`.

## Non-Negotiable Acceptance Criteria

- No hotkey may be delayed until a later command, `LIST`, or Return.
- No hotkey byte may remain in `KEYD_COUNT` / `KEYD_BUFFER`.
- `Ctrl+B` from ReadyBASIC returns to launcher.
- `F2` and `F4` from ReadyBASIC switch to the expected loaded apps.
- Round trips through at least Editor and one non-editor app preserve behavior.
- Hotkeys remain disabled during actual BASIC program execution, but work during prompt editing and `LIST` output.
- Empty BASIC free bytes do not regress.
- Visible resident code does not overlap `$2AC0` / `$2AC1`.
- `$A000-$A7FF` helper budget deltas are measured and documented.
- Focused key-path tests pass before full regular and EasyFlash suites are run.

## Current Lessons

- Poking `rb_hotkey_pending` proves only the yield plumbing. It is not a hotkey test.
- Poking `$0277/$00C6` proves only artificial keyboard-buffer behavior. It is not the same as the existing app automation path.
- VICE `input.sequence` with `keys: [2]`, `[137]`, and `[138]` is already a valid control path because other app tests use it successfully for `Ctrl+B`, `F2`, and `F4`.
- The focused proof must compare ReadyBASIC against a known-good app in the same run: same image, same app-entry delay, same key bytes. If Editor/another app switches and ReadyBASIC does not, ReadyBASIC is broken.
- GUI/host-key injection is not the primary proof path. It can stay optional/manual only if needed later.
- Injecting `EXIT` is semantically wrong; hotkeys must call the internal suspend/yield/switch path directly.

## Implementation Plan

1. Stabilize the worktree.
   - Preserve existing user/generated changes.
   - Remove any temporary debug writes and failed diagnostic markers.
   - Keep or replace harness changes only if they become part of the verified key-path proof.
   - Record the exact starting diff and map in `agentworking/readybasic_hotkeys_fix/notes.md`.

2. Build the correct ReadyBASIC hotkey service.
   - Add one shared service routine for `Ctrl+B`, `F2`, and `F4`.
   - The service drains `KEYD_COUNT` and clears any hotkey bytes in `$0277`.
   - `Ctrl+B` calls the internal ready/exit yield path directly, not `EXIT` text injection.
   - `F2`/`F4` call the hidden app selection helpers, then the normal shim switch path.
   - Keep all final routines within the existing resident/hidden/bridge contracts.

3. Choose safe polling points.
   - First trace where the existing VICE app hotkey bytes enter ReadyBASIC:
     - prompt input,
     - KERNAL GETIN vector,
     - KERNAL CHRIN vector,
     - IRQ/CINV buffered key path,
     - LIST output path.
   - Service the hotkey at the first safe point that actually sees the same bytes other apps receive:
     - prompt input / GETIN / CHRIN path,
     - ReadyBASIC execute vector before ordinary statement dispatch,
     - LIST/vector path if LIST bypasses the prompt input path.
   - Do not depend on Return, `LIST`, or a later command to notice the pending action.
   - Preserve the “disabled during actual RUN” guard with an explicit runtime-state check, not a prompt-only `TXTPTR < BASIC_START` check that breaks LIST.

4. Add A/B VICE automation using the established app hotkey path.
   - Use `input.sequence`, not `memory.write`, for accepted hotkey proof.
   - Use the same byte values existing app automation uses:
     - `Ctrl+B`: `keys: [2]`
     - `F2`: `keys: [137]`
     - `F4`: `keys: [138]`
   - In one focused run, first enter Editor and one non-editor app, wait a couple seconds after app entry, then prove `Ctrl+B`, `F2`, and `F4` switch immediately there.
   - Enter ReadyBASIC with the same pause and send the same hotkey bytes. ReadyBASIC must behave the same way.
   - If the control apps pass and ReadyBASIC fails, treat that as conclusive ReadyBASIC failure, not a harness failure.

5. Replace the focused ReadyBASIC hotkey probe.
   - Remove `POKE rb_hotkey_pending` and `$0277/$00C6` hotkey simulation from acceptance tests.
   - Use the same `input.sequence` key steps proven by the control app phase.
   - Exercise:
     - ReadyBASIC prompt -> `Ctrl+B` -> launcher.
     - Launcher -> Editor -> launcher -> ReadyBASIC -> `Ctrl+B` again.
     - ReadyBASIC `LIST` active/just completed -> `F4` -> previous app.
     - Return to ReadyBASIC -> `F2` -> next app.
     - Repeat with at least one non-editor app.
   - After each hotkey transition assert:
     - expected screen/app,
     - `KEYD_COUNT == 0`,
     - `$0277` does not contain `2`, `137`, or `138`,
     - `rb_hotkey_pending == 0`,
     - no delayed switch occurs after a harmless command such as `PRINT "QUEUEOK"` or `LIST`.

6. Add negative RUN coverage.
   - Start a BASIC program that loops or prints.
   - Press `Ctrl+B`, `F2`, and `F4`.
   - Assert no app switch occurs during actual program execution.
   - Stop/return to prompt and assert hotkeys work again.

7. Memory and size checks.
   - Run `make readybasic-plugin-static-check`.
   - Run `make readybasic-memory-report`.
   - Record before/after map lines for `ENTRY`, `RESIDENT`, `HIDDEN`, `BRIDGE`.
   - Record empty BASIC free bytes.
   - Confirm resident end remains below `$2AC0`.
   - Document any `$A000-$A7FF` delta.

8. Full verification sequence.
   - Focused matrix-smoke VICE test.
   - Focused ReadyBASIC hotkey timing probe.
   - `make readybasic-plugin-static-check`.
   - `make readybasic-memory-report`.
   - `make readybasic-vice-suites`.
   - `make easyflash-readybasic-vice-suites`.
   - Record every run directory in `agentworking/readybasic_hotkeys_fix/notes.md`.

## Stop Conditions

- Do not run full suites until the real key-path focused probe passes.
- Do not declare success if the test uses `POKE rb_hotkey_pending`, `$0277/$00C6`, or `EXIT` injection as the final proof.
- If VICE cannot automate real matrix key states locally, produce a clear blocked result with the matrix-smoke fixture evidence and keep the code changes out of “accepted” status.
