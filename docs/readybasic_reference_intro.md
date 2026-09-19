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
