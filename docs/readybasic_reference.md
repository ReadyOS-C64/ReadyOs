# ReadyBASIC: commands, modules and every example

ReadyBASIC adds structured routines, REU resources, graphics and sound to C64
BASIC V2. Start ReadyOS, launch ReadyBASIC, then load a demonstration from the
mounted disk, for example `LOAD "RBGFX02",8` followed by `RUN`. These are BASIC
programs to run inside ReadyBASIC, not independent ReadyOS apps. The source
listings below explain the working combinations of commands and ordinary BASIC.

The current release profiles do not all carry the same examples. Regular D81
and Ultimate D81 contain 44: three language tests, 32 graphics examples, seven
sound examples and two combined demos. The original 41-example set remains in
the smaller focused ReadyBASIC D64, D71 and KFF2 profiles. The separate rsdebug
D81 has an older 27-example subset.
The old `rbsnd08_neon.bas` is retained as historical source, not a current disk
entry. The profile inventory below is generated from the actual configurations.

## Choose a starting point

- **Language:** RBTEST1 shows command expressions and strings; RBPROC1 covers
  PROC/FUNC calls, typed returns and normal BASIC expressions. RBPROCERR is an
  intentional error test, not a demo expected to run to completion.
- **Graphics:** RBGFX01–05 introduce modes and drawing. RBGFX13/16 cover sprites;
  17–20 polygons; 23 display lists; 24 tilemaps; 27/31 REU surface capture/restore.
- **Sound:** RBSND01–06 control SID registers immediately. RBSND07 loads a vetted
  PAL tune and lets music continue while BASIC runs.
- **Combined demo:** RBGFXSNDDEMO presents Orbital Echoes without Ultimate speed
  commands. RBUGFXSNDDEMO requires C64 Ultimate software turbo registers. Both
  use two cached backgrounds, animated READY sprites and palette-preserving
  lines. See [the media walkthrough](readybasic_orbital_echoes.md).

Many graphics examples wait for a key before returning to text. The combined
demo uses **Space** to restore the current cached picture, **Q** to stop music
and release its memory, and **M** to return to text while music continues.
After M, enter `MUSDROP():CLR:MEMCAP(40960)` to release the reserved arena.
Some older examples are small command probes and leave handles for the session;
their source is retained verbatim rather than silently changed for this guide.

## Built-in commands versus disk modules

The normal graphics, immediate SID sound, input and memory commands are already
registered when ReadyBASIC starts. Their code is fetched from assigned REU
resources into command slots as needed; no `LDMOD` is needed for them.

Disk packages are SEQ files. `LDMOD("RBM.MEDIA",M%)` registers eight media
commands and returns their count in M%. It keeps package code/descriptors in
REU, not the BASIC program area. The package is loaded only when requested.
`RBM.SAMPLE1`, `RBM.SAMPLE2` and `RBM.SAMPLE3` are developer proofs of module,
span and replacement-overlay dispatch; they are not prerequisites for media.
Use module packages built with the matching ReadyBASIC executable.

`PAUSE` and `LDMOD` are built in. The scalar, array and slot/overlay proofs
are disk-only examples with plain names. RBTEST1/RBPROC1 load sample1 on line 5.
Sample1 and sample2 coexist; sample3 replaces their demo entries while preserving
all production and media commands. See the generated inventory below and the
[sample module guide](../src/apps/readybasic/READYBASIC_SAMPLE_MODULES.md).

The loaders use **drive 8**. LDMOD uses logical file 1; media resource loading
uses logical file 14. Keep those channels available while loading. These commands
do not accept a device argument.

## New built-in commands

| Form | Meaning and boundaries |
| --- | --- |
| `MEMCAP(address)` | Set a page-aligned, exclusive BASIC memory ceiling, at most 40960 ($A000). Reserve before allocating strings. A changed cap needs an empty dynamic string heap and room beyond program/variables/arrays. It does not relocate strings or call CLR. Numeric variables/arrays survive; an unchanged cap is harmless. Invalid changes report error 14 without committing pointers. |
| `BORDER(color)` | Set border color using the low four bits, in any display mode. Built-in GFXCORE. |
| `USPEED(mhz)` | C64 Ultimate / U64 Elite-II speed setter: 1, 2, 3, 4, 6, 8, 10, 12, 14, 16, 20, 24, 32, 40, 48, 64. Requires software turbo registers; preserves D031 bit 7 and checks readback. Error 14 for unsupported argument; 24 when unavailable. It is not the older U64 48-MHz table. |
| `UMHZ()` / `S%=UMHZ()` | Read the live nominal MHz selection, not the settings-menu preference or a throughput benchmark. Reports error 24 when the software registers are unavailable. |

MEMCAP reserves an arena by lowering BASIC's allocator ceiling; it is not memory
protection. POKE, SYS and ordinary oversized LOAD can still overwrite it.
The empty default remains 30013 BASIC free bytes starting at $2AC1. Reserving
4096 bytes for music lowers the ceiling to $9000; raising it while the media
lifetime lock exists is rejected. Drop music before restoring memory, and use
CLR explicitly if heap strings need discarding.

## The eight on-demand media commands

| Form | What it does |
| --- | --- |
| `MUSTUNE("RB.SUMMER")` | Load a vetted fixed-address PAL PSID v2 tune into previously reserved RAM and install its driver, without playing. Reserve with MEMCAP(36864) first. |
| `MUSPLAY(1)` | Initialize or restart a one-based subtune and attach raster-driven playback. BASIC can continue working. |
| `MUSHALT()` | Detach the player IRQ and silence SID, retaining tune data and its memory reservation. |
| `MUSDROP()` | Halt and clear the tune lifetime lock. Does not increase the BASIC ceiling. |
| `RSCFILE(name$)` | Load an RBR1 resource (destination and length in its header) into the arena above MEMCAP, ending no later than $A000. Caller owns the raw resource's lifetime. |
| `MCFILE(name$)` | Load a standard Koala file ($6000 prefix) or packed RKC1 stream: 8000 bitmap bytes, 1000 screen, 1000 color and one background byte. |
| `SPRFILE(name$)` | Load RBR1 sprite data into aligned 64-byte blocks within $CA00–$CBFF. Configure with SPRSET before loading art, since SPRSET generates a pattern. |
| `MCLINE(x1,y1,x2,y2,slot)` | Inclusive line in MBITMAP coordinates 0–159 / 0–199, using pixel slot 0–3. Only bitmap bits change; palette attributes remain untouched. Invalid arguments fail before writing. |

This is a constrained, vetted-player implementation, not an arbitrary SID
player. Accepted PSID headers specify PAL VBI, one SID at $D400, RTS init/play
entries within the loaded $9200–$9FFF payload, and a zero speed mask. The driver
occupies $9000 and reserves through $91FF. The included Shiru tune was relocated
and checked for its $FC–$FD scratch contract. Header checks cannot prove arbitrary
machine code safe. RSID, NTSC playback, CIA/multispeed, digi and multi-SID players
are outside the supported contract.

Load images and sprites **before** MUSTUNE. Loaders reject an existing music
lifetime (including halted-but-not-dropped music), invalid filenames, occupied
LFN 14, bad headers, short input and trailing bytes. Filenames are limited to
16 bytes and cannot select DOS write/replacement modes. Errors are 21 (I/O),
22 (format/arguments), 23 (lifetime/channel/IRQ conflict). An I/O failure can
leave partial resource bytes; these loaders do not promise memory rollback.
Sprites are disabled during streamed disk I/O and their enable state restored;
MCFILE also hides an incomplete bitmap until the transfer completes.

END or STOP alone does not detach the music player. Use MUSHALT/MUSDROP.
ReadyOS navigation from the ReadyBASIC prompt detaches playback; warm return
retains the cap/tune and requires MUSPLAY to restart it. SIDOFF clears SID
registers but cannot detach the player's IRQ. Avoid mixing immediate SID writes
with a live music player unless that competition for the voices is intentional.

```basic
10 memcap(36864)
20 ldmod("rbm.media",m%)
30 mustune("rb.summer"):musplay(1)
40 print "music runs while basic works"
50 get a$:if a$="" then 50
60 musdrop():clr:memcap(40960)
```

## Safe loading and display behavior

Use 1 MHz for boot, BASIC program LOAD and resource disk reads on Ultimate.
The Ultimate demo selects 64 MHz for precalculation, returns to 1 MHz for file
loading and selects 64 MHz for playback, then 1 MHz on either exit. Select
**C64U Turbo Registers** in Turbo Control; Manual mode prevents software speed
control. Loading the BASIC program happens before its USPEED(1) can execute.

During physical Ultimate loading, automation must observe the **video stream**
until visible completion. REST memory reads, decoded-screen checks and RAM
captures can lock up disk loading; a fixed delay does not prove completion.
After an interrupted transfer, start from a fresh boot.

The combined demo preloads two 40-page REU surfaces and restores them without
disk I/O. GFXTGT selects the destination of GFXSYNC; immediate drawing still
targets the visible surface. GFXSYNC captures bitmap/screen/color into the
selected REU surface; GFXBLIT restores it. D021 background color is not cached.
The demo keeps both backgrounds black. Its animation advances with completed
iterations, so CPU speed changes the motion; only background renewal uses TI.

## Language and command conventions

Ordinary BASIC V2 statements, expressions and assignments still use the ROM.
ReadyBASIC adds PROC/EXEC/ENDP, FUNC with RET/RET%/RET$, REPEAT/UNTIL,
LABEL/JUMP and prompt EXIT. Put routine definitions after END. Formals are
global BASIC variables, not lexical locals; nested routine/loop stacks are
bounded. See [syntax and limits](../src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md).

Write command spellings in lowercase in host `.bas` files passed to petcat, as
the supplied examples do. Embedded BASIC keywords can otherwise tokenize inside
names. Use explicit `THEN :COMMAND(...)` for command statements in stored files.
The media demonstrations use proven numeric IF targets and integer returns;
their recorded float FUNC/assignment workaround is not an interpreter fix.

The implemented inventory below is drawn from command descriptors. Design
documents also preserve proposals: names such as GFXINFO, FILL, TEXTAT, SCROLL
and KEYDOWN are not implemented commands in this build. FCIRCLE currently
fills the bounding rectangle, and FPOLY is a convex fan rather than a general
concave polygon filler. Those limitations are not hidden by the command names.

## Further reading

- [Current implementation and memory](../src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md)
- [Graphics design, implementation phases and proposals](../src/apps/readybasic/READYBASIC_GRAPHICS_COMMAND_DESIGN.md)
- [Immediate SID commands](../src/apps/readybasic/READYBASIC_SOUND_COMMAND_DESIGN.md)
- [Writing commands and packages](../src/apps/readybasic/READYBASIC_MAKING_COMMAND_GUIDE.md)
- [Media development evidence and caveats](readybasic_media_learnings.md)
- [Open audit findings](documentation_audit_2026-09.md)

The rest of this page is generated from source. Refresh it with
`python3 build_support/build_readybasic_reference.py` after changing examples
or command registrations. Existing example sources are reproduced in full.

## Implemented built-in inventory

| Command | Form and purpose |
| --- | --- |
| `UPPER` | `UPPER(S$,T$) or T$=UPPER(S$)` — uppercase a bounded string. |
| `LOWER` | `LOWER(S$,T$) or T$=LOWER(S$)` — lowercase a bounded string. |
| `BUFMAKE` | `BUFMAKE(BYTES,H%)` — allocate a persistent REU buffer handle, rounded to pages. |
| `BUFFILL` | `BUFFILL(H%,BYTE)` — fill a REU buffer. |
| `BUFDROP` | `BUFDROP(H%)` — release a persistent resource handle. |
| `MEMAVL` | `MEMAVL()` — report live BASIC free memory. |
| `SCRCAP` | `SCRCAP(H%)` — capture text and color as a typed REU screen handle. |
| `FADD` | `FADD(A,B,R) or R=FADD(A,B)` — BASIC floating-point addition. |
| `PAUSE` | `PAUSE(UNITS)` — busy-loop delay using the low byte; duration depends on CPU speed. |
| `ERRCODE` | `ERRCODE() or ERRCODE(E%)` — last ReadyBASIC runtime error code. |
| `ERRLINE` | `ERRLINE() or ERRLINE(L%)` — last ReadyBASIC error line, zero for direct mode. |
| `LDMOD` | `LDMOD(NAME$,N%)` — stream/register a SEQ command package; N% receives descriptor count. |
| `GFXMODE` | `GFXMODE(MODE$), M%=GFXMODE()` — set/query TEXT, HIRES, MBITMAP, TILE or MTILE. |
| `GFXTEXT` | `GFXTEXT()` — restore normal text display. |
| `GFXCLEAR` | `GFXCLEAR(C)` — clear visible screen/color and applicable bitmap memory. |
| `GFXSURF` | `H%=GFXSURF(MODE$)` — allocate a typed REU graphics surface. |
| `GFXTGT` | `GFXTGT(H%)` — select surface for GFXSYNC; zero selects visible target. Primitives still draw visible RAM. |
| `GFXBLIT` | `GFXBLIT(H%)` — restore bitmap/screen/color from REU to visible graphics RAM. |
| `GFXSYNC` | `GFXSYNC()` — capture visible bitmap/screen/color into the selected REU surface; no dirty-rectangle queue. |
| `PLOT` | `PLOT(X,Y,C)` — set a hires bit, multicolor pair or tile cell according to mode. |
| `PNT` | `PNT(X,Y,P%)` — read a hires bit, multicolor pair code (not resolved color), or cell. |
| `LINE` | `LINE(X1,Y1,X2,Y2,C)` — draw a line with immediate primitive color semantics. |
| `RECT` | `RECT(X1,Y1,X2,Y2,C)` — draw a rectangle outline. |
| `FBOX` | `FBOX(X1,Y1,X2,Y2,C)` — fill a rectangle. |
| `CIRCLE` | `CIRCLE(X,Y,R,C)` — midpoint outline circle. |
| `FCIRCLE` | `FCIRCLE(X,Y,R,C)` — current placeholder fills the bounding rectangle. |
| `TILE` | `TILE(X,Y,CH,C)` — write character/color cell in tile or ordinary text mode. |
| `CHARAT` | `CHARAT(X,Y,CH,C)` — alias for TILE. |
| `SPRSET` | `SPRSET(N,ON,C,PATTERN)` — enable/configure sprite and generate pattern; do this before SPRFILE. |
| `SPRMOVE` | `SPRMOVE(N,X,Y)` — update hardware sprite position. |
| `SPRROW` | `SPRROW(N,ROW,B1,B2,B3)` — write one 24-bit sprite row. |
| `SPRSIZE` | `SPRSIZE(N,XON,YON)` — set horizontal/vertical expansion. |
| `SPRPRI` | `SPRPRI(N,BEHIND)` — set sprite/background priority. |
| `SPRMUL` | `SPRMUL(N,ON)` — enable multicolor sprite mode. |
| `SPRCOL` | `SPRCOL(N,C)` — set sprite primary color. |
| `SPRMCO` | `SPRMCO(C1,C2)` — set shared multicolor sprite colors. |
| `SPRSCAN` | `SPRSCAN()` — latch sprite collision registers for polling. |
| `SPRCOLL` | `SPRCOLL(N,C%)` — read latched collision state. |
| `JOY` | `JOY(PORT,J%)` — poll a joystick port. |
| `KEYP` | `KEYP(K%)` — poll the current key. |
| `KEYSCAN` | `KEYSCAN()` — refresh keyboard polling state. |
| `KEYLAST` | `KEYLAST(K%)` — retrieve the last scanned key. |
| `POLY` | `POLY(A%(0),COUNT,C)` — outline integer-array coordinate pairs. |
| `FPOLY` | `FPOLY(A%(0),COUNT,C)` — conservative convex fan fill. |
| `PBMAKE` | `PBMAKE(COUNT,H%)` — allocate typed REU point buffer. |
| `PBUFSET` | `PBUFSET(H%,INDEX,X,Y)` — set a zero-based point. |
| `PBDROP` | `PBDROP(H%)` — release point buffer. |
| `POLYH` | `POLYH(H%,COUNT,C)` — outline points from REU buffer. |
| `FPOLYH` | `FPOLYH(H%,COUNT,C)` — convex fan fill from REU points. |
| `DLMAKE` | `DLMAKE(COUNT,H%)` — allocate one-page retained display-list handle. |
| `DLRST` | `DLRST(H%)` — clear retained records. |
| `DLPLOT` | `DLPLOT(H%,X,Y,C)` — append a plot record. |
| `DLLINE` | `DLLINE(H%,X1,Y1,X2,Y2)` — append a line record. |
| `DLRECT` | `DLRECT(H%,X1,Y1,X2,Y2)` — append rectangle outline. |
| `DLFBOX` | `DLFBOX(H%,X1,Y1,X2,Y2)` — append filled rectangle. |
| `DLDRAW` | `DLDRAW(H%)` — replay records in list order. |
| `CHRMAKE` | `CHRMAKE(COUNT,H%)` — allocate eight-page charset resource. |
| `CHRROW` | `CHRROW(H%,CHAR,ROW,BYTE)` — set one character bitmap row. |
| `CHRUSE` | `CHRUSE(H%)` — copy charset to visible Bank D layout. |
| `TSMAKE` | `TSMAKE(COUNT,H%)` — allocate one-page tileset. |
| `TSSET` | `TSSET(H%,INDEX,CHAR,C)` — define a tileset entry. |
| `TMMAKE` | `TMMAKE(COUNT,H%)` — allocate four-page 40x25 tilemap. |
| `TMSET` | `TMSET(H%,INDEX,TILE,FLAGS)` — set a map cell. |
| `TMDRAW` | `TMDRAW(MAP%,TILES%)` — render tilemap through tileset. |
| `MCELL` | `MCELL(CX,CY,S1,S2,S3)` — set multicolor bitmap cell palette attributes. |
| `MCBG` | `MCBG(C)` — set shared background color. |
| `SIDRST` | `SIDRST()` — clear SID registers $D400–$D418. |
| `SIDOFF` | `SIDOFF()` — alias for SIDRST; does not detach an IRQ player. |
| `VOL` | `VOL(V)` — master volume 0–15, preserving filter mode bits. |
| `FRQ` | `FRQ(V,F)` — raw 16-bit frequency, voice 1–3. |
| `PITCH` | `PITCH(V,SEMITONE,OCTAVE)` — PAL pitch table, semitone 0–11 / octave 0–7; does not gate voice. |
| `PULSE` | `PULSE(V,WIDTH)` — pulse width 0–4095. |
| `ADSR` | `ADSR(V,A,D,S,R)` — envelope nibbles 0–15. |
| `WAVE` | `WAVE(V,MASK)` — write complete voice control/wave/gate byte. |
| `GATE` | `GATE(V,ON)` — alter gate bit alone. |
| `VOICE` | `VOICE(V,F,W,AD,SR)` — packed frequency, control and envelope setup. |
| `FILTER` | `FILTER(CUTOFF,RES,ROUTE,MODE)` — cutoff 0–2047, resonance 0–15, route and mode masks. |
| `SOUND` | `SOUND(V,F,D,W)` — blocking tone; duration is spin-delay units, not milliseconds. |
| `MEMCAP` | `MEMCAP(ADDRESS)` — reserve memory by lowering the BASIC ceiling; see contract above. |
| `BORDER` | `BORDER(C)` — set low four border-color bits. |
| `USPEED` | `USPEED(MHZ)` — Ultimate-specific built-in speed control; see supported table above. |
| `UMHZ` | `UMHZ()` — Ultimate-specific live nominal speed query. |
| `SCRPUT` | `SCRPUT(H%)` — restore a captured text/color screen. |

## Developer sample-module commands

The scalar, array, scratch-allocation and slot/overlay examples are disk-only.
Their names have no Z prefix. PAUSE and LDMOD remain built in.
`LDMOD("RBM.SAMPLE1",N%)` returns 12; SAMPLE2 returns 7 and SAMPLE3 returns 32.
Load sample1 before running RBTEST1/RBPROC1 or typing the scalar examples.
Those two programs also load it on line 5 when RUN.
Sample1 and sample2 coexist. Sample3 replaces their demo registry entries
and supplies its own COPY/CPYRST commands alongside 30 stateful entries.
Reload sample1/sample2 when returning to those examples. All packages preserve
the production commands and media registry; there are 49 distinct demo names
and 51 package entries because COPY/CPYRST occur in two packages.

Media occupies core-bank $1A40-$1B3F. Sample1 uses $1B40-$1CBF;
sample2 uses $1CC0-$1D9F; sample3 uses $1B40-$1F3F.
All disk payloads and the runtime must be rebuilt together.

| Package | Command | Diagnostic behavior |
| --- | --- | --- |
| sample1 | `CPYRST` | reset overlay-copy instrumentation. |
| sample1 | `COPY` | inspect overlay-copy instrumentation. |
| sample1 | `ECHO1` | return the integer proof value. |
| sample1 | `ADD16` | add integer arguments. |
| sample1 | `HIDDENRAM` | exercise string input in the under-ROM worker. |
| sample1 | `SUMNUMARRAY` | sum N integer elements. |
| sample1 | `RANGENUMARRAY` | write an integer range into an array. |
| sample1 | `TEMPSCRATCH` | allocate/free temporary workspace and return its page count. |
| sample1 | `FAIL` | deliberately raise a ReadyBASIC error to test output clearing. |
| sample1 | `SLOT0` | Return 30; slot/span/overlay dispatch proof |
| sample1 | `SLOT1` | Return 31; slot/span/overlay dispatch proof |
| sample1 | `DM1` | Return 61; slot/span/overlay dispatch proof |
| sample2 | `SLOT2` | Return 32; slot/span/overlay dispatch proof |
| sample2 | `SPAN` | Return 40; slot/span/overlay dispatch proof |
| sample2 | `DM2S` | Return 74; slot/span/overlay dispatch proof |
| sample2 | `OVL1` | Return 51; slot/span/overlay dispatch proof |
| sample2 | `DOV1` | Return 72; slot/span/overlay dispatch proof |
| sample2 | `OVL2` | Return 52; slot/span/overlay dispatch proof |
| sample2 | `DOV2` | Return 73; slot/span/overlay dispatch proof |
| sample3 | `CPYRST` | reset overlay-copy instrumentation. |
| sample3 | `COPY` | inspect overlay-copy instrumentation. |
| sample3 | `S6AA` | Stateful diagnostic, submodule 6, overlay 1; paired A/B entrypoints share overlay-local state |
| sample3 | `S6AB` | Stateful diagnostic, submodule 6, overlay 1; paired A/B entrypoints share overlay-local state |
| sample3 | `S6BA` | Stateful diagnostic, submodule 6, overlay 2; paired A/B entrypoints share overlay-local state |
| sample3 | `S6BB` | Stateful diagnostic, submodule 6, overlay 2; paired A/B entrypoints share overlay-local state |
| sample3 | `S6CA` | Stateful diagnostic, submodule 6, overlay 3; paired A/B entrypoints share overlay-local state |
| sample3 | `S6CB` | Stateful diagnostic, submodule 6, overlay 3; paired A/B entrypoints share overlay-local state |
| sample3 | `S6DA` | Stateful diagnostic, submodule 6, overlay 4; paired A/B entrypoints share overlay-local state |
| sample3 | `S6DB` | Stateful diagnostic, submodule 6, overlay 4; paired A/B entrypoints share overlay-local state |
| sample3 | `S6EA` | Stateful diagnostic, submodule 6, overlay 5; paired A/B entrypoints share overlay-local state |
| sample3 | `S6EB` | Stateful diagnostic, submodule 6, overlay 5; paired A/B entrypoints share overlay-local state |
| sample3 | `S7AA` | Stateful diagnostic, submodule 7, overlay 1; paired A/B entrypoints share overlay-local state |
| sample3 | `S7AB` | Stateful diagnostic, submodule 7, overlay 1; paired A/B entrypoints share overlay-local state |
| sample3 | `S7BA` | Stateful diagnostic, submodule 7, overlay 2; paired A/B entrypoints share overlay-local state |
| sample3 | `S7BB` | Stateful diagnostic, submodule 7, overlay 2; paired A/B entrypoints share overlay-local state |
| sample3 | `S7CA` | Stateful diagnostic, submodule 7, overlay 3; paired A/B entrypoints share overlay-local state |
| sample3 | `S7CB` | Stateful diagnostic, submodule 7, overlay 3; paired A/B entrypoints share overlay-local state |
| sample3 | `S7DA` | Stateful diagnostic, submodule 7, overlay 4; paired A/B entrypoints share overlay-local state |
| sample3 | `S7DB` | Stateful diagnostic, submodule 7, overlay 4; paired A/B entrypoints share overlay-local state |
| sample3 | `S7EA` | Stateful diagnostic, submodule 7, overlay 5; paired A/B entrypoints share overlay-local state |
| sample3 | `S7EB` | Stateful diagnostic, submodule 7, overlay 5; paired A/B entrypoints share overlay-local state |
| sample3 | `S8AA` | Stateful diagnostic, submodule 8, overlay 1; paired A/B entrypoints share overlay-local state |
| sample3 | `S8AB` | Stateful diagnostic, submodule 8, overlay 1; paired A/B entrypoints share overlay-local state |
| sample3 | `S8BA` | Stateful diagnostic, submodule 8, overlay 2; paired A/B entrypoints share overlay-local state |
| sample3 | `S8BB` | Stateful diagnostic, submodule 8, overlay 2; paired A/B entrypoints share overlay-local state |
| sample3 | `S8CA` | Stateful diagnostic, submodule 8, overlay 3; paired A/B entrypoints share overlay-local state |
| sample3 | `S8CB` | Stateful diagnostic, submodule 8, overlay 3; paired A/B entrypoints share overlay-local state |
| sample3 | `S8DA` | Stateful diagnostic, submodule 8, overlay 4; paired A/B entrypoints share overlay-local state |
| sample3 | `S8DB` | Stateful diagnostic, submodule 8, overlay 4; paired A/B entrypoints share overlay-local state |
| sample3 | `S8EA` | Stateful diagnostic, submodule 8, overlay 5; paired A/B entrypoints share overlay-local state |
| sample3 | `S8EB` | Stateful diagnostic, submodule 8, overlay 5; paired A/B entrypoints share overlay-local state |

Sample3 tests whether overlay-local state survives reuse and resets after
another image replaces it. It is temporary state, rather than application storage.
Workers live in `src/apps/readybasic/sample_low.s`; packaging and generated
slot/overlay payloads are in `build_support/build_readybasic_disk_modules.py`.

## Packaging by profile

This inventory describes source configuration, not proof that every existing
binary image has been rebuilt. Example counts exclude applications/utilities.

| Profile | Example PRGs | Disk packages |
| --- | ---: | --- |
| `precog-d81-rsdebug` | 27 | `rbm.sample1`, `rbm.sample2`, `rbm.sample3` |
| `precog-d81` | 44 | `rbm.media`, `rbm.sample1`, `rbm.sample2`, `rbm.sample3` |
| `precog-dual-d64` | 1 | `rbm.sample1` |
| `precog-dual-d71-rsdebug` | 41 | `rbm.sample1`, `rbm.sample2`, `rbm.sample3` |
| `precog-dual-d71` | 41 | `rbm.sample1`, `rbm.sample2`, `rbm.sample3` |
| `precog-kung-fu-flash-2-d81` | 41 | `rbm.sample1`, `rbm.sample2`, `rbm.sample3` |
| `precog-solo-d64-a` | 1 | `rbm.sample1` |
| `precog-solo-d64-b` | 1 | `rbm.sample1` |
| `precog-solo-d64-c` | 1 | `rbm.sample1` |
| `precog-solo-d64-d` | 1 | `rbm.sample1` |
| `precog-solo-d64-e-rsdebug` | 1 | `rbm.sample1` |
| `precog-solo-d64-e` | 1 | `rbm.sample1` |
| `precog-solo-d64-readybasic` | 41 | `rbm.sample1`, `rbm.sample2`, `rbm.sample3` |
| `precog-ultimate` | 44 | `rbm.media`, `rbm.sample1`, `rbm.sample2`, `rbm.sample3` |

The Ultimate SKU is a D81 pair: all 44 BASIC examples are on drive 9;
ReadyBASIC itself, RBM packages and media assets remain on drive 8.
For example, use `LOAD"RBUGFXSNDDEMO",9` then `RUN`.

EasyFlash preloads the runtime from CRT, but its companion data disk does
not currently package this BASIC example/media collection. Supplying the
runtime on cartridge is different from supplying the disk-loadable assets.

## Every example at a glance

| Example | Topic |
| --- | --- |
| [rbgfx01_modes](#rbgfx01-modes) | Switch/query all five graphics modes, then restore text. |
| [rbgfx02_hires_plot](#rbgfx02-hires-plot) | Plot a hires grid and diagonal, waiting for a key before exit. |
| [rbgfx03_hires_lines](#rbgfx03-hires-lines) | Draw intersecting hires line fans and restore text after a key. |
| [rbgfx04_rects](#rbgfx04-rects) | Compare rectangle outlines with filled boxes. |
| [rbgfx05_point_read](#rbgfx05-point-read) | Read a plotted pixel, its neighbor, then the cleared pixel with PNT. |
| [rbgfx06_reu_surface](#rbgfx06-reu-surface) | Early surface-handle probe; it predates capture/blit and does not initialize its new surface. |
| [rbgfx07_mbitmap](#rbgfx07-mbitmap) | Draw multicolor lines and pixels using the immediate primitive color encoding. |
| [rbgfx08_tile](#rbgfx08-tile) | Use plot/rectangle primitives on character cells in tile mode. |
| [rbgfx09_sprites](#rbgfx09-sprites) | Configure two generated sprites, position them and change color. |
| [rbgfx10_collision](#rbgfx10-collision) | Overlap sprites and inspect collision state; retained polling probe. |
| [rbgfx11_input](#rbgfx11-input) | Poll joystick and keyboard values without an IRQ input sampler. |
| [rbgfx12_showcase](#rbgfx12-showcase) | Combine hires lines, boxes, sprite positioning and pixel readback. |
| [rbgfx13_sprite_steps](#rbgfx13-sprite-steps) | Construct sprite art row by row and step through positions/colors with keys. |
| [rbgfx14_phase2_prims](#rbgfx14-phase2-prims) | Compare circle outline with the current rectangular FCIRCLE placeholder. |
| [rbgfx15_phase2_tiles](#rbgfx15-phase2-tiles) | Write visible character/color tiles using TILE and CHARAT. |
| [rbgfx16_phase2_sprite_ctrl](#rbgfx16-phase2-sprite-ctrl) | Step through sprite expansion, multicolor, priority and color controls. |
| [rbgfx17_poly_array](#rbgfx17-poly-array) | Outline coordinate-pair polygons held in BASIC integer arrays. |
| [rbgfx18_fpoly_array](#rbgfx18-fpoly-array) | Fill convex polygons held in BASIC integer arrays. |
| [rbgfx19_poly_reu](#rbgfx19-poly-reu) | Allocate REU point buffers, set indexed vertices and draw outlines. |
| [rbgfx20_fpoly_reu_showcase](#rbgfx20-fpoly-reu-showcase) | Fill REU-backed polygons alongside lines and rectangles. |
| [rbgfx21_mbitmap_prims](#rbgfx21-mbitmap-prims) | Exercise immediate primitives and color slots in multicolor bitmap mode. |
| [rbgfx22_mbitmap_point](#rbgfx22-mbitmap-point) | Check that PNT returns multicolor pixel slot codes 1, 2 and 3. |
| [rbgfx23_dlist](#rbgfx23-dlist) | Record plot/line/rectangle/box commands and replay a retained display list. |
| [rbgfx24_tilemap](#rbgfx24-tilemap) | Build a custom charset, tileset and tilemap, then render them together. |
| [rbgfx25_mbcells](#rbgfx25-mbcells) | Change multicolor cell palette attributes independently of bitmap drawing. |
| [rbgfx26_mode_matrix](#rbgfx26-mode-matrix) | Compare visible primitives and PNT across hires, multicolor and tile modes. |
| [rbgfx27_target_blit](#rbgfx27-target-blit) | Probe surface selection, capture/blit and error reporting before visible drawing. |
| [rbgfx28_tile_visible](#rbgfx28-tile-visible) | Draw and read back ordinary visible tile cells. |
| [rbgfx29_mtile_visible](#rbgfx29-mtile-visible) | Draw and read back multicolor tile cells. |
| [rbgfx30_mbitmap_dlist](#rbgfx30-mbitmap-dlist) | Replay a retained display list in multicolor bitmap mode. |
| [rbgfx31_sync_blit](#rbgfx31-sync-blit) | Capture a visible hires scene into REU, clear it and restore it with a key. |
| [rbgfx32_convex_poly](#rbgfx32-convex-poly) | Draw a polygon outline and a convex filled polygon from integer arrays. |
| [rbgfxsnddemo](#rbgfxsnddemo) | Orbital Echoes: two cached scenes, READY sprites, palette-safe lines and PAL music; no Ultimate speed calls. |
| [rbproc1](#rbproc1) | Structured routine/function and typed-result regression examples, including nested expressions. |
| [rbprocerr](#rbprocerr) | Intentional negative cases; run individual numbered sections to test argument/type/stack errors. |
| [rbsnd01_sid_basics](#rbsnd01-sid-basics) | Hear separate triangle, saw, pulse and noise tones using immediate SID control. |
| [rbsnd02_voice_state](#rbsnd02-voice-state) | Change pulse width and envelope, gating voices explicitly. |
| [rbsnd03_notes](#rbsnd03-notes) | Play a chromatic scale with the PAL pitch table. |
| [rbsnd04_filter](#rbsnd04-filter) | Compare low-pass, band-pass and high-pass SID filtering. |
| [rbsnd05_voice_batch](#rbsnd05-voice-batch) | Set frequency, waveform and packed envelopes together with VOICE. |
| [rbsnd06_three_voice](#rbsnd06-three-voice) | Combine three voices into a chord, then a bass/high-voice pair. |
| [rbsnd07_psid](#rbsnd07-psid) | Load the media module and vetted PSID; play, halt, restart and release its arena. |
| [rbsnd08_neon](#rbsnd08-neon) | Historical single-scene clock-paced music/graphics demo; use the newer combined demos. |
| [rbtest1](#rbtest1) | Small command-expression, integer-output and uppercase-string smoke test. |
| [rbugfxsnddemo](#rbugfxsnddemo) | Ultimate Orbital Echoes: the same media lifecycle, with safe-speed disk loading and 64 MHz computation/animation. |

## rbgfx01_modes {#rbgfx01-modes}

Switch/query all five graphics modes, then restore text.

[Original source](../src/apps/readybasic/rbgfx01_modes.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`.

Disk name: `RBGFX01`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx01 modes"
30 gfxmode("hires"):m%=gfxmode():print "hires mode";m%
40 gfxclear(0)
50 gfxmode("mbitmap"):m%=gfxmode():print "mbitmap mode";m%
60 gfxclear(1)
70 gfxmode("tile"):m%=gfxmode():print "tile mode";m%
80 gfxclear(2)
90 gfxmode("mtile"):m%=gfxmode():print "mtile mode";m%
100 gfxclear(3)
110 gfxtext()
120 print "back to text"
```

## rbgfx02_hires_plot {#rbgfx02-hires-plot}

Plot a hires grid and diagonal, waiting for a key before exit.

[Original source](../src/apps/readybasic/rbgfx02_hires_plot.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `PAUSE`, `PLOT`.

Disk name: `RBGFX02`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx02 hires plot"
30 gfxmode("hires"):gfxclear(0)
40 for x=0 to 319 step 16
50 plot(x,0,1):plot(x,199,1)
60 next x
70 for y=0 to 199 step 16
80 plot(0,y,1):plot(319,y,1)
90 next y
100 for i=0 to 199
110 plot(i,i,1)
120 next i
130 print "grid and diagonal"
140 pause(30)
150 get a$:if a$="" then 140
160 gfxtext():print "rbgfx02 complete"
```

## rbgfx03_hires_lines {#rbgfx03-hires-lines}

Draw intersecting hires line fans and restore text after a key.

[Original source](../src/apps/readybasic/rbgfx03_hires_lines.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`.

Disk name: `RBGFX03`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx03 hires lines"
30 gfxmode("hires"):gfxclear(0)
40 for y=0 to 199 step 20
50 line(0,0,319,y,1)
60 next y
70 for x=0 to 319 step 32
80 line(319,199,x,0,1)
90 next x
100 line(0,199,319,0,1)
110 print "fan lines"
120 pause(30)
130 get a$:if a$="" then 120
140 gfxtext():print "rbgfx03 complete"
```

## rbgfx04_rects {#rbgfx04-rects}

Compare rectangle outlines with filled boxes.

[Original source](../src/apps/readybasic/rbgfx04_rects.bas).

Commands used: `FBOX`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `RECT`.

Disk name: `RBGFX04`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx04 rects"
30 gfxmode("hires"):gfxclear(0)
40 rect(8,8,311,191,1)
50 rect(32,24,288,176,1)
60 fbox(64,48,128,96,1)
70 fbox(192,96,256,160,1)
80 line(0,0,319,199,1)
90 line(0,199,319,0,1)
100 print "outline and filled rects"
110 pause(30)
120 get a$:if a$="" then 110
130 gfxtext():print "rbgfx04 complete"
```

## rbgfx05_point_read {#rbgfx05-point-read}

Read a plotted pixel, its neighbor, then the cleared pixel with PNT.

[Original source](../src/apps/readybasic/rbgfx05_point_read.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `PAUSE`, `PLOT`, `PNT`.

Disk name: `RBGFX05`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx05 pnt read"
30 gfxmode("hires"):gfxclear(0)
40 plot(40,40,1)
50 pnt(40,40,a%)
60 pnt(41,40,b%)
70 print "pnt 40,40";a%
80 print "pnt 41,40";b%
90 plot(40,40,0)
100 pnt(40,40,c%)
110 print "after clear";c%
120 pause(30)
130 get a$:if a$="" then 120
140 gfxtext():print "rbgfx05 complete"
```

## rbgfx06_reu_surface {#rbgfx06-reu-surface}

Early surface-handle probe; it predates capture/blit and does not initialize its new surface.

[Original source](../src/apps/readybasic/rbgfx06_reu_surface.bas).

Commands used: `GFXBLIT`, `GFXCLEAR`, `GFXMODE`, `GFXSURF`, `GFXTEXT`, `LINE`.

Disk name: `RBGFX06`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx06 reu surface"
30 h%=gfxsurf("hires")
40 print "surface handle";h%
50 gfxmode("hires"):gfxclear(0)
60 line(0,0,319,199,1)
70 gfxblit(h%)
80 print "blit validates handle in phase 1"
90 gfxtext()
```

## rbgfx07_mbitmap {#rbgfx07-mbitmap}

Draw multicolor lines and pixels using the immediate primitive color encoding.

[Original source](../src/apps/readybasic/rbgfx07_mbitmap.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `PLOT`.

Disk name: `RBGFX07`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx07 mbitmap"
30 gfxmode("mbitmap"):gfxclear(0)
40 for y=10 to 190 step 12
50 line(0,y,159,199-y,7)
60 next y
70 for x=16 to 144 step 16
80 plot(x,100,55)
90 next x
100 print "multicolor bitmap register path"
110 pause(30)
120 get a$:if a$="" then 110
130 gfxtext():print "rbgfx07 complete"
```

## rbgfx08_tile {#rbgfx08-tile}

Use plot/rectangle primitives on character cells in tile mode.

[Original source](../src/apps/readybasic/rbgfx08_tile.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `PAUSE`, `PLOT`, `PNT`, `RECT`.

Disk name: `RBGFX08`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx08 tile"
30 gfxmode("tile"):gfxclear(0)
40 for y=0 to 23 step 2
50 for x=0 to 38 step 2
60 plot(x,y,1):plot(x+1,y+1,1)
70 next x
80 next y
90 rect(2,2,37,22,5)
100 pnt(3,3,a%)
110 print "tile cells use plot";a%
120 pause(30)
130 get a$:if a$="" then 120
140 gfxtext():print "rbgfx08 complete"
```

## rbgfx09_sprites {#rbgfx09-sprites}

Configure two generated sprites, position them and change color.

[Original source](../src/apps/readybasic/rbgfx09_sprites.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `SPRCOL`, `SPRMOVE`, `SPRSET`.

Disk name: `RBGFX09`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx09 sprites"
30 gfxmode("tile"):gfxclear(0)
40 sprset(0,1,2,0)
50 sprset(1,1,7,1)
60 sprmove(0,80,80)
70 sprmove(1,180,110)
80 sprcol(0,5)
90 for t=1 to 60000:next t
100 sprset(0,0,0,0):sprset(1,0,0,1)
110 gfxtext():print "rbgfx09 complete"
```

## rbgfx10_collision {#rbgfx10-collision}

Overlap sprites and inspect collision state; retained polling probe.

[Original source](../src/apps/readybasic/rbgfx10_collision.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `SPRCOLL`, `SPRMOVE`, `SPRSCAN`, `SPRSET`.

Disk name: `RBGFX10`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx10 collision"
30 gfxmode("tile"):gfxclear(0)
40 sprset(0,1,2,0)
50 sprset(1,1,3,0)
60 sprmove(0,120,100)
70 sprmove(1,124,104)
80 for t=1 to 60000:next t
90 sprcoll(0,c%):gfxtext()
100 print "sprite collision";c%
110 sprscan()
```

## rbgfx11_input {#rbgfx11-input}

Poll joystick and keyboard values without an IRQ input sampler.

[Original source](../src/apps/readybasic/rbgfx11_input.bas).

Commands used: `JOY`, `KEYLAST`, `KEYP`, `KEYSCAN`.

Disk name: `RBGFX11`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx11 input"
30 for i=1 to 30
40 joy(2,j%)
50 keyp(k%)
60 keylast(l%)
70 print "joy";j%;" key";k%;" last";l%
80 keyscan()
90 next i
100 print "polling only, no irq sampler"
```

## rbgfx12_showcase {#rbgfx12-showcase}

Combine hires lines, boxes, sprite positioning and pixel readback.

[Original source](../src/apps/readybasic/rbgfx12_showcase.bas).

Commands used: `FBOX`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `PNT`, `RECT`, `SPRMOVE`, `SPRSET`.

Disk name: `RBGFX12`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx12 showcase"
30 gfxmode("hires"):gfxclear(0)
40 rect(4,4,315,195,1)
50 for y=16 to 184 step 16
60 line(8,y,311,199-y,1)
70 next y
80 fbox(136,72,184,120,1)
90 sprset(0,1,6,2):sprmove(0,160,100)
100 pnt(160,100,p%)
110 print "center pnt";p%
120 print "showcase done"
130 pause(30)
140 get a$:if a$="" then 130
150 gfxtext():print "rbgfx12 complete"
```

## rbgfx13_sprite_steps {#rbgfx13-sprite-steps}

Construct sprite art row by row and step through positions/colors with keys.

[Original source](../src/apps/readybasic/rbgfx13_sprite_steps.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `PAUSE`, `SPRMOVE`, `SPRROW`, `SPRSET`.

Disk name: `RBGFX13`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx13 sprite steps"
30 gfxmode("tile"):gfxclear(0)
40 sprset(0,1,2,0):sprset(1,1,7,0)
50 sprrow(0,0,0,24,0):sprrow(0,1,0,60,0)
60 sprrow(0,2,0,126,0):sprrow(0,3,0,255,0)
70 sprrow(0,4,1,255,128):sprrow(0,5,3,255,192)
80 sprrow(0,6,7,255,224):sprrow(0,7,15,255,240)
90 sprrow(0,8,31,255,248):sprrow(0,9,63,255,252)
100 sprrow(0,10,127,255,254):sprrow(0,11,63,255,252)
110 sprrow(0,12,31,255,248):sprrow(0,13,15,255,240)
120 sprrow(0,14,7,255,224):sprrow(0,15,3,255,192)
130 sprrow(0,16,1,255,128):sprrow(0,17,0,255,0)
140 sprrow(0,18,0,126,0):sprrow(0,19,0,60,0)
150 sprrow(0,20,0,24,0)
160 sprrow(1,0,255,255,0):sprrow(1,1,128,1,0)
170 sprrow(1,2,191,253,0):sprrow(1,3,160,5,0)
180 sprrow(1,4,175,245,0):sprrow(1,5,168,21,0)
190 sprrow(1,6,171,85,0):sprrow(1,7,170,85,0)
200 sprrow(1,8,171,85,0):sprrow(1,9,168,21,0)
210 sprrow(1,10,175,245,0):sprrow(1,11,160,5,0)
220 sprrow(1,12,191,253,0):sprrow(1,13,128,1,0)
230 sprrow(1,14,255,255,0):sprrow(1,15,0,0,0)
240 sprrow(1,16,255,255,0):sprrow(1,17,128,1,0)
250 sprrow(1,18,255,255,0):sprrow(1,19,0,0,0)
260 sprrow(1,20,255,255,0)
270 sprmove(0,60,70):sprmove(1,190,110)
280 pause(30)
290 get a$:if a$="" then 280
300 sprmove(0,120,88):sprmove(1,145,110)
310 pause(30)
320 get a$:if a$="" then 310
330 sprset(0,1,5,0):sprset(1,1,3,0)
340 pause(30)
350 get a$:if a$="" then 340
360 gfxtext():print "sprite demo done"
```

## rbgfx14_phase2_prims {#rbgfx14-phase2-prims}

Compare circle outline with the current rectangular FCIRCLE placeholder.

[Original source](../src/apps/readybasic/rbgfx14_phase2_prims.bas).

Commands used: `CIRCLE`, `FCIRCLE`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `RECT`.

Disk name: `RBGFX14`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx14 phase2 prims"
30 gfxmode("hires"):gfxclear(0)
40 circle(82,82,38,1)
50 fcircle(210,92,26,1)
60 rect(180,58,240,126,1)
70 line(20,170,300,170,1)
80 pause(30)
90 get a$:if a$="" then 80
100 gfxtext():print "phase2 prims done"
```

## rbgfx15_phase2_tiles {#rbgfx15-phase2-tiles}

Write visible character/color tiles using TILE and CHARAT.

[Original source](../src/apps/readybasic/rbgfx15_phase2_tiles.bas).

Commands used: `CHARAT`, `GFXTEXT`, `PAUSE`, `TILE`.

Disk name: `RBGFX15`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx15 phase2 tiles"
30 gfxtext():print chr$(147)
40 for y=4 to 16
50 for x=6 to 33
60 tile(x,y,160,1+(x+y)-int((x+y)/8)*8)
70 next x
80 next y
90 charat(10,7,160,2):charat(11,7,160,3)
100 charat(12,7,160,4):charat(13,7,160,5)
110 charat(10,9,160,6):charat(11,9,160,7)
120 charat(12,9,160,8):charat(13,9,160,9)
130 pause(30)
140 get a$:if a$="" then 130
150 gfxtext():print "phase2 tiles done"
```

## rbgfx16_phase2_sprite_ctrl {#rbgfx16-phase2-sprite-ctrl}

Step through sprite expansion, multicolor, priority and color controls.

[Original source](../src/apps/readybasic/rbgfx16_phase2_sprite_ctrl.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `PAUSE`, `SPRCOL`, `SPRMCO`, `SPRMOVE`, `SPRMUL`, `SPRPRI`, `SPRROW`, `SPRSET`, `SPRSIZE`, `TILE`.

Disk name: `RBGFX16`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx16 sprite controls"
30 gfxmode("tile"):gfxclear(0)
40 for y=7 to 12
50 for x=13 to 24
60 tile(x,y,160,6)
70 next x
80 next y
90 sprset(0,1,2,0):sprmove(0,120,86)
100 sprrow(0,0,0,60,0):sprrow(0,1,0,126,0)
110 sprrow(0,2,0,255,0):sprrow(0,3,1,255,128)
120 sprrow(0,4,3,255,192):sprrow(0,5,7,255,224)
130 sprrow(0,6,15,255,240):sprrow(0,7,31,255,248)
140 sprrow(0,8,63,255,252):sprrow(0,9,127,255,254)
150 sprrow(0,10,255,255,255):sprrow(0,11,127,255,254)
160 sprrow(0,12,63,255,252):sprrow(0,13,31,255,248)
170 sprrow(0,14,15,255,240):sprrow(0,15,7,255,224)
180 sprrow(0,16,3,255,192):sprrow(0,17,1,255,128)
190 sprrow(0,18,0,255,0):sprrow(0,19,0,126,0)
200 sprrow(0,20,0,60,0)
210 pause(30)
220 get a$:if a$="" then 210
230 sprsize(0,1,1):sprmove(0,134,92)
240 pause(30)
250 get a$:if a$="" then 240
260 sprmco(5,14)
270 sprmul(0,1)
280 sprpri(0,1)
290 sprcol(0,3)
300 pause(30)
310 get a$:if a$="" then 300
320 gfxtext():print "phase2 sprites done"
```

## rbgfx17_poly_array {#rbgfx17-poly-array}

Outline coordinate-pair polygons held in BASIC integer arrays.

[Original source](../src/apps/readybasic/rbgfx17_poly_array.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `POLY`.

Disk name: `RBGFX17`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 rem demo 17
30 dim p%(7),q%(9)
40 gfxmode("hires"):gfxclear(0)
50 p%(0)=50:p%(1)=35:p%(2)=140:p%(3)=35
60 p%(4)=170:p%(5)=105:p%(6)=85:p%(7)=145
70 poly(p%(0),4,1)
80 q%(0)=190:q%(1)=30:q%(2)=235:q%(3)=55
90 q%(4)=245:q%(5)=125:q%(6)=220:q%(7)=155
100 q%(8)=175:q%(9)=95
110 poly(q%(0),5,1)
120 line(20,175,250,175,1)
130 pause(30)
140 get a$:if a$="" then 130
150 gfxtext():print "phase3 done 17"
```

## rbgfx18_fpoly_array {#rbgfx18-fpoly-array}

Fill convex polygons held in BASIC integer arrays.

[Original source](../src/apps/readybasic/rbgfx18_fpoly_array.bas).

Commands used: `FPOLY`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `PAUSE`, `RECT`.

Disk name: `RBGFX18`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 rem demo 18
30 dim p%(7),q%(7)
40 gfxmode("hires"):gfxclear(0)
50 p%(0)=45:p%(1)=48:p%(2)=130:p%(3)=34
60 p%(4)=165:p%(5)=118:p%(6)=70:p%(7)=150
70 fpoly(p%(0),4,1)
80 q%(0)=185:q%(1)=42:q%(2)=245:q%(3)=70
90 q%(4)=230:q%(5)=150:q%(6)=175:q%(7)=115
100 fpoly(q%(0),4,1)
110 rect(18,22,252,176,1)
120 pause(30)
130 get a$:if a$="" then 120
140 gfxtext():print "phase3 done 18"
```

## rbgfx19_poly_reu {#rbgfx19-poly-reu}

Allocate REU point buffers, set indexed vertices and draw outlines.

[Original source](../src/apps/readybasic/rbgfx19_poly_reu.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `PAUSE`, `PBMAKE`, `PBUFSET`, `POLYH`.

Disk name: `RBGFX19`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 rem demo 19
30 gfxmode("hires"):gfxclear(0)
40 pbmake(5,h%)
50 pbufset(h%,0,70,40):pbufset(h%,1,155,45)
60 pbufset(h%,2,180,115):pbufset(h%,3,105,155)
70 pbufset(h%,4,45,95)
80 polyh(h%,5,1)
90 pbmake(3,g%)
100 pbufset(g%,0,215,45):pbufset(g%,1,245,135)
110 pbufset(g%,2,190,150)
120 polyh(g%,3,1)
130 rem demo
140 pause(30)
150 get a$:if a$="" then 140
160 gfxtext():print "phase3 done 19"
```

## rbgfx20_fpoly_reu_showcase {#rbgfx20-fpoly-reu-showcase}

Fill REU-backed polygons alongside lines and rectangles.

[Original source](../src/apps/readybasic/rbgfx20_fpoly_reu_showcase.bas).

Commands used: `FPOLYH`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `PBMAKE`, `PBUFSET`, `RECT`.

Disk name: `RBGFX20`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 rem demo 20
30 gfxmode("hires"):gfxclear(0)
40 pbmake(4,h%)
50 pbufset(h%,0,42,58):pbufset(h%,1,118,35)
60 pbufset(h%,2,168,120):pbufset(h%,3,72,156)
70 fpolyh(h%,4,1)
80 pbmake(5,g%)
90 pbufset(g%,0,185,42):pbufset(g%,1,244,55)
100 pbufset(g%,2,248,112):pbufset(g%,3,226,158)
110 pbufset(g%,4,170,106)
120 fpolyh(g%,5,1)
130 line(15,182,250,182,1):rect(176,24,252,170,1)
140 rem demo
150 pause(30)
160 get a$:if a$="" then 150
170 gfxtext():print "phase3 done 20"
```

## rbgfx21_mbitmap_prims {#rbgfx21-mbitmap-prims}

Exercise immediate primitives and color slots in multicolor bitmap mode.

[Original source](../src/apps/readybasic/rbgfx21_mbitmap_prims.bas).

Commands used: `CIRCLE`, `FBOX`, `FCIRCLE`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `PLOT`, `RECT`.

Disk name: `RBGFX21`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx21 mbitmap prims"
30 gfxmode("mbitmap"):gfxclear(0)
40 line(4,18,155,18,17)
50 line(4,22,155,70,34)
60 rect(8,82,72,144,20)
70 fbox(86,82,148,144,37)
80 circle(46,112,28,51)
90 fcircle(118,112,22,58)
100 for x=0 to 159 step 8
110 plot(x,170,16+(x/8)-int((x/8)/15)*15)
120 next x
130 pause(30)
140 get a$:if a$="" then 130
150 gfxtext():print "mbitmap prims done"
```

## rbgfx22_mbitmap_point {#rbgfx22-mbitmap-point}

Check that PNT returns multicolor pixel slot codes 1, 2 and 3.

[Original source](../src/apps/readybasic/rbgfx22_mbitmap_point.bas).

Commands used: `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PAUSE`, `PLOT`, `PNT`.

Disk name: `RBGFX22`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx22 mbitmap pnt"
30 gfxmode("mbitmap"):gfxclear(0)
40 plot(20,40,17):plot(24,40,34):plot(28,40,51)
50 pnt(20,40,a%):pnt(24,40,b%):pnt(28,40,c%)
60 line(12,80,148,80,17)
70 line(12,92,148,124,34)
80 line(12,150,148,108,51)
90 pause(30)
100 get a$:if a$="" then 90
110 gfxtext():print chr$(147);"mbitmap pnt:";a%;b%;c%
120 if a%<>1 or b%<>2 or c%<>3 then print "?mbitmap pnt fail":stop
130 print "mbitmap pnt done"
```

## rbgfx23_dlist {#rbgfx23-dlist}

Record plot/line/rectangle/box commands and replay a retained display list.

[Original source](../src/apps/readybasic/rbgfx23_dlist.bas).

Commands used: `DLDRAW`, `DLFBOX`, `DLLINE`, `DLMAKE`, `DLPLOT`, `DLRECT`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`.

Disk name: `RBGFX23`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx23 dlist"
30 gfxmode("hires"):gfxclear(0)
40 dlmake(12,h%)
50 dlplot(h%,80,60,1)
60 dlline(h%,20,20,300,160)
70 dlrect(h%,40,50,140,130)
80 dlfbox(h%,180,70,260,140)
90 dldraw(h%)
100 get a$:if a$="" then 100
110 gfxtext():print "phase4 dlist done"
```

## rbgfx24_tilemap {#rbgfx24-tilemap}

Build a custom charset, tileset and tilemap, then render them together.

[Original source](../src/apps/readybasic/rbgfx24_tilemap.bas).

Commands used: `CHRMAKE`, `CHRROW`, `CHRUSE`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `TMDRAW`, `TMMAKE`, `TMSET`, `TSMAKE`, `TSSET`.

Disk name: `RBGFX24`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx24 tilemap"
30 gfxmode("tile"):gfxclear(0)
40 chrmake(256,c%)
50 for r=0 to 7:chrrow(c%,1,r,255):next
60 chrrow(c%,2,0,255):chrrow(c%,2,1,129)
70 chrrow(c%,2,2,189):chrrow(c%,2,3,165)
80 chrrow(c%,2,4,165):chrrow(c%,2,5,189)
90 chrrow(c%,2,6,129):chrrow(c%,2,7,255)
100 chrrow(c%,3,0,24):chrrow(c%,3,1,60)
110 chrrow(c%,3,2,126):chrrow(c%,3,3,219)
120 chrrow(c%,3,4,24):chrrow(c%,3,5,24)
130 chrrow(c%,3,6,60):chrrow(c%,3,7,0)
140 for r=0 to 7:chrrow(c%,4,r,170):next
150 chruse(c%)
160 tsmake(64,t%):tmmake(1000,m%)
170 tsset(t%,0,32,0)
180 tsset(t%,1,1,2)
190 tsset(t%,2,2,5)
200 tsset(t%,3,3,7)
210 tsset(t%,4,4,1)
220 for x=0 to 39
230 tmset(m%,x,1,0):tmset(m%,960+x,1,0)
240 next
250 for y=0 to 24
260 i=y*40:tmset(m%,i,1,0):tmset(m%,i+39,1,0)
270 next
280 for x=6 to 33
290 tmset(m%,160+x,2,0):tmset(m%,320+x,3,0)
300 tmset(m%,480+x,4,0)
310 next
320 tmdraw(m%,t%)
330 get a$:if a$="" then 330
340 gfxtext():print "phase4 tilemap done"
```

## rbgfx25_mbcells {#rbgfx25-mbcells}

Change multicolor cell palette attributes independently of bitmap drawing.

[Original source](../src/apps/readybasic/rbgfx25_mbcells.bas).

Commands used: `FBOX`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `MCBG`, `MCELL`, `RECT`.

Disk name: `RBGFX25`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx25 mbitmap cells"
30 gfxmode("mbitmap"):gfxclear(0):mcbg(0)
40 fbox(8,16,31,47,17)
50 fbox(40,16,63,47,34)
60 fbox(72,16,95,47,51)
70 mcell(2,2,6,10,2)
80 mcell(10,2,4,12,7)
90 mcell(18,2,1,14,5)
100 line(4,120,150,170,49)
110 rect(20,72,140,112,50)
120 get a$:if a$="" then 120
130 gfxtext():print "phase4 mbitmap cells done"
```

## rbgfx26_mode_matrix {#rbgfx26-mode-matrix}

Compare visible primitives and PNT across hires, multicolor and tile modes.

[Original source](../src/apps/readybasic/rbgfx26_mode_matrix.bas).

Commands used: `CIRCLE`, `FBOX`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `MCBG`, `PLOT`, `PNT`, `RECT`.

Disk name: `RBGFX26`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx26 mode matrix"
30 gfxmode("hires"):gfxclear(0)
40 plot(20,20,1):line(4,36,300,180,1)
50 rect(32,48,138,112,1):fbox(178,72,260,140,1)
60 circle(82,150,28,1):pnt(20,20,a%)
70 get a$:if a$="" then 70
80 gfxmode("mbitmap"):gfxclear(0):mcbg(0)
90 plot(20,20,17):line(4,36,155,180,34)
100 rect(8,70,70,128,20):fbox(88,78,148,142,51)
110 circle(48,150,24,49):pnt(20,20,b%)
120 get a$:if a$="" then 120
130 gfxmode("tile"):gfxclear(0)
140 plot(4,4,5):line(1,1,38,20,6)
150 rect(3,5,20,16,7):fbox(24,8,34,18,2)
160 pnt(4,4,c%)
170 get a$:if a$="" then 170
180 gfxmode("mtile"):gfxclear(0)
190 plot(4,4,9):line(1,22,38,2,10)
200 rect(5,4,21,17,11):fbox(25,7,35,19,12)
210 pnt(4,4,d%)
220 get a$:if a$="" then 220
230 gfxtext():print "matrix";a%;b%;c%;d%
240 print "mode matrix done"
```

## rbgfx27_target_blit {#rbgfx27-target-blit}

Probe surface selection, capture/blit and error reporting before visible drawing.

[Original source](../src/apps/readybasic/rbgfx27_target_blit.bas).

Commands used: `ERRCODE`, `GFXBLIT`, `GFXCLEAR`, `GFXMODE`, `GFXSURF`, `GFXSYNC`, `GFXTEXT`, `GFXTGT`, `LINE`, `RECT`.

Disk name: `RBGFX27`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx27 target blit"
30 h%=gfxsurf("hires")
40 print "surf";h%;" err";errcode()
50 gfxtgt(h%):print "target surf err";errcode()
60 gfxsync():print "sync err";errcode()
70 gfxblit(h%):print "blit err";errcode()
80 gfxtgt(0):print "target visible err";errcode()
90 gfxmode("hires"):gfxclear(0)
100 line(0,0,319,199,1):rect(40,40,220,150,1)
110 get a$:if a$="" then 110
120 gfxtext():print "target blit done"
```

## rbgfx28_tile_visible {#rbgfx28-tile-visible}

Draw and read back ordinary visible tile cells.

[Original source](../src/apps/readybasic/rbgfx28_tile_visible.bas).

Commands used: `FBOX`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PLOT`, `PNT`, `RECT`.

Disk name: `RBGFX28`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx28 tile visible"
30 gfxmode("tile"):gfxclear(0)
40 plot(4,4,5):plot(5,4,6):plot(6,4,7)
50 line(1,1,38,20,3)
60 rect(3,5,20,16,10):fbox(24,8,34,18,12)
70 pnt(4,4,a%)
80 get a$:if a$="" then 80
90 gfxtext():print "tile visible";a%
```

## rbgfx29_mtile_visible {#rbgfx29-mtile-visible}

Draw and read back multicolor tile cells.

[Original source](../src/apps/readybasic/rbgfx29_mtile_visible.bas).

Commands used: `FBOX`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `LINE`, `PLOT`, `PNT`, `RECT`.

Disk name: `RBGFX29`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx29 mtile visible"
30 gfxmode("mtile"):gfxclear(0)
40 plot(4,4,9):plot(5,4,10):plot(6,4,11)
50 line(1,22,38,2,6)
60 rect(5,4,21,17,13):fbox(25,7,35,19,15)
70 pnt(4,4,a%)
80 get a$:if a$="" then 80
90 gfxtext():print "mtile visible";a%
```

## rbgfx30_mbitmap_dlist {#rbgfx30-mbitmap-dlist}

Replay a retained display list in multicolor bitmap mode.

[Original source](../src/apps/readybasic/rbgfx30_mbitmap_dlist.bas).

Commands used: `DLDRAW`, `DLFBOX`, `DLLINE`, `DLMAKE`, `DLPLOT`, `DLRECT`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `MCBG`.

Disk name: `RBGFX30`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx30 mbitmap dlist"
30 gfxmode("mbitmap"):gfxclear(0):mcbg(0)
40 dlmake(12,h%)
50 dlplot(h%,20,28,17)
60 dlline(h%,4,38,155,66)
70 dlrect(h%,12,82,72,144)
80 dlfbox(h%,88,86,148,148)
90 dldraw(h%)
100 get a$:if a$="" then 100
110 gfxtext():print "mbitmap dlist done"
```

## rbgfx31_sync_blit {#rbgfx31-sync-blit}

Capture a visible hires scene into REU, clear it and restore it with a key.

[Original source](../src/apps/readybasic/rbgfx31_sync_blit.bas).

Commands used: `ERRCODE`, `GFXBLIT`, `GFXCLEAR`, `GFXMODE`, `GFXSURF`, `GFXSYNC`, `GFXTEXT`, `GFXTGT`, `LINE`, `RECT`.

Disk name: `RBGFX31`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx31 sync blit"
30 gfxmode("hires"):gfxclear(0)
40 line(0,0,319,199,1):rect(42,38,220,150,1)
50 h%=gfxsurf("hires")
60 gfxtgt(h%):gfxsync():gfxtgt(0)
70 get a$:if a$="" then 70
80 gfxclear(0)
90 get a$:if a$="" then 90
100 gfxblit(h%)
110 get a$:if a$="" then 110
120 gfxtext():print "sync blit done";errcode()
```

## rbgfx32_convex_poly {#rbgfx32-convex-poly}

Draw a polygon outline and a convex filled polygon from integer arrays.

[Original source](../src/apps/readybasic/rbgfx32_convex_poly.bas).

Commands used: `FPOLY`, `GFXCLEAR`, `GFXMODE`, `GFXTEXT`, `POLY`.

Disk name: `RBGFX32`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem see graphics design md
20 print chr$(147);"rbgfx32 convex poly"
30 gfxmode("hires"):gfxclear(0)
40 dim a%(7)
50 a%(0)=32:a%(1)=32:a%(2)=146:a%(3)=24
60 a%(4)=184:a%(5)=120:a%(6)=64:a%(7)=170
70 poly(a%(0),4,1)
80 dim b%(9)
90 b%(0)=196:b%(1)=36:b%(2)=278:b%(3)=58
100 b%(4)=292:b%(5)=132:b%(6)=238:b%(7)=178
110 b%(8)=178:b%(9)=108
120 fpoly(b%(0),5,1)
130 get a$:if a$="" then 130
140 gfxtext():print "convex poly done"
```

## rbgfxsnddemo {#rbgfxsnddemo}

Orbital Echoes: two cached scenes, READY sprites, palette-safe lines and PAL music; no Ultimate speed calls.

[Original source](../src/apps/readybasic/rbgfxsnddemo.bas).

Commands used: `BORDER`, `BUFDROP`, `GFXBLIT`, `GFXCLEAR`, `GFXMODE`, `GFXSURF`, `GFXSYNC`, `GFXTEXT`, `GFXTGT`, `LDMOD`, `MCBG`, `MCFILE`, `MCLINE`, `MEMCAP`, `MUSDROP`, `MUSPLAY`, `MUSTUNE`, `SPRFILE`, `SPRMCO`, `SPRMOVE`, `SPRMUL`, `SPRPRI`, `SPRSET`, `SPRSIZE`.

Disk name: `RBGFXSNDDEMO`.
Profiles: `precog-d81`, `precog-ultimate`.

```basic
10 rem =====================================
20 rem rbgfxsnddemo - orbital echoes
30 rem art: luis zuno / ansimuz - cc0
40 rem opengameart.org/content/space-background-3
42 rem opengameart.org/content/warped-city - cc0
50 rem adapted: layers, exposure, c64 palette
60 rem music: summer vacation - shiru 2010
70 rem shiru.untergrund.net/music.shtml
80 rem cc by 3.0 - sidreloc $9200 / zp $fc-fd
90 rem ready lettering: original pixel art

100 rem --- safe setup, before disk reads ---
110 if peek(49663)<>0 then :musdrop()
120 memcap(36864)
130 bc=peek(53280) and 15:bg=peek(53281) and 15
140 fc=peek(646)
150 print chr$(147);"ready / orbital echoes"
160 print "q: stop all   m: leave music playing"
170 print "space: renew image / auto: 15 seconds"
180 print "precalculating motion - please wait"
190 dim s%(511),sy%(335)
200 dim lx%(1023),ly%(1023),rx%(1023),ry%(1023)
210 exec prep
220 ldmod("rbm.media",m%):h%=gfxsurf("mbitmap")
222 g%=gfxsurf("mbitmap")
225 print "orbital show running"
230 exec scene
240 mustune("rb.summer"):musplay(1)
265 rem sp%: sprite step / ld%: sprite passes per line
270 sp%=2:ld%=3
280 p%=0:lc%=0:lp%=0:mi%=0:c%=1:rt=ti:rc=0:im%=0

300 rem --- raw steps: no clock-driven motion or catchup ---
310 repeat
320   p%=(p%+sp%) and 255
330   exec glyphs
340   lc%=lc%+1
350   if lc%<ld% then 390
360   lc%=0:exec weave
370   tt=ti:if tt<rt then rt=tt
375   if tt-rt<900 then 390
380   im%=1-im%:exec clean
390   get a$
395   if a$<>" " then 400
397   exec clean
400 until a$="q" or a$="m"
410 exec finish
420 if a$="m" then 500
430 musdrop():clr:memcap(40960)
440 print "all stopped - basic memory:";fre(0)
450 end

500 rem music still owns its protected memory
510 clr
520 print "music continues - memory stays reserved"
530 print "to release it:":print "musdrop():clr:memcap(40960)"
540 end

1000 rem --- all sine and coordinate work happens once ---
1010 proc prep()
1020   for i=0 to 511
1030     s%(i)=int(100*sin(i*0.0122718463))
1040   next i
1050   for i=0 to 255
1060     sy%(i)=78+int(s%(i*2)*0.16+0.5)
1065   next i
1067   for i=0 to 511
1070     lx%(i)=80+int(s%(i)*0.75)
1080     j=(i*2) and 511:ly%(i)=112+int(s%(j)*0.70)
1090     j=(i*3+128) and 511:rx%(i)=80+int(s%(j)*0.75)
1100     j=(i*5+192) and 511:ry%(i)=112+int(s%(j)*0.70)
1110     j=i+512
1120     lx%(j)=159-lx%(i):ly%(j)=199-ly%(i)
1130     rx%(j)=159-rx%(i):ry%(j)=199-ry%(i)
1140   next i
1150   rem extra samples eliminate wrapping per letter
1160   for i=0 to 79:sy%(i+256)=sy%(i):next i
1170 endp

1300 rem --- disk setup, then cache the complete scene ---
1310 proc scene()
1320   gfxmode("mbitmap"):gfxclear(0):border(0)
1330   for i=0 to 4
1340     sprset(i,1,3,0):sprmul(i,1)
1350     sprsize(i,1,1):sprpri(i,0)
1360     sprmove(i,56+i*52,78)
1370   next i
1380   sprmco(1,6):sprfile("rb.ready")
1390   mcfile("rb.warp")
1395   gfxtgt(g%):gfxsync():gfxtgt(0)
1397   mcfile("rb.neon")
1400   gfxtgt(h%):gfxsync():gfxtgt(0)
1410 endp

1600 rem --- no for loop or sine math in the hot path ---
1610 proc glyphs()
1620   sprmove(0,56,sy%(p%))
1630   sprmove(1,108,sy%(p%+20))
1640   sprmove(2,160,sy%(p%+40))
1650   sprmove(3,212,sy%(p%+60))
1660   sprmove(4,264,sy%(p%+80))
1670 endp

1900 rem --- mirrored pairs share one sequential sample ---
1910 proc weave()
1920   wi%=lp%+mi%*512
1930   mcline(lx%(wi%),ly%(wi%),rx%(wi%),ry%(wi%),c%)
1940   mi%=1-mi%:if mi%<>0 then 1970
1950   lp%=(lp%+1) and 511
1960   c%=c%+1:if c%=4 then c%=1
1970 endp

2200 rem --- instant refresh from the owned reu surface ---
2210 proc clean()
2220   if im%=1 then 2250
2230   gfxblit(h%):goto 2260
2250   gfxblit(g%)
2260   rt=ti:rc=rc+1
2270 endp

2500 rem --- both exits restore the original text colors ---
2510 proc finish()
2530   for i=0 to 4:sprset(i,0,0,0):next i
2540   gfxtext():mcbg(bg):border(bc)
2550   poke 646,fc:print chr$(147);"orbital echoes complete"
2555   bufdrop(h%):bufdrop(g%)
2560   print "image refreshes:";rc
2570 endp
```

## rbproc1 {#rbproc1}

Structured routine/function and typed-result regression examples, including nested expressions.

[Original source](../src/apps/readybasic/rbproc1.bas).

Commands used: `ADD16`, `FADD`, `LDMOD`, `UPPER`.

Disk name: `RBPROC1`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
5 ldmod("rbm.sample1",m%)
10 print "procfunc"
20 exec show0
30 exec showi(7)
40 exec shows("yo")
50 a%=addlate(4,5)
60 print "sum";a%
70 a$=greet("ready")
80 print a$
90 print "chain":exec showi(8):print "after"
100 if 1 then :a%=addi(1,2)
110 print "if";a%
120 a%=outer(3)
130 print "nested";a%
140 print "efn";addi(6,7)
150 b=addi(2,3):print "efass";b
160 t$=greet("expr"):print t$
170 a%=idi(9):print "ret%";a%
180 t$=ids("ok"):print "ret$ ";t$
190 print "cex";abs(add16(1,6)-10)
200 t$=upper("mix"):print "cs$ ";t$
210 print "fpex";addi(1,2+4)
220 a%=add16(3,10):print "fcmd";a%
230 t$=funcupper("yo"):print "fs$ ";t$
240 print "fminus";addi(1,6)-10
250 print "fnabs";abs(addi(1,6)-10)
260 t$=left$(greet("ready"),2):print "fnleft ";t$
270 print "fparen";addi(1,(2+4))
280 print "cparen";add16(1,(2+4))
290 print "dparen";addi((1+2),(3+4))
300 print "fadd";fadd(1.2,2.3)
310 f=fadd(1.5,fadd(2.25,3.25)):print "nfadd";f
320 s=scale(2.25):print "scale";s
330 print "naddi";addi(1,addi(2,3))
340 print "fabs";abs(fadd(1.2,2.3)-3)
350 fadd(1.2,2.3,q):print "sfadd";q
360 t$=left$(greet("ready")+"!",3):print "cat ";t$
370 t$=left$(upper(greet("ready")),2):print "ngs ";t$
380 end
1000 proc show0()
1010 print "proc0"
1020 endp
1100 proc showi(p%)
1110 print "proci";p%
1120 endp
1200 proc shows(s$)
1210 print "procs ";s$
1220 endp
1300 func addi(x%,y%)
1310 ret x%+y%
1320 endp
1400 func greet(n$)
1410 r$="hi "+n$
1420 ret r$
1430 endp
1500 func inner(x%)
1510 ret x%+10
1520 endp
1600 func outer(x%)
1610 r%=inner(x%)
1620 r%=r%+1
1630 ret r%
1640 endp
1700 func addlate(x%,y%)
1710 r%=x%+y%
1720 ret r%
1730 endp
1800 func idi(x%)
1810 ret% x%
1820 endp
1900 func ids(s$)
1910 ret$ s$
1920 endp
2100 func funcupper(n$)
2110 r$=upper(n$)
2120 ret r$
2130 endp
2200 func scale(x)
2210 ret x*1.5
2220 endp
```

## rbprocerr {#rbprocerr}

Intentional negative cases; run individual numbered sections to test argument/type/stack errors.

[Original source](../src/apps/readybasic/rbprocerr.bas).

Commands used: `FADD`.

Disk name: `RBPROCERR`.
Profiles: `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem readybasic proc/fun negative probes
20 rem run 100,200,...,1500 one case at a time
100 exec missing(1)
110 end
200 exec needi()
210 end
300 exec needi("bad")
310 end
400 exec needs(7)
410 end
500 exec fni(7)
510 end
600 exec nop(1,a%)
610 end
700 endp
710 end
800 exec d1
810 end
900 exec needi((1+2)
910 end
1000 exec needi((1+2)))
1010 end
1100 exec needs(left$("bad",2))
1110 end
1200 fadd(1.2,2.3,a%)
1210 end
1300 a=ids("bad")
1310 end
1400 a$=addi(1,2)
1410 end
1500 print addi(1,(2+4)
1510 end
1600 print addi(1,(2+4)))
1610 end
2000 proc needi(p%)
2010 endp
2100 proc needs(s$)
2110 endp
2200 func fni(p%)
2210 ret p%+1
2220 endp
2300 proc nop(p%)
2310 endp
2400 proc d1()
2410 exec d2
2420 endp
2500 proc d2()
2510 exec d3
2520 endp
2600 proc d3()
2610 exec d4
2620 endp
2700 proc d4()
2710 exec d5
2720 endp
2800 proc d5()
2810 endp
```

## rbsnd01_sid_basics {#rbsnd01-sid-basics}

Hear separate triangle, saw, pulse and noise tones using immediate SID control.

[Original source](../src/apps/readybasic/rbsnd01_sid_basics.bas).

Commands used: `ADSR`, `PAUSE`, `PULSE`, `SIDOFF`, `SIDRST`, `SOUND`, `VOL`.

Disk name: `RBSND01`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd01 sid basics"
30 print "you should hear triangle, saw, pulse,"
40 print "then noise. each tone is separate."
50 sidrst():vol(15):adsr(1,0,5,12,3)
60 print:print "triangle tone"
70 sound(1,4455,45,16):pause(20)
80 print "saw tone"
90 sound(1,4455,45,32):pause(20)
100 print "pulse tone"
110 pulse(1,2048):sound(1,4455,45,64):pause(20)
120 print "noise burst"
130 sound(1,4455,45,128):pause(20)
140 sidoff()
150 print:print "rbsnd01 done"
```

## rbsnd02_voice_state {#rbsnd02-voice-state}

Change pulse width and envelope, gating voices explicitly.

[Original source](../src/apps/readybasic/rbsnd02_voice_state.bas).

Commands used: `ADSR`, `FRQ`, `GATE`, `PAUSE`, `PULSE`, `SIDOFF`, `SIDRST`, `VOL`, `WAVE`.

Disk name: `RBSND02`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd02 voice state"
30 print "manual sid voice setup."
40 print "listen for a pulse pitch, then changed"
50 print "pulse width and envelope."
60 sidrst():vol(15)
70 print:print "wide pulse, soft release"
80 adsr(1,0,9,12,6):pulse(1,3072):frq(1,4455)
90 wave(1,64):gate(1,1):pause(70):gate(1,0):pause(50)
100 print "narrow pulse, snappier release"
110 adsr(1,0,3,15,2):pulse(1,512):frq(1,5612)
120 wave(1,64):gate(1,1):pause(70):gate(1,0):pause(50)
130 sidoff()
140 print:print "rbsnd02 done"
```

## rbsnd03_notes {#rbsnd03-notes}

Play a chromatic scale with the PAL pitch table.

[Original source](../src/apps/readybasic/rbsnd03_notes.bas).

Commands used: `ADSR`, `GATE`, `PAUSE`, `PITCH`, `SIDOFF`, `SIDRST`, `VOL`, `WAVE`.

Disk name: `RBSND03`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd03 pitchs"
30 print "chromatic pitch command."
40 print "you should hear c through b, then c"
50 print "one octave higher."
60 sidrst():vol(15):adsr(1,0,5,12,3):wave(1,32)
70 for n=0 to 11
80 pitch(1,n,4):gate(1,1):pause(22):gate(1,0):pause(6)
90 next n
100 pitch(1,0,5):gate(1,1):pause(45):gate(1,0)
110 sidoff()
120 print:print "rbsnd03 done"
```

## rbsnd04_filter {#rbsnd04-filter}

Compare low-pass, band-pass and high-pass SID filtering.

[Original source](../src/apps/readybasic/rbsnd04_filter.bas).

Commands used: `ADSR`, `FILTER`, `FRQ`, `GATE`, `PAUSE`, `SIDOFF`, `SIDRST`, `VOL`, `WAVE`.

Disk name: `RBSND04`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd04 filter"
30 print "saw wave through sid filter."
40 print "listen for low-pass, band-pass,"
50 print "then high-pass color changes."
60 sidrst():vol(15):adsr(1,0,9,15,4)
70 frq(1,2230):wave(1,33)
80 print:print "low-pass sweep-ish steps"
90 for c=150 to 950 step 200:filter(c,8,1,1):pause(35):next c
100 print "band-pass"
110 filter(700,12,1,2):pause(90)
120 print "high-pass"
130 filter(700,12,1,4):pause(90)
140 gate(1,0):sidoff()
150 print:print "rbsnd04 done"
```

## rbsnd05_voice_batch {#rbsnd05-voice-batch}

Set frequency, waveform and packed envelopes together with VOICE.

[Original source](../src/apps/readybasic/rbsnd05_voice_batch.bas).

Commands used: `GATE`, `PAUSE`, `PULSE`, `SIDOFF`, `SIDRST`, `VOICE`, `VOL`, `WAVE`.

Disk name: `RBSND05`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd05 voice batch"
30 print "voice uses packed sid envelope bytes:"
40 print "voice(v,f,wave,ad,sr)."
50 print "ad=$09 and sr=$c3 in decimal."
60 sidrst():vol(15):pulse(1,2048)
70 print:print "one batch command starts the voice"
80 voice(1,4455,65,9,195):pause(80)
90 print "now raw ctrl turns gate off"
100 wave(1,64):pause(50)
110 print "same voice, higher frquency"
120 voice(1,6672,65,9,195):pause(80)
130 gate(1,0):sidoff()
140 print:print "rbsnd05 done"
```

## rbsnd06_three_voice {#rbsnd06-three-voice}

Combine three voices into a chord, then a bass/high-voice pair.

[Original source](../src/apps/readybasic/rbsnd06_three_voice.bas).

Commands used: `ADSR`, `GATE`, `PAUSE`, `PITCH`, `PULSE`, `SIDOFF`, `SIDRST`, `VOL`, `WAVE`.

Disk name: `RBSND06`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
10 rem readybasic sound design: sound design md
20 print chr$(147);"rbsnd06 three voice"
30 print "three sid voices: c major, then"
40 print "a lower pulse bass with a high saw."
50 sidrst():vol(15)
60 adsr(1,0,5,12,4):adsr(2,0,5,12,4):adsr(3,0,5,12,4)
70 print:print "c major chord"
80 pitch(1,0,4):pitch(2,4,4):pitch(3,7,4)
90 wave(1,33):wave(2,33):wave(3,33):pause(100)
100 gate(1,0):gate(2,0):gate(3,0):pause(45)
110 print "pulse bass plus high saw"
120 pulse(1,1536):pitch(1,7,2):pitch(2,2,5)
130 wave(1,65):wave(2,33):pause(110)
140 gate(1,0):gate(2,0):sidoff()
150 print:print "rbsnd06 done"
```

## rbsnd07_psid {#rbsnd07-psid}

Load the media module and vetted PSID; play, halt, restart and release its arena.

[Original source](../src/apps/readybasic/rbsnd07_psid.bas).

Commands used: `BORDER`, `LDMOD`, `MEMCAP`, `MUSDROP`, `MUSHALT`, `MUSPLAY`, `MUSTUNE`.

Disk name: `RBSND07`.
Profiles: `precog-d81`, `precog-ultimate`.

```basic
10 rem =====================================
20 rem rbsnd07 - music while basic works
30 rem summer vacation by shiru (2010)
40 rem https://shiru.untergrund.net/music.shtml
50 rem cc by 3.0: creativecommons.org/licenses/by/3.0/
60 rem modified: sidreloc to $9200; zp $fc-$fd
70 rem =====================================

100 rem --- main: read this as a story ---
110 rem reserve ram before making strings
120 memcap(36864)
130 bc=peek(53280) and 15
140 exec title
150 exec assets

200 rem play: basic animates the border
210 musplay(1)
220 print "playing - basic is still running"
230 exec animate(8)

300 rem halt keeps the song in reserved ram
310 mushalt()
320 print "halted - ram still reserved"
330 exec linger(2)

400 rem play again restarts the subtune
410 musplay(1)
420 print "restart from beginning"
430 exec animate(5)

500 rem detach before returning ram to basic
510 musdrop()
520 border(bc)
530 print "dropped - driver detached"
540 rem clr discards any dynamic strings
550 clr
560 memcap(40960)
570 print "memory returned:";fre(0)
580 print "rbsnd07 done"
590 end

1000 rem --- procedures: named actions ---
1010 proc title()
1020   print chr$(147);"rbsnd07 / summer vacation"
1030   print "music: shiru / cc by 3.0"
1040   print "proc / func / repeat / until"
1050   print
1060   print "reserved 4096 bytes: ";fre(0)
1070 endp

1200 proc assets()
1210   ldmod("rbm.media",m%)
1220   print "module commands:";m%
1250   rem psid player and data at $9200
1260   mustune("rb.summer")
1270 endp

1400 proc animate(span)
1410   rem span is seconds; mk is start time
1420   mk=ti
1425   c%=0
1430   repeat
1440     c%=shade(c%)
1450     border(c%)
1452     rem leave each color visible a moment
1454     wt=ti
1456     repeat
1458     until age(wt)>=0.25
1460   until age(mk)>=span
1470 endp

1600 proc linger(span)
1610   rem the same clock, without animation
1620   mk=ti
1630   repeat
1640   until age(mk)>=span
1650 endp

2000 rem --- functions: return values ---
2010 func shade(ix%)
2020   rem wrap the counter through colors 0-15
2030   ret% (ix%+1) and 15
2040 endp

2200 func age(mark)
2210   rem ti counts 60 ticks per second
2220   dt=ti-mark
2230   rem allow for midnight; true is -1
2240   ret (dt-5184000*(dt<0))/60
2250 endp
```

## rbsnd08_neon {#rbsnd08-neon}

Historical single-scene clock-paced music/graphics demo; use the newer combined demos.

[Original source](../src/apps/readybasic/rbsnd08_neon.bas).

Commands used: `BORDER`, `BUFDROP`, `GFXBLIT`, `GFXCLEAR`, `GFXMODE`, `GFXSURF`, `GFXSYNC`, `GFXTEXT`, `GFXTGT`, `LDMOD`, `MCFILE`, `MCLINE`, `MEMCAP`, `MUSDROP`, `MUSPLAY`, `MUSTUNE`, `SPRFILE`, `SPRMCO`, `SPRMOVE`, `SPRMUL`, `SPRPRI`, `SPRSET`, `SPRSIZE`.

Historical source retained; no current profile disk entry. Use the newer combined demos.

```basic
10 rem =====================================
20 rem rbsnd08 - ready / orbital echoes
30 rem art: luis zuno / ansimuz - cc0
40 rem opengameart.org/content/space-background-3
50 rem adapted: layers, exposure, c64 palette
60 rem music: summer vacation - shiru 2010
70 rem shiru.untergrund.net/music.shtml
80 rem cc by 3.0 - sidreloc $9200 / zp $fc-fd
90 rem ready sprite lettering: original art

100 rem --- setup: all disk reads happen here ---
110 memcap(36864)
120 bc=peek(53280) and 15
130 print chr$(147);"ready / orbital echoes"
140 print "space art: ansimuz / cc0"
150 print "summer vacation: shiru / cc by 3.0"
160 print "preparing picture, sprites and music"
170 print "q quits / image renews every 30 sec"
180 ldmod("rbm.media",m%)
190 dim s%(255)
200 for i=0 to 255
210 s%(i)=int(100*sin(i*0.0245436926))
220 next i
225 print "orbital show running"
230 h%=gfxsurf("mbitmap")
240 exec scene
250 mustune("rb.summer")
260 musplay(1)
270 tb=ti:rt=ti:rc=0:c%=1

300 rem --- show: clock-driven, additive trails ---
310 repeat
320   ft=ti
330   tm=ti-tb:tm=(tm-5184000*(tm<0))/60
335   tm=tm-int(tm/8)*8
340   p%=int(tm*32) and 255
350   exec glyphs
360   exec weave
370   c%=c%+1:if c%=4 then c%=1
375   ag=ti-rt:ag=(ag-5184000*(ag<0))/60
380   if ag<30 then 390
385   exec clean
390   get a$
400   rem cap fast machines at 30 updates/sec
410   repeat
420   until elapsed(ft)>=0.0333333
430 until a$="q"
440 exec finish
450 clr:memcap(40960)
460 print "basic memory returned:";fre(0)
470 end

1000 rem --- initialize the bank-d scene ---
1010 proc scene()
1020   gfxmode("mbitmap"):gfxclear(0):border(0)
1030   for i=0 to 4
1040     sprset(i,1,3,0)
1050     sprmul(i,1):sprsize(i,1,1):sprpri(i,0)
1055     sprmove(i,56+i*52,78)
1060   next i
1070   sprmco(1,6)
1080   rem sprset makes patterns: replace them now
1090   sprfile("rb.ready")
1100   mcfile("rb.neon")
1110   rem cache pixels, screen and color in reu
1120   gfxtgt(h%):gfxsync():gfxtgt(0)
1130 endp

1300 rem --- five independent sine-wave letters ---
1310 proc glyphs()
1320   for i=0 to 4
1330     ph%=(p%+i*20) and 255
1340     yy=78+s%(ph%)/10
1350     sprmove(i,56+i*52,yy)
1360   next i
1370 endp

1500 rem --- kinetic line art, not palette writes ---
1510 proc weave()
1520   x1=80+s%(p%)*0.75
1530   ph%=(p%*2) and 255
1540   y1=112+s%(ph%)*0.70
1550   ph%=(p%*3+64) and 255
1560   x2=80+s%(ph%)*0.75
1570   ph%=(p%*5+96) and 255
1580   y2=112+s%(ph%)*0.70
1590   rem slots 1-3 keep each 4x8 cell's colors
1600   mcline(x1,y1,x2,y2,c%)
1610   mcline(159-x1,199-y1,159-x2,199-y2,c%)
1620 endp

1800 rem --- a clean image, with no disk access ---
1810 proc clean()
1820   gfxblit(h%)
1830   rt=ti:rc=rc+1
1840 endp

2000 rem --- leave the machine as we found it ---
2010 proc finish()
2020   musdrop()
2030   for i=0 to 4:sprset(i,0,0,0):next i
2040   gfxtext():border(bc):bufdrop(h%)
2050   print chr$(147);"orbital echoes complete"
2060   print "image refreshes:";rc
2090 endp

2300 rem --- elapsed seconds, including midnight ---
2310 func elapsed(mark)
2320   dt=ti-mark
2330   ret (dt-5184000*(dt<0))/60
2340 endp
```

## rbtest1 {#rbtest1}

Small command-expression, integer-output and uppercase-string smoke test.

[Original source](../src/apps/readybasic/rbtest1.bas).

Commands used: `ADD16`, `ECHO1`, `LDMOD`, `UPPER`.

Disk name: `RBTEST1`.
Profiles: `precog-d81-rsdebug`, `precog-d81`, `precog-dual-d64`, `precog-dual-d71-rsdebug`, `precog-dual-d71`, `precog-kung-fu-flash-2-d81`, `precog-solo-d64-a`, `precog-solo-d64-b`, `precog-solo-d64-c`, `precog-solo-d64-d`, `precog-solo-d64-e-rsdebug`, `precog-solo-d64-e`, `precog-solo-d64-readybasic`, `precog-ultimate`.

```basic
5 ldmod("rbm.sample1",m%)
10 echo1(p%)
20 print "readybasic";p%
30 for i=1 to 3
40 add16(i,10,a%)
50 print "loop";a%
60 next i
70 print "expradd";add16(5,6)
80 b=add16(8,9):print "exprass";b
90 s$="ready":t$=upper(s$):print "exprstr";t$
```

## rbugfxsnddemo {#rbugfxsnddemo}

Ultimate Orbital Echoes: the same media lifecycle, with safe-speed disk loading and 64 MHz computation/animation.

[Original source](../src/apps/readybasic/rbugfxsnddemo.bas).

Commands used: `BORDER`, `BUFDROP`, `GFXBLIT`, `GFXCLEAR`, `GFXMODE`, `GFXSURF`, `GFXSYNC`, `GFXTEXT`, `GFXTGT`, `LDMOD`, `MCBG`, `MCFILE`, `MCLINE`, `MEMCAP`, `MUSDROP`, `MUSPLAY`, `MUSTUNE`, `SPRFILE`, `SPRMCO`, `SPRMOVE`, `SPRMUL`, `SPRPRI`, `SPRSET`, `SPRSIZE`, `USPEED`.

Disk name: `RBUGFXSNDDEMO`.
Profiles: `precog-d81`, `precog-ultimate`.

```basic
10 rem =====================================
20 rem rbugfxsnddemo - orbital echoes / ultimate
30 rem art: luis zuno / ansimuz - cc0
40 rem opengameart.org/content/space-background-3
42 rem opengameart.org/content/warped-city - cc0
50 rem adapted: layers, exposure, c64 palette
60 rem music: summer vacation - shiru 2010
70 rem shiru.untergrund.net/music.shtml
80 rem cc by 3.0 - sidreloc $9200 / zp $fc-fd
90 rem ready lettering: original pixel art

95 rem requires c64u turbo registers, not manual mode
100 rem --- safe setup, before disk reads ---
105 uspeed(1)
110 if peek(49663)<>0 then :musdrop()
120 memcap(36864)
130 bc=peek(53280) and 15:bg=peek(53281) and 15
140 fc=peek(646)
150 print chr$(147);"ready / orbital echoes"
160 print "q: stop all   m: leave music playing"
170 print "space: renew image / auto: 15 seconds"
180 print "precalculating motion - please wait"
185 uspeed(64)
190 dim s%(511),sy%(335)
200 dim lx%(1023),ly%(1023),rx%(1023),ry%(1023)
210 exec prep
215 uspeed(1)
220 ldmod("rbm.media",m%):h%=gfxsurf("mbitmap")
222 g%=gfxsurf("mbitmap")
225 print "orbital show running"
230 exec scene
240 mustune("rb.summer"):musplay(1)
260 uspeed(64)
265 rem sp%: sprite step / ld%: sprite passes per line
270 sp%=2:ld%=3
280 p%=0:lc%=0:lp%=0:mi%=0:c%=1:rt=ti:rc=0:im%=0

300 rem --- raw steps: no clock-driven motion or catchup ---
310 repeat
320   p%=(p%+sp%) and 255
330   exec glyphs
340   lc%=lc%+1
350   if lc%<ld% then 390
360   lc%=0:exec weave
370   tt=ti:if tt<rt then rt=tt
375   if tt-rt<900 then 390
380   im%=1-im%:exec clean
390   get a$
395   if a$<>" " then 400
397   exec clean
400 until a$="q" or a$="m"
410 exec finish
420 if a$="m" then 500
430 musdrop():clr:memcap(40960)
440 print "all stopped - basic memory:";fre(0)
450 end

500 rem music still owns its protected memory
510 clr
520 print "music continues - memory stays reserved"
530 print "to release it:":print "musdrop():clr:memcap(40960)"
540 end

1000 rem --- all sine and coordinate work happens once ---
1010 proc prep()
1020   for i=0 to 511
1030     s%(i)=int(100*sin(i*0.0122718463))
1040   next i
1050   for i=0 to 255
1060     sy%(i)=78+int(s%(i*2)*0.16+0.5)
1065   next i
1067   for i=0 to 511
1070     lx%(i)=80+int(s%(i)*0.75)
1080     j=(i*2) and 511:ly%(i)=112+int(s%(j)*0.70)
1090     j=(i*3+128) and 511:rx%(i)=80+int(s%(j)*0.75)
1100     j=(i*5+192) and 511:ry%(i)=112+int(s%(j)*0.70)
1110     j=i+512
1120     lx%(j)=159-lx%(i):ly%(j)=199-ly%(i)
1130     rx%(j)=159-rx%(i):ry%(j)=199-ry%(i)
1140   next i
1150   rem extra samples eliminate wrapping per letter
1160   for i=0 to 79:sy%(i+256)=sy%(i):next i
1170 endp

1300 rem --- disk setup, then cache the complete scene ---
1310 proc scene()
1320   gfxmode("mbitmap"):gfxclear(0):border(0)
1330   for i=0 to 4
1340     sprset(i,1,3,0):sprmul(i,1)
1350     sprsize(i,1,1):sprpri(i,0)
1360     sprmove(i,56+i*52,78)
1370   next i
1380   sprmco(1,6):sprfile("rb.ready")
1390   mcfile("rb.warp")
1395   gfxtgt(g%):gfxsync():gfxtgt(0)
1397   mcfile("rb.neon")
1400   gfxtgt(h%):gfxsync():gfxtgt(0)
1410 endp

1600 rem --- no for loop or sine math in the hot path ---
1610 proc glyphs()
1620   sprmove(0,56,sy%(p%))
1630   sprmove(1,108,sy%(p%+20))
1640   sprmove(2,160,sy%(p%+40))
1650   sprmove(3,212,sy%(p%+60))
1660   sprmove(4,264,sy%(p%+80))
1670 endp

1900 rem --- mirrored pairs share one sequential sample ---
1910 proc weave()
1920   wi%=lp%+mi%*512
1930   mcline(lx%(wi%),ly%(wi%),rx%(wi%),ry%(wi%),c%)
1940   mi%=1-mi%:if mi%<>0 then 1970
1950   lp%=(lp%+1) and 511
1960   c%=c%+1:if c%=4 then c%=1
1970 endp

2200 rem --- instant refresh from the owned reu surface ---
2210 proc clean()
2220   if im%=1 then 2250
2230   gfxblit(h%):goto 2260
2250   gfxblit(g%)
2260   rt=ti:rc=rc+1
2270 endp

2500 rem --- both exits restore the original text colors ---
2510 proc finish()
2520 uspeed(1)
2530   for i=0 to 4:sprset(i,0,0,0):next i
2540   gfxtext():mcbg(bg):border(bc)
2550   poke 646,fc:print chr$(147);"orbital echoes complete"
2555   bufdrop(h%):bufdrop(g%)
2560   print "image refreshes:";rc
2570 endp
```
