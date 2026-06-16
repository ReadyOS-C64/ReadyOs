# ReadyBASIC Graphics Phase 2 Implementation Log

## 2026-06-15

- Committed completed Phase 1 baseline as `2ddfcb5`.
- Added `CIRCLE(X,Y,R,C)` and `FCIRCLE(X,Y,R,C)` command descriptors and
  `GFXPRIM` module bodies.
- Added `TILE(X,Y,CH,C)` and `CHARAT(X,Y,CH,C)` command descriptors and cell
  write support for Bank D tile modes plus ordinary text-mode demo output.
- Added sprite expansion, priority, per-sprite multicolor enable, shared
  multicolor register, and tokenizer-safe alias descriptors in `GFXSPR`.
- Added `rbgfx14_phase2_prims.bas`, `rbgfx15_phase2_tiles.bas`, and
  `rbgfx16_phase2_sprite_ctrl.bas`.
- Added the Phase 2 demos to the ReadyBASIC demo list and regular D81 developer
  profiles.
- Added `build_support/run_readybasic_gfx_phase2_demo.sh`, which boots ReadyOS,
  waits for ReadyBASIC loaded by ReadyOS, loads/runs Phase 2 demos from inside
  ReadyBASIC, captures staged screenshots, and asserts no BASIC error.
- Updated graphics/current/plugin architecture docs with implemented Phase 2
  commands, aliases, payload sizes, and deferred work.
- Added `CMDPACK2` at `$6200-$7FFF` and changed `GFXSPR`/`INPUTEV` into true
  slot-2 replacement overlays. They are stored in the assigned command-code REU
  bank at `$5000` and `$5800`, respectively, then fetched into `$B800` when
  called.
- Updated static guardrails and the HTML memory report generator for sparse
  built-in overlay storage.
