# ReadyBASIC Graphics Phase 5 Test Log

## Focused Run

Command:

```sh
make bin/readybasic.prg
make readybasic-plugin-static-check
make readybasic-gfx-phase5-vice
```

Result:

- `make bin/readybasic.prg`: pass
- `make readybasic-plugin-static-check`: pass
- `make readybasic-gfx-phase5-vice`: pass

Run directory:

- `logs/vice_auto_20260617_005937`
- Later focused rerun after the `SPRCOL` phase-1 demo fix and full-suite prep:
  `logs/vice_auto_20260617_011251`

Key screenshots:

- `readybasic_gfx_phase5_tile_visible.png`
- `readybasic_gfx_phase5_mtile_visible.png`
- `readybasic_gfx_phase5_mbitmap_dlist.png`
- `readybasic_gfx_phase5_sync_source.png`
- `readybasic_gfx_phase5_sync_cleared.png`
- `readybasic_gfx_phase5_sync_restored.png`
- `readybasic_gfx_phase5_convex_poly.png`

PNG sanity counts from the focused run:

- tile visible: 6 colors, 38344 nonblack pixels
- mtile visible: 5 colors, 38416 nonblack pixels
- mbitmap dlist: 3 colors, 1310 nonblack pixels
- sync source: 3 colors, 2009 nonblack pixels
- sync cleared: 2 colors, 1116 nonblack pixels
- sync restored: 3 colors, 2009 nonblack pixels
- convex poly: 3 colors, 12894 nonblack pixels

## Focused Graphics Regression Batch

Command:

```sh
make readybasic-plugin-static-check readybasic-gfx-phase1-vice readybasic-gfx-phase2-vice readybasic-gfx-phase3-vice readybasic-gfx-mbitmap-vice readybasic-gfx-phase4-vice readybasic-gfx-phase5-vice
```

Result: pass.

Key run directories:

- Phase 1: `logs/vice_auto_20260617_010936`
- Phase 2: `logs/vice_auto_20260617_011006`
- Phase 4: `logs/vice_auto_20260617_011146`
- Phase 5: `logs/vice_auto_20260617_011251`

## Full ReadyBASIC VICE Suite

Command:

```sh
make readybasic-vice-suites
```

Result: pass on the final normal run.

Final run directories observed during the suite:

- `readybasic_demo_suite`: `logs/vice_auto_20260617_015512`
- `readybasic_repeat_label_probe`: `logs/vice_auto_20260617_020549`
- `readybasic_lifecycle_probe`: `logs/vice_auto_20260617_020643`
- `readybasic_plugin_command_probe`: `logs/vice_auto_20260617_020808`
- `readybasic_program_probe`: `logs/vice_auto_20260617_021011`
- `readybasic_rbtest1_probe`: `logs/vice_auto_20260617_021120`
- `readybasic_large_vars_probe`: `logs/vice_auto_20260617_021227`
- `readybasic_hotkey_launcher_cycle`: `logs/vice_auto_20260617_021311`
- `readybasic_reuviewer_f2_chain_probe`: `logs/vice_auto_20260617_021513`
- `readybasic_cross_app_resume_probe`: `logs/vice_auto_20260617_021557`
- `readybasic_second_entry_editor_probe`: `logs/vice_auto_20260617_021819`
- `readybasic_full_suite_visual_verification`: `logs/vice_auto_20260617_022010`
