# ReadyBASIC Graphics Phase 1 Implementation Log

## 2026-06-15

- Started Phase 1 command-only implementation.
- Cleaned AGENTWORKING top level by archiving older artifacts.
- Added built-in graphics descriptors for `GFXCORE`, `GFXPRIM`, `GFXSPR`, and
  `INPUTEV`.
- Added command-only signatures for graphics mode strings, graphics surfaces,
  3-arg primitives, 5-arg primitives, sprite setup, and no-arg input commands.
- Implemented Bank D mode setup, clears, immediate plot/point/line/rect/frect,
  sprite setup/move/color/collision polling, and joystick/keyboard polling.
- Moved command descriptor lookup under hidden RAM to keep resident code within
  the `$1200-$2ABF` budget.
- Added `rbgfx01` through `rbgfx12` BASIC demos and Makefile/profile wiring.
- Added `build_support/run_readybasic_gfx_phase1_probe.sh`.
- Updated ReadyBASIC graphics/current/plugin architecture docs.
- Regenerated the regular D81 profile with the built-in graphics demos present.
- Regenerated the ReadyBASIC memory report.
- Added `build_support/run_readybasic_readyos_loaded_apps_suite.sh`, which boots
  ReadyOS preboot, waits for ReadyBASIC loaded by ReadyOS, then loads/lists/runs
  `RBTEST1`, `RBPROC1`, and all `RBGFXxx` PRGs from inside ReadyBASIC.
- Fixed demo programs so they only use existing ReadyBASIC/C64 BASIC syntax:
  numeric loop variables instead of integer loop variables, and no graphics
  command immediately after `THEN`.
- Fixed `GFXMODE` VIC register setup so stack restoration no longer overwrites
  the intended `$D011`/`$D016` values.
- Fixed bitmap `GFXCLEAR` screen RAM initialization so plotted foreground pixels
  are visible in hires and multicolor bitmap modes.
- Added `SPRROW(N,ROW,B1,B2,B3)` to the `GFXSPR` overlay so BASIC demos can
  define actual 24-bit hardware sprite rows in Bank D sprite memory.
- Fixed slot-2 overlay descriptors to copy `GFXSPR`/`INPUTEV` from their real
  linked payload offsets and use entry offsets relative to slot 2.
- Added `RBGFX13` plus `build_support/run_readybasic_sprite_steps_demo.sh` and
  `make readybasic-sprite-steps-vice` for ReadyOS-hosted staged sprite
  screenshots.
