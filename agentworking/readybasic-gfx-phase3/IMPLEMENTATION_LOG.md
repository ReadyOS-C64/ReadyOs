# ReadyBASIC Graphics Phase 3 Implementation Log

## 2026-06-16

- Added linker segment `OVL3PACK` for a third slot-2 replacement overlay.
- Added `RB_SUBMOD_GFXPOLY = 20` and `RB_CODE_GFXPOLY_OFF = $6000`.
- Added typed REU point-buffer handle kind `RB_HANDLE_TYPE_POINTBUF = 4`.
- Added command descriptors for `POLY`, `FPOLY`, `PBUFNEW`, `PBUFSET`,
  `PBUFFREE`, `POLYH`, and `FPOLYH`.
- Added the `SIG_POLY` parser signature for BASIC integer-array polygon input.
- Reused existing numeric signatures for point-buffer handle commands where
  possible to avoid resident growth.
- Added `GFXPOLY` body code in `OVL3PACK`, including array point reads, REU
  point-buffer page fetch/store, outline polygon closure, and conservative
  filled polygon fan drawing.
- Extended cold prestash to copy `OVL3PACK` from `CMDPACK2` into assigned
  code-bank offset `$6000`.
- Updated static verification to enforce `GFXPOLY` placement, size, submodule
  id, handle type, and `RB_CODE_GFXPOLY_OFF=$6000`.
- Updated memory-report generation and regenerated
  `docs/readybasic_memory_diagrams.html`.
- Added Phase 3 BASIC demos and ReadyOS/ReadyBASIC VICE automation.
- Fixed polygon loop state after VICE exposed REU-demo hangs: point count now
  lives in dedicated copy scratch instead of fields clobbered by line drawing.
- Kept demo labels neutral so `LIST`/`RUN` automation proves the files without
  accidentally invoking command parsing from display-only text.

Implementation decision:

- The requested dynamic same-name handle form would have required more resident
  parser/dispatch code. With `RESIDENT` ending at `$2ABF`, the implementation
  uses `POLYH` and `FPOLYH` for REU point-buffer handles to preserve the fixed
  BASIC start/free-byte invariant.
