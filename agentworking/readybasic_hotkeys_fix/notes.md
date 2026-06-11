# ReadyBASIC Hotkey Re-entry Scratch

## 2026-06-10 field failure to reproduce

- Rebuilt regular D81 still reports `0.2.5F`, but manual testing does not match the previous passing probes.
- Entering another app and pressing F2 back into ReadyBASIC can leave the READY prompt blinking while typed letters, F2, and Ctrl+B are ignored.
- A multi-hop chain can lock on the second F2; Ctrl+B may still return to the launcher from some states.
- Typed `EXIT` after some re-entry paths can return to the launcher and then automatically re-enter ReadyBASIC, which means pending keyboard/editor state is not clean enough at yield.
- The previous `readybasic_reuviewer_f2_chain_probe` rewrote `$C836-$C838` to only ReadyBASIC and REU Viewer before exercising F2. That can hide real app/resource bitmap selection failures and does not prove the full manual path.

## Current acceptance bar

- Prove ReadyBASIC remains keyboard-live after F2 from another ReadyOS app back into ReadyBASIC.
- Prove ReadyBASIC F2/F4/Ctrl+B work after multiple ReadyBASIC app exits and re-entries.
- Prove typed `EXIT` returns to the launcher and stays there until an explicit new selection.
- Cover the same path in regular D81 and EasyFlash ReadyBASIC suites, without changing other apps or the shim unless the repro proves the contract itself is broken.
- Keep the ReadyOS memory contract: no BASIC-start movement, no `$C800-$C9FF` assumptions beyond shim ABI, and no expansion into normal BASIC RAM.

## 2026-06-10 implementation notes

- ReadyBASIC owns prompt hotkeys only while active, direct, and keyboard-input backed. It now installs CINV `$0314/$0315` plus KEYLOG `$028F/$0290`, not a CHRIN hook.
- CINV directly scans CIA1 only for Ctrl+B, Shift+F1/F2, and Shift+F3/F4, restores the CIA scan-port state, records `rb_hotkey_pending`, and queues a harmless `REM` line so BASIC reaches the execute hook normally.
- KEYLOG catches KERNAL-decoded special-key cases, consumes the key state, and preserves the CIA port state.
- Every ReadyBASIC yield path clears `rb_hotkey_pending`, `$C6`, `$0277`, `SHFLAG`, `LSTX`, and `SFDX`; typed `EXIT` and Ctrl+B set the launcher suppress-startup flag so run-first configs do not auto-reenter ReadyBASIC.
- F2/F4 store the selected target bank in `rb_hotkey_target_bank` before `prepare_shim_yield`, then write `$C820` only after hidden save, `CLRCHN`, and vector restore.
- The loaded-app scan uses the shim bitmap plus the control-bank app registry so ReadyBASIC resource/code banks do not look like app-switch targets.
- Hidden helper warm restore comes from the assigned ReadyBASIC core bank shadow at `$3000`, not from a visible `$C280` shadow.

## 2026-06-10 harness rules

- Cold/first ReadyBASIC entry may wait for the `READYBASIC` title.
- Warm entries must not rely on the title being present. The chain probe uses a small delay plus `ready.`, then proves the state with `$C834`, ReadyBASIC IRQ/KEYLOG vectors, empty keyboard buffer, and an actual typed `PRINT` sentinel.
- The boot screen can contain `ready.` text; do not use `ready.` as the only proof of ReadyBASIC entry.
- In app context, VICE Binary `input.sequence` for Ctrl+B can drain `$C6` without exercising the app hotkey path. The focused chain uses monitor `keybuf \x02` for app Ctrl+B and normal function-key input for app F2.
- Host-key mode is useful for manual-like proof but can fail on macOS automation permissions (`osascript is not allowed to send keystrokes`) independent of ReadyBASIC.

## Current memory/headroom

- `BASIC_START` remains `$2AC1`; empty BASIC free bytes remain `30013`.
- `ENTRY` is `$1000-$11EA`, `$01EB` / 491B.
- `RESIDENT` is `$1200-$2AB6`, `$18B7` / 6327B, leaving 9B before the `$2ABF` resident budget end and the `$2AC0` sentinel.
- `HIDDEN` is `$A000-$A6A7`, `$06A8` / 1704B, leaving `$0158` / 344B in the 2K common helper area.
- `BRIDGE` is `$C000-$C1FD`, `$01FE` / 510B, leaving 2B before `$C200`.
- BASIC RAM contract is unchanged; the added cost is paid in entry/helper/bridge/resident bytes, not by moving BASIC start.

## Proof run log

- `make readybasic-plugin-static-check readybasic-memory-report`: passed after the hotkey implementation; regenerated `docs/readybasic_memory_diagrams.html`.
- Focused regular D81 run-first chain: `/bin/bash build_support/run_readybasic_reuviewer_f2_chain_probe.sh` passed, manifest `logs/vice_auto_20260610_181217/manifest.json`, `failed_step=null`, `degraded_steps=[]`.
- Focused regular D81 keylog/monitor chain: `READYBASIC_SKIP_BUILD=1 READYBASIC_HOTKEY_INPUT_MODE=keylog /bin/bash build_support/run_readybasic_reuviewer_f2_chain_probe.sh` passed, manifest `logs/vice_auto_20260610_181305/manifest.json`.
- Focused regular D81 launcher-mode chain passed, manifest `logs/vice_auto_20260610_180903/manifest.json`.
- Focused host-key chain was environment-blocked by macOS keyboard automation permission, manifest `logs/vice_auto_20260610_181352/manifest.json`.

## 2026-06-10 final proof run

- `make readybasic-plugin-static-check readybasic-memory-report`: passed after the final rebuild. `verify_readybasic_plugin.py` reported `readybasic plugin static check OK`; `docs/readybasic_memory_diagrams.html` was regenerated from `obj/readybasic.map`.
- Clean regular D81 aggregate: `READYBASIC_SKIP_BUILD=1 make readybasic-vice-suites` passed. Manifest audit: all listed manifests have `status=success`, `failed_step=null`, and `degraded_steps=[]`.
  - `logs/vice_auto_20260610_195856/manifest.json` - demo suite.
  - `logs/vice_auto_20260610_200925/manifest.json` - repeat/label.
  - `logs/vice_auto_20260610_201013/manifest.json` - lifecycle.
  - `logs/vice_auto_20260610_201034/manifest.json` - module overlay.
  - `logs/vice_auto_20260610_201125/manifest.json` - plugin command.
  - `logs/vice_auto_20260610_201322/manifest.json` - program mode.
  - `logs/vice_auto_20260610_201426/manifest.json` - rbtest1.
  - `logs/vice_auto_20260610_201434/manifest.json` - state preservation.
  - `logs/vice_auto_20260610_201520/manifest.json` - large vars.
  - `logs/vice_auto_20260610_201557/manifest.json` - Ctrl+B/F2/F4 focused hotkey.
  - `logs/vice_auto_20260610_201620/manifest.json` - ReadyBASIC/REU Viewer F2 chain.
  - `logs/vice_auto_20260610_201649/manifest.json` - 10-hop cross-app resume.
  - `logs/vice_auto_20260610_201904/manifest.json` - second-entry editor stress.
  - `logs/vice_auto_20260610_202049/manifest.json` - full visual verification.
- Clean EasyFlash aggregate: `make easyflash-readybasic-vice-suites` rebuilt the cartridge SKU and passed. Manifest audit: all listed manifests have `status=success`, `failed_step=null`, and `degraded_steps=[]`.
  - `logs/vice_auto_20260610_202900/manifest.json` - demo suite.
  - `logs/vice_auto_20260610_204104/manifest.json` - repeat/label.
  - `logs/vice_auto_20260610_204202/manifest.json` - lifecycle.
  - `logs/vice_auto_20260610_204232/manifest.json` - module overlay.
  - `logs/vice_auto_20260610_204335/manifest.json` - plugin command.
  - `logs/vice_auto_20260610_204542/manifest.json` - program mode.
  - `logs/vice_auto_20260610_204657/manifest.json` - rbtest1.
  - `logs/vice_auto_20260610_204716/manifest.json` - minimal resume.
  - `logs/vice_auto_20260610_204750/manifest.json` - screen/REU temp.
  - `logs/vice_auto_20260610_204922/manifest.json` - state preservation.
  - `logs/vice_auto_20260610_205019/manifest.json` - large vars.
  - `logs/vice_auto_20260610_205107/manifest.json` - F4 hotkey.
  - `logs/vice_auto_20260610_205136/manifest.json` - F2 hotkey.
  - `logs/vice_auto_20260610_205151/manifest.json` - ReadyBASIC/REU Viewer F2 chain.
  - `logs/vice_auto_20260610_205229/manifest.json` - 10-hop cross-app resume.
  - `logs/vice_auto_20260610_205455/manifest.json` - second-entry editor stress.
  - `logs/vice_auto_20260610_205649/manifest.json` - full visual verification.
- The chain proof now matches the field failure instead of only a cold title wait: first entry may wait for `READYBASIC`, warm entries use capture plus `$C834` app-bank checks, ReadyBASIC IRQ/KEYLOG vector checks, `$C6==0`, and typed `PRINT` sentinels. The boot screen's `READY.` text is not accepted as sufficient proof.
- The chain proof includes: ReadyBASIC Ctrl+B, explicit reentry, ReadyBASIC Ctrl+B again, REU Viewer launch, REU Viewer F2 to ReadyBASIC, typed ReadyBASIC liveness, ReadyBASIC F2 back to REU Viewer, app vectors restored, REU Viewer Ctrl+B to launcher, ReadyBASIC reentry/liveness, typed `EXIT`, launcher stability/no auto-reenter, and final ReadyBASIC liveness.

## 2026-06-10 final memory/headroom

- `BASIC_START` remains `$2AC1`; empty BASIC formula free bytes remain `30013`.
- `ENTRY` is `$1000-$11EA`, `$01EB` / 491B.
- `RESIDENT` is `$1200-$2AB6`, `$18B7` / 6327B, leaving 9B before `$2ABF` / `$2AC0`.
- `HIDDEN` is `$A000-$A6A7`, `$06A8` / 1704B, leaving `$0158` / 344B in the 2K common helper area.
- `LOWPACK` is `$A800-$AECD`, `$06CE` / 1742B.
- `SLOTPACK1` is `$B000-$B23A`, `$023B` / 571B.
- `BRIDGE` is `$C000-$C1FD`, `$01FE` / 510B, leaving 2B before `$C200`.
- The ReadyOS contract is preserved: no BASIC-start movement, no normal BASIC RAM expansion, no shim/app changes for this fix, and no ReadyBASIC private assumptions in `$C800-$C9FF` beyond the shim ABI.
