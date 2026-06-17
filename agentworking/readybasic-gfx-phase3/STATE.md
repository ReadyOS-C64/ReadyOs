# ReadyBASIC Graphics Phase 3 State

Objective: implement polygon-first graphics commands as ReadyBASIC commands only,
without changing ReadyBASIC language behavior, tokenization, control flow, native
expression rules, `BASIC_START`, or empty BASIC free bytes.

Design source: `src/apps/readybasic/READYBASIC_GRAPHICS_COMMAND_DESIGN.md`.

Current status:

- Phase 3 adds built-in module id `3`, submodule `20`, named `GFXPOLY`.
- `GFXPOLY` is a slot-2 replacement overlay fetched into `$B800-$BF99`.
- The overlay is stored in the ReadyBASIC assigned command-code REU bank at
  offset `$6000`; its current size is `$079A` / 1946B.
- `BASIC_START` remains `$2AC1`; formula empty BASIC free bytes remain `30013`.
- Array-backed commands are `POLY(A%(0),COUNT,C)` and
  `FPOLY(A%(0),COUNT,C)`.
- REU point-buffer commands are `PBUFNEW(COUNT,H%)`,
  `PBUFSET(H%,INDEX,X,Y)`, `POLYH(H%,COUNT,C)`, `FPOLYH(H%,COUNT,C)`, and
  `PBUFFREE(H%)`.
- Point buffers are typed REU handle type `4` and currently use one heap page,
  supporting up to 64 zero-based points.
- New demos are `rbgfx17_poly_array.bas`, `rbgfx18_fpoly_array.bas`,
  `rbgfx19_poly_reu.bas`, and `rbgfx20_fpoly_reu_showcase.bas`.
- New automation is `build_support/run_readybasic_gfx_phase3_demo.sh` and
  `make readybasic-gfx-phase3-vice`.

Known limits:

- Same-name handle overloads `POLY(H%,COUNT,C)` and `FPOLY(H%,COUNT,C)` were
  not implemented. `POLYH` and `FPOLYH` keep the resident parser small enough to
  preserve `BASIC_START=$2AC1`.
- `FPOLY` and `FPOLYH` use a conservative convex fan fill, not a full
  scanline/concave polygon fill.
- Polygon line drawing uses the current compact step-toward-endpoint primitive,
  so uneven slopes are approximate.
- Tile modes draw/fill through the existing cell-space plotting path.
