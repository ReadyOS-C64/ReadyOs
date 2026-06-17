# ReadyBASIC Graphics Phase 5 Implementation Log

## 2026-06-17

- Added demos `rbgfx28_tile_visible.bas` through `rbgfx32_convex_poly.bas`.
- Added `build_support/run_readybasic_gfx_phase5_demo.sh` and Makefile target `readybasic-gfx-phase5-vice`.
- Added the new demo PRGs to the ReadyBASIC-capable D81 profiles.
- Fixed the four-character mode parser path:
  - `TILE` now returns `RB_GFX_MODE_TILE`.
  - `TEXT` still returns `RB_GFX_MODE_TEXT`.
- Added command-side mode state storage at `RB_GFX_MODE_STATE`.
- Updated `gfx_get_mode`, primitive, display-list, and polygon mode checks to use the stored mode state.
- Seeded simple solid tile glyphs during `GFXCLEAR` for `TILE`/`MTILE` so immediate primitive demos render without requiring custom charset setup.
- Fixed `DLPLOT` color replay and added an MBITMAP display-list plot path.
- Fixed `GFXSYNC` and `GFXBLIT` REU DMA memory configuration:
  - REU surface transfers use `$35` so Bank D RAM and REU registers are both reachable.
  - General under-ROM command dispatch uses `$36` so command code runs with BASIC ROM hidden and I/O visible.

