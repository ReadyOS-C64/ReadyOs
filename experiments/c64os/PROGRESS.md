# Experiment progress — 2026-09-25

The initial goal turn made progress: branch, verified backup, real native C64
OS partition test, normal ReadyOS boot with IDE64, and a working native
whole-RAM transfer gate. The full two-OS objective remains active and incomplete.

## Authoritative working state

- Branch `experiment/c64os-readyos-bridge` from `f505038`.
- Private environment `../c64os-readyos-experiment`, baseline and runtime copies
  plus SHA-256 manifest. Original `../c64os` was not edited.
- The runtime HDD has a deliberately local 32-bank capacity cap in rec.lib.o.
  Installation/readback evidence: `reu-cap-install.json`.
- Latest regular D81 at this checkpoint: `readyos-v0.5c-d81.d81`, with skip 39,
  built by `/bin/bash experiments/c64os/run.sh --build-only`.
- Running ReadyOS test process was launched through the normal run.sh wrapper;
  binary/text monitors 6511/6611. Inspect its live state before using it again.
  The standalone C64 OS preparation process on 6510/6610 has exited.
- Generated release, bin and version-file changes in the checkout are from
  the normal build flow. Pre-existing untracked Ultimate releases and uZIP
  build output were present before this work; do not remove them.

## Tests and discoveries

1. Native C64 OS boot baseline via copied `run-vice.sh ide64`, correct copied
   `idedos20190819-c64.crt`, 16 MB REU. Desktop screenshot + VSF recorded.
2. `python3 experiments/c64os/cap_native_reu.py`: native LOAD matches known
   detector, SAVE changes only the copied HDD, reload matches patched code.
3. CPU-executed `reu_canary.s` followed by native C64 OS LOAD/RUN: 448 pages
   outside banks 0–31 retained their patterns. Full screenshot/VSF evidence.
   The earlier monitor-only seed/fetch attempt did not execute DMA and is invalid.
4. ReadyOS build + boot via wrapper: PRG autostart failed under IDEDOS; replaced
   in the wrapper by native D81 LOAD"*",8/RUN. Fast traps returned device not
   present; true 1581 emulation worked. Launcher visible, live shim skip=$27.
5. `python3 experiments/c64os/verify_bridge_ui.py`: three destructive transfer
   cycles pass, entire shim compared, successful return to launcher. Proof
   comes from the real launcher app and keyboard-driven UI, not injected app
   execution or simulator snapshot restoration.
6. `python3 build_support/verify_release_directory_order.py --profile precog-d81`:
   PASS on v0.5c, PREBOOT first, experimental app in normal program group.
7. `git diff --check`: PASS.

## Next implementation work

The app currently exposes only the S transfer proof. Do not describe it as a
working C64 OS switcher. Extend it to cold-load C64 OS and warm-resume its saved
image. Build/install a native 1.09 utility for the reverse direction. Preserve
all of the original requested state, including interrupts, ZP, stack and shim.

The trampoline at $0400 uses a delayed 64 KB REU transfer with I/O mapped out.
Both images must contain identical trampoline instructions when fetched. It
resumes via a pointer and saved stack pointer at $0702/$0704 into native code,
which then replaces the displaced $0400–$07FF bytes and color RAM. `$FF00` is
explicitly preserved because it triggers delayed DMA. The proof intentionally
destroys ZP, stack and shim between save and fetch; remove that destructive
step from normal cross-OS operation while retaining it as a diagnostic gate.

The current wrapper does not capture CIA/VIC/SID state or arbitrate disk/network
channels, and it is not sufficient for switching OSes yet. CIA masks/timer
latches and SID registers cannot be recovered by a naive I/O read. Baseline
C64 OS uses VIC IRQ with CIA1/CIA2 masks zero and timers stopped. Investigate
IDE64 cartridge RAM/file-channel state as well as C64 main RAM.

The online utility vector documentation is stale relative to installed 1.09
headers. Verify the actual HDD utility ABI; the loose header has init, message,
quit, freeze, thaw, identity (six words). Also solve utility reopening after
resume through normal C64 OS lifecycle, rather than leaving a permanently
registered invisible utility that cannot be invoked twice.
