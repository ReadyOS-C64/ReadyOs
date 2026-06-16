# ReadyBASIC Graphics Phase 2 Test Log

## 2026-06-15

- `make bin/readybasic.prg obj/rbgfx16_phase2_sprite_ctrl.prg` passed after
  adding tokenizer-safe sprite aliases.
- `build_support/run_readybasic_gfx_phase2_demo.sh` passed.
  Run dir: `logs/vice_auto_20260615_232955`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-gfx-phase2-vice` passed.
  Run dir: `logs/vice_auto_20260615_233307`.
- `make readybasic-plugin-static-check` passed.
- `build_support/run_readybasic_gfx_phase2_demo.sh` passed after the
  `CMDPACK2`/replacement-overlay rework.
  Run dir: `logs/vice_auto_20260616_132309`.
- The Phase 2 VICE run booted ReadyOS preboot, waited for ReadyBASIC loaded by
  ReadyOS, then loaded and ran `RBGFX14`, `RBGFX15`, and `RBGFX16` inside
  ReadyBASIC.
- Captured screenshots:
  - `readybasic_gfx_phase2_prompt`
  - `readybasic_gfx_phase2_list_rbgfx14`
  - `readybasic_gfx_phase2_primitives`
  - `readybasic_gfx_phase2_prims_done`
  - `readybasic_gfx_phase2_tiles`
  - `readybasic_gfx_phase2_tiles_done`
  - `readybasic_gfx_phase2_sprite_normal`
  - `readybasic_gfx_phase2_sprite_expanded`
  - `readybasic_gfx_phase2_sprite_multi_priority`
  - `readybasic_gfx_phase2_sprites_done`
- Earlier failed runs caught PETCAT tokenization collisions in `SPREXPAND`,
  `SPRMULTI`, and `SPRMCLR`; aliases now keep demo source token-safe.
- There are no polygon screenshots because `POLY` and `FPOLY` are not
  implemented Phase 2 commands; they remain documented future/deferred work.

## 2026-06-16

- `make readybasic-plugin-static-check` passed after expanding the static
  guardrails for `CMDPACK2`, replacement slot-2 overlay run addresses, and
  fixed code-bank offsets `$5000`/`$5800`.
- `make readybasic-memory-report` regenerated
  `docs/readybasic_memory_diagrams.html` with the sparse built-in payload map.
- `make readybasic-vice-suites` passed after the CMDPACK2/replacement-overlay
  rework. Final run status was success with no failed or degraded steps.
  Representative run dirs:
  - graphics demo: `logs/vice_auto_20260616_132309`
  - module overlay probe: `logs/vice_auto_20260616_141715`
  - plugin command probe: `logs/vice_auto_20260616_141814`
  - cross-app resume probe: `logs/vice_auto_20260616_142612`
  - second-entry/editor probe: `logs/vice_auto_20260616_142835`
  - full suite visual verification: `logs/vice_auto_20260616_143027`
- The module overlay probe retained the ReadyBASIC free-byte assertion.
