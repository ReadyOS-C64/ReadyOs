# ReadyBASIC disk sample modules — 0.5 RC2

`LDMOD` and `PAUSE` are built in. All former Z-prefixed proof commands are
disk-only examples with the prefix removed. A cold ReadyBASIC session has 83
production command descriptors and 45 empty registry slots. Demo worker code
is absent from the built-in `LOWPACK`, slot-1/slot-2 packs and overlays; the
built-in `SPANPACK` has been removed.

## Loading and using examples

```basic
5 LDMOD("RBM.SAMPLE1",M%)
10 PRINT "COMMANDS";M%
20 PRINT "ECHO";ECHO1()
30 PRINT "SUM";ADD16(7,8)
```

`LDMOD(NAME$)` or `LDMOD(NAME$,N%)` streams an RBM SEQ package from device 8
into the assigned REU banks. The returned integer is the descriptor count:
12 for sample1, 7 for sample2, 32 for sample3, or 8 for media. Loader failures
return diagnostic values 240–245. Use the disk name without a host `.seq`
suffix, for example `RBM.SAMPLE1`. Packages and runtime must be rebuilt
together because the scalar sample resolves private runtime helper addresses
from the matching link's `obj/readybasic.labels`.

`RBTEST1` and `RBPROC1` load sample1 on line 5. Interactive examples and probes
must load the package before their first demo command. `NEW` retains registered
modules. A cold ReadyBASIC start restores the production registry.

`PAUSE(UNITS)` preserves the existing busy-loop behavior: it uses the low byte
of the argument (0–255), and elapsed time depends on CPU speed. It does not
measure KERNAL jiffies. Graphics and sound samples can use it immediately.

## Sample1: scalar, array, temporary memory and two slots

Logical module 7 contains submodule 32 in slot 0 and submodule 33 in slot 1.
The slot-0 assembly is `sample_low.s`; its allocator proof carries its own
bitmap helpers. Resident code still owns argument parsing and result commit.
The disk worker for ECHO1 returns the value; there is no resident ECHO1
precomputation shortcut.

| Command | Form and result |
| --- | --- |
| `ECHO1` | `ECHO1(P%)` or `P%=ECHO1()` returns 1. |
| `ADD16` | `ADD16(A,B,R%)` or `R%=ADD16(A,B)` returns the 16-bit sum. |
| `HIDDENRAM` | `HIDDENRAM(S$,R%)` or `R%=HIDDENRAM(S$)` sums uppercase-normalized string bytes under BASIC ROM. |
| `SUMNUMARRAY` | `SUMNUMARRAY(A%(0),N,R%)` or `R%=SUMNUMARRAY(A%(0),N)` sums consecutive integer elements. |
| `RANGENUMARRAY` | `RANGENUMARRAY(START,COUNT,A%(0))` writes consecutive integers to the output array. |
| `TEMPSCRATCH` | `TEMPSCRATCH(BYTES,R%)` or `R%=TEMPSCRATCH(BYTES)` allocates/frees temporary bitmap pages and returns their count. No persistent handle remains. |
| `FAIL` | `FAIL(CODE,R%)` clears R% and deliberately reports the requested runtime error; zero maps to 127. |
| `SLOT0` | `SLOT0()` returns 30 from slot 0. |
| `CPYRST` | `CPYRST()` clears the byte-sized diagnostic payload-copy counter and returns 0. |
| `COPY` | `COPY()` returns the diagnostic counter, including a copy needed to load the counter helper itself. |
| `SLOT1` | `SLOT1()` returns 31 from slot 1. |
| `DM1` | `DM1()` returns 61 from the same slot-1 payload. |

The no-argument integer examples also accept an explicit output variable,
such as `SLOT1(R%)`. COPY/CPYRST are instrumentation for these controlled
examples, not an application-wide performance accounting API. The runtime
also uses that scratch byte in graphics paths.

## Sample2: slot 2, a two-slot span and replacement overlays

Logical module 8 supplies seven descriptors. Load sample1 and sample2 together
to demonstrate all three slots and the copy counter.

| Commands | Submodule / overlay | Runtime placement | Return values |
| --- | --- | --- | --- |
| `SLOT2` | 34 / 0 | Slot 2 | 32 |
| `SPAN`, `DM2S` | 35 / 0 | Slots 1+2 | 40, 74 |
| `OVL1`, `DOV1` | 36 / 1 | Slot 2 | 51, 72 |
| `OVL2`, `DOV2` | 36 / 2 | Slot 2 | 52, 73 |

The paired names in each row share one payload image. Calling the other entry
in that image tests reuse; switching overlay replaces the slot-2 image.

## Sample3: stateful overlays

Logical module 9 supplies COPY and CPYRST plus 30 stateful commands. Each name
starts with its submodule: `S6`, `S7`, or `S8`; then overlay `A`–`E` and entry `A` or `B`.
This avoids creating the BASIC `TAB(` token when removing the old prefix.
For example, `S6AA()` and `S6AB()` are two entrypoints in the same image.

| Family | Commands | Submodule | Placement |
| --- | --- | --- | --- |
| S6 | S6AA, S6AB, S6BA, S6BB, S6CA, S6CB, S6DA, S6DB, S6EA, S6EB | 6 | Slot 2 |
| S7 | S7AA, S7AB, S7BA, S7BB, S7CA, S7CB, S7DA, S7DB, S7EA, S7EB | 7 | Slot 2 |
| S8 | S8AA, S8AB, S8BA, S8BB, S8CA, S8CB, S8DA, S8DB, S8EA, S8EB | 8 | Slots 1+2 |

Each overlay has one shared counter byte. Reusing the image increments it;
reloading resets it. For example, S6AA returns 4 on first load and S6AB returns
6 when called next without replacement. Replacing the image and calling S6AA
again returns 4.

## Registry and payload layout

Sample1 and sample2 coexist. Sample3 replaces their demo entries and brings
its own counter commands. Reload sample1/sample2 before returning to their
examples. Loading sample1 after sample3 does not unregister the remaining
sample3 entries; only the new package's descriptor range is replaced.
All these packages preserve the production and media command entries.

| Core-bank registry range | Use |
| --- | --- |
| `$1000–$1A3F` | 82 contiguous built-in commands, UPPER through UMHZ. |
| `$1A40–$1B3F` | Eight media descriptors. |
| `$1B40–$1CBF` | Twelve sample1 descriptors. |
| `$1CC0–$1D9F` | Seven sample2 descriptors. |
| `$1B40–$1F3F` | Alternate 32-descriptor sample3 set, replacing sample1/2. |
| `$1F40–$1FDF` | Five spare descriptors. |
| `$1FE0–$1FFF` | Built-in SCRPUT, retained at slot 128. |

Sample payloads use separate code-bank storage: slot-0 examples at `$A000`,
slot-1 examples at `$A800`, sample2 images at `$B000/$B100/$B200/$B300`, and
sample3 images at `$C000–$CE3C`. These are REU offsets, separate from their
C64 execution addresses. Production overlays remain at `$5000–$7FFF` and
media at `$8000–$8FFF`.

## Space recovered

The built-in slot-0 payload shrinks from 2012 to 1620 bytes (392 bytes).
Removing the slot/span/overlay sentinels saves another 105 packed bytes, and
removing ECHO1 precomputation saves 35 resident bytes: 532 bytes of production
code in total. The hardware-tested decimal-display fix adds 3 helper bytes,
so the combined change saves 529 bytes. The fixed padded PRG remains 28674 bytes; BASIC still starts at
$2AC1 with 30013 empty free bytes. Fifteen command slots become empty registry
capacity for packages rather than increasing the BASIC workspace.

## Verification

Build through `/bin/bash ./run.sh --profile precog-d81 --build-only`.
`verify_readybasic_plugin.py`, `verify_readybasic_media.py` and
`verify_readybasic_samples.py` check the built layout without launching VICE.
The sample checker can additionally run the compiled 6502 workers with
`--workers` when Python's `py65` package is available. That checks worker
results and state transitions; it does not replace full ReadyOS runtime tests.

The ReadyBASIC probe scripts load the required samples and expect the current
names/counts. `READYBASIC_GENERATE_PLAN_ONLY=1 READYBASIC_SKIP_BUILD=1`
generates their YAML plans without starting or closing VICE. The physical
C64 Ultimate adapter in `build_support/run_readybasic_c64u_suites.py` runs the
same assertions through the Ultimate backend, using normal ReadyOS boot, an
exact-path DMA-enabled `apps.cfg`, and video-only loading-completion gates.
VICE is not started or stopped by this adapter.

### Physical C64 Ultimate results — 18 September 2026

Seven suites passed 225 assertions on a PAL C64 Ultimate at 1 MHz with 16 MB
REU and firmware 3.14:

| Suite | Assertions passed |
| --- | ---: |
| Module/overlay | 19 |
| Plugin commands, including built-in PAUSE before loading samples | 61 |
| Program | 18 |
| RBTEST1 | 4 |
| REPEAT/label | 15 |
| Lifecycle | 11 |
| Full suite, including RBPROC1 | 97 |

The full suite was verified in two consecutive segments on the same boot.
Its setup now loads the disk examples before ECHO1, and its retired-name test
rejects ZADD16. After correcting the old test that incorrectly rejected ADD16,
the affected scalar fixture was restored to `A%=15` before resuming. Every
current assertion has a retained successful result; this was not a single
uninterrupted full-suite run.

The test image's `apps.cfg` enabled DMA and named its exact uploaded image:
`/USB1/automation/readybasic-suites/8ef0343e/RBS8ef0343e.D81`.
FTP readback matched the local disk image. The launcher's REU status record
independently confirmed the same path, DMA-used flags `$1F`, and error zero.
Temporary diagnostic RAM was restored byte-for-byte afterward.

The stock Ultimate catalog exceeded D81 capacity. This test catalog omitted
game2048, deminer and dizzy, retaining all ReadyBASIC packages and samples;
these results do not certify the full stock Ultimate image. VICE was left
untouched. Raw VICE monitor/hotkey suites were not run, and five CPU-register
diagnostic dumps were omitted because the Ultimate backend cannot provide
them. All assertions in the seven selected suites were retained.

The hardware run also exposed a decimal-display bug: internal zeroes in the
free-memory count printed as ones. The formatter now prints 30013 correctly;
compiled formatter tests cover zero, internal zeroes and the 16-bit limit.
