# ReadyBASIC Graphics Phase 1 Test Log

## 2026-06-15

- `make bin/readybasic.prg` passed.
- `make obj/rbgfx01_modes.prg obj/rbgfx02_hires_plot.prg obj/rbgfx03_hires_lines.prg obj/rbgfx04_rects.prg obj/rbgfx05_point_read.prg obj/rbgfx06_reu_surface.prg obj/rbgfx07_mbitmap.prg obj/rbgfx08_tile.prg obj/rbgfx09_sprites.prg obj/rbgfx10_collision.prg obj/rbgfx11_input.prg obj/rbgfx12_showcase.prg bin/readybasic.prg` passed.
- `make readybasic-plugin-static-check` passed.
- `READYBASIC_SKIP_BUILD=1 make readybasic-gfx-phase1-vice` passed.
  Run dir: `logs/vice_auto_20260615_194952`.
- Current-image rerun of `READYBASIC_SKIP_BUILD=1 make
  readybasic-gfx-phase1-vice` passed after the final D81 rebuild.
  Run dir: `logs/vice_auto_20260615_202122`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-module-overlay-vice` passed.
  Run dir: `logs/vice_auto_20260615_195343`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-plugin-command-vice` passed.
  Run dir: `logs/vice_auto_20260615_195029`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-program-vice` passed.
  Run dir: `logs/vice_auto_20260615_195225`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-rbtest1-vice` passed.
  Run dir: `logs/vice_auto_20260615_195328`.
- `make readybasic-memory-report` passed and regenerated
  `docs/readybasic_memory_diagrams.html`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-vice-suites` was run after focused
  checks. It passed through demo, repeat/label, lifecycle, module overlay,
  plugin command, program, rbtest1, state, and large-vars probes, then hung in
  `readybasic-hotkey-vice` launcher-cycle startup waiting for launcher text at
  `wait_launcher_initial`.
  Repro run dir: `logs/vice_auto_20260615_201724`.
- Focused default hotkey probe passed after that hang:
  `bash build_support/run_readybasic_hotkey_probe.sh`.
  Run dir: `logs/vice_auto_20260615_201648`.
- `READYBASIC_SKIP_BUILD=1 make readybasic-readyos-loaded-apps-vice` passed.
  Run dir: `logs/vice_auto_20260615_205414`.
  This is the explicit ReadyOS context suite: it boots ReadyOS preboot, waits
  for ReadyBASIC loaded by ReadyOS, then loads/lists/runs `RBTEST1`, `RBPROC1`,
  and all `RBGFXxx` PRGs from inside ReadyBASIC with screenshots.
- The ReadyOS-loaded visual suite caught and validated fixes for:
  - BASIC demo loop variables using `%` where C64 BASIC syntax rejects them.
  - Unsupported command-after-`THEN` syntax in `rbgfx08_tile.bas`.
  - `GFXMODE` clobbering VIC register values while restoring the caller's
    return address.
  - Bitmap clears filling screen RAM with an invisible foreground/background
    color pair.
- `build_support/run_readybasic_sprite_steps_demo.sh` passed after rebuilding
  the regular D81 profile.
  Run dir: `logs/vice_auto_20260615_224423`.
  This boots ReadyOS preboot with ReadyBASIC as the first app, loads/lists/runs
  `RBGFX13` inside ReadyBASIC, captures initial sprite placement, movement,
  recolor, and final completion screenshots, and asserts no BASIC error.
