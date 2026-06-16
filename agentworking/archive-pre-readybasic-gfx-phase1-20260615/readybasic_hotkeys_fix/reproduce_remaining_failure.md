# Remaining Keyboard Regression Failure

Status as of 2026-06-11: the focused regular and EasyFlash hotkey suites passed, but the newer keyboard regression probe still exposes one unresolved case.

## Failure

- Probe: `readybasic_keyboard_regression_probe_easyflash`
- Latest failed run: `logs/vice_auto_20260611_153009`
- Failed step: `wait_editor_after_partial_f2`
- Failure text: timeout waiting for screen text `editor`

The probe proves these earlier cases before the failure:

- ReadyBASIC launches from the EasyFlash launcher.
- ReadyBASIC CHRIN and KEYLOG vectors are installed.
- Warp-mode normal typing preserves spaces (`PRINT "A B C"`).
- A partially typed prompt line `10` plus `Ctrl+B` switches immediately to the launcher.
- No visible `REM` line is injected or later listed.
- ReadyBASIC remains usable after reentry.
- Editor can be loaded and can return to the launcher via `Ctrl+B`.

The remaining failure is the later partial-line `F2` case after an Editor round trip:

1. Enter ReadyBASIC from the launcher.
2. Type `20` at the ReadyBASIC prompt, without pressing Return.
3. Press `F2`.
4. Expected: immediate switch to Editor.
5. Actual in the latest automated EasyFlash run: the probe timed out waiting for `editor`.

## Manual Regular D81 Reproduction

The regular D81 can be launched interactively with:

```sh
cd /Users/karlprosserpp/dev/c64projects/readyosprecog
/opt/homebrew/bin/x64sc -logtostdout -verbose -warp \
  -8 Releases/0.2.5/precog-d81/readyos-v0.2.5m-d81.d81 \
  -autostart Releases/0.2.5/precog-d81/readyos-v0.2.5m-d81-preboot.prg
```

Manual check:

1. At the launcher, enter ReadyBASIC.
2. Type `10`, then press `Ctrl+B`; it should switch to the launcher immediately.
3. Reenter ReadyBASIC and run `LIST`; `REM` should not appear.
4. Switch to Editor, then back to ReadyBASIC.
5. Type `20`, then press `F2`; this is the unresolved case to test carefully.

## Automated Reproduction

Regular D81:

```sh
cd /Users/karlprosserpp/dev/c64projects/readyosprecog
READYBASIC_KEYBOARD_BOOT_MODE=launcher \
READYBASIC_KEYBOARD_LAUNCH_KEYS=17,17,17,17,13 \
READYBASIC_SKIP_BUILD=1 \
bash build_support/run_readybasic_keyboard_regression_probe.sh
```

EasyFlash:

```sh
cd /Users/karlprosserpp/dev/c64projects/readyosprecog
bash build_support/run_easyflash_vice_suites.sh readybasic_keyboard_regression_probe
```

Expected current automated failure for the unresolved case: `failed_step=wait_editor_after_partial_f2`.
