# RBSND08 — READY / Orbital Echoes

Load `RBSND08` in ReadyBASIC, then `RUN`. Press **Q** to return to text mode,
silence/detach the music, release the REU surface and restore the BASIC ceiling.
This is a PAL demo using the same vetted $9200 Shiru tune as RBSND07.

The readable BASIC source keeps setup, sine-wave lettering, additive ribbons,
30-second restoration and cleanup in separate procedures. A 256-entry integer
sine table avoids repeated trigonometry in the animation loop. Motion uses the
jiffy clock, with a maximum 30 updates/second; a 1 MHz C64 may draw fewer frames
than accelerated hardware. Refreshes use a cached REU surface, not disk reads.

## New commands in rbm.media

| Command | Contract |
| --- | --- |
| `MCFILE(name$)` | Read a standard uncompressed Koala SEQ file: $6000 prefix, 8000 bitmap bytes, 1000 screen bytes, 1000 color bytes, one background byte. |
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
  occupy $1BE0-$1CDF. Built-in command IDs, interpreter/parser and BASIC start
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

First probe exposed a **test timing issue**, not a passing demo: the fixed
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

For a future deployment, `prepare_readybasic_neon_ultimate.py` makes a fresh
exact-path build and plan without contacting hardware;
`deploy_readybasic_neon_ultimate.py` uploads/boots that prepared image from
Terminal. Allow normal disk boot to finish before running the generated
hardware plan. Audit its run directory with
`verify_readybasic_neon_ultimate.py <run-directory>`.

The final media package is 2163 bytes (1885-byte payload, with the unchanged
241-byte reserved-RAM driver). No resident ReadyBASIC code, built-in command
descriptors, BASIC start address or normal empty workspace was changed.
