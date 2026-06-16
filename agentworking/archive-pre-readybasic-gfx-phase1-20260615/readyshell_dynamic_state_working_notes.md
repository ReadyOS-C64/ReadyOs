# ReadyShell Dynamic State Bank Notes

## Goal

Make ReadyShell's former fixed `$48` scratch/value/state arena loader-owned,
dynamically allocated, represented in the REU control-bank relationship data,
and unloadable by the launcher, while keeping ReadyShell's own resident impact
small.

## Design Decisions

- `$48` is no longer a fixed ReadyShell bank. `reu_mgr` only keeps the remaining
  fixed debug bank; the launcher allocates a fourth ReadyShell resource bank as
  `REU_RS_SCRATCH`.
- ReadyShell does not parse the rich bank `0` registry. The launcher seeds one
  high-RAM cache byte at `$CFF2`, and ReadyShell validates it against
  `REU_ALLOC_TABLE == REU_RS_SCRATCH`.
- `$CFF1` was rejected because ReadyShell already uses it as
  `RS_CMD_SESSION_EPOCH`.
- Former `$48xxxx` addresses are expressed as relative offsets and resolved via
  `rs_reu_state_abs()`.
- Disk launcher and EasyFlash both use the same logical shape: three ReadyShell
  overlay cache banks plus one state/scratch/value bank.
- Unload remains launcher-owned. The launcher clears rich resource records,
  zeroes ReadyShell metadata in the assigned state bank, and frees all four
  ReadyShell banks.

## Final Measured Impact

Measured with `agentworking/readyshell_dynamic_state_headroom_before.json` and
`agentworking/readyshell_dynamic_state_headroom_final.json`.

| App | Headroom before | Headroom after | Delta | Notes |
| --- | ---: | ---: | ---: | --- |
| launcher | 951 | 445 | -506 | Owns fourth bank table, resume copy, metadata commit/restore/free. |
| readyshell | 18327 | 18207 | -120 | CODE +120 only; BSS/RODATA/DATA unchanged; heap 3988 -> 3868. |
| readybasic | 1029 | 1029 | 0 | No impact. |
| reuviewer | 28781 | 28827 | +46 | Incidental shrink from regenerated/current tree. |

Binary comparison against pre-change ReadyShell snapshots:

| Binary | Before | After | Delta |
| --- | ---: | ---: | ---: |
| `bin/readyshell.prg` | 27841 | 27961 | +120 |
| `bin/readyshell_easyflash.prg` | 27739 | 27885 | +146 |

## Verification Status

Build/static verification:

- `make bin/readyshell.prg bin/readyshell_easyflash.prg easyflash` passed.
- `make verify` passed.
- `python3 build_support/report_app_headroom.py --output agentworking/readyshell_dynamic_state_headroom_final.json` passed.
- Final post-test headroom snapshot:
  `agentworking/readyshell_dynamic_state_headroom_post_tests.json`.

Regular SKU verification:

- ReadyShell host REU tests passed via `make readyshell-reu-tests-host`.
- ReadyShell regular cross-app VICE probe passed:
  `logs/vice_auto_20260605_152650/manifest.json`.
- Full regular ReadyBASIC VICE suite passed, including:
  - demo suite: `logs/vice_auto_20260605_152728/manifest.json`
  - repeat/label probe: `logs/vice_auto_20260605_153801/manifest.json`
  - lifecycle probe: `logs/vice_auto_20260605_153904/manifest.json`
  - module overlay probe: `logs/vice_auto_20260605_153939/manifest.json`
  - plugin command probe: `logs/vice_auto_20260605_154046/manifest.json`
  - program probe: `logs/vice_auto_20260605_154258/manifest.json`
  - state probe: `logs/vice_auto_20260605_154442/manifest.json`
  - large vars probe: `logs/vice_auto_20260605_154545/manifest.json`
  - cross-app resume probe: `logs/vice_auto_20260605_154641/manifest.json`
  - second-entry editor probe: `logs/vice_auto_20260605_154913/manifest.json`
  - full visual verification: `logs/vice_auto_20260605_155114/manifest.json`

Cartridge/EasyFlash verification:

- Full EasyFlash VICE suite passed via `make easyflash-vice-suites`.
- One EasyFlash boot stalled during the first `readybasic_large_vars_probe`
  attempt at the booter preload screen. The suite's cold-start retry reran the
  same plan and it passed completely. Failed first attempt:
  `logs/vice_auto_20260605_162211/manifest.json`.
- Successful EasyFlash manifests include:
  - ReadyBASIC demo suite: `logs/vice_auto_20260605_155931/manifest.json`
  - repeat/label probe: `logs/vice_auto_20260605_161127/manifest.json`
  - lifecycle probe: `logs/vice_auto_20260605_161236/manifest.json`
  - module overlay probe: `logs/vice_auto_20260605_161316/manifest.json`
  - plugin command probe: `logs/vice_auto_20260605_161428/manifest.json`
  - program probe: `logs/vice_auto_20260605_161645/manifest.json`
  - rbtest1 probe: `logs/vice_auto_20260605_161810/manifest.json`
  - resume-min probe: `logs/vice_auto_20260605_161838/manifest.json`
  - screen REU temp probe: `logs/vice_auto_20260605_161923/manifest.json`
  - state probe: `logs/vice_auto_20260605_162104/manifest.json`
  - large vars retry: `logs/vice_auto_20260605_163046/manifest.json`
  - cross-app resume probe: `logs/vice_auto_20260605_163144/manifest.json`
  - second-entry editor probe: `logs/vice_auto_20260605_163419/manifest.json`
  - full visual verification: `logs/vice_auto_20260605_163623/manifest.json`
  - ReadyShell cross-app probe: `logs/vice_auto_20260605_164427/manifest.json`
