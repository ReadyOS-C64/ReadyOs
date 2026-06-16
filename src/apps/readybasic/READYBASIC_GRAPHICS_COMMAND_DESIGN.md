# ReadyBASIC Graphics And Event Command Design

This note reframes the old BASIC-extender command audit for the current
ReadyBASIC architecture: bare parenthesized commands, expression-capable
commands where useful, descriptor-backed assembler payloads, module/submodule
loading, and REU-backed handles.

It intentionally avoids native language features such as `WHILE`, `LABEL`,
`JUMP`, `PROC`, `EXEC`, and `FUNC`, and it does not design general error
handling. The focus is the command infrastructure: what can be done by resident
parsing plus under-ROM command modules, while protecting resident RAM and BASIC
bytes free.

## Current Constraints

ReadyBASIC's current command architecture has the right shape for a large
graphics catalog:

- User syntax is visible BASIC text: `COMMAND(arg,arg)` and selected
  expression forms such as `A%=COMMAND(arg,arg)`.
- Command descriptors live in the assigned ReadyBASIC core bank, not in the
  BASIC workspace.
- Command implementation code lives in the assigned code bank and is fetched
  into under-ROM slots at `$A800`, `$B000`, and `$B800`.
- Resident code should only parse BASIC-facing parameters, call BASIC ROM
  helpers, clear output variables, and commit results.
- Module code should do the heavy work: direct VIC/SID/CIA access, REU DMA,
  graphics drawing, input/event sampling, buffer traversal, and blits.
- Persistent objects should be typed REU handles, not BASIC arrays or resident
  RAM structures.

The current steady-state BASIC workspace is `$2AC1-$9FFF`, with about 30013
empty BASIC bytes. Every permanent resident byte pushes that start upward.
Graphics must therefore be a resource-command system, not a resident library.

## Design Principles

1. Keep graphics modes explicit. A graphics command must know exactly which VIC
   bank, screen memory, bitmap memory, charset memory, color RAM, and REU
   handles it owns.
2. Keep BASIC-visible state tiny. BASIC sees mode numbers, small integers,
   strings, and handles. REU holds bitmaps, display lists, sprite sheets,
   tile maps, dirty rectangles, and command-private state.
3. Separate immediate drawing from retained drawing. Plot/line/circle commands
   should exist, but a retained display-list model is better for complex scenes.
4. Prefer module-local managers over resident managers. The resident core should
   not grow for sprite collision history, key states, display lists, or graphics
   allocators.
5. Treat Bank D as the default graphics/video bank. This keeps the visible
   graphics framebuffer out of the BASIC workspace and out of ReadyBASIC's
   under-ROM command slots.
6. Use REU as the large backing store. Use small C64 buffers only for a scanline,
   tile row, sprite record page, command page, or dirty-rect batch.

## VIC Bank D Baseline

On a C64, a VIC bank is 16KB. "Bank D" here means the VIC sees the `$C000-$FFFF`
physical 64KB range as its video bank.

That is attractive because ReadyBASIC's BASIC workspace ends at `$9FFF`, and
command code runs under BASIC ROM at `$A000-$BFFF`. The Bank D visible region
uses:

| Range | Role |
|---:|---|
| `$C000-$C5FF` | ReadyBASIC bridge/shared-frame area while BASIC runs. Do not use for visible graphics state. |
| `$C600-$C7FF` | ReadyOS REU metadata. Never use as graphics scratch. |
| `$C800-$C9FF` | ReadyOS shim ABI. Never use as graphics scratch. |
| `$CA00-$CFFF` | Candidate screen/charset/sprite-pointer area, but must be allocated intentionally. |
| `$D000-$DFFF` | I/O when CPU sees I/O, but VIC fetches video data from underlying RAM in this bank. Good for bitmap/charset data from the VIC side, awkward for CPU writes unless banking is controlled carefully. |
| `$E000-$FFFF` | KERNAL ROM from CPU by default, but VIC can fetch underlying RAM. Good for bitmap/screen data if command modules write with ROM hidden. |

The safest Bank D rule is:

- CPU-visible ReadyBASIC state at `$C000-$C9FF` remains off-limits.
- Screen RAM, bitmaps, charsets, and sprite data are placed at or above `$CA00`,
  preferably with layouts that avoid `$C000-$C9FF`.
- Commands that write into `$D000-$FFFF` must explicitly manage `$01` banking
  and restore it before returning.

## Potential Graphics Modes

These are the practical mode families ReadyBASIC should expose. The command API
should hide the VIC register details but not hide the tradeoffs.

| Mode | Resolution/shape | Memory | Strengths | Costs |
|---|---|---:|---|---|
| Text 40x25 | 1000 chars + color RAM | 1KB screen + 1KB color | Fast, readable, works with BASIC text habits, cheap snapshots. | Cell-based; not pixel graphics. |
| Custom charset text | 40x25 with user glyphs | 1KB screen + 2KB charset + color | Best for tile games, dashboards, UI, maps. | Glyph upload and charset bank management; per-cell color limits remain. |
| Extended background color text | 40x25, four background choices by char bits | 1KB screen + charset + color | Useful for coarse tile worlds. | Only 64 chars available per charset bank mode. |
| Multicolor text | 40x25, chunky cells | 1KB screen + charset + color | Colorful tile/sprite-like scenes at low memory cost. | Half horizontal resolution inside chars; color rules are quirky. |
| Standard bitmap hi-res | 320x200, 1 bit per pixel | 8KB bitmap + 1KB screen | Crisp line art, UI panels, graphs. | Two colors per 8x8 cell; full clears/blits cost time. |
| Multicolor bitmap | 160x200 logical pixels | 8KB bitmap + 1KB screen + color | More color per cell, good for game/art screens. | Chunky pixels; color rules and per-cell attributes complicate drawing. |
| Sprite overlay mode | 8 hardware sprites over any mode | Sprite data + pointers | Fast moving objects, cursors, player/enemies. | Only 8 visible sprites per raster line; multiplexing needs IRQ work. |
| Hybrid retained scene | Any above plus REU display list | REU list + optional backing buffer | High-level scenes, redraw optimization, mode-independent commands. | Needs a renderer per target mode; more design up front. |

### Recommended Public Mode Names

Avoid names tied too tightly to old extenders. Use clear mode constants or
numbers and allow aliases later:

```basic
GFXMODE("TEXT")
GFXMODE("TILE")
GFXMODE("MTILE")
GFXMODE("HIRES")
GFXMODE("MBITMAP")
```

The statement form should set the mode. The expression form can return the
current mode:

```basic
GFXMODE("HIRES")
M%=GFXMODE()
```

## Implemented Phase 1 Snapshot

Phase 1 is implemented as command infrastructure only. It adds built-in
descriptor-backed commands and command-only parser signatures; it does not
change ReadyBASIC control flow, native routine syntax, crunch normalization, or
general BASIC expression parsing.

Built-in graphics payloads are prestashed in the ReadyBASIC code bank:

| Family | Module/submodule | Runtime area | Commands |
|---|---:|---:|---|
| `GFXCORE` | module `3`, submodule `16` | slot 1 `$B000-$B7FF` | `GFXMODE`, `GFXTEXT`, `GFXCLEAR`, `GFXTARGET`, `GFXSYNC` |
| `GFXPRIM` | module `3`, submodule `17` | slot 2 `$B800-$BFFF` | `PLOT`, `POINT`, `LINE`, `RECT`, `FRECT`; Phase 2 also adds `CIRCLE`, `FCIRCLE`, `TILE`, `CHARAT` |
| `GFXSPR` | module `3`, submodule `18` | slot-2 overlay 1 | `SPRSET`, `SPRMOVE`, `SPRCOLOR`, `SPRROW`, `SPRSCAN`, `SPRCOLL`; Phase 2 also adds size/priority/multicolor controls |
| `INPUTEV` | module `3`, submodule `19` | slot-2 overlay 2 | `JOY`, `KEYP`, `KEYSCAN`, `KEYLAST` |
| Surface handle stubs | system slot 0 | slot 0 `$A800-$AFFF` | `GFXSURF`, `GFXBLIT` |

The surface handle commands use the existing typed REU handle allocator and
therefore live in the default command slot for Phase 1. They allocate and
validate handle type `3` (`RB_HANDLE_TYPE_GFXSURF`) using 40 heap pages. Full
offscreen drawing into those handles and true REU-to-Bank-D bitmap blitting are
not yet implemented; `GFXTARGET(0)` is the visible target and `GFXBLIT(H%)`
currently validates a graphics-surface handle. This keeps the Phase 1 ABI in
place without growing resident RAM.

Implemented syntax:

```basic
GFXMODE("HIRES")       : rem also "MBITMAP", "TILE", "MTILE", "TEXT"
M%=GFXMODE()
GFXTEXT()
GFXCLEAR(C)
H%=GFXSURF("HIRES")
GFXTARGET(0)
GFXBLIT(H%)
GFXSYNC()
PLOT(X,Y,C)
POINT(X,Y,P%)          : rem long form registered
PNT(X,Y,P%)            : rem short readback alias used by demos
LINE(X1,Y1,X2,Y2,C)
RECT(X1,Y1,X2,Y2,C)
FRECT(X1,Y1,X2,Y2,C)
CIRCLE(X,Y,R,C)
FCIRCLE(X,Y,R,C)
TILE(X,Y,CH,C)
CHARAT(X,Y,CH,C)
SPRSET(N,ON,COLOR,PATTERN)
SPRMOVE(N,X,Y)
SPRCOLOR(N,COLOR)
SPRCOL(N,COLOR)       : rem token-safe alias for demos
SPRROW(N,ROW,B1,B2,B3)
SPREXPAND(N,XON,YON)
SPRSIZE(N,XON,YON)    : rem token-safe alias for demos
SPRPRI(N,BEHIND)
SPRMULTI(N,ON)
SPRMUL(N,ON)          : rem token-safe alias for demos
SPRMCOLOR(C1,C2)
SPRMCO(C1,C2)         : rem token-safe alias for demos
SPRSCAN()
SPRCOLL(N,C%)
JOY(PORT,J%)
KEYP(K%)
KEYSCAN()
KEYLAST(L%)
```

Bank D Phase 1 layout:

| Range | Phase 1 use |
|---:|---|
| `$C000-$C5FF` | ReadyBASIC bridge/shared frames; never used by graphics. |
| `$C600-$C9FF` | ReadyOS REU metadata and shim ABI; static checks block graphics-owned symbols here. |
| `$CA00-$CBFF` | Hardware sprite data, eight 64-byte definitions. |
| `$CC00-$CFFF` | Graphics screen RAM and sprite pointer table at `$CFF8`. |
| `$D800-$DBE7` | Color RAM. |
| `$E000-$FFFF` | Bitmap/charset RAM, written with BASIC/KERNAL ROM hidden. |

Mode tradeoffs in the implemented renderer:

| Mode | What Phase 1 does | Tradeoff |
|---|---|---|
| `TEXT` / `GFXTEXT()` | Restores ordinary VIC bank and text-ish control registers. | Returns the user to readable BASIC output. |
| `HIRES` | Sets Bank D bitmap mode and draws one-bit pixels into `$E000`. | Crisp 320x200 plotting; color attributes are still coarse and minimal. |
| `MBITMAP` | Sets bitmap plus multicolor control bit. | Establishes the hardware mode; primitive plotting is still the simple one-bit bitmap worker. |
| `TILE` | Sets Bank D text/tile screen and treats `PLOT(X,Y,C)` as cell write. | Fast cell graphics; `POINT()` returns the cell byte. |
| `MTILE` | Sets tile screen plus multicolor control bit. | Establishes the mode; Phase 1 keeps the same cell-level primitive behavior. |

Known Phase 1 limits:

- `PNT(X,Y,OUT%)` is the preferred Phase 1 readback spelling in demos. `POINT`
  remains registered as the long form.
- Primitive coordinate callers are expected to stay in range: bitmap
  `0<=X<320`, `0<=Y<200`; tile `0<=X<40`, `0<=Y<25`.
- `LINE` is a small step-toward-endpoint worker, not full Bresenham. It is good
  for horizontal, vertical, 45-degree, fans, and demos, but uneven slopes are
  approximate.
- `MBITMAP` and `MTILE` set the hardware mode bits but do not yet expose full
  multicolor attribute semantics.
- `SPRROW(N,ROW,B1,B2,B3)` is the Phase 1 explicit sprite-pixel path. It
  writes one 24-bit sprite row into Bank D sprite memory at `$CA00 + N*64`,
  where `ROW` is `0..20`. This is intentionally simple and demo-friendly until
  Phase 2 adds sprite-sheet/REU definitions.
- `SPRSCAN()` polls and clears the VIC collision latches. There is no IRQ
  sampler in Phase 1.
- `KEYP()`, `KEYSCAN()`, and `KEYLAST()` read the ROM keyboard buffer
  opportunistically. They do not reserve resident RAM or install a background
  event queue.

## Implemented Phase 2 Snapshot

Phase 2 is also command-only. It does not add any ReadyBASIC syntax or control
flow. The implementation adds visible, demoable primitives and sprite controls
inside the existing built-in graphics module structure:

| Command | Module | Current behavior |
|---|---|---|
| `CIRCLE(X,Y,R,C)` | `GFXPRIM` | Draws a compact outline circle/diamond approximation by reusing the existing `LINE` worker. |
| `FCIRCLE(X,Y,R,C)` | `GFXPRIM` | Draws a filled circular placeholder as the bounding filled rectangle. This proves the command path and mode targeting; a true midpoint/disk fill is still future work. |
| `TILE(X,Y,CH,C)` | `GFXPRIM` | Writes a character/color cell in Bank D tile modes, and also writes ordinary `$0400` text cells after `GFXTEXT()` so demos can show ROM charset output. |
| `CHARAT(X,Y,CH,C)` | `GFXPRIM` | Alias for `TILE`; intended for text-cell style programs. |
| `SPREXPAND(N,XON,YON)` / `SPRSIZE(N,XON,YON)` | `GFXSPR` | Controls VIC sprite X/Y expansion bits. `SPRSIZE` avoids PETCAT splitting `EXP` inside the command name. |
| `SPRPRI(N,BEHIND)` | `GFXSPR` | Controls the VIC sprite-background priority bit. |
| `SPRMULTI(N,ON)` / `SPRMUL(N,ON)` | `GFXSPR` | Controls VIC sprite multicolor enable bits. `SPRMUL` is the token-safe demo spelling. |
| `SPRMCOLOR(C1,C2)` / `SPRMCO(C1,C2)` | `GFXSPR` | Sets the two shared sprite multicolor registers. `SPRMCO` avoids PETCAT splitting `CLR`/`COLOR`-like names. |
| `SPRCOL(N,C)` | `GFXSPR` | Token-safe alias for `SPRCOLOR(N,C)`. |

Phase 2 demo programs:

- `rbgfx14_phase2_prims.bas`: `CIRCLE`, `FCIRCLE`, `RECT`, and `LINE`.
- `rbgfx15_phase2_tiles.bas`: `TILE` and `CHARAT` in visible text cells.
- `rbgfx16_phase2_sprite_ctrl.bas`: explicit `SPRROW` sprite pixels, movement,
  expansion, priority, multicolor, and recolor stages.

Phase 2 deliberately does not yet implement retained REU display lists,
sprite-sheet loaders, true REU offscreen drawing, dirty-rect `GFXSYNC`, polygon
fill, or true circular fill. `SCROLL` was also deferred because the current
slot-2 command pack budget is tight and the first implementation pushed the
payload over the 2KB slot limit.

## Command Families

### 1. Mode And Surface Commands

These commands own VIC setup, Bank D allocation, and visible/backing surface
selection.

| Command | Meaning |
|---|---|
| `GFXMODE(mode$)` | Switch to a text/tile/bitmap/sprite-capable mode. |
| `GFXTEXT()` | Return to ordinary text mode and restore expected screen pointers. |
| `GFXCLEAR(color)` | Clear the current visible surface using mode-aware fill logic. |
| `GFXSURF(mode$,H%)` / `H%=GFXSURF(mode$)` | Allocate a REU-backed surface handle compatible with a mode. |
| `GFXTARGET(H%)` | Draw subsequent immediate commands into a REU surface instead of the visible Bank D surface. |
| `GFXTARGET(0)` | Draw subsequent immediate commands directly to the visible surface. |
| `GFXBLIT(H%)` | Copy a compatible REU surface to the visible Bank D layout. |
| `GFXSYNC()` | Apply dirty rectangles or pending blit work. |
| `GFXINFO(H%,A%(0))` | Return handle/mode/width/height/stride metadata into an array. |

The key split is visible target versus REU target. `GFXTARGET(0)` is simple and
direct. `GFXTARGET(H%)` enables off-screen drawing, then `GFXBLIT(H%)` makes the
result visible.

### 2. Immediate Pixel/Primitive Commands

These are the classic BASIC-extender graphics commands. They operate on the
current target.

| Command | Applies to | Notes |
|---|---|---|
| `PLOT(x,y,c)` | Bitmap, multicolor bitmap, maybe tile modes | Pixel semantics depend on current mode. |
| `POINT(x,y)` | Bitmap modes | Expression-safe color/readback command. |
| `LINE(x1,y1,x2,y2,c)` | Bitmap modes; tile fallback optional | Bresenham-style worker in module code. |
| `RECT(x1,y1,x2,y2,c)` | Bitmap and tile modes | Outline rectangle. |
| `FRECT(x1,y1,x2,y2,c)` | Bitmap and tile modes | Filled rectangle. |
| `CIRCLE(x,y,r,c)` | Bitmap modes | Expensive; module overlay candidate. |
| `FCIRCLE(x,y,r,c)` | Bitmap modes | More expensive; likely overlay. |
| `POLY(A%(0),count,c)` | Bitmap modes | Points passed as array pairs. |
| `FPOLY(A%(0),count,c)` | Bitmap modes | Filled polygon; good overlay candidate. |
| `FILL(x,y,c)` | Bitmap modes | Flood fill needs REU/stack workspace, not resident stack. |

Tradeoff: immediate commands are intuitive but can be slow if each command maps
banking, fetches a module, updates visible RAM, and returns to BASIC. They are
still essential because users expect them and they make small programs pleasant.

### 3. Tile/Text Graphics Commands

These should not be treated as second-class. On a C64 they are often the best
performance/clarity tradeoff.

| Command | Meaning |
|---|---|
| `TILESET(H%)` | Set active REU-backed charset/tile resource. |
| `TILEDEF(index,dataH%)` | Define one tile from a handle or byte array. |
| `TILE(x,y,index,color)` | Put one tile/cell. |
| `TILEMAP(H%)` | Set active REU-backed tilemap. |
| `TILEDRAW(H%,x,y,w,h)` | Draw tilemap region to visible screen. |
| `SCROLL(dx,dy)` | Scroll text/tile screen with mode-aware memory copies. |
| `CHARAT(x,y,ch,color)` | Text cell put. |
| `TEXTAT(x,y,text$,color)` | Text string put. |

Tile modes should be first-class because they are fast, compact, and do not
require full 8KB bitmap blits for most game/UI screens.

### 4. Sprite Commands

Hardware sprites are independent of the bitmap/text choice, so the sprite module
should be mostly mode-agnostic.

| Command | Meaning |
|---|---|
| `SPRDEF(n,H%,offset)` | Copy sprite data from a REU sprite-sheet handle into Bank D sprite memory. |
| `SPRSET(n,on,color,multi,priority)` | Configure sprite flags. |
| `SPRMOVE(n,x,y)` | Set sprite position. |
| `SPRXY(n,X%,Y%)` / `X%=SPRX(n)` | Read sprite position. |
| `SPREXPAND(n,xon,yon)` | Set X/Y expansion flags. |
| `SPRCOLOR(n,c)` | Set primary color. |
| `SPRROW(n,row,b1,b2,b3)` | Phase 1 direct row write for an 8-sprite Bank D pattern. |
| `SPRMCOLOR(c1,c2)` | Set shared multicolor registers. |
| `SPRCOLL(n)` | Read and optionally clear collision state for sprite n. |
| `SPRSCAN()` | Poll collision registers and latch results into REU event state. |

Sprite memory in Bank D should be allocated from a known high region, with the
sprite pointer table placed in the selected screen RAM page. A sprite-sheet
handle in REU can store many 64-byte definitions; only the currently needed
eight, or a small active set, need to be copied to visible Bank D RAM.

### 5. Retained Display-List Commands

This is the richer model and should be a separate graphics module family, not
mixed into the primitive module. It lets BASIC build a scene description once,
store it in REU, and ask the renderer to draw it efficiently for the current
mode.

| Command | Meaning |
|---|---|
| `DLNEW(H%)` / `H%=DLNEW()` | Allocate a display-list handle. |
| `DLCLEAR(H%)` | Clear all records. |
| `DLADDLINE(H%,x1,y1,x2,y2,c,z)` | Append a line record. |
| `DLADDRECT(H%,x1,y1,x2,y2,c,fill,z)` | Append a rectangle record. |
| `DLADDPOLY(H%,pointsH%,count,c,fill,z)` | Append a polygon record. |
| `DLADDSPR(H%,sprite,x,y,z)` | Append a logical sprite/object record. |
| `DLADDTEXT(H%,x,y,text$,color,z)` | Append text record. |
| `DLDRAW(H%)` | Render the list top-to-bottom into the current target. |
| `DLDRAW(H%,surfaceH%)` | Render into a REU surface. |
| `DLBLIT(H%)` | Render to backing surface, then blit to visible. |

Recommended display-list record shape in REU:

| Field | Size | Meaning |
|---|---:|---|
| Type | 1 | Line, rect, circle, poly, sprite, text, tilemap, command. |
| Flags | 1 | Filled, visible, dirty, mode hints. |
| Z/order | 1 | Draw order bucket. |
| Color/style | 1 | Mode-specific pen/style index. |
| X/Y/W/H or first args | 8 | Four 16-bit fields. |
| Aux handle/id | 2 | Points handle, sprite id, text handle, tilemap handle. |
| Aux offset/count | 4 | Record-specific detail. |

Keep records fixed-size for v1, even if a few bytes are wasted. Fixed records
make REU paging and sorting predictable. A later module can add compact variable
records if it is worth the complexity.

The renderer should support two strategies:

1. Preserve order: draw records in append order. Fast, simple, predictable.
2. Z-bucket: sort or bucket by small `z` values into REU scratch, then draw
   back-to-front/top-to-bottom. More expensive, better for scenes.

Do not make resident code understand display lists. Resident code should only
parse `DLDRAW(H%)`; the graphics module owns traversal.

## REU Surface Strategies

### Direct Visible Drawing

Command draws straight into Bank D.

Pros:

- Lowest memory use.
- Immediate visual feedback.
- Good for `PLOT`, `LINE`, cursor, simple drawing tools.

Cons:

- Flicker/tearing risk.
- Slow BASIC loops show partial drawing.
- Commands must handle ROM/I/O banking every time.

### Full REU Backing Surface Then Blit

Commands draw into a REU surface handle. `GFXBLIT(H%)` copies to Bank D.

Pros:

- No visible partial drawing.
- Easy redraw from a clean backing buffer.
- Surfaces can be saved, restored, double-buffered, or composed.

Cons:

- A full bitmap is about 8KB plus attribute/screen data.
- REU-to-C64 blit still takes time and must stage around color RAM/I/O quirks.
- Mode-specific blitters are required.

### Small RAM Buffer Plus REU

Commands use `$C500` or another small page/row buffer to render chunks, then DMA
or copy chunks into Bank D.

Pros:

- Low resident/RAM cost.
- Good for scanlines, tile rows, dirty rectangles, and color RAM staging.
- Keeps complex temporary state out of the stack.

Cons:

- More bookkeeping.
- Random pixel writes into REU are awkward unless the module has helpers for
  addressing and page fetch/store.

### Dirty Rectangle Blit

Immediate or retained drawing records dirty rectangles in REU. `GFXSYNC()`
copies only touched areas.

Pros:

- Huge win for UI, tile games, cursors, sprites, and small animations.
- Avoids full 8KB bitmap copies when only a panel changes.

Cons:

- Merging rectangles takes code.
- Worst case is slower than a full blit if everything is dirty.
- Color RAM and bitmap data have different layouts, so dirty copying is
  mode-specific.

## Mode Effects On Command Semantics

The same command name should behave predictably, but not all modes can support
the same semantics cheaply.

| Command | Text/tile | Hi-res bitmap | Multicolor bitmap |
|---|---|---|---|
| `PLOT` | Optional cell-level alias, not true pixel | Natural 320x200 pixel | Natural 160x200 logical pixel |
| `LINE` | Optional char approximation | Strong | Strong but chunky |
| `FILL` | Tile flood fill possible | Expensive but normal | Expensive and color-rule-sensitive |
| `TEXTAT` | Natural | Requires glyph renderer | Requires chunky glyph renderer |
| `TILE` | Natural | Could blit tile graphics | Could blit chunky tile graphics |
| `SPRITE` | Natural overlay | Natural overlay | Natural overlay |
| `DLADD*` | Renderer chooses cell/tile or bitmap implementation | Renderer draws primitives | Renderer maps coordinates/colors to chunky layout |

The command contract should return a mode error for unsupported combinations
rather than silently doing a bad approximation. Optional approximations can be
separate commands or flags.

## Module Layout Proposal

Use multiple modules so a user can load only what they need.

| Module | Slot shape | Purpose |
|---|---|---|
| `GFXCORE` | Slot 1 stable manager | Mode state, handle validation, target selection, Bank D layout, blit dispatcher. |
| `GFXPRIM` | Slot 2 overlays | Plot, line, rect, circle, polygon, fill workers. |
| `GFXTILE` | Slot 2 overlays | Text/tile/charset/tilemap operations. |
| `GFXSPR` | Slot 2 or slot 1+2 | Sprite definition, movement, collision polling. |
| `GFXDL` | Slot 1+2 span or stable+overlay | Display-list traversal, z-buckets, renderer dispatch. |
| `INPUTEV` | Slot 2 small module | Key/joystick/paddle polling and event-state access. |

`GFXCORE` should be sticky while graphics mode is active. Other modules can
rotate through slot 2. A larger display-list renderer may span slots 1+2 and
temporarily evict `GFXCORE`, but only if it includes the helpers it needs.

## Resident Parser Signatures To Reuse

Keep new resident parser growth low by designing commands around a small set of
shared signatures:

| Signature shape | Examples |
|---|---|
| No args, optional integer expression result | `GFXMODE()`, `SPRSCAN()`, `KEYSCAN()` |
| String input | `GFXMODE("HIRES")`, `ZMODLD("RBM.GFX")` |
| Handle input | `GFXBLIT(H%)`, `DLDRAW(H%)`, `TILESET(H%)` |
| Numeric pair | `PLOT(x,y,c)`, `SPRMOVE(n,x,y)` |
| Four numeric coords | `LINE(x1,y1,x2,y2,c)`, `RECT(...)` |
| Array base plus count | `POLY(A%(0),count,c)` |
| Handle plus numeric args | `DLADDLINE(H%,x1,y1,x2,y2,c,z)` |
| Optional output handle | `GFXSURF("HIRES",H%)` and `H%=GFXSURF("HIRES")` |

If many graphics commands arrive, a compact REU-backed signature table becomes
important. The resident parser should not gain one custom case per primitive.

## Event Detection Without Resident RAM Growth

There are three viable strategies for keypress, joystick, and sprite collision
state.

### Strategy A: Pure Polling Commands

Commands read hardware state only when called:

```basic
J%=JOY(2)
K%=KEYP()
C%=SPRCOLL(0)
```

Pros:

- Almost no resident growth.
- No IRQ lifecycle risk.
- Easy to keep compatible with BASIC and ReadyOS.

Cons:

- Misses short key/collision events between polls.
- BASIC loops can be too slow for action games.

Use this first. It is the simplest and most robust.

### Strategy B: Command-Installed IRQ Sampler With REU Event Ring

A command installs a small IRQ routine, but the routine writes only tiny event
state and queues to a fixed or handle-backed REU area:

```basic
EVINIT(H%)
EVON()
EVOFF()
KEYGET(K%)
JOYGET(2,J%)
COLLGET(C%)
```

Possible state layout:

| Location | Meaning |
|---|---|
| Bridge byte | IRQ installed/enabled flag and previous vector pointer. |
| Small C64 shadow | Last key byte, joystick byte, collision latch. |
| REU event handle | Ring buffer of key/collision/joystick timestamp records. |

Pros:

- Captures short events.
- Keeps history in REU, not BASIC arrays.
- Commands can read and clear event records later.

Cons:

- Any IRQ hook is global machine state and must be disabled/restored on `EXIT`.
- REU DMA from IRQ is risky; better to keep IRQ writes in a tiny C64 shadow and
  flush to REU from a command.
- More edge cases with ROM editor, prompt hotkeys, and ReadyOS navigation.

Recommended variant: IRQ stores only tiny state in existing bridge/scratch bytes;
`EVFLUSH()` copies batches to REU from normal command context. Do not perform
REU DMA inside the IRQ in v1.

### Strategy C: Raster/Sprite Manager Module

A graphics module owns a frame loop helper:

```basic
GFXFRAME()
SPRSCAN()
EVSCAN()
```

The BASIC program calls `GFXFRAME()` once per loop. That command samples keys,
joystick, sprite collisions, updates sprite positions, renders dirty display-list
records, and blits.

Pros:

- No global IRQ install required.
- One command can amortize module fetch/banking overhead.
- Good fit for BASIC game loops.

Cons:

- Events are still sampled once per frame.
- User must structure code around the frame command.

This is the best middle path. It avoids resident growth and IRQ lifecycle
fragility while providing a clear high-level model.

## Sprite Collision Handling

The VIC collision registers are latch-like; reading them clears them. A direct
`SPRCOLL(n)` command is fine for simple programs, but a richer sprite module
should centralize reads so one command does not accidentally clear state before
another sees it.

Recommended model:

- `SPRSCAN()` reads sprite-sprite and sprite-background collision registers once.
- The module writes the latched result into module event state in REU or a small
  C64 shadow.
- `SPRCOLL(n)` reads the stored state, not necessarily the raw VIC register.
- `SPRCLEAR(n)` or `SPRCLEAR()` clears stored state.
- `GFXFRAME()` calls `SPRSCAN()` internally.

This keeps collision behavior deterministic and avoids hidden coupling between
multiple collision query commands.

## Keypress Detection

Do not expand the existing prompt hotkey system into a full game keyboard
scanner. Prompt navigation and running-program input have different contracts.

Recommended commands:

| Command | Meaning |
|---|---|
| `KEYP()` | Return current decoded key or 0, polling only. |
| `KEYDOWN(code)` | Return current matrix state for a key code. |
| `KEYSCAN()` | Update module-owned keyboard state snapshot. |
| `KEYGET(K%)` / `K%=KEYGET()` | Return last scanned key/event. |
| `KEYCLR()` | Clear module keyboard event state. |

For action use, `GFXFRAME()` should call `KEYSCAN()` and store a stable state
snapshot. Programs can then query `KEYDOWN()` without each query rescanning or
mutating the state.

## Recommended Command Set By Phase

### Phase 1: Safe Visible Results

- `GFXMODE`, `GFXTEXT`, `GFXCLEAR`
- `PLOT`, `POINT`, `LINE`, `RECT`, `FRECT`
- `GFXSURF`, `GFXTARGET`, `GFXBLIT`
- `SPRSET`, `SPRMOVE`, `SPRCOLOR`, `SPRROW`, `SPRSCAN`, `SPRCOLL`
- `KEYP`, `JOY`, `KEYSCAN`, `KEYGET`

This gives useful graphics without display-list complexity or IRQ hooks.

### Phase 2: REU Resources

- Implemented now: `TILE`, `CHARAT`, `CIRCLE`, `FCIRCLE`, sprite expansion,
  sprite priority, sprite multicolor enable, and shared sprite multicolor
  registers.
- Deferred: `TILESET`, `TILEDEF`, `TILEMAP`, `TILEDRAW`, `SPRDEF`, `SPRLOAD`,
  sprite-sheet handles, `GFXSYNC` dirty-rect blits, `POLY`, `FPOLY`, and `FILL`.

This phase starts using overlay-heavy primitive and sprite workers while keeping
REU resource/rendering work out of resident RAM. Full REU-backed tilemaps,
sprite sheets, and retained resources remain a later phase.

### Phase 3: Retained Scene System

- `DLNEW`, `DLCLEAR`, `DLADDLINE`, `DLADDRECT`, `DLADDPOLY`, `DLADDSPR`,
  `DLADDTEXT`, `DLDRAW`, `DLBLIT`
- Optional z-bucket renderer.
- Optional mode-specific display-list backends.

This phase provides the high-level "list of things to draw" model and lets the
renderer choose the best draw order and target-specific implementation.

### Phase 4: Optional Event IRQ

- `EVINIT`, `EVON`, `EVOFF`, `EVFLUSH`, `EVGET`
- Only after pure polling and `GFXFRAME()` are proven.
- IRQ must be restored before ReadyOS yield and must not perform REU DMA in v1.

## Tradeoff Summary

| Choice | Wins | Costs |
|---|---|---|
| Bank D visible graphics | Avoids BASIC workspace; VIC can see ROM-underlying RAM. | Must avoid `$C000-$C9FF`; CPU writes to ROM/I/O ranges need careful banking. |
| Direct drawing | Simple and immediate. | Flicker and visible partial renders. |
| REU backing surfaces | Clean redraws, double-buffering, handle-based resources. | More blit code and mode-specific layout handling. |
| Small RAM buffer | Minimal resident pressure. | More complex chunked algorithms. |
| Display lists | High-level scene model, renderer can optimize. | More REU data structures and renderer code. |
| Pure polling events | Safest and smallest. | Can miss fast events. |
| IRQ event sampler | Captures short events. | Global-state lifecycle risk; must avoid REU DMA in IRQ. |
| Module-per-family | Low resident cost and scalable catalog. | Module fetch/overlay management becomes important for performance. |
| Many command-specific parser signatures | Simple workers. | Resident growth. Prefer shared or data-driven signatures. |

## Bottom Line

ReadyBASIC should not clone old extenders command-for-command. It should expose
familiar graphics vocabulary, but implement it as a handle-oriented graphics
runtime:

1. Bank D is the visible video bank.
2. REU handles hold surfaces, spritesheets, tilemaps, display lists, and event
   history.
3. Immediate commands exist for simple programs.
4. Display-list commands exist for richer programs.
5. `GFXFRAME()` becomes the high-level command that samples input, scans sprite
   collisions, renders dirty state, and blits.
6. IRQ/event support is optional and late, with tiny C64 shadows and command-time
   REU flushing.

This gives ReadyBASIC the feel of a powerful BASIC extender without paying for
hundreds of features in resident RAM or BASIC bytes free.
