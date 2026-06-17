# ReadyBASIC Sound Phase 1 State

Started: 2026-06-17

Goal: add command-only SID sound support to ReadyBASIC without changing language behavior, BASIC start, or BASIC bytes free.

Current implementation:

- New built-in sound module id `4`.
- New `SIDCORE` submodule id `23`.
- New slot-2 overlay `OVL6PACK`, prestashed to the ReadyBASIC code REU bank at `$7800`.
- Public commands: `SIDCLR`, `SILENCE`, `VOL`, `FREQ`, `NOTE`, `PULSE`, `ADSR`, `ENV`, `WAVE`, `GATE`, `CTRL`, `VOICE`, `FILTER`, `FILT`, `SOUND`, `SND`.
- `VOICE` intentionally uses the existing five-number parser signature: `VOICE(V,F,W,AD,SR)`.
- No IRQ music engine, no resident scheduler, no new language features.

Verification status:

- `make bin/readybasic.prg` passes after removing the resident-growing seven-argument parser experiment.
- `make readybasic-plugin-static-check` passes.
- `make readybasic-memory-report` passes; BASIC free remains `30013`.
- `make readybasic-sound-phase1-vice` passes inside ReadyOS-loaded ReadyBASIC.
- `make readybasic-vice-suites` passes.
