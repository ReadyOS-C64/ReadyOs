# ReadyBASIC Graphics Phase 5 State

Date: 2026-06-17

Phase 5 is command-only. It keeps BASIC start/free memory and ReadyBASIC language behavior unchanged.

Implemented in this slate:

- Fixed `GFXMODE("TILE")` so it selects Bank D tile mode instead of falling through to `TEXT`.
- Kept graphics mode in `RB_GFX_MODE_STATE` so command overlays do not need to re-read VIC/CIA mode state.
- Made under-ROM command overlay calls enter with BASIC ROM hidden and I/O visible.
- Made `GFXSYNC` snapshot visible Bank D bitmap/screen/color layout into the selected `GFXTGT(H%)` surface.
- Made `GFXBLIT(H%)` restore typed REU graphics surfaces to visible Bank D.
- Added visible `TILE`, visible `MTILE`, MBITMAP display-list, sync/blit, and polygon showcase demos.
- Added `make readybasic-gfx-phase5-vice`.

Current focused screenshot run:

- `logs/vice_auto_20260617_011251`

Final full ReadyBASIC VICE suite:

- `make readybasic-vice-suites`: pass
