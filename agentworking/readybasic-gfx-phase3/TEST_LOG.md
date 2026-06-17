# ReadyBASIC Graphics Phase 3 Test Log

## 2026-06-16

Planned focused checks:

- `make bin/readybasic.prg`
- `make readybasic-plugin-static-check`
- `make readybasic-memory-report`
- `make readybasic-gfx-phase3-vice`
- `make readybasic-gfx-phase2-vice`
- `make readybasic-module-overlay-vice`
- `make readybasic-plugin-command-vice`

Planned full regression:

- `make readybasic-vice-suites`

Capture labels expected from the Phase 3 VICE demo:

- `readybasic_gfx_phase3_poly_array`
- `readybasic_gfx_phase3_fpoly_array`
- `readybasic_gfx_phase3_poly_reu`
- `readybasic_gfx_phase3_fpoly_reu_showcase`

Completed focused checks so far:

- `make bin/readybasic.prg`: passed.
- `make readybasic-plugin-static-check`: passed.
- `make readybasic-memory-report`: passed.
- `make readybasic-gfx-phase3-vice`: passed, including ReadyOS boot,
  ReadyBASIC-loaded demos, one `LIST` assertion, `PBUFNEW`/`PBUFFREE` text
  probe, no BASIC error prompt, and all four screenshot captures.
- `make readybasic-gfx-phase2-vice`: passed.
- `make readybasic-module-overlay-vice`: passed.
- `make readybasic-plugin-command-vice`: passed.

Full regression:

- `make readybasic-vice-suites`: passed.

Relevant VICE run directories:

- Phase 3 polygon screenshots:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_180808`
- Phase 2 graphics regression:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_172349`
- Module overlay focused regression:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_172427`
- Plugin command focused regression:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_172530`
- Full suite final visual verification:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_175215`

Screenshot capture note:

- The final Phase 3 demos use the proven Phase 2 `GET`/SPACE gate after a
  short `ZPAUSE(30)`. Earlier pause-only demo captures returned to text before
  the harness screenshot step, so screenshot demos should hold graphics mode
  until automation explicitly advances them.
