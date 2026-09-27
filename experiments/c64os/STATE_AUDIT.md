# Bridge state audit — 2026-09-25

Update: the concrete startup omissions and raster-line mismatch below were
subsequently fixed in build v0.5s. See PROGRESS.md, "Startup and native-resume
corrections", for new test evidence and deployment status. The original audit
below records the pre-fix findings; the broader hardware limits still apply.

Read-only audit of the switching implementation, installed C64OS 1.09 binaries,
stock 901227-03 KERNAL, and existing diagnostic snapshots. No emulator reads,
input injection, reset, or application-code changes were made for this audit.
The earlier `$0289` fix is already in the source; the findings below are not
additional fixes. No fresh cold/warm integration run was performed.

## Confirmed startup differences

The cold loader runs RAMTAS, clearing zero page and pages 2/3, and reconstructs
only part of normal KERNAL startup. Full CINT would destroy the live trampoline
at `$0400`; blindly adding that call is not a safe fix.

Offline evidence, from native `baseline.vsf` / `guard-before-utility.vsf` versus
the suspended C64OS image in REU bank 34 of
`roundtrip-before-readyos-work.vsf` / `roundtrip-after-c64os-work.vsf`:

| State | Native C64OS | Bridge-started C64OS | Consequence |
|---|---|---|---|
| `$0289`, printable buffer limit | 10 | 0 | Rejects printable keys; already repaired live and in startup source. |
| `$028B`, repeat countdown | 4 | 0 | First repeat countdown underflows to 255; unusually long initial hold before repeat. |
| `$028C`, initial repeat delay | 10 | 0 | Another omitted default; a new physical key resets it to 16 in ROM, so this alone does not explain missing typing. |
| `$02A6`, PAL flag | 1 | 0 | PAL machine is presented to C64OS as NTSC. |
| `$EB–$EE`, C64OS timing table | `32 21 19 11` | `3C 28 1E 14` | Native PAL timing thresholds are replaced with NTSC thresholds. |

The current `$0289`-only fix does not repair the repeat countdown or PAL flag.
The keyboard driver calls ROM `$EAE4`, which reaches `$EB17` and decrements
`$028B`. Starting from zero requires 256 decrements before reaching zero,
instead of four. Several seconds of initial repeat delay is a code-derived
prediction, not a new live reproduction. The driver does not initialize this
counter. Normal CINT initializes it to four.

BOOTER at `$1151` selects the four-byte timing table using `$02A6`. Later in
startup it also uses that flag for CIA1 TOD frequency selection. The
bridge's cold loader always programs the PAL KERNAL timer period `$4025`, yet
leaves the cleared PAL flag at zero. This is internally inconsistent. Preserve
the machine's existing video-standard flag across RAMTAS, then select the cold
KERNAL timer period from it. The steady-state C64OS timing table must be correct
at boot; merely changing `$02A6` after boot would not repair the derived table.

There is also a startup-only keyboard hazard: RAMTAS clears the KERNAL keyboard
translation vector `$028F/$0290`, and the loader enables the ROM keyboard IRQ
before initializing that vector. ROM `$EADC` jumps through it when scanning a
pressed key. C64OS later reuses these addresses for mouse settings, so they must
only be initialized to the ROM defaults (`$EB48`) for the pre-BOOTER interval,
never forced back during warm resume. No live boot-key failure was reproduced.

## Warm switching: hardware state is approximate

`native_context.inc` preserves RAM, displaced trampoline/staging pages, color
RAM, CPU registers/flags/stack/ports, vectors, and readable hardware settings.
`bridge_core.s` also saves/restores each side's 28 KB IDE64 DOS SRAM. The
following hardware state is not a faithful per-OS snapshot:

| Area | Actual behavior | Practical limit |
|---|---|---|
| Raster IRQ | Always writes `$D012=0` and clears compare bit 8. | Native 1.09 BOOTER writes line 1, confirmed in native snapshots, including after rejected utility lifecycle. The comment claiming native line 0 is inaccurate for these references. Resume moves the normal IRQ one scanline; visible impact not established. |
| CIA IRQ masks/pending events | Clears both masks and pending flags; restores only ReadyOS CIA1 timer-A enable. | Custom CIA IRQ/NMI users lose their setup; an already pending event can be discarded. |
| CIA timer reload/phase | Reads current counters, writes them as latches, forces reload. ReadyOS timer A gets its known standard period. | Other timer latch values and phase are not preserved. Unsupported active timers can return with a changed period. |
| CIA ports | Reads pin values, later writes them back as output latches; DDRs restored before port data. | Not a generic electrical-state snapshot; custom user-port/serial devices need an explicit contract. |
| CIA TOD / serial | TOD not restored; serial register was read but is not restored. | TOD is shared and continues; active serial work cannot be suspended this way. |
| SID | Clears three voice-control registers and master volume; restores none of them. | Sound/beeps/music can remain silent until their driver reinitializes the SID. Other SID registers can retain the other OS's values. |
| VIC collisions / light pen | Collision registers are read during capture, then deliberately not written back. | Reading collision latches acknowledges them. Not suitable for preserving a running collision-based application. |
| REU engine | Restores `$DF02–$DF0A`, not pending command/status or internal autoload history. | Requires an idle REU with no outstanding deferred transfer/IRQ. |
| IDE64 / drives | Saves DOS SRAM and ROM bank; forces STD mapping. Does not capture ATA/controller state, selected interface, drive CPUs, or IEC transactions. | Completed, quiet I/O only. Shared disks also need cache/refresh discipline if both systems modify the same files. |
| NMI transition | Uses a temporary RTI handler while the trampoline is present. | An NMI during that interval is swallowed, not replayed; entry/exit and cold-start vector replacement are not an atomic NMI-safe protocol. |

These are mostly existing design boundaries, rather than newly observed failures
in Tasks or Editor. The bridge currently relies on the callers honoring them;
it does not test every condition before switching. The raster-line discrepancy
is a specific correction to the documented standard-mode assumption.

## Other persistent changes and guard limits

- The copied HDD's `rec.lib.o` has a six-byte capacity override. It reports
  32 banks and bypasses full-size probing. It persists for native boots of that
  copied HDD too; it is not undone when switching. This is intentional partition
  protection, specific to the configured 16 MB REU. The original HDD is separate.
- The ReadyOS utility/menu entry are installed on the copied HDD. The utility
  closes normally after resume; previous tests check that close/reopen lifecycle.
- ReadyOS uses skip 39, with bridge snapshots at 32–35 and control data in 39.
  This is configuration/ownership, not temporary state to undo on return.
- Snapshot validity is signature/version plus valid bytes, without a payload
  checksum or session generation. Ordinary repeated switching is covered;
  stale/corrupt records after unusual reset/reconfiguration are not established
  safe. The control-bank code preserves an already valid bank, including its
  reserved tail. Avoid claiming the current guard detects every stale snapshot.
- VICE warp, drive traps, mouse mode and sound enable are host settings, outside
  the bridge snapshot. The recording scripts change warp/traps globally; the
  wrapper starts with warp on and host sound disabled. They are not restored
  separately for each OS. Manual testing currently has warp off.

## Smallest corrective scope and verification

1. Complete cold-start initialization: keep the `$0289` fix, initialize repeat
   state and the temporary ROM keyboard vector, preserve PAL/NTSC and choose
   its correct cold timer. Audit other omitted CINT fields by actual use;
   do not overwrite C64OS's repurposed fields after it has booted.
2. Correct the native text raster contract to line 1 after checking both normal
   utility entry and return-to-text behavior. Keep custom raster drivers outside
   the supported boundary.
3. Add tests that exercise VICE's emulated key matrix, not direct KERNAL buffers
   or C64OS event queues: fresh boot, held keys during loading, text input,
   modifiers, repeat/delete, and the same actions after repeated round trips.
   Compare PAL/NTSC flag and derived timing table with a native boot.
4. Test sound explicitly and decide whether to restore a known idle SID setup
   or require the OS to reinitialize it. Do not present silent-mode tests as
   proof of audio preservation.
5. Preserve the quiet-I/O contract for this prototype; broader device ownership,
   pending transfers, and shared-media coherence are separate work.

Existing tests establish destructive ZP/stack/shim restoration, six warm round
trips, failed-load recovery, native file save/reopen, and whole REU partition
isolation for the tested workloads. They do not establish complete hardware
restoration. Their injected C64OS input bypassed the physical keyboard path.

Private evidence: `../c64os-readyos-experiment/state-audit-20260925/` contains
the offline JSON comparison and disassemblies. No licensed binaries are added
to this repository. The current live session was left untouched.

References: local installed C64OS 1.09 BOOTER, input and keyboard driver;
stock VICE KERNAL `kernal-901227-03.bin`;
[C64OS input API](https://c64os.com/c64os/programmersguide/usingkernal_input);
[VICE IDE64 implementation](https://github.com/VICE-Team/svn-mirror/blob/main/vice/src/c64/cart/ide64.c).
