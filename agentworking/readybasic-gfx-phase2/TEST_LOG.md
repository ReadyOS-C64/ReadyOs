# ReadyBASIC Graphics Phase 2 Test Log

## 2026-06-15

- `make bin/readybasic.prg obj/rbgfx16_phase2_sprite_ctrl.prg` passed after
  adding tokenizer-safe sprite aliases.
- `build_support/run_readybasic_gfx_phase2_demo.sh` passed.
  Run dir: `logs/vice_auto_20260615_232955`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-gfx-phase2-vice` passed.
  Run dir: `logs/vice_auto_20260615_233307`.
- `make readybasic-plugin-static-check` passed.
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
