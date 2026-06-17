# Test Log

- `make bin/readybasic.prg` passed.
- `make obj/rbgfx23_dlist.prg obj/rbgfx24_tilemap.prg obj/rbgfx25_mbcells.prg` passed.
- `make readybasic-plugin-static-check` passed.
- `make obj/rbgfx26_mode_matrix.prg obj/rbgfx27_target_blit.prg` passed.
- `READYBASIC_SKIP_BUILD=1 make readybasic-gfx-phase4-vice` passed in ReadyOS-hosted VICE context with 34/34 steps.
- `make readybasic-vice-suites` passed after the Phase 4 demo/doc updates. The last broad suite stages completed successfully, including:
  - `readybasic_reuviewer_f2_chain_probe`, 64/64 steps, run directory `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_234214`.
  - `readybasic_cross_app_resume_probe`, 211/211 steps, run directory `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_234257`.
  - `readybasic_second_entry_editor_probe`, 150/150 steps, run directory `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_234519`.
  - `readybasic_full_suite_visual_verification`, 184/184 steps, run directory `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_234711`.
- Screenshot run directory with latest focused pass:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260616_230724`.

Screenshot observations:

- Display-list demo shows retained plot/line/rect/frect output.
- Multicolor bitmap cell demo shows visible slot/cell color changes.
- Mode matrix shows visible `HIRES` and `MBITMAP` primitive coverage, and its final text capture reports `MATRIX 1  1  1  73`, proving `PNT` returned values in all four tested modes.
- Target/blit demo reports `SURF 6`, `TARGET SURF ERR 0`, `SYNC ERR 0`, `BLIT ERR 0`, and `TARGET VISIBLE ERR 0`.
- Tilemap demo currently captures a blank blue Bank D tile screen even though a debug pass showed `TMDRAW` wrote screen byte `81`, color byte `2`, and `ERRCODE()` returned `0`.
