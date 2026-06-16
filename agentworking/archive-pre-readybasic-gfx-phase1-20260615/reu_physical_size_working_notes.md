# REU Physical Size Working Notes

Date: 2026-06-06

Goal: make physical REU size a launcher/system-owned fact, mirror it into the
global REU control bank, keep the resident shim area at the existing 1KB
shape, and make REU Viewer totals correct on smaller REU sizes.

Implementation summary:

- Added shared table helpers in `src/lib/reu_phys.c` and declarations in
  `src/lib/reu_phys.h`.
- Added launcher-only destructive alias probe in `src/lib/reu_phys_probe.c`.
- Launcher probes once during startup, before dynamic app/resource allocation.
- Banks beyond the physical end are marked `REU_UNAVAIL` in `$C600-$C6FF`.
- Control bank schema is now `RCB0` v4 and publishes physical count at header
  byte `44`, first unavailable at byte `45`, and flags at byte `46`.
- REU Viewer consumes launcher-published physical size instead of probing.
- EasyFlash preload marking skips `REU_UNAVAIL`.

Memory deltas:

| App | Before | After | Delta | CODE | RODATA | DATA | BSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| launcher | 5861 | 5374 | -487 | +486 | 0 | 0 | +1 |
| launcher_easyflash | 18209 | 17757 | -452 | +451 | 0 | 0 | +1 |
| reuviewer | 29540 | 29411 | -129 | +131 | 0 | 0 | -2 |

Focused screenshot probes:

- 8MB run: `logs/vice_auto_20260606_193409`
  - showed `PHYS:128`, `FREE:83`, `CB:OK`, and `$80-$FF` unavailable.
- 16MB run: `logs/vice_auto_20260606_193505`
  - showed `PHYS:256`, `FREE:211`, and no unavailable tail.

Verification completed:

- `python3 build_support/verify_reu_control_bank.py`
- `python3 build_support/verify_dynamic_launcher.py`
- `git diff --check`
- `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 make readybasic-vice-suites`
- `READYSHELL_VISIBLE=0 build_support/run_readyshell_cross_app_resume_probe.sh`
- `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 READYSHELL_VISIBLE=0 make easyflash-vice-suites`

Result: regular and cartridge VICE suites passed. The REU Viewer impossible
free count is fixed by counting only banks below the launcher-published
physical end and by treating `REU_UNAVAIL` as outside the usable total.
