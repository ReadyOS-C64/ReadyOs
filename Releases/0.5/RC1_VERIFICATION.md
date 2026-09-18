# ReadyOS 0.5 RC1 verification

Verified on 2026-09-18 from source commit `42475fd` on branch `0.5-RC1`.
RC1 is a Git milestone; the shipped version is **0.5**, with no rolling letter.

## Build and packaged assets

Full forced rebuild through the supported entry point:

```sh
/bin/bash ./run.sh --build-all --for-release
```

- All 15 SKUs rebuilt: 14 disk profiles and EasyFlash.
- All manifests and the generated version header report exactly `0.5`;
  `READYOS_VERSION_SUFFIX` is empty.
- All 53 manifest-listed outputs exist and are nonempty, including 20 disk
  images, the EasyFlash CRT/raw image, boot PRGs and cartridge layout.
- `verify.py --profile <id>` passed for every disk profile.
- `audit_release_seq_rel.py` passed byte-for-byte checks of packaged support
  files and module SEQs against their authoritative/generated sources.
- `verify_release_directory_order.py` passed for every disk image, including
  the EasyFlash companion D64.
- `readyos_easyflash.py verify-release` passed for the cartridge package.
- Regular-D81 ReadyBASIC, demos, media module and resource bytes passed exact
  readback with `verify_readybasic_gfxsnd_disks.py --profile precog-d81`.
- Release help was regenerated against the rebuilt manifests. Tracked VICE
  plans were regenerated for the unsuffixed artifacts and current symbols;
  generating a plan is not a claim that its entire test suite was executed.

## Additional checks

Passed UCI, SETUP, uZIP, resume, shim (including EasyFlash), memory-map,
REU-control-bank, dynamic-launcher, DMA-gate, ReadyBASIC-plugin and media-ABI
checks. Editor, Tasklist, Simple Files and ReadyShell host tests passed.
The built 6502 resource decoder passed all 20 simulated-KERNAL cases, including
malformed input, bounds and display cleanup.

Documentation generation, release-help reproducibility, current contracts,
local links and the five byte-verified shim HTML listings passed.

## Runtime scope

A fresh regular-D81 VICE run passed all eight smoke-test steps: ReadyOS boot,
launcher readiness, ReadyBASIC launch, `PRINT "RC1 READY";2+3`, verification of
`RC1 READY 5`, and return to the launcher. Successful local evidence is in
`logs/vice_auto_20260918_160021/manifest.json` (not shipped).

Two preliminary harness attempts used an insufficient readiness check and/or
an app-row offset that omitted the two utility rows. After correcting the
test navigation, the untouched release image passed; no runtime code was
changed to make the smoke test pass.

This is a full rebuild and packaged-asset/static/host verification, plus the
regular-D81 runtime smoke test. It is not a fresh physical Ultimate/KFF2 test
or an execution of every interactive regression suite. Existing documented
hardware and media compatibility limitations still apply.

Private documentation, local test logs, scratch files and obsolete untracked
letter-suffixed images are excluded from this release commit.
