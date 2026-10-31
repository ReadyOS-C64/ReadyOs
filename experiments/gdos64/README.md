# GDOS64 bridge experiment

The stock single-D81 release excludes bridges. The private lab still builds a
custom regular D81 using `experiments/gdos64/apps.ini`; that explicit catalog
activates the GDOS payload. Stock Ultimate instead carries optional drive-9
bridge apps/manifests, while `precog-ultimate-gdos` enables switching with skip71.
The native accessory is built from `accessory.s` and shipped as a companion CVT.

The initial supported test environment is GDOS64 English 1.03 build 260511,
a plain PAL C64, 16 MiB Commodore-compatible REU, and two native C1581 drives.
GDOS uses a 4 MiB DACC. External C=REU RAMNative drives, cartridges, active
spooling, music and running CIA timers are outside this experimental contract.
This is emulator-tested development software, not a general hardware release.

## Memory ownership

| Physical REU banks | Owner |
| --- | --- |
| 0–63 | GDOS DACC: 4 MiB, including its native kernel/data banks |
| 64 | ReadyOS full 64 KiB C64 RAM snapshot |
| 66 | GDOS full 64 KiB C64 RAM snapshot |
| 65, 67–70 | Bridge reserve |
| 71 | ReadyOS control bank; bridge record at `$FD40` |
| 72–255 | ReadyOS application banks |

The regular release keeps `reu_bank_skip=0`. The stock Ultimate bridge shows a setup-required
screen and refuses to switch. Only the experiment configuration uses skip 71.
GDOS's native 4 MiB setting and ReadyOS's skip 71 are a matched pair; arbitrary
sizes are not supported by this first implementation. No shim code or public
shim ABI changed. Build-time skip validators were extended to 71.

## Build and run the private lab

From the ReadyOS checkout:

```sh
/bin/bash ./run.sh --profile precog-d81 --config experiments/gdos64/apps.ini --for-release --build-only
python3 experiments/gdos64/tools/build.py
/bin/bash experiments/gdos64/run.sh
```

The native build caches the experimental ReadyOS disk in the sibling project.
The launcher copies both disks into that project's private `runtime` directory;
the original GDOS lab and its interactive VICE instance are left alone. Ports
6540/6640 belong exclusively to this experiment. Native GDOS boots on device 9;
ReadyOS and `gdos64.cfg` are on device 8. The bridge's location screen can edit,
save and verify device, partition, directory and boot filename. Location changes
take effect on the next cold boot, not on a warm resume.

The original lab backup is
`../backups/GDOS64-pre-bridge-20260926-195800.tar.gz`, with checksum metadata beside
it. Rebuilding native test media intentionally replaces only private copies.

## Transition boundary

The native **ReadyOS Bridge** desk accessory runs from GDOS's normal accessory
menu. It checks the GDOS revision, REU allocation, drive types, timers, spooler
and ReadyOS session signature before offering a return. It waits for button/key
release and uses native `PurgeTurbo` for drive handoff. The transport restores
RAM, stack, zero page, mapping, color RAM and the supported hardware state.
It returns through GDOS `RstrAppl`, preserving the displaced caller.

The accessory saves GDOS's 417-byte `dlgBoxRamBuf` before displaying its own
dialog, then restores it before `RstrAppl`, following GDOS's own help accessory.
Without this, nested dialogs overwrite the caller's saved vectors and graphics
state. `GEOS_InitSystem` must not be called at this boundary. A forced redraw is
not a substitute for preserving that state.

## Verification and recording

With a fresh private session, run `experiments/gdos64/tools/test_roundtrip.py` using
a Python installation with Pillow. It tests destructive RAM recovery, missing
boot recovery, cold and warm transitions, native geoWrite editing after resume,
ReadyOS editor preservation, rendering, and byte-for-byte REU ownership.
`tools/record_demo.py` in the sibling project records the same real UI workflow
with paced typing. Reports and video are under its ignored `evidence` directory.
VICE snapshots are dumped only to inspect REU contents; they are never restored
to perform a transition. GDOS inputs use its native input queues, so these tests
do not independently certify host keyboard or physical mouse drivers.

Restore stock release artifacts after caching the experimental disk:

```sh
/bin/bash ./run.sh --profile precog-d81 --for-release --build-only
python3 build_support/verify_release_directory_order.py --profile precog-d81
```
