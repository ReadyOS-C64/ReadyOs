# ReadyOS ↔ C64 OS bridge experiment

Branch: `experiment/c64os-readyos-bridge` (based on `f505038`).

The ReadyOS bridge app cold-boots C64 OS from its boot device, then switches
between saved machine contexts. The native C64 OS **ReadyOS** utility returns
through its regular menu and utility lifecycle. This is a prototype for the
REU layout below, not a general C64 task switcher. IDE64 handling is conditional;
plain IEC uses ordinary KERNAL loading without cartridge operations.

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

Select **c64os bridge** in the ReadyOS launcher. The app uses the shared TUI
header, arrow selector and footer. Up/Down select an action; Return runs it.
The letter shortcuts shown at the end of each item work too:

- **C** starts C64 OS or resumes its saved context.
- **S** runs the destructive ZP/stack/shim save-and-restore proof.
- **X** attempts to load missing `COOTER`, testing recovery to ReadyOS.
- **Ctrl+B** returns to the ReadyOS launcher (F1 remains an alias).
- **F2/F4** switch to the next/previous preloaded ReadyOS app.

In C64 OS, select **ReadyOS** from the hamburger menu, or use the installed
Control+Commodore+Shift+R shortcut. A missing/invalid ReadyOS snapshot is rejected.
The utility uses a normal one-shot timer so its loader has returned before the
switch. When C64 OS resumes, the utility closes itself normally and can reopen.

## Installation

The supported packaging path is now the D81 companion `bridge.car`. It installs
native **Bridge Setup** and the **ReadyOS** return utility through C64OS Installer.
Setup checks C64OS 1.09 and the complete REU library before applying/updating a
limit or undoing it. Read [the user installation guide](../../docs/c64os/bridge.md)
for matched settings, restart requirements and recovery.

The normal D81 ships the optional `app.c64os` manifest but still skips zero banks;
it refuses to switch. This experiment's separate `apps.ini` uses skip 39 and
lists the bridge by default for tests. Bridge Setup source/build lives in the
sibling `c64os-bridge`; the D81 packager imports its CAR and verifies the native
return utility matches this repository's shared core.

`install_native_utility.py --reset` and `cap_native_reu.py` remain legacy private
preparation/debugging tools. They require the owned HDD and are not needed by a
user installing the CAR. The former replaces the test environment's Utilities
menu from its baseline; the CAR deliberately preserves the user's menu.

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
704-byte reserved tail. They hold ASCII `RBG4`, version 4, ReadyOS-valid,
C64-OS-valid, and the last cold-load error. Validity is published only after both
main RAM and any required IDE64 RAM are saved. ABI changes require updating both native sides
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
contracts: ReadyOS KERNAL timer-A IRQ, C64 OS VIC IRQ at raster one, no active
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

## Startup-state fixes and isolated testing

Build v0.5s restores the cold KERNAL keyboard capacity/repeat defaults and its
temporary translation vector, preserves PAL/NTSC across RAMTAS, and uses the
ROM's matching timer setup. Native C64OS warm resume uses raster line 1.
Cold scratch at `$0710–$072A` is inside the already saved/displaced low page;
there are no new shim fields or persistent REU control fields.

The normal round-trip suite now checks native timing and keyboard defaults.
`verify_startup_state.py` additionally uses test-only `py65`, local stock ROMs,
and the licensed keyboard driver to check actual scanning through a simulated
CIA key matrix. Disk calls are stubbed in that focused CPU test; the separate
VICE suite performs real loading and disk work. It is not a host-key UI test.

For an isolated copied HDD, launcher and primary verification/installation
tools accept `READYOS_BRIDGE_ENV`, `READYOS_BRIDGE_BINARY_PORT`, and
`READYOS_BRIDGE_TEXT_PORT`. Defaults remain the original environment and ports.
Use a separate writable HDD and D81; never run two instances against one HDD.
The environment needs the existing installation manifests, ROM/HDD support
files, and baseline menu/driver files. The utility installer verifies the
mounted HDD path before resetting or writing anything.

The updated return utility must be installed with the documented native
installer after a rebuild; merely refreshing the ReadyOS D81 does not update
the HDD's utility. Current live contexts retain their old code until a fresh
boot. See STATE_AUDIT.md for remaining quiet-I/O and hardware-state limits.

## Conditional IDE64 handling (v0.5u, ABI 4)

Both native sides use the documented stable `IDE` signature at `$DE60–$DE62`,
then validate v4.1 STD mapping at `$DE32`. The probe does not write cartridge
registers. A signature with unsupported revision/mapping is rejected. Without
the signature, context byte `$0706` is zero, and both save and restore bypass
the entire IDE64 SRAM/mapping routine. Banks 33/35 remain reserved but untouched.
Cold entry restores saved IDEDOS vectors only for IDE64; plain IEC keeps the
ROM vectors installed by KERNAL RESTOR. Shim code and REU partitioning are unchanged.

Boot-location defaults (editable with **L** in the bridge):

| Detected environment | Device | Preparation | File |
|---|---|---|---|
| IDE64 v4.1 | 12 | `CP1`, then `CD//OS` | `BOOTER` |
| No IDE64 | 8 | None; device must already be in the C64OS boot directory | `BOOTER` |

The device, partition, directory and filename are editable and saved to device
8 as `c64os.cfg`. Defaults are implemented in `src/apps/c64os/config.c`.
The native wrapper stages 117 bytes at `$0730–$07A4`, inside the displaced low
page, and the cold loader uses those values after RAMTAS. No new shim or REU
control fields are used. Saved contexts retain their old boot location until a
fresh cold boot. The app manifest `app.c64os` is separate from these settings.

Bridge Setup replaces the host-only REU patch procedure for users. It retains
presence detection, bypasses the capacity probe, and reports the selected 8–32
banks. Undo restores the original six bytes. For every selection the bridge
snapshots remain at 32–38 and ReadyOS skip remains **39**. Normal release builds
use skip **0** and show a setup-required screen instead of switching.

Every alternate C64OS installation must have the REU cap and new native return
utility prepared before bridge boot; an unpatched detector can overwrite the
ReadyOS REU region. ABI 4 makes older return utilities reject new bridge records
rather than silently mix the two implementations. Install both sides together
and use a fresh boot.

The standard VICE IDE64 regression passes cold plus six warm round trips. With
`READYOS_BRIDGE_IDE64=0`, the normal wrapper can run a separate no-cartridge
instance: `verify_noide_ui.py` passes three destructive RAM proofs and missing
BOOTER/probe recovery through ordinary device-8 I/O, checking all shim bytes
and untouched IDE64 snapshot banks. The CPU startup suite checks both routes,
seven detector cases, and both PAL/NTSC. This does not certify a complete C64OS
boot on physical SD2IEC/CMD hardware or custom ROMs we do not have.
