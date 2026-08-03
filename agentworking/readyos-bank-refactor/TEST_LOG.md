# ReadyOS Bank Refactor Test Log

## Pre-change baseline

| Gate | Result |
|---|---|
| All release-profile and EasyFlash builds | Pass |
| 512-byte shim verification | Pass |
| Old resume-contract verification | Pass |
| Schema-v4 control-bank verification | Pass |
| ReadyBASIC aggregate suite | Pass after two clean reruns for baseline flakes |
| ReadyBASIC ten-cycle cross-app state preservation | Pass, 212/212 |

Post-change results will be appended here with exact commands, manifests, first
failing step, and rerun classification.

## Post-change build and static gates

| Command / gate | Result |
|---|---|
| `/bin/bash ./run.sh --build-all` (final EasyFlash cleanup, build `0.2.5D`) | Pass, exit 0 |
| `verify_readyos_shim.py --check-easyflash-bin` | Pass; exact 512-byte disk/EasyFlash shim |
| resume, schema-v5, dynamic-launcher, memory-map, ReadyBASIC shape verifiers | Pass before final full rebuild; repeat pending below |
| HTML classification and canonical full-shim-source verifier | Pass; 34 HTML documents classified, 5 full listings byte-exact |
| `python3 verify.py --profile precog-d81` | Pass; `ALL CHECKS PASSED` after updating obsolete schema-v4/C64-mirror assertions |
| `make easyflash-smoke` | Pass; loader preload markers, launcher entry/UI, persisted 16 MiB REU image, schema-v5 header, ReadyOS bank type, and every EasyFlash token mapping/status verified |

## EasyFlash smoke hardening findings

- An initial monitor-only schema fetch did not execute because EasyFlash owns
  the `$DFxx` I/O window while monitor-injected code is active.  The gate now
  persists VICE's raw REU image and verifies physical `Skip+1` directly.
- That stronger check found the binary magic was `D2 C3 C2 35`, because cc65
  target-character translation had been applied to C character literals.
  `REUCB_MAGIC0..3` now use explicit ASCII ABI bytes `52 43 42 35`; the rebuilt
  EasyFlash smoke passes with the corrected on-REU header.

## Focused post-change runtime probes

| Command / manifest | Result |
|---|---|
| `make launcher-reu-state-vice`, `logs/vice_auto_20260802_003232` | Pass before final micromodule optimization |
| `make quicknotes-owned-reu-vice`, `logs/vice_auto_20260802_004417` and `...004436` | Pass both legs; owned bank visible before unload and free afterward |
| `make readyshell-cross-app-resume-vice`, `logs/vice_auto_20260802_010521` | Pass, 33/33, no degraded steps; Editor/ReadyShell/ReadyBASIC switching and resume state stable |
| `LAUNCHER_REU_STATE_SKIP_BUILD=1 make launcher-reu-state-vice`, `logs/vice_auto_20260802_013756` | Pass, 28/28 on final exact assembly alias; unload invalidation, reload, relaunch, and ReadyBASIC stability print verified |

The first final-alias launcher attempt exposed a dropped lone RETURN after the
reload/dump sequence, not a runtime-state failure: cold launch, exit, unload,
reload, and the launcher screen had all passed. The probe now uses the same
deterministic text-monitor key-buffer path as the cross-app suite; the complete
rerun passed without degraded steps.

## Final static gate before aggregate suites

The schema-v5 control-bank verifier, dynamic launcher verifier, resume-contract
verifier, exact 512-byte shim verifier (including the then-current 104 zero
padding bytes), and
full memory-map verifier all passed against the final unique-name assembly
adapter build. Aggregate runtime results follow below.

## Final regular-D81 aggregate

All 26 targets in `make readybasic-vice-suites` passed without degraded steps.
This includes demo, repeat/labels, lifecycle, module overlay, plugin commands,
program/rbtest1, minimal and large-variable resume, screen-REU temporary data,
state, hotkeys, keyboard regression, ReuViewer F2 chain, ten-cycle cross-app
resume, second-entry Editor, graphics phases 1-5, multicolor bitmap, sprites,
sound, loaded-app coverage, and the 186-step visual suite. Salient manifests:

- ten-cycle cross-app: `logs/vice_auto_20260802_024131`, 263/263;
- second-entry Editor: `logs/vice_auto_20260802_024615`, 192/192;
- loaded apps: `logs/vice_auto_20260802_025157`, 215/215;
- full visual suite: `logs/vice_auto_20260802_025522`, 186/186.

Focused final probes also passed: launcher state 28/28 at
`logs/vice_auto_20260802_014929`; QuickNotes ownership/unload 15/15 and 17/17
at `...030335` and `...030355`; ReadyShell cross-app 33/33 at `...030440`.

## Final EasyFlash aggregate

All 19 generated EasyFlash plans passed without degraded steps. Salient
manifests are:

- ten-cycle cross-app: `logs/vice_auto_20260802_031224`, 265/265;
- demo: `logs/vice_auto_20260802_031419`, 80/80;
- full visual: `logs/vice_auto_20260802_032629`, 188/188;
- plugin commands: `logs/vice_auto_20260802_033724`, 123/123;
- authoritative ReadyOS-status F2 chain:
  `logs/vice_auto_20260802_035055`, 70/70;
- second-entry Editor: `logs/vice_auto_20260802_035317`, 194/194;
- state: `logs/vice_auto_20260802_035511`, 59/59;
- ReadyShell cross-app: `logs/vice_auto_20260802_035555`, 35/35.

The F2-chain harness was corrected to DMA its constrained token-status image
to ReadyOS-bank `$BA40`; writing the retired `$C836-$C838` bytes correctly has
no effect after this refactor. Deterministic direct-token re-entry replaced two
launcher-selection assumptions. The corrected run passed both F2 directions,
vector restoration, keyboard drainage, resume state, and exit suppression.

## Final build, documentation, and deletion gates

| Command / gate | Result |
|---|---|
| `/bin/bash ./run.sh --build-all` (`0.2.5I`) | Pass, all disk SKUs, KFF2, and EasyFlash |
| `make verify` (subsequent verification build `0.2.5J`) | Pass |
| `verify_readyos_shim.py --check-easyflash-bin` | Pass; 512 bytes; SHA-256 `7e34e228b6553b84bc47431d9938e3c53a9cc7c060f4cb26960f1b24f137e2a3` |
| `make readyshell-host-tests` | Pass |
| ReadyBASIC and ReadyShell memory-report regeneration | Pass |
| HTML status/source verification | Pass; 38 classified HTML files and 5 full shim listings (175 canonical directives each) |
| `git diff --check` | Pass |
| tracked deletion audit | Pass; zero deleted paths after restoring version-pruned release artifacts |
