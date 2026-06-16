# ReadyShell State Bank Debug/CAT Refactor Notes

## Goal

Remove ReadyShell's last fixed `$43` REU dependency by moving:

- the one-byte REU availability probe;
- the overlay debug ring;
- CAT command staging.

All three should live in the existing loader-assigned ReadyShell state bank.
CAT remains a shared command-scratch user, not a separately reserved bank.

## Intended Layout

```text
ReadyShell state bank

+$0000-$7DDF  shared transient command scratch
              CAT uses +$0000-$17FF while CAT is active
+$7DE0-$7FFF  ReadyShell diagnostics/probe tail
+$8000-$811F  heap metadata, command registry, overlay metadata, UI flags
+$8120-$FEFF  persistent value arena
+$FF00-$FFFF  unused tail
```

## Baseline

`agentworking/readyshell_statebank_debug_cat_headroom_before.json`

## Implemented Shape

- Removed the fixed ReadyShell debug bank type (`REU_RS_DEBUG`) and physical
  bank constant (`REU_BANK_RS_DEBUG`).
- Moved the REU availability probe to `ReadyShell state bank + $7FFF`.
- Moved debug head/data to `ReadyShell state bank + $7DE0/$7DF0`.
- Shrank shared command scratch from `$8000` to `$7DE0` bytes so the diagnostic
  tail is not trampled.
- Moved CAT staging to the shared command scratch base. CAT remains bounded by
  its existing `$1800` staging envelope, so it does not care about the scratch
  shrink.
- REU Viewer now labels ReadyShell state/scratch banks with `T`; no fixed `D`
  debug bank remains.

## Verification Targets

- Static: memory map, dynamic launcher, control bank, shim verifiers.
- ReadyShell VICE: cross-app resume probe now includes `CAT "RSHELP"` and a
  post-CAT `VER`.
- Regular SKU: ReadyBASIC suite plus ReadyShell probe.
- Cartridge SKU: EasyFlash suite, including mirrored ReadyBASIC/ReadyShell
  probes.

## Current Results

- `bash run.sh --build-all`: passed after source changes.
- Static verifiers: `verify_memory_map.py`, `verify_dynamic_launcher.py`,
  `verify_reu_control_bank.py`, and `verify_readyos_shim.py` passed.
- Regular ReadyShell CAT/cross-app probe:
  `READYSHELL_SKIP_BUILD=1 READYSHELL_VISIBLE=0 build_support/run_readyshell_cross_app_resume_probe.sh`
  passed; run `logs/vice_auto_20260606_144857`.
- Regular ReadyBASIC suite:
  `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 make readybasic-vice-suites`
  passed; notable runs include `logs/vice_auto_20260606_150934` for the
  211-step cross-app resume probe and `logs/vice_auto_20260606_151405` for the
  184-step full-suite visual verification.
- Cartridge/EasyFlash suite:
  `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 READYSHELL_VISIBLE=0 make easyflash-vice-suites`
  passed; notable runs include `logs/vice_auto_20260606_154237` for the
  213-step ReadyBASIC cross-app resume probe, `logs/vice_auto_20260606_154716`
  for the 186-step ReadyBASIC full-suite visual verification, and
  `logs/vice_auto_20260606_155519` for ReadyShell `VER`/`LST`/`CAT` coverage.
- Headroom deltas are in
  `agentworking/readyshell_statebank_debug_cat_headroom_after.json`; only
  ReadyShell lost resident headroom (`-22` bytes), while launcher/reuviewer and
  REU-manager consumers recovered small amounts.
