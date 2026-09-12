# READY / Orbital Echoes — standard and Ultimate

Latest Ultimate deployment: **0.5L**, two-scene 64 MHz raw-motion version, at
`/USB1/automation/readybasic-media/neon-d91a3778/RBd91a3778.D81` with matching
DMA apps.cfg. Both CC0 backgrounds are packed losslessly and cached separately
in REU. Instrumented physical runs measured about 91 sprite batches/s at 64 MHz,
versus 1.7 at 1 MHz, with three batches per line. The historical build/test
sections below retain earlier evidence; see the latest section of the
[learnings](readybasic_media_learnings.md) for current results. The standard
source is updated, but its image has not been rebuilt in this Ultimate-only round.

Load `RBGFXSNDDEMO` (standard) or `RBUGFXSNDDEMO` (C64 Ultimate) in ReadyBASIC,
then `RUN`. These replace the old `RBSND08` release-disk entry; its source remains
as a historical example. Press **Q** to return to text mode,
silence/detach the music, release both REU surfaces and restore the BASIC ceiling.
Press **M** to return to text mode while keeping the music and its reserved
memory. Both paths restore the original border, background and text colors.
After M, `MUSDROP():CLR:MEMCAP(40960)` releases the music arena. Do not raise the
ceiling while the music is live. RUN again stops the old player before disk I/O.
This is a PAL demo using the same vetted $9200 Shiru tune as RBSND07.

Press **Space** to restore the cached background immediately, clearing the
accumulated lines without disk I/O or stopping the music and sprites. This
also restarts the 15-second automatic-refresh countdown.

The readable BASIC source keeps setup, sine-wave lettering, additive ribbons,
15-second restoration and cleanup in separate procedures. Sine, sprite heights
and both sets of line endpoints are precomputed before graphics. Motion is
deliberately raw and iteration-driven, not clock-driven. Line 270 exposes two
knobs: `SP%=2` advances sprite phase by two samples per completed pass, and
`LD%=3` draws one line every three sprite batches. Adjust these for the chosen
playback MHz. There is no catch-up, skipped clock sample or frame wait. The
five sprite calls are unrolled; lines alternate mirrored endpoints.
Both halves use the same independent LP% sample;
only after the pair is drawn do the sample and color advance. Slow drawing
therefore does not skip through the line pattern. Refreshes use a cached REU
surface, not disk reads. Only the 15-second background timer uses TI. The old
PHASE function was removed; PROC/EXEC and REPEAT/UNTIL still structure the
example without putting another routine lookup in the motion hot path.

## Ultimate speed command

`USPEED(mhz)` is built into the existing INPUTEV overlay, so it can run before
loading any disk module. The Ultimate variant selects 1 MHz as its first command,
64 MHz for array allocation and sine/coordinate precalculation, then explicitly
returns to 1 MHz before loading the module or any resource. It selects 64 MHz
after all resources are loaded, and 1 MHz on either exit. Loading the
BASIC program itself still requires a safe speed before RUN can execute.

Select **C64U Turbo Registers** in this machine's Turbo Control configuration
(called **U64 Turbo Registers** in the general Ultimate documentation).
Manual/Off do not expose software speed control and produce ReadyBASIC error 24;
unsupported speed arguments produce error 14. This command targets the **C64
Ultimate / U64 Elite-II** speed table: 1, 2, 3, 4, 6, 8, 10, 12, 14, 16, 20, 24,
32, 40, 48, 64 MHz. It is not the older 48 MHz U64 table. D031 bit 7 is preserved.
See the [official turbo documentation](https://1541u-documentation.readthedocs.io/en/latest/config/turbo_mode.html).
No interpreter changes or additional resident workspace were needed.

Use `PRINT UMHZ()` to read the live nominal MHz setting, or `S%=UMHZ()` to
store it. The Ultimate settings menu/API's CPU Speed value is a configuration
preference, not this live register readout. The current raw-motion demo does
speed up with CPU throughput; SP% and LD% control its visual rates. Earlier
clock-paced versions kept the wave period independent of CPU speed.
The setter checks register readback. UMHZ reports error 24 when software speed
registers are unavailable; it is not a throughput benchmark and does not count
cycles stolen by the VIC or external devices. In Turbo Enable Bit mode it also
honors the D030 enable gate. Use C64U Turbo Registers mode for this demo.

Physical proof for the query/setter is
`build_support/run_readybasic_uspeed_ultimate.py --idle-confirmed`, run from a
Terminal-owned shell only after visually confirming an idle ReadyBASIC prompt.
It performs no disk operations. The 2026-09-11 run passed 32 checks including
all supported speeds; its 1,000-iteration BASIC loop took 89/6/2 clock ticks
at 1/16/64 MHz. Live 64 MHz was verified while the settings API still reported
the configured preference of 1. Evidence and deployment details are in the
[learnings](readybasic_media_learnings.md#live-ultimate-speed-query-2026-09-11).

## New commands in rbm.media

| Command | Contract |
| --- | --- |
| `MCFILE(name$)` | Read standard Koala ($6000 prefix) or losslessly packed RKC1 from a SEQ file. Both produce 8000 bitmap bytes, 1000 screen bytes, 1000 color bytes and one background byte. |
| `SPRFILE(name$)` | Read RBR1 sprite data, restricted to aligned 64-byte blocks within $CA00-$CBFF. Set sprite controls/pointers with existing commands first; SPRSET generates a pattern and would overwrite loaded art if called afterward. |
| `MCLINE(x1,y1,x2,y2,slot)` | In MBITMAP mode, draw an inclusive all-octant line with pixel slot 0–3. Coordinates must be in 0–159 / 0–199. Invalid arguments fail before writing. |

MCLINE changes **only bitmap bits**. It never chooses a global palette color or
changes the 4x8 cell attributes. The resulting visible color therefore changes
as the line crosses cells. Slot 0 writes background pixels; the demo cycles
slots 1–3. Existing LINE/PLOT semantics are unchanged.

MCFILE/SPRFILE reject active music lifetimes, bad headers, empty/oversized names,
DOS mode delimiters, occupied logical file 14, truncated input and trailing
bytes. They own/close only their own logical file. Load resources before
MUSTUNE. Like RSCFILE, an I/O failure may leave a partially overwritten resource;
there is no promise of transactional graphics loading.

### Packed images and the two-scene demo

The current sources preload two independent 40-page REU surfaces, before
starting the SID. The show starts with space and alternates with Warped City
every 900 PAL jiffies (15 seconds). Space restores the current scene and restarts
that countdown. Neither operation reads the disk. Both global backgrounds are
black, since the surface API does not cache D021. Both exits release both handles.

Line coordinates use 512 precomputed phases rather than 256; the mirrored pair
still advances exactly one sample after both lines. This halves the angular
increment without clock-based skipping. Sprite phase remains the original
256-sample raw loop, two samples per batch and three batches per line.

`build_support/pack_readybasic_images.py` preserves the canonical Koala assets
and emits RKC1 build outputs. Four magic bytes `RKC1` precede packets: the low
seven control bits plus one give a 1–128 byte count; bit 7 selects repetition
of the next byte, otherwise that many literal bytes follow. Expansion must
end at exactly 10,001 bytes and physical EOF. Packets can cross image-section
boundaries; only the bounded loader chooses destinations. Short packets,
oversized expansion and trailing input are rejected. Ordinary Koala and RBR1
sprite input remain supported. This is not a general-purpose resource format.

The two packed pictures total 9,141 bytes (4,230 + 4,911), versus 10,003 for
the previous single picture. SND08 was already absent from the release disk;
no other apps or demos were removed to make room. The module payload is now
2,106/4,096 bytes, still within the same two module slots; package size is 2,384
bytes. No resident interpreter, command descriptors or BASIC memory map changed.

Local decoder regression, after a normal build, using a test environment with
`py65` installed: `python build_support/verify_readybasic_image_decoder.py`.
It executes the actual assembled loader against a simulated KERNAL byte stream,
checking valid formats, rejection cases, memory guards and display restoration.
It does not replace physical IEC/loading tests.

## Memory and compatibility

- BASIC ceiling $9000; driver $9000-$90F0; song $9200-$9C40.
- Sprites $CA00-$CBFF, screen $CC00-$CFE7, pointers $CFF8-$CFFF,
  bitmap $E000-$FF3F and color RAM $D800-$DBE7. Neither loader writes shim RAM
  ($C600-$C9FF), unused bitmap tail/CPU vectors, or sprite pointer bytes.
- GFXSURF allocates a typed 40-page REU surface. GFXSYNC caches pixels, screen
  and color; GFXBLIT restores all three. The global background register is not
  in the surface: this demo keeps it unchanged at zero.
- Media remains logical module 6 / submodule 24, occupying slots 1+2. New
  commands are IDs 116–118, skipping built-in BORDER (115). Eight descriptors
  occupy $1C20-$1D1F, after UMHZ's built-in descriptor at $1C00. Other built-in command IDs, interpreter/parser and BASIC start
  $2AC1 remain unchanged. Use the matching rebuilt module package.
- Palette-preserving integer lines avoid floating-point math in the module.
  Each pixel read/modify/write hides KERNAL briefly with interrupts masked,
  restoring $01 before IRQ state. Interrupts are not masked for the whole line:
  at 1 MHz that could defer or lose music frames. Since the dispatcher itself
  enters with SEI, MCLINE explicitly admits IRQs between pixels when the
  certified media player is active, then restores the dispatcher's flags.
  KERNAL remains visible between pixels and the driver preserves FB-FE scratch.
  The music player stays outside every overlay slot.

## Verification

The dedicated two-exit test is `python3 build_support/run_readybasic_gfxsnd_probe.py`.
Its real-time VICE run at `logs/vice_auto_20260911_151833/manifest.json` passed
all 41 steps plus the byte/animation audit: both exits, saved colors, protected
music at the prompt, re-running with music active, changing sprite positions,
unchanged palettes and nonzero automatic refresh count.

`python3 build_support/verify_readybasic_gfxsnd_disks.py` checks both current
release D81s against the local runtime, module, two demos and three resources,
and checks the Ultimate DMA path. To repeat the physical two-exit test, first
prepare a fresh image with `prepare_readybasic_neon_ultimate.py`, then run
`run_readybasic_gfxsnd_ultimate.py` from a Terminal-owned shell. It deliberately
reboots, enables software turbo and leaves the Ultimate variant running.
The runner now requires visual confirmation at the terminal after ReadyOS boot
and after the BASIC program LOAD; do not acknowledge until the physical display
has returned to an idle prompt. Resource-loading RUNs are gated automatically
by streamed video. These two conservative operator gates were added after the
successful hardware run below; that run used fixed waits for boot/program LOAD.

Final physical proof passed in four inspection blocks:
`logs/ultimate_auto_20260911_154905/manifest.json` (native 1/16/64 MHz),
`155215` (scene, refresh and M), `155457` (music-active rerun and Q), and
`155722` (running handoff). Both exits restored colors and 1 MHz; palette and
sprite bytes matched their resources, music advanced at the M prompt, and Q
released the music and BASIC ceiling. All three startups took about 118 seconds.
The runner watches streamed video until the READY sprites move before reading
RAM: REST memory/screen reads during disk loading can hang physical C64U.
A fixed 110-second delay was too short and must not be treated as completion.

Current images are regular **0.5D** and Ultimate **0.5E**. The physical image is
`/USB1/automation/readybasic-media/neon-5807f3ef/RB5807f3ef.D81`; its embedded
apps.cfg matches that path with DMA_LOADING=1. Full upload readback SHA-256:
`bd6a80b606762bf1d820620802ea29857fac7cf9bcb95567b011cb03cf3a0d94`.
Only one free block remains in this Ultimate image.

Existing media regression also passed all 122 steps plus animation/audio audit
(`logs/vice_auto_20260911_155241/manifest.json`). The broader aggregate is only
partially verified: 11 core targets passed, but the hotkey fixture requires a
launcher-first image instead of this ReadyBASIC-first demo build. See the
[learnings](readybasic_media_learnings.md) for the retry and fixture limitations.

For the subsequent physical startup reliability fix and newer verified images,
see [resource-load investigation](readybasic_media_load_investigation.md).
The loaders now suppress sprite DMA during disk transfers; MCFILE hides its
unfinished bitmap until pixels and palettes have been read. Failed transfers
restore sprite/display controls, but do not provide full image-memory rollback.
Use `READYBASIC_TRUE_DRIVE=1` for the untrapped IEC version of the VICE probe.
Load/run setup at **1 MHz**. Faster post-load playback is verified, but forced
Manual turbo during disk loading is not supported; that Ultimate mode prevents
software from temporarily selecting a safe disk-I/O speed.

`python3 build_support/run_readybasic_neon_probe.py` uses the normal ReadyOS
disk boot in visible VICE. It compares the complete bitmap load and REU restore,
checks line output against a host-side rasterizer and checks invalid-coordinate
atomicity. During the real disk demo it checks unchanged palettes, exact sprite
data/pointers, movement, SID ticks, timed refresh and final cleanup.

### Historical first implementation checks (RBSND08)

The first probe exposed a **test timing issue**, not a passing demo: the fixed
25-second wait sampled a partially loaded picture with music not yet installed.
The probe now gives loading an uninterrupted interval and checks the installed
music state before examining the scene. Status text is printed before graphics
loading; PRINT after loading corrupts the shared color RAM even when the stock
text screen is hidden. The palette probe uses GET-controlled phases without
returning to the text editor between captures, for the same reason.
Successful completion by itself is not proof of animation; moving sprite
registers and advancing music ticks are checked separately.

Keep procedure/command names clear of embedded BASIC V2 keywords. For example,
LETTERS contains LET, RIBBONS contains ON and RENEW contains NEW; petcat stores
those as ROM tokens (LOGO also contains LOG). This demo uses GLYPHS, WEAVE and CLEAN rather than extending
the native identifier parser. MCFILE similarly avoids LOAD inside KOALOAD.

The first full scene rendered and played music but stopped with a BASIC syntax
error at `IF ELAPSED(RT)>=30 THEN EXEC CLEAN`. Keep the native function call in
an assignment, then use an ordinary numeric IF/line target and a separate EXEC.
This follows the existing ROM-expression boundaries without an interpreter
change. The probe now rejects hidden text-screen errors before every scene
capture; an intact bitmap and live IRQ alone do not prove BASIC is still running.

The float function-assignment form also left the demo's phase/refresh values at
zero in the first sustained run. Direct clock arithmetic in the main loop fixed
this; the known-working FUNC/UNTIL elapsed-time form remains for pacing. This
is a scoped demo workaround, not a claim that the underlying interpreter issue
has been diagnosed or repaired. Phase reduction happens before integer
conversion so the animation cannot overflow BASIC's signed integer range after
about 17 minutes.

VICE proof: `logs/vice_auto_20260911_115237/manifest.json`, normal ReadyOS 0.5L
regular D81 boot. All 62 harness steps and the separate binary/animation audit
passed: exact Koala bitmap, exact REU restore, reference-rasterizer match,
invalid-coordinate nonmutation, unchanged palettes, exact sprites/pointers,
changing sprite positions and SID ticks, nonzero refresh count, and clean exit.

Physical Ultimate proof: `logs/ultimate_auto_20260911_115927/manifest.json`.
All 39 hardware steps passed with no degraded steps. Real captured video
frames changed at 1, 16 and 64 MHz; the separate hardware audit confirmed the
palette/sprite bytes and active music state at each speed, advancing SID IRQ
ticks, a recorded image refresh, and final MEMSIZ=$A000 / detached music.
Video inspection confirmed the READY wave and accumulating trails. This is not
a claim of audio-quality measurement; the music player/tune are unchanged from
RBSND07 and this run checks IRQ liveness under graphics load. The machine was
returned to 1 MHz with RBSND08 still loaded at the ReadyBASIC prompt.

Final images: regular `readyos-v0.5l-d81.d81`, Ultimate
`readyos-v0.5m-ultimate.d81`. Both were built through run.sh; all four packaged
demo/module/graphics resources were extracted and byte-compared with the final
local files. Release directory-order checks passed for both profiles.

The Ultimate image was uploaded into the fresh owned folder
`/USB1/automation/readybasic-media/neon-447af4e4/RB447af4e4.D81`, then read back
and compared in full (SHA-256
`2884bd6f7a7106549fbebe7b5e771d49476bc10f483f6b93d22dbfbe970f352c`). Its apps.cfg
contains that case-insensitive path, DMA_LOADING=1 and RUNAPPFIRST=READYBASIC.
No other image was overwritten. Deployment used a Terminal-owned shell and
normal LOAD "*",8,1 / RUN boot, not a standalone app launch.

The old fixed-wait hardware demo plan has been retired. For current deployments,
use the preparation and video-gated runner described above, not this historical
speed-sweep procedure. `verify_readybasic_neon_ultimate.py` remains an auditor
for historical speed-sweep artifacts.

The final media package is 2163 bytes (1885-byte payload, with the unchanged
241-byte reserved-RAM driver). No resident ReadyBASIC code, built-in command
descriptors, BASIC start address or normal empty workspace was changed.
