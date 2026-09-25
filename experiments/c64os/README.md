# ReadyOS ↔ C64 OS bridge experiment

Branch: `experiment/c64os-readyos-bridge` (based on `f505038`).

The ReadyOS bridge app cold-boots C64 OS from the copied IDE64 HDD, then switches
between saved machine contexts. The native C64 OS **ReadyOS** utility returns
through its regular menu and utility lifecycle. This is a prototype for the
fixed environment below, not a general C64 task switcher.

## Environment and launch

The private sibling `../c64os-readyos-experiment` contains an untouched `baseline`
copy of the original C64 OS environment, a separate writable `runtime` copy,
and `backup-manifest.json` with verified sizes/SHA-256 for all 7,230 original
files. Licensed C64 OS files, cartridge ROM and HDD are outside this repository.
The original `../c64os` and the baseline have not been modified.

Build and launch the regular ReadyOS D81 through the normal build/run flow:

```sh
/bin/bash experiments/c64os/run.sh --build-only
/bin/bash experiments/c64os/run.sh --skipbuild
```

The wrapper delegates to the root `run.sh --profile precog-d81`, adds the copied
IDE64 v4.1 cartridge/HDD, sets a 16 MB REU, and mounts an owned runtime D81.
It boots `LOAD"*",8` / `RUN` through PREBOOT and launcher. PRG autostart and fast
IEC traps do not work with this IDEDOS setup; the wrapper uses true 1581 emulation.
Only one process may use the writable HDD. Its PID is in `readyos/vice.pid`;
monitor ports are 6511 (binary) and 6611 (text). A changed build refreshes the
runtime D81 after backing up its previous contents under `readyos/runtime-backups`.
An unchanged build retains the writable runtime disk.

Select **C64 OS bridge** in the ReadyOS launcher. The app uses the shared TUI
header, arrow selector and footer. Up/Down select an action; Return runs it.
The letter shortcuts shown at the end of each item work too:

- **C** starts C64 OS or resumes its saved context.
- **S** runs the destructive ZP/stack/shim save-and-restore proof.
- **X** attempts to load missing `!OOTER`, testing recovery to ReadyOS.
- **Ctrl+B** returns to the ReadyOS launcher (F1 remains an alias).
- **F2/F4** switch to the next/previous preloaded ReadyOS app.

In C64 OS, select **ReadyOS** from the hamburger menu, or use the installed
Control+Commodore+Shift+R shortcut. A missing/invalid ReadyOS snapshot is rejected.
The utility uses a normal one-shot timer so its loader has returned before the
switch. When C64 OS resumes, the utility closes itself normally and can reopen.

## Native utility installation

The copied HDD already has the utility, corrected menu and 32-bank REU cap.
After rebuilding changed utility code, close any other experiment VICE, launch
the wrapper above, then run:

```sh
python3 experiments/c64os/install_native_utility.py --reset
```

This explicitly resets the verified owned emulator to BASIC, installs only the
experimental utility and menu on its copied HDD using native IDEDOS file I/O,
and reads both back. It validates the menu's encoded child counts. It leaves
VICE at BASIC; close that instance and launch the wrapper again. Do not install
into the original HDD. The installer is preparation tooling, not a dependency
of a running switch: neither direction uses host snapshots or monitor execution.

`cap_native_reu.py` originally installed the local capacity override in the HDD's
`rec.lib.o`. The presence check remains; capacity detection reports 32 banks
without probing the ReadyOS range. It is deliberately specific to this 16 MB
experiment. The original 215-byte detector was verified by native LOAD before
patching, and the patched file was reloaded and compared afterward.

## Memory ownership

| Physical REU banks | Owner |
| --- | --- |
| 0–31 | C64 OS workspace, fast app slots and application expansion |
| 32 | ReadyOS whole 64 KB RAM image |
| 33 | ReadyOS IDE64 DOS RAM, offsets `$1000–$7FFF` |
| 34 | C64 OS whole 64 KB RAM image |
| 35 | C64 OS IDE64 DOS RAM, offsets `$1000–$7FFF` |
| 36–38 | Reserved for bridge extensions |
| 39 | ReadyOS launcher/control bank (`reu_bank_skip=39`) |
| 40–255 | ReadyOS apps and resources |

The bridge owns just **eight bytes at bank 39, `$FD40–$FD47`**, inside the existing
704-byte reserved tail. They hold ASCII `RBG3`, version 3, ReadyOS-valid,
C64-OS-valid, and the last cold-load error. Validity is published only after both
main RAM and IDE64 RAM are saved. ABI changes require updating both native sides
and the record version; do not combine old saved contexts with changed cores.

The entire `$C600–$C9FF` shim lives in the ReadyOS RAM image. The common trampoline
executes at `$0400–$07FF`, stages IDE64 RAM through `$0800`, and performs delayed
64 KB REU DMA with I/O mapped out. Native wrappers preserve/restore the displaced
`$0400–$08FF` bytes, color RAM, readable VIC/CIA state, IRQ/NMI vectors, CPU ports,
registers, flags, stack and REU registers. Each restored context supplies its own
resume pointer and stack pointer. The C64 OS utility and BSS are link-bounded
below `$F71C`, where native `util.frame` begins.

IDE64's 28 KB cartridge RAM contains DOS buffers and channel state. It is copied
through the documented OPEN/STD mapping using CPU loads/stores; the design does
not assume REU DMA can directly access cartridge SRAM. The cold loader recreates
KERNAL/BASIC workspace while retaining IDEDOS's patched I/O vectors.

## Verification

With a fresh normal wrapper boot and the current native utility installed:

```sh
python3 experiments/c64os/verify_tui_ui.py  # fresh boot; also preloads demo apps
# Use another fresh boot for the full disk/partition regression:
python3 experiments/c64os/verify_roundtrip_ui.py
python3 build_support/verify_release_directory_order.py --profile precog-d81
```

The regression exercises three destructive RAM proofs, a cold round trip, six
warm round trips, and failed cold loads before/after a saved C64 OS context.
It compares all 1,024 shim bytes, checks native utility close/reopen, preserves
unsaved ReadyOS editor text, saves/reopens `BRG25TEST` through native D81 dialogs,
and reads the D81 through C64 OS File Manager. It compares the **entire 2 MB**
C64 OS REU partition across ReadyOS app/disk work and **all 217 ReadyOS banks**
across C64 OS activity. It also checks the native File Manager view after resume.

ReadyOS input uses VICE keyboard feed. C64 OS tests enqueue documented mouse/key
UI events, handled by the normal event loop/menu dispatcher. No application
functions or program counters are injected. Diagnostic VSFs are read only for
comparison; they are never restored to implement or fake a switch.

The separate missing-snapshot guard test starts at fresh BASIC after the
installer reset, before entering the ReadyOS bridge:

```sh
python3 experiments/c64os/verify_utility_guard.py
```

It boots native C64 OS, invokes the installed utility, verifies rejection and
normal UI return, and compares every REU byte outside banks 0–31. The initial
partition boot probe also verified 448 boundary canary pages in banks 32–255.

To check the unsupported-size guard, close the owned emulator, boot with
`/bin/bash experiments/c64os/run.sh --skipbuild --reu-size 4096`, then run
`python3 experiments/c64os/verify_rejection_ui.py`. The test verifies the actual
4 MB resource value, rejection message and return to launcher. Close that
instance and restart with the default 16 MB configuration afterward.

Evidence stays in the private sibling environment: `roundtrip-ui.json`,
`utility-guard.json`, `rejected-4mb.json`, `native-utility-install.json`,
`final-original-integrity.json`, screenshots and VSFs. Final integrity checks
matched all 7,230 files in both the original and baseline against the backup
manifest. `verification.json` here records the portable result summary.
`roundtrip-final.d81`, captured after VICE exited, also passed host readback of
`brg25test,s`: exactly `BRIDGE STATE SEPTEMBER 25` followed by CR.
`editor-after-roundtrip-flushed.d81` independently contains the earlier native
save/reopen test (`brg0925a,s`, content `BRIDGE STATE SEPTEMBER 25` plus CR).
A live true-drive image can have unflushed tracks; use native reopening or an
image captured after VICE exits when checking disk contents from the host.

## Supported boundary and limits

This has been exercised in VICE with quiet text-mode ReadyOS Editor/launcher and
C64 OS File Manager. Switch from normal foreground UI callbacks after file I/O
finishes. Arbitrary active disk transfers and shared open IEC channels are not
suspended safely by a RAM snapshot alone.

CIA interrupt masks and timer reload latches, VIC raster compare, and SID
registers cannot all be read back. This prototype uses explicit standard-mode
contracts: ReadyOS KERNAL timer-A IRQ, C64 OS VIC IRQ at raster zero, no active
CIA2 timer/serial workload. Timers restart; it is not cycle-exact emulation.
CIA time-of-day continues. SID is silenced, not restored: music playback and
applications owning custom IRQ/CIA/SID drivers remain unverified. Physical
Ultimate hardware, other IDE64 revisions and other REU sizes are not certified.

## Protocol references

- [C64 OS service API](https://c64os.com/c64os/programmersguide/usingkernal_service)
- [C64 OS screen API](https://c64os.com/c64os/programmersguide/usingkernal_screen)
- [C64 OS memory API](https://c64os.com/c64os/programmersguide/usingkernal_memory)
- [IDE64 programmer details](https://ide64.org/ide_fix.html)
- [IDE64 guide](https://ide64.org/guide.html)
- [VICE IDE64 implementation](https://github.com/VICE-Team/svn-mirror/blob/main/vice/src/c64/cart/ide64.c)
- [VICE binary monitor](https://vice-emu.sourceforge.io/vice_13.html)

The installed native binaries/headers are authoritative where older web docs
conflict, notably the six-vector utility ABI. See `PROGRESS.md` for failed
experiments and the evidence that supersedes them.
