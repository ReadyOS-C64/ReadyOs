# Ultimate speed control: mechanism versus animation pacing

Research and physical comparison, 2026-09-11. This is a recommendation, not an
implemented ReadyOS-wide speed policy.

Current demo update: after the initial line-spacing correction below, the user
explicitly requested raw motion. Both demos now advance sprite phase by SP%
per completed pass, draw a line every LD% sprite batches, and use TI only for
the 15-second image restore. Defaults are SP%=2 and LD%=3. The historical
clock-paced observations below explain the regression; they are not a
description of the final motion loop. The 64/1/16 MHz setup stages remain.

## What the two mechanisms actually do

The [official Ultimate register documentation](https://1541u-documentation.readthedocs.io/en/latest/config/turbo_mode.html)
describes two software-control modes:

| Mechanism | Normal operation | Good fit |
| --- | --- | --- |
| C64U Turbo Registers, `$D031` | Select a hardware-specific speed index; bit 7 controls badline handling. | Explicit MHz requests and per-app speed choices. |
| TurboEnable Bit, `$D030` | 0 selects 1 MHz with badlines; 1 selects the menu's speed and badline settings. | One user-selected fast speed plus a compatibility switch. |

Manual mode does not permit register speed changes. The external bus remains
1 MHz and VIC activity still constrains CPU throughput; a nominal MHz setting
is not a promise that every graphics operation scales by that factor.

The firmware configuration handler reads the CPU Speed preference and writes
the hardware preference/update registers. That configuration is not a live
CPU throughput meter. See official
[u64_config.cc](https://github.com/GideonZ/1541ultimate/blob/master/software/u64/u64_config.cc),
`setCpuSpeed()`. Physically, D031 acceleration was measured while the menu/API
preference remained 1 MHz.

## What the Gouraud example establishes

Sibling project `UltimateTester/firecube/firecube_gouraud.s`,
`read_fuel_keys`, explicitly documents lowering speed for KERNAL SCNKEY's
keyboard-matrix timing and returning to 64 MHz. Its deployment script selects
TurboEnable Bit and a menu preference of 64 MHz. `UltimateTester/fire/README.md`
documents the same rationale. The rendering loop is assembly and not the
ReadyBASIC demo's clock-paced loop.

This is evidence of a deliberate, sensible compatibility choice for that
application. No comparative research was found establishing that D030 is
universally faster or more reliable than D031. Do not attribute such a
conclusion to the original experiment.

## Independent wall-clock comparison

Physical C64 Ultimate, firmware 3.14 / FPGA 121 / core 1.47. Identical stock
BASIC `FOR I=1 TO 10000:NEXT`, timed with host `time.monotonic()`, not TI or
UMHZ. Badline timing enabled in every case:

| Control path | Menu preference | Host seconds |
| --- | ---: | ---: |
| D031 index 0 | 1 | 11.3827 |
| D031 index 9 | 1 | 0.8422 |
| Manual 16 | 16 | 0.7338 |
| D030 enabled | 16 | 0.7209 |
| D030 disabled | 16 | 11.2422 |

Both software mechanisms accelerate the processor. These samples include REST
polling and network overhead; the small differences among fast cases do not
establish a performance advantage for one mode. REST memory reads briefly stop
the CPU, and must never be used during disk loading on this machine.

Reproducible probe: `build_support/benchmark_ultimate_turbo_modes.py
--idle-confirmed`, from a Terminal-owned shell. Add `--readybasic` only at an
idle ReadyBASIC prompt to exercise USPEED rather than raw POKE. Recorded run:
`build/readybasic-media-tools/turbo-wall-1789173589448975000/results.json`.

## Why the graphics can look equally fast

The pre-fix `rbugfxsnddemo.bas` sampled time, not completed loop count:

- Line 320 derives the phase from the jiffy clock; the PHASE function doubles
  the low byte and wraps to 256 samples. The intended period is about 2.13 s.
- Lines 315–325 wait until the phase changes. A faster CPU cannot make the
  clock advance sooner, but can avoid missing samples.
- Line 360 permits a new line only after four jiffies. The nominal ceiling is
  15 lines/s, lower when command overhead and drawing time are included.
- Background renewal is also clock-based: 900 jiffies, approximately 15 s.

Consequently the right tests are completed five-sprite batches/s, completed
lines/s, visible smoothness and sustained correct rendering—not just whether
the letters complete a wave sooner. A CPU benchmark alone does not establish
that the application's hot path benefits as intended.

The earlier RBSND08 used an eight-second phase period and drew both mirrored
lines from one sample. The later demo reused the faster 2.13-second sprite
phase for its lines too, and alternated one mirrored side per pass. This
increased the line phase rate by 3.75x and made paired sides use different
time samples. It explains a real spacing regression independently of whether
turbo was enabled. The correction gives lines a separate LP% sample, advances
it by one only after both mirrored sides, and leaves sprite time independent.

The graphics comparison before this correction measured 12.32 / 0.75 / 12.44
completed sprite-position batches/s at 16 / 1 / 16 MHz, and 11.33 / 0.62 / 11.08
completed lines/s. Evidence:
`build/readybasic-media-tools/graphics-speed-1789174360205122000/results.json`.
These are instrumented throughput samples, not display refresh measurements.
An earlier sample was about 9.08 batches/s at 16 MHz; line lengths and refresh
work vary, so do not treat any one short sample as a fixed FPS guarantee.

Thus turbo did accelerate the live graphics loop, but did not guarantee smooth
animation or correct line spacing. The source also shows a cost worth profiling:
`rb_find_routine` scans program statements from BASIC_START on every PROC/FUNC
lookup. Its share of total time has not been measured; no interpreter/cache
refactor is justified solely by these throughput samples.

`build_support/benchmark_readybasic_demo_ultimate.py` adds RAM-only completion
counters and temporary speed keys to the actual loaded demo, waits for video
proof that loading is over, then samples at 16/1/16 MHz. The instrumentation
adds overhead; its absolute rates are not uninstrumented production FPS.
It removes its lines after Q and never saves them to the disk.
Use `--verify-setup` with the corrected demo to additionally check 64 MHz
precalculation, the return to 1 MHz before disk I/O, and LP% progression against
the completed-line counter. The current probe also verifies raw sprite phase
against its completed-batch counter and the three-batches-per-line cadence.
No memory inspections occur during those setup
stages: their CPU-side query results are inspected only after video readiness.
The corrected physical run passed those checks, Space/music continuity and Q
cleanup. Video readiness fell from 116.5 s to 37.6 s, and 16 MHz delivered about
13 completed sprite batches/s while preserving consecutive line samples. This
fixes spacing and startup time, not the remaining limit on sprite update rate.

Smoothness also needs its own review: the five SPRMOVE calls are sequential,
not an atomic frame-aligned sprite batch, and jiffy-paced work is not the same
as synchronizing to the PAL display's refresh. If the measured turbo throughput
is healthy but motion still looks uneven, investigate frame-aligned batching
and the BASIC/overlay dispatch cost before replacing the turbo mechanism.
These are candidate improvements, not proven causes of the user's earlier run.

## ReadyOS recommendation

Prefer **C64U Turbo Registers behind one shared speed-control contract** if
ReadyOS is to support explicit MHz requests and different app speeds. Use
normal-mode guards around timing-sensitive operations, as the Gouraud example
does; the useful principle is temporary compatibility speed, not a unique
property of D030.

If the product instead commits to exactly one global user-selected turbo speed
and a normal/fast toggle, D030 is a simpler, legitimate alternative. Do not
mix both modes casually or use REST configuration writes as a per-frame API.

Before a global rollout:

1. Define safe boot/load/exit speed and app preferred speeds. Keep legacy
   KERNAL IEC and keyboard timing safe; independently validate other I/O paths.
2. Save/restore speed and badline state with nested guards. Returning from an
   inner operation must not prematurely re-enable turbo. For compatibility,
   1 MHz alone is not identical to 1 MHz **with badlines enabled**.
3. Account for IRQ paths, app switches, warm REU resume and error exits.
   A SID that tolerates turbo does not prove all KERNAL/IRQ work does.
4. Detect hardware/table differences (older U64 versus C64U/Elite-II), and
   report unsupported/Manual mode rather than pretending a request succeeded.
5. Separate configured preference, live nominal selection and measured
   application throughput in diagnostics. Audit SYSINFO and UMHZ together.
6. Validate physical hardware at 1, 16 and the configured top speed, including
   transitions during real app workflows—not only register readback.

The current ReadyBASIC setter preserves the badline bit and is C64U-table
specific. It is not yet the complete compatibility guard or shared OS service
described here. The OS policy must also accommodate ReadyBASIC's intentional
shim-independent command implementation without duplicating contradictory
hardware rules.
