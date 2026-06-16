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

- Historical note: this pass used CINV `$0314/$0315` plus KEYLOG `$028F/$0290` and queued a visible `REM` line. The 2026-06-11 keyboard-regression pass below supersedes that design.
- The earlier CINV path directly scanned CIA1 only for Ctrl+B, Shift+F1/F2, and Shift+F3/F4, restored the CIA scan-port state, recorded `rb_hotkey_pending`, and queued `REM` plus Return so BASIC reached the execute hook normally.
- KEYLOG caught KERNAL-decoded special-key cases, consumed the key state, and preserved the CIA port state.
- Every ReadyBASIC yield path clears `rb_hotkey_pending`, `$C6`, `$0277`, `SHFLAG`, `LSTX`, and `SFDX`; typed `EXIT` and Ctrl+B set the launcher suppress-startup flag so run-first configs do not auto-reenter ReadyBASIC.
- F2/F4 store the selected target bank in `rb_hotkey_target_bank` before `prepare_shim_yield`, then write `$C820` only after hidden save, `CLRCHN`, and vector restore.
- Double-switch follow-up: cold/warm entry now scans the physical matrix and quarantines any still-held ReadyBASIC hotkey until release. Prompt hotkey yield waits for the selected chord to release with a jiffy-clock timeout, then clears editor/KERNAL key state again.
- The loaded-app scan uses the shim bitmap plus the control-bank app registry so ReadyBASIC resource/code banks do not look like app-switch targets.
- Hidden helper warm restore comes from the assigned ReadyBASIC core bank shadow at `$3000`, not from a visible `$C280` shadow.

## 2026-06-10 harness rules

- Cold/first ReadyBASIC entry may wait for the `READYBASIC` title.
- Warm entries must not rely on the title being present. The chain probe uses a small delay plus `ready.`, then proves the state with `$C834`, ReadyBASIC CHRIN/KEYLOG vectors, empty keyboard buffer, and an actual typed `PRINT` sentinel.
- The boot screen can contain `ready.` text; do not use `ready.` as the only proof of ReadyBASIC entry.
- In app context, VICE Binary `input.sequence` for Ctrl+B can drain `$C6` without exercising the app hotkey path. The focused chain uses monitor `keybuf \x02` for app Ctrl+B and normal function-key input for app F2.
- Host-key mode is useful for manual-like proof but can fail on macOS automation permissions (`osascript is not allowed to send keystrokes`) independent of ReadyBASIC.

## 2026-06-11 keyboard regression implementation notes

- ReadyBASIC now owns prompt hotkeys only while the ROM editor is inside ReadyBASIC's CHRIN hook, input is from the keyboard, and BASIC is at a direct prompt.
- KEYLOG `$028F/$0290` records Ctrl+B/F2/F4, consumes `SHFLAG`/`SFDX`, and queues only Return. It does not inject command text.
- CHRIN `$0324/$0325` wraps original CHRIN, preserves ordinary `A` and status flags, then dispatches a pending hotkey before BASIC can store or execute a partial prompt line.
- The hotkey gate should follow `TXTPTR < BASIC_START`, not `CURLIN`, so partially typed prompt lines stay eligible while RUN-mode input still stays out of scope.
- The CINV/IRQ matrix scanner is not installed for prompt hotkeys. Ordinary typing, repeat timing, cursor state, and space handling remain owned by the ROM editor.
- READY-mode hotkey yields force saved `RUNTIME_SP` to `$F8` instead of preserving transient CHRIN/editor stack depth.
- Entry-time hotkey quarantine and yield-time release waits remain: cold/warm entry clears editor state and suppresses a still-held ReadyOS hotkey until release; accepted Ctrl+B/F2/F4 waits for the selected chord to release before yielding.
- Focused regression coverage now includes `10`+Ctrl+B and `20`+F2 without storing partial lines, absence of visible `REM`, warp-mode single-space typing, normal typing after reentry, and app liveness after switch. The cartridge regression probe should keep the partial-line F2 case as a first-class check until it passes cleanly on both regular and EasyFlash builds.

## Current memory/headroom

- `BASIC_START` remains `$2AC1`; empty BASIC free bytes remain `30013`.
- `ENTRY` is `$1000-$11FF`, `$0200` / 512B.
- `RESIDENT` is `$1200-$2ABD`, `$18BE` / 6334B, leaving 2B before the `$2ABF` resident budget end and the `$2AC0` sentinel.
- `HIDDEN` is `$A000-$A6E9`, `$06EA` / 1770B, leaving `$0116` / 278B in the 2K common helper area.
- `BRIDGE` is `$C000-$C1FE`, `$01FF` / 511B, leaving 1B before `$C200`.
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
- The chain proof now matches the field failure instead of only a cold title wait: first entry may wait for `READYBASIC`, warm entries use capture plus `$C834` app-bank checks, ReadyBASIC CHRIN/KEYLOG vector checks, `$C6==0`, and typed `PRINT` sentinels. The boot screen's `READY.` text is not accepted as sufficient proof.
- The chain proof includes: ReadyBASIC Ctrl+B, explicit reentry, ReadyBASIC Ctrl+B again, REU Viewer launch, REU Viewer F2 to ReadyBASIC, typed ReadyBASIC liveness, ReadyBASIC F2 back to REU Viewer, app vectors restored, REU Viewer Ctrl+B to launcher, ReadyBASIC reentry/liveness, typed `EXIT`, launcher stability/no auto-reenter, and final ReadyBASIC liveness.

## 2026-06-10 final memory/headroom

- `BASIC_START` remains `$2AC1`; empty BASIC formula free bytes remain `30013`.
- `ENTRY` is `$1000-$11F6`, `$01F7` / 503B.
- `RESIDENT` is `$1200-$2AB8`, `$18B9` / 6329B, leaving 7B before `$2ABF` / `$2AC0`.
- `HIDDEN` is `$A000-$A6C7`, `$06C8` / 1736B, leaving `$0138` / 312B in the 2K common helper area.
- `LOWPACK` is `$A800-$AECD`, `$06CE` / 1742B.
- `SLOTPACK1` is `$B000-$B23A`, `$023B` / 571B.
- `BRIDGE` is `$C000-$C1FD`, `$01FE` / 510B, leaving 2B before `$C200`.
- The ReadyOS contract is preserved: no BASIC-start movement, no normal BASIC RAM expansion, no shim/app changes for this fix, and no ReadyBASIC private assumptions in `$C800-$C9FF` beyond the shim ABI.

## 2026-06-11 final proof refresh

- `make readybasic-plugin-static-check readybasic-memory-report`: passed; `verify_readybasic_plugin.py` reported `readybasic plugin static check OK`; `docs/readybasic_memory_diagrams.html` was regenerated from `obj/readybasic.map`.
- Regular D81 focused chain:
  - `/bin/bash build_support/run_readybasic_reuviewer_f2_chain_probe.sh` passed, manifest `logs/vice_auto_20260611_002202/manifest.json`.
  - `READYBASIC_HOTKEY_INPUT_MODE=keylog /bin/bash build_support/run_readybasic_reuviewer_f2_chain_probe.sh` passed, manifest `logs/vice_auto_20260611_002247/manifest.json`.
- Regular D81 aggregate: `make readybasic-vice-suites` passed. Manifest audit for `002428`, `003503`, `003556`, `003621`, `003719`, `003921`, `004030`, `004045`, `004136`, `004219`, `004254`, `004334`, `004555`, and `004745` found `status=success`, `failed_step=null`, and `degraded_steps=[]` for every manifest.
- EasyFlash focused chain after launcher-navigation adaptation passed, manifest `logs/vice_auto_20260611_025528/manifest.json`.
- EasyFlash aggregate: `make easyflash-readybasic-vice-suites` rebuilt the cartridge SKU and completed. Successful final/retry manifests `025643`, `030852`, `031001`, `031041`, `031153`, `031410`, `031535`, `032434`, `032519`, `032701`, `032807`, `032906`, `032948`, `033638`, `033728`, `034003`, and `034208` all have `status=success`, `failed_step=null`, and `degraded_steps=[]`.
- EasyFlash produced two pre-app boot/preload flake manifests during the final aggregate (`031604`, `033016`), both stuck before ReadyBASIC at `READYOS EASYFLASH BOOT` / `BOOTER PRELOADING REU SNAPSHOTS`; suite retries passed with the successful manifests above. These did not exercise ReadyBASIC app code.
- EasyFlash chain generation now keeps the chain launcher-owned, with no `--start-app readybasic` wrapper. The converter adapts only the chain's first launcher navigation to the EasyFlash app order: ReadyBASIC is four cursor-downs from the top, while REU Viewer remains three down from ReadyBASIC.
- `run_easyflash_vice_suites.sh` now has configurable plan start/retry pauses (`EASYFLASH_PLAN_START_PAUSE_S`, `EASYFLASH_PLAN_RETRY_PAUSE_S`) to reduce VICE/EasyFlash cold-start flakiness without changing boot, launcher, shim, or app code. The default first-attempt pause now matches the proven 20s retry pause.

## 2026-06-11 remaining failure to reproduce

- The newest keyboard regression probe added partial prompt-line coverage that is stricter than the earlier focused hotkey suites.
- Latest failed run: `logs/vice_auto_20260611_153009`, plan `readybasic_keyboard_regression_probe_easyflash`, failed step `wait_editor_after_partial_f2`.
- What passed before that failure: ReadyBASIC launch, CHRIN/KEYLOG vector checks, warp-mode `PRINT "A B C"` spacing, partial-line `10` plus Ctrl+B to launcher, no visible or listed `REM`, ReadyBASIC reentry/liveness, Editor launch, and Editor Ctrl+B back to launcher.
- Remaining unresolved case: after an Editor round trip, type `20` at the ReadyBASIC prompt without Return, then press F2. Expected result is an immediate switch to Editor; latest EasyFlash automation timed out waiting for `editor`.
- Reproduction details are recorded in `agentworking/readybasic_hotkeys_fix/reproduce_remaining_failure.md`.

## 2026-06-12 launcher-owned Editor/REU Viewer proof refresh

- Supersedes the 2026-06-11 remaining-failure note for the regular D81 path. The root cause was prompt detection after `RUN`: BASIC could be visibly at `READY.` while `CURLIN/TXTPTR` still pointed into the just-run program, so KEYLOG rejected Ctrl+B/F2/F4. ReadyBASIC now hooks IMAIN `$0302/$0303` to mark the direct prompt active and clears that flag at execute entry.
- The focused launcher-cycle probe now uses only launcher-owned app loading and exactly these companion apps: Editor, REU Viewer, and ReadyBASIC. It does not preload ReadyBASIC and does not use ReadyShell.
- Test shape:
  - Boot regular D81 to the launcher.
  - Select Editor and press F3 to load it to REU.
  - Return to launcher, select REU Viewer, and press F3 to load it to REU.
  - Return to launcher, select ReadyBASIC, and launch it.
  - Enter a one-line BASIC program, `LIST`, then run two full F2 cycles: ReadyBASIC -> Editor -> REU Viewer -> ReadyBASIC, twice, with `$C834`, `$C6`, screen text, capture, and post-delay stability assertions at every hop.
  - Run two full F4 cycles: ReadyBASIC -> REU Viewer -> Editor -> ReadyBASIC, twice, with the same assertions.
  - Return to ReadyBASIC, `LIST`, `RUN`, Ctrl+B to launcher, explicitly re-enter ReadyBASIC, `LIST`, `RUN`, then typed `EXIT`; assert the launcher remains stable and does not auto-reenter an app.
- Focused regular D81 result: `READYBASIC_HOTKEY_BOOT_MODE=launcher READYBASIC_HOTKEY_SCENARIO=launcher_cycle /bin/bash build_support/run_readybasic_hotkey_probe.sh` passed, manifest `logs/vice_auto_20260612_001111/manifest.json`, `status=success`, `failed_step=null`, `degraded_steps=[]`.
- Fresh focused regular D81 rerun for the Editor + REU Viewer acceptance path passed all 161 steps after a launcher-mode rebuild, manifest `logs/vice_auto_20260612_005705/manifest.json`, `status=success`, `failed_step=null`, `degraded_steps=[]`.
- Keyboard regression result: `READYBASIC_SKIP_BUILD=1 READYBASIC_KEYBOARD_BOOT_MODE=launcher /bin/bash build_support/run_readybasic_keyboard_regression_probe.sh` passed, manifest `logs/vice_auto_20260612_001321/manifest.json`, `status=success`, `failed_step=null`, `degraded_steps=[]`. This includes warp-mode single-space typing, `10`+Ctrl+B, `20`+F2, no `REM`, normal typing after reentry, and app liveness after switch.
- REU Viewer chain result after removing stale harness assumptions: `READYBASIC_SKIP_BUILD=1 /bin/bash build_support/run_readybasic_reuviewer_f2_chain_probe.sh` passed, manifest `logs/vice_auto_20260612_003840/manifest.json`.
- Remaining regular ReadyBASIC suite targets after the harness correction also passed: cross-app resume `logs/vice_auto_20260612_003938/manifest.json`, second-entry editor `logs/vice_auto_20260612_004200/manifest.json`, and full visual verification `logs/vice_auto_20260612_004350/manifest.json`.
- Static/memory guardrails: `make readybasic-plugin-static-check readybasic-memory-report` passed. Current map: `BASIC_START=$2AC1`, empty BASIC free bytes `30013`, `ENTRY=$1000-$11FF` / `$0200`, `RESIDENT=$1200-$2ABD` / `$18BE`, `HIDDEN=$A000-$A6E9` / `$06EA`, `LOWPACK=$A800-$AECD` / `$06CE`, `SLOTPACK1=$B000-$B23A` / `$023B`, `BRIDGE=$C000-$C1FE` / `$01FF`.
- Scope stayed within ReadyBASIC source, ReadyBASIC harness/docs/notes, and generated artifacts. No shim, boot, launcher, or other app source change was required for this fix.

## 2026-06-12 should be done working checkpoint

- Status: should be done working. This branch now has the ReadyBASIC prompt hotkeys implemented, documented, and proved by the regular and EasyFlash ReadyBASIC VICE suites.
- Final regular proof: `make readybasic-vice-suites` passed with 15 ReadyBASIC manifests, all `status=success`, `failed_step=null`, and `degraded_steps=[]`.
- Final EasyFlash proof: `make easyflash-readybasic-vice-suites` passed with 18 final ReadyBASIC manifests, all `status=success`, `failed_step=null`, and `degraded_steps=[]`.
- Final focused hotkey proof includes the launcher-owned Editor + REU Viewer + ReadyBASIC path, not ReadyShell and not a single preloaded ReadyBASIC shortcut. It loads Editor and REU Viewer, launches ReadyBASIC from the launcher, writes/lists/runs a one-line BASIC program, cycles F2 through ReadyBASIC -> Editor -> REU Viewer -> ReadyBASIC twice, cycles F4 the other direction twice, proves `$C834`, `$C6`, screen text, screenshots/captures, and delayed stability at each hop, then checks Ctrl+B, explicit reentry, `LIST`, `RUN`, and typed `EXIT` launcher stability.
- The REU Viewer chain proof covers the original manual failure family: ReadyBASIC Ctrl+B, ReadyBASIC reentry, Ctrl+B again, REU Viewer launch, REU Viewer F2 back into ReadyBASIC, ReadyBASIC keyboard liveness, ReadyBASIC F2 back to REU Viewer, REU Viewer Ctrl+B to launcher, ReadyBASIC reentry/liveness, and `EXIT` not auto-reentering ReadyBASIC.

### Regressions and false fixes that mattered

- Waiting for the word `READY.` alone was not a proof of ReadyBASIC readiness because boot screens and transitional screens can contain `READY.`. First entry may use the ReadyBASIC title, but warm entries and EasyFlash conversion must prove readiness with the BASIC prompt plus app-bank/vector/keyboard liveness checks.
- Waiting for the ReadyBASIC title in EasyFlash was also wrong: the title can be visible while free bytes still show `MMMM BASIC BYTES` and BASIC is not ready for typed input. The EasyFlash converted plans now wait for the actual `READY.` prompt before entering BASIC text.
- The visible `REM` deferred-dispatch path was a bad design. It could appear on screen, become part of a partial prompt line such as `10`, and lock or store unintended BASIC text. The final path queues only Return and dispatches from the CHRIN hook before BASIC stores or executes the partial line.
- A prompt-level CINV/IRQ matrix scanner was too invasive. It disturbed normal ROM-editor ownership of repeat timing and could make VICE fast/warp typing produce repeated spaces. The final design leaves ordinary key scanning/repeat/cursor editing to the ROM editor and accepts hotkeys through KEYLOG plus CHRIN only at the direct prompt.
- Treating F2/F4 as just another typed character was insufficient. F-keys and Ctrl+B live in the C64 keyboard matrix/KERNAL preprocessing path, not the normal BASIC line text stream. The final tests exercise the ReadyBASIC KEYLOG/CHRIN hotkey path through monitor/keylog stubs and app keyboard input, not by poking `rb_hotkey_pending` or typing `EXIT`.
- The selected F2/F4 target bank must survive suspend/yield preparation. The fix stores the selected bank immediately after the loaded-app scan and writes the shim switch target after `prepare_shim_yield`, instead of relying on scratch bytes that hidden save logic can reuse.
- A single F2/F4 must not be observed twice. Entry-time hotkey quarantine and yield-time release waits are required so a held key does not switch ReadyBASIC to the next app and then immediately switch again. The focused probes now assert delayed stability and `$C6==0` after every hotkey switch.
- Prompt state cannot be inferred only from `CURLIN`. After `RUN`, BASIC may visually be at `READY.` while interpreter pointers still make the prompt look like program context. The final implementation uses the IMAIN vector to mark direct-prompt readiness and clears that flag at execute entry.
- Ctrl+B/F2/F4 must be ignored while BASIC is running. The final gate is keyboard-device and direct-prompt aware, so file/device input and RUN-mode BASIC execution do not become ReadyOS navigation.
- Every ReadyBASIC yield path must scrub editor/KERNAL state: `rb_hotkey_pending`, `$C6`, `$0277`, `SHFLAG`, `LSTX`, and `SFDX`. Typed `EXIT`, Ctrl+B, F2, and F4 all need the same cleanup discipline.
- ReadyBASIC must restore global page-3 vectors before yielding to ReadyOS. Those vectors are not part of the app snapshot, so leaving CHRIN/KEYLOG/execute/crunch hooks installed can break the launcher or next app.
- Cartridge/EasyFlash app order is different from the regular D81 focused set. The cartridge F2/F4 tests should walk a few apps forward and back and assert actual screens/banks, not assume a small two- or three-app universe or try to cycle the entire preloaded cartridge list.
- EasyFlash cold preload can occasionally stall before the launcher at `READYOS EASYFLASH BOOT` / `BOOTER PRELOADING REU SNAPSHOTS`. That is a harness/VICE cold-start issue before ReadyBASIC code runs. The suite now gives launcher waits and wrapper timeouts enough budget and retries whole plans after a cold-start pause.

### Final design rules for future BASIC extensions

- For prompt hotkeys, do not inject command text. Consume the physical/editor hotkey, queue Return only, and make the CHRIN hook discard the editor-returned character before BASIC can store or execute a partial line.
- Keep ROM editor behavior intact. Do not install an IRQ scanner for ordinary prompt handling unless a future test proves there is no other option and then proves normal repeat/space/cursor behavior under warp.
- Treat partial prompt lines as first-class acceptance cases. `10`+Ctrl+B and `20`+F2/F4 must not create program lines, show `REM`, lock the prompt, or leave the destination app keyboard-dead.
- Prove app switching with launcher-owned multi-app journeys. Load companion apps through the launcher, switch both directions more than once, and assert no double-hop after a single F2/F4.
- For ReadyBASIC specifically, every hotkey test must prove all of: app bank `$C834`, vector ownership/restoration, empty keyboard buffer `$C6`, screen identity, delayed stability, and typed input liveness after reentry.
- Keep the memory contract visible in every final proof: `BASIC_START=$2AC1`, empty BASIC free bytes `30013`, bridge below `$C200`, no private use of `$C800-$C9FF`, and no shim/launcher/other-app changes unless a focused repro proves a contract bug outside ReadyBASIC.
