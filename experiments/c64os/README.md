# ReadyOS / C64 OS experiment

Branch: `experiment/c64os-readyos-bridge`, starting at `f505038`.

This is work in progress, not a working switcher yet. Completion requires a
ReadyOS launcher app, a native C64 OS utility, persistent state in both
directions, and repeated real UI round trips with the regular D81 on IDE64.

## Private environment

The sibling directory `../c64os-readyos-experiment` contains:

- `baseline/`: complete untouched copy of `../c64os` (7,230 files).
- `backup-manifest.json`: sizes and SHA-256 for every baseline file, verified
  against the source before starting VICE. No source VICE process was running.
- `runtime/`: separate working copy. All experimental HDD mutations belong here.
- `baseline.png`, `baseline.vsf`: C64 OS boot evidence from the working copy,
  with IDE64 and a 16 MB REU. The snapshot is diagnostic evidence, not part of
  the implementation of the OS switch.

Licensed OS files, HDD images and ROMs stay outside this repository.
The original environment and baseline must not be used as writable test media.
The original `vice.local.env` contains absolute paths to the original tree;
do not source it in the experiment.

## REU layout (boot partition verified; snapshots under development)

| Physical banks | Owner |
| --- | --- |
| 0–31 | C64 OS, including its workspace, fast app slots and app expansion |
| 32 | ReadyOS 64 KB machine RAM snapshot |
| 33 | ReadyOS color RAM, hardware state and trampoline scratch backup |
| 34 | C64 OS 64 KB machine RAM snapshot |
| 35 | C64 OS color RAM, hardware state and trampoline scratch backup |
| 36–38 | Bridge control / reserved |
| 39 | ReadyOS launcher and allocator metadata (`reu_bank_skip=39`) |
| 40–255 | ReadyOS dynamic apps and resources |

The ReadyOS control bank's existing reserved area is `$FD40-$FFFF` (704 bytes).
Use a defined suballocation there for bridge metadata; the complete 1 KB shim
does not fit in that tail. The whole-machine RAM image includes the entire shim.
Do not steal space from launcher snapshot or the schema-v5 catalog/allocator.

The existing ReadyOS skip mechanism supports at most 39. C64 OS fast app slot
configuration alone does **not** limit its REU capacity: application expansion
can use the remainder. The experiment needs an early capacity cap, before
`reuboot.o` and application bank allocation. Both systems must reject a missing
or undersized REU. Probing must preserve occupied banks.

## Evidence and constraints

- Canonical ReadyOS shim: `src/boot/readyos_shim.inc`. The `src/shim` modules
  and `src/lib/ready_os.h` are retired, not the active ABI.
- ReadyOS saves only `$1000-$C5FF` through its normal app mechanism. This is
  insufficient to suspend a whole OS. Preserve the entire `$C600-$C9FF` shim,
  zero page, CPU stack, low RAM, RAM under ROM/I/O, and color RAM separately.
- The installed loose 1.09 `os/S/app.s.S00` has six utility vector words:
  init, message, quit, freeze, thaw, identity. The online guide describes an
  older four-word layout. Verify the actual HDD version before assembling.
- Loose `io_rec.s` defines `reubanks=$0281` (0 means 256, $FF means absent),
  `appreubk=$0282`, `recshunt=$022A`. Baseline native boot reads `$0281=0` and
  `$0282=$1A`; the loose `memory.t` has a different slot configuration.
- Loose `rec.lib.o` probes and restores 8 bytes in every REU bank. Do not assume
  that the loose PC64 files match the HDD bytes; verify through native loading.
- IRQ/NMI shutdown needs `SEI`, CIA masks and pending flags, VIC IRQ mask and
  acknowledgement, plus a safe NMI vector while ROM mapping changes.
  CIA interrupt masks and timer latches, VIC raster compare, and SID write-only
  registers cannot be recovered by blindly reading the I/O block.
- IDE64 has its own RAM, open channels and device state. A main-RAM image alone
  is not evidence that suspended disk activity will resume correctly. Switch
  only at cooperative boundaries and test disk use after returning.
- Proposed common RAM trampoline must be installed identically in both images,
  back up the displaced bytes, and return to restored native code before
  replacing itself. Never DMA over currently executing code with arbitrary
  target bytes. Delayed REU transfers permit RAM-under-I/O access but require
  separate proof of CPU-port, stack and resume-PC handling.

## Sources consulted

- [C64 OS architecture](https://c64os.com/c64os/programmersguide/architecture)
- [C64 OS memory API](https://c64os.com/c64os/programmersguide/usingkernal_memory)
- [C64 OS configuration](https://c64os.com/c64os/usersguide/configuration)
- [C64 OS development environment](https://c64os.com/c64os/programmersguide/devenvironment)
- [VICE binary monitor](https://vice-emu.sourceforge.io/vice_13.html)
- Local launch and UI harnesses in `../c64os`, `../c64os-tasks`, and
  `../agenticdevharness/tools/vice_readyshell_automation.py`.

## Verified progress

- Native IDEDOS LOAD confirmed the HDD `rec.lib.o` exactly matches the loose
  file (215 code bytes, SHA-256
  `bd2af8eaf41aaf3f2993620f8f534c316d7b86ebefec5221e6792848fb9e5916`).
- `cap_native_reu.py` installed the private capacity override using native
  IDEDOS SAVE, cleared the RAM copy, reloaded the file and verified all bytes.
  The presence check remains; the detected-capacity path at `$646D` reports
  32 banks without touching higher banks. This assumes the launcher's fixed
  16 MB runtime and is not a general-purpose capacity detector.
- Capped C64 OS reached File Manager, reporting `reubanks=32`, `appreubk=26`.
- `reu_canary.s` executed on the C64 before boot; all 448 canary pages (first
  and last page of every bank 32–255, 114,688 bytes total) survived boot.
  Evidence: sibling `capped-boot-canaries.vsf`, `.png`, `.json`.
  This does not yet certify application workloads or bank interiors.
- Regular ReadyOS D81 built through `run.sh` and reached its launcher alongside
  IDE64, with live `$C83B=$27` (39). Evidence: `readyos-true-drive.png`.
- IDE64 breaks VICE PRG autostart and fast IEC traps in this setup. The wrapper
  keeps the real D81 mounted, uses native `LOAD"*",8` / `RUN`, and enables true
  1581 emulation. It still delegates build and launch to the normal `run.sh`.
- C64 OS idle CIA1/CIA2 interrupt masks were both zero, with timers stopped;
  its raster ISR owns the tick. This is baseline evidence, not a promise about
  all applications, drivers or open I/O.
- The bridge app is built into the experimental regular D81. Its destructive
  full-RAM round trip passed three automated repeats after an initial manual
  cycle: zero page, CPU stack and all four shim pages were deliberately
  overwritten, then restored; all 1,024 shim bytes compared equal every time.
  F1 returned through the restored shim to the functioning launcher.
  Evidence: `bridge-proof.json`, `bridge-proof.png`, `bridge-proof-return.png`.
  This is a same-context transfer test, not a C64 OS handoff.

The first canary attempt used monitor register writes and did not execute DMA;
that failed attempt is explicitly superseded by the native CPU probe and VSF
readback above. Do not reuse monitor-only DMA as evidence.

## Commands

Build the experiment with the normal D81 generation/preservation flow:

```sh
/bin/bash experiments/c64os/run.sh --build-only
/bin/bash experiments/c64os/run.sh --skipbuild
python3 experiments/c64os/verify_bridge_ui.py
```

Use only one emulator at a time with the working HDD. Ports 6511/6611 belong
to this wrapper; the separate native C64 OS installation session used 6510/6610
and has been closed. The wrapper never sources the original absolute-path env.

## Verification still required

1. Extend the proven trampoline to capture and restore each OS's hardware
   state, using defined contracts for write-only registers. Allocate/version
   the bridge record within the ReadyOS control bank's reserved tail.
2. Native HDD utility ABI verification and
   C64 OS utility installation.
3. Cold ReadyOS app → C64 OS boot, native utility → ReadyOS return.
4. Warm round trips preserving edited documents, active app, input, display,
   REU allocations and shim. Multiple cycles with disk reads/writes afterward.
5. Negative cases: missing/undersized REU, invalid bridge signature, failed
   C64 OS load; no unguarded jump into an invalid saved image.
