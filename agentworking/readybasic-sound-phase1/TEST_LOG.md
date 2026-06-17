# ReadyBASIC Sound Phase 1 Test Log

## 2026-06-17

- `make bin/readybasic.prg`
  - Initial result: failed; adding a seven-argument `VOICE` parser grew `RESIDENT` by 31 bytes.
  - Fix: remove the new parser signature and use existing `SIG_LINE` for packed `VOICE(V,F,W,AD,SR)`.
  - Current result: pass.

- `make obj/rbsnd01_sid_basics.prg obj/rbsnd02_voice_state.prg obj/rbsnd03_notes.prg obj/rbsnd04_filter.prg obj/rbsnd05_voice_batch.prg obj/rbsnd06_three_voice.prg`
  - Result: pass.

- `make readybasic-plugin-static-check`
  - Result: pass.

- `make readybasic-memory-report`
  - Result: pass.
  - Confirmed `RESIDENT` remains `$1200-$2ABF` and BASIC free memory remains `30013`.
  - Confirmed `OVL6PACK` / `SIDCORE` uses 526 bytes of a 2KB slot and is stashed at REU code-bank offset `$7800`.

- `make readybasic-sound-phase1-vice`
  - Result: pass.
  - ReadyOS booted first, ReadyBASIC was loaded by ReadyOS, and each `rbsndNN` demo was loaded/listed/run inside ReadyBASIC.
  - Run directory: `logs/vice_auto_20260617_115052`.

- `make readybasic-vice-suites`
  - Result: pass.
  - Includes the new sound demo suite and the full ReadyBASIC visual/command verification suite.
  - Sound suite run directory from full regression: `logs/vice_auto_20260617_121614`.
  - Full visual verification run directory from full regression: `logs/vice_auto_20260617_121645`.

Human audio note:

- VICE automation proves the demos load, list, run, and complete without BASIC errors in the correct ReadyOS context. It does not prove audible SID output quality; the demos deliberately print what should be heard and pause long enough for human listening.
