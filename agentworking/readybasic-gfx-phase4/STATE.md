# ReadyBASIC Graphics Phase 4 State

- Implemented command-only overlays `GFXDL` and `GFXTILE`.
- Added retained display-list commands, fixed-format charset/tileset/tilemap handles, explicit multicolor bitmap cell commands, mode-matrix and target/blit demos, and ReadyOS-hosted VICE automation.
- `BASIC_START` remains `$2AC1`; resident size remains `$18C0`.
- Latest focused run: `READYBASIC_SKIP_BUILD=1 make readybasic-gfx-phase4-vice` passed with 34/34 steps.
- Known issue: `TMDRAW` writes expected Bank D screen/color bytes, but the current VICE screenshot is still blank in `GFXMODE("TILE")`; remaining issue is Bank D charset/tile display visibility.
