# ReadyBASIC Graphics Phase 2 State

Objective: implement Phase 2 graphics as ReadyBASIC commands only, without
changing language/runtime behavior.

Design source: `src/apps/readybasic/READYBASIC_GRAPHICS_COMMAND_DESIGN.md`.

Current status:

- Phase 1 was committed as `2ddfcb5`.
- Phase 2 visible command additions are implemented in built-in graphics
  modules; no external `ZMODLD` step is required.
- New `GFXPRIM` commands: `CIRCLE`, `FCIRCLE`, `TILE`, and `CHARAT`.
- New `GFXSPR` commands and aliases: `SPREXPAND`/`SPRSIZE`, `SPRPRI`,
  `SPRMULTI`/`SPRMUL`, `SPRMCOLOR`/`SPRMCO`, and `SPRCOL`.
- New demos: `rbgfx14_phase2_prims.bas`, `rbgfx15_phase2_tiles.bas`, and
  `rbgfx16_phase2_sprite_ctrl.bas`.
- New VICE automation: `build_support/run_readybasic_gfx_phase2_demo.sh` and
  `make readybasic-gfx-phase2-vice`.
- Current map after the replacement-overlay rework: `BASIC_START` remains
  `$2AC1`, formula free bytes remain `30013`, `RESIDENT` remains `$18BB`
  / 6331B, and `bin/readybasic.prg` is `28674` bytes because `CMDPACK2`
  extends the cold-load seed image through `$7FFF`.
- Runtime slot headroom: slot 0 has 259B free, slot 1 has 907B free,
  `GFXPRIM` has 747B free, `GFXSPR` has 1422B free, and `INPUTEV` has
  1939B free.

Known limits:

- `CIRCLE` is an outline approximation that reuses `LINE`.
- `FCIRCLE` currently fills the bounding rectangle as a command-path proof.
- Full REU-backed offscreen drawing/blitting, dirty-rect `GFXSYNC`, retained
  display lists, sprite-sheet handles, polygon fill, and true circular fill are
  deferred.
- `POLY`/`FPOLY` and `SCROLL` are not implemented commands yet, so the Phase 2
  screenshot automation does not include polygon screenshots.
