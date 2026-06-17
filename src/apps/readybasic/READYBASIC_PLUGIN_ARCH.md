# ReadyBASIC Lean REU Plugin Architecture

## Current Module/Submodule Update

This file keeps the lean-plugin history and earlier V1 notes below. The current
command-module design uses the module-aware placement model while preserving the
same ReadyOS and REU discipline.

- `BASIC_START = $2AC1`; BASIC owns `$2AC1-$9FFF`, with `30013` formula empty
  free bytes.
- `RESIDENT` is `$1200-$2ABF` (`$18C0`, 6336B).
- `BRIDGE` is `$C000-$C1FE` (`$01FF`, 511B), still below `$C200`.
- Under BASIC ROM, `$A000-$A7FF` is the common helper area, currently using
  `$A000-$A7E8`; `$A800-$AFFF`,
  `$B000-$B7FF`, and `$B800-$BFFF` are three 2KB submodule slots.
- ReadyBASIC uses two launcher-assigned REU resource banks. The core bank is
  registry/runtime storage; the code bank is built-in and disk-loaded module
  payload storage. Current code resolves physical banks at startup from
  `REU_RB_CORE` and `REU_RB_CODE` ownership/type metadata instead of assuming
  fixed `$44/$45` banks.
- Descriptors remain 32 bytes but now carry module id, submodule id, overlay
  id, slot mask, payload REU offset/size, runtime destination, and entry offset.
- The disk-loader proof command is `ZMODLD(name$)` in module 2/slot 1. It opens
  ReadyBasicModule SEQ packages named `rbm.<name>` and streams them into REU.
  Sample packages add `ZDM1`, `ZDM2S`, `ZDOV1`, `ZDOV2`, and `ZSAA`-`ZUEB`.
- Graphics commands are built-in module id `3`: `GFXCORE` submodule `16` in
  slot 1, `GFXPRIM` submodule `17` in the base slot-2 image, `GFXSPR`
  submodule `18` as a slot-2 replacement overlay prestashed at code-bank offset
  `$5000`, `INPUTEV` submodule `19` as a slot-2 replacement overlay prestashed
  at code-bank offset `$5800`, and `GFXPOLY` submodule `20` as a slot-2
  replacement overlay prestashed at code-bank offset `$6000`.

## Pre-Module V1 Layout Snapshot

- `BASIC_START = $2AC1`; BASIC owns `$2AC1-$9FFF`, with `30013` formula empty free bytes (29.3K).
- `$1000-$1102`: tiny app entry (`$0103`, 259B) that copies hidden helpers and bridge state before BASIC starts.
- `$1200-$2AB9`: visible resident core (`$18BA`, 6330B). This is the only code that calls BASIC ROM helpers.
- `$2AC0`: BASIC sentinel byte; it must stay zero before stored-program `RUN`.
- `$A900-$AF3C`: low command overlay slot under BASIC ROM. In this pre-module
  snapshot, the packed low command image was `$063D` bytes (1.6K, 1597 exact bytes).
- `$C200-$C5FF`: fixed call frame, result frame, descriptor buffer, command-name buffer, page buffer, and warm-resume staging (`$0400`, 1.0K).
- `$A000-$A376`: hidden helper code (`$0377`, 887B), restored from the visible `$C280` shadow.
- `$A800-$A84C`: hidden worker overlay slot (`$004D`, 77B) used by `ZHIDDENRAM`.
- `$C000` bridge state plus native routine return stack and flow-control
  scratch; this pre-module snapshot also stayed below `$C200`.

## REU Banks

- Assigned core bank is ReadyBASIC common/system storage.
- Assigned code bank is packed command code storage.
- ReadyOS REU type constants are mirrored as `REU_RB_CORE = 14` and `REU_RB_CODE = 15`.
- The launcher marks the assigned physical banks in `$C600-$C6FF`, and
  ReadyBASIC re-marks the resolved banks during startup/resume so REU Viewer
  and allocator state know those banks are owned.
- Full registry/code prestash runs only on cold ReadyBASIC entry. Warm resume
  re-marks ownership but does not reread `CMDPACK`, hidden/bridge load images,
  or `REGSEED`, because those load-image addresses become normal BASIC
  workspace after launch.
- Native `PROC`/`FUNC` definitions are ordinary BASIC program text. They do not
  use descriptors, `LOWPACK`, `HIDDENPACK`, or assigned command-code storage.
- Native `REPEAT`/`UNTIL` and `LABEL`/`JUMP` markers are also visible BASIC text
  handled by resident parser code; they do not allocate descriptor slots.

## Assigned Core Bank Regions

- `$0000`: registry header (`RBPL`, version, descriptor count, descriptor size, frame offsets).
- `$0400`: current call-frame snapshot.
- `$0400`: current result-frame snapshot.
- `$0600`: reserved REU debug ring region.
- `$0800-$09FF`: REU-backed handle directory, 128 descriptors at 4 bytes each.
- `$0A00`: ReadyOS suspend/resume zero-page snapshot.
- `$0B00`: ReadyOS suspend/resume stack-page snapshot.
- `$0C00-$0CFF`: 192-page heap bitmap plus reserved bytes.
- `$1000-$1FFF`: 128 compact command descriptor slots, 32 bytes each. Slot 14 is `SCRCAP`, slot 128 is `SCRPUT`, and zero-filled filler slots are unused.
- `$2000-$3FFF`: reserved common/system expansion space.
- `$4000-$FFFF`: typed 48KB heap for buffer, screen, and Phase 1 graphics
  surface handles.

## Assigned Code Bank Regions

- `$0000-$07DB`: built-in module 1 slot-0 payload, fetched into
  `$A800-$AFDB` (`$07DC`, 2012B). The linker symbol is still named
  `LOWPACK` for compatibility, but the current runtime slot base is `$A800`.
- `$07DC-$0C50`: built-in module 2 slot-1 proof, streaming `ZMODLD` loader,
  and Phase 1 `GFXCORE`, fetched into `$B000-$B474` (`$0475`, 1141B).
- `$0C51-$13A9`: built-in slot-2 proof plus Phase 1/2 `GFXPRIM`, fetched into
  `$B800-$BF58` (`$0759`, 1881B).
- `$13AA-$13BE`: two-slot span proof payload (`$0015`, 21B).
- `$13BF-$14FF`: free gap before the current disk-module proof offsets
  (`$0141`, 321B).
- `$1500-$151F`: `rbm.sample1` descriptor proof for `ZDM1`.
- `$1600-$165F`: `rbm.sample2` descriptors for `ZDM2S`, `ZDOV1`, and `ZDOV2`;
  submodule 5 appears twice because those entries are overlays 1 and 2.
- `$1700-$1ABF`: `rbm.sample3` descriptors for `ZSAA`-`ZUEB`.
- `$3000-$3014`, `$3200-$3214`, `$3300-$3314`, `$3400-$3414`: small
  disk-loaded module payload proofs.
- `$3800-$463C`: `rbm.sample3` payload records for `ZSAA`-`ZUEB`.
- `$5000-$5271`: built-in `GFXSPR` replacement overlay, loaded from cold-only
  `CMDPACK2` and fetched into `$B800-$BA71` when sprite commands run.
- `$5272-$57FF`: reserved `GFXSPR` growth headroom inside the assigned code bank.
- `$5800-$586C`: built-in `INPUTEV` replacement overlay, loaded from cold-only
  `CMDPACK2` and fetched into `$B800-$B86C` when input commands run.
- `$586D-$5FFF`: reserved `INPUTEV` growth headroom.
- `$6000-$6799`: built-in `GFXPOLY` replacement overlay, loaded from cold-only
  `CMDPACK2` and fetched into `$B800-$BF99` when polygon commands run.
- `$679A-$67FF`: reserved `GFXPOLY` growth headroom.
- `$6800-$6E29`: built-in `GFXDL` replacement overlay, loaded from cold-only
  `CMDPACK2` and fetched into `$B800-$BE29` when display-list commands run.
- `$6E2A-$6FFF`: reserved `GFXDL` growth headroom.
- `$7000-$74D9`: built-in `GFXTILE` replacement overlay, loaded from cold-only
  `CMDPACK2` and fetched into `$B800-$BCD9` when charset, tileset, tilemap, or
  explicit multicolor-cell commands run.
- `$74DA-$77FF`: reserved `GFXTILE` growth headroom.
- `$7800-$FFFF`: currently unreserved assigned code-bank tail for future
  built-in command payloads or a later resource-loader/codebank split.

Descriptors store payload offsets, payload sizes, slot masks, runtime
destinations, and entry offsets. Heap and screen commands currently fetch the
whole slot-0 payload because their allocator, REU descriptor, bitmap, and
screen-copy helpers live together in that payload rather than resident core RAM.

## Descriptor ABI

Each descriptor is 32 bytes:

- `0`: command id.
- `1`: module id.
- `2-3`: payload offset in the assigned code bank.
- `4-5`: payload size.
- `6`: submodule id.
- `7`: overlay id.
- `8`: slot mask; bit 0 is `$A800`, bit 1 is `$B000`, bit 2 is `$B800`.
- `9`: generation/check byte.
- `10-11`: runtime destination offset from `$A000`.
- `12-13`: entry offset within the loaded payload.
- `14`: signature id.
- `15`: uppercase command-name length.
- `16-31`: uppercase command-name bytes, padded with zeroes.

## Frames

- Call frame starts at `$C200`.
- Result frame starts at `$C300`.
- Descriptor buffer starts at `$C480`.
- Command buffer starts at `$C4A0`.
- Page buffer starts at `$C500`.
- V1 supports up to the requested frame size, but implemented sample signatures use direct fixed slots rather than a generalized signature VM.
- Numeric expressions are evaluated through BASIC ROM `FRMNUM` and `GETADR`.
- Variable and array references use BASIC ROM `PTRGET`; output integers are cleared before command execution.
- String output heap mutation happens in visible resident code only.
- Native `EXEC` reuses the same BASIC ROM expression and variable helpers where
  possible: integer inputs use `FRMNUM`/`GETADR`, plain numeric inputs preserve
  BASIC's five-byte float value, statement output actuals use the existing
  output-variable capture and result commit paths, and string values use the
  existing 64-byte staging cap.

## Implemented Commands

- `ZECHO1(OUT%)` / `ZECHO1()`: resident-precomputed scalar result, returns `1`.
- `ZADD16(A,B,OUT%)` / `ZADD16(A,B)`: module 1 slot 0 payload, returns 16-bit sum.
- `UPPER(S$,OUT$)` / `UPPER(S$)`: module 1 slot 0 payload, copies and uppercases a string variable or quoted literal.
- `LOWER(S$,OUT$)` / `LOWER(S$)`: module 1 slot 0 payload, lowercases string byte values; tests verify bytes with `ASC()` because screen display case depends on the C64 charset mode.
- `ZHIDDENRAM(S$,OUT%)` / `ZHIDDENRAM(S$)`: module 1 slot 0 under-ROM worker proof, returns a simple checksum.
- `ZSUMNUMARRAY(A%(0),COUNT,OUT%)` / `ZSUMNUMARRAY(A%(0),COUNT)`: module 1 slot 0 payload, sums integer array elements.
- `ZRANGENUMARRAY(START,COUNT,A%(0))`: module 1 slot 0 payload, stages integer array output and resident commit writes it.
- `BUFNEW(LEN,H%)` / `BUFNEW(LEN)`: module 1 slot 0 payload, creates a persistent handle in bank `$44`.
- `BUFFILL(H%,BYTE)`: module 1 slot 0 payload, fills buffer handle pages and rejects non-buffer handles.
- `BUFFREE(H%)`: module 1 slot 0 payload, frees any valid handle type.
- `ZTEMPSCRATCH(LEN,OUT%)` / `ZTEMPSCRATCH(LEN)`: module 1 slot 0 payload, allocates and frees temporary pages, returning page count.
- `ZFAIL(CODE,OUT%)`: module 1 slot 0 payload, exercises the error path after output clearing.
- `FREEMEM()`: module 1 slot 0 payload, prints the current live BASIC free-byte count.
- `SCRCAP(H%)` / `SCRCAP()`: module 1 slot 0 payload, captures screen text plus color RAM into a typed screen handle.
- `FADD(A,B,OUT)` / `FADD(A,B)`: resident-computed demo command, returns a plain C64 BASIC float.
- `ZPAUSE(TICKS)`: module 1 slot 0 payload, waits for a number of jiffies.
- `ERRCODE(OUT%)` / `ERRCODE()`: resident-precomputed, returns the last ReadyBASIC runtime error code.
- `ERRLINE(OUT%)` / `ERRLINE()`: resident-precomputed, returns the last ReadyBASIC runtime error line, or `0` in direct mode.
- `SCRPUT(H%)`: module 1 slot 0 payload, validates a typed screen handle and restores screen text plus color RAM. This descriptor lives in slot 128 to prove full-table lookup.
- `ZSLOT0`, `ZSLOT1`, `ZSLOT2`, `ZSPAN`, `ZOVL1`, `ZOVL2`, `ZCPYRST`, and
  `ZCOPY`: built-in slot/span/overlay proof commands.
- `ZMODLD(name$)`: module 2 slot 1 SEQ package loader proof command.
- `ZDM1`, `ZDM2S`, `ZDOV1`, `ZDOV2`, and `ZSAA`-`ZUEB`: disk-loaded sample module commands.
- `GFXMODE(mode$)` / `GFXMODE()`: module 3 `GFXCORE`, switches or reads
  `TEXT`, `HIRES`, `MBITMAP`, `TILE`, and `MTILE`.
- `GFXTEXT()`, `GFXCLEAR(C)`, `GFXTARGET(H%)` / `GFXTGT(H%)`, and
  `GFXSYNC()`: module 3 `GFXCORE` Bank D setup/control commands.
  `GFXTARGET(0)` / `GFXTGT(0)` selects the visible target; `GFXTGT` is the
  stored-program-safe alias used by demos.
- `GFXSURF(mode$)` and `GFXBLIT(H%)`: slot-0 allocator-backed surface handle
  commands. They allocate/validate typed handle `3`; full REU drawing/blitting
  is future work.
- `PLOT(X,Y,C)`, `POINT(X,Y,OUT%)` / `PNT(X,Y,OUT%)`, `LINE(X1,Y1,X2,Y2,C)`,
  `RECT(X1,Y1,X2,Y2,C)`, `FRECT(X1,Y1,X2,Y2,C)`, `CIRCLE(X,Y,R,C)`,
  `FCIRCLE(X,Y,R,C)`, `TILE(X,Y,CH,C)`, and `CHARAT(X,Y,CH,C)`: module 3
  `GFXPRIM` immediate primitive/cell commands. In `MBITMAP`, these primitives
  use 160x200 logical coordinates and color-slot encoding: `0` clears, `1..15`
  writes color RAM and draws pair code `3`, `16..31` draws pair code `1`,
  `32..47` draws pair code `2`, and `48..63` explicitly draws pair code `3`.
- `POLY(A%(0),COUNT,C)`, `FPOLY(A%(0),COUNT,C)`, `POLYH(H%,COUNT,C)`,
  `FPOLYH(H%,COUNT,C)`, `PBUFNEW(COUNT,H%)`, `PBUFSET(H%,INDEX,X,Y)`, and
  `PBUFFREE(H%)`: module 3 `GFXPOLY` overlay commands. Point buffers are typed
  REU handle type `4`; `POLYH`/`FPOLYH` are explicit handle aliases because the
  same-name handle overload was not added to the resident parser.
- `SPRSET(N,ON,COLOR,PATTERN)`, `SPRMOVE(N,X,Y)`, `SPRCOLOR(N,COLOR)` /
  `SPRCOL(N,COLOR)`, `SPRROW(N,ROW,B1,B2,B3)`, `SPREXPAND(N,XON,YON)` /
  `SPRSIZE(N,XON,YON)`, `SPRPRI(N,BEHIND)`, `SPRMULTI(N,ON)` / `SPRMUL(N,ON)`,
  `SPRMCOLOR(C1,C2)` / `SPRMCO(C1,C2)`, `SPRSCAN()`, and `SPRCOLL(N,OUT%)`:
  module 3 `GFXSPR` overlay commands.
- `JOY(PORT,OUT%)`, `KEYP(OUT%)`, `KEYSCAN()`, and `KEYLAST(OUT%)`: module 3
  `INPUTEV` polling input commands.

Native reusable BASIC routines:

- `PROC NAME(P%,S$) ... ENDP`: input-only routine, called with `EXEC NAME(...)`.
- `FUNC NAME(P%,S$) ... RET expr ... ENDP`: input formals only; `RET`, `RET%`, or `RET$` supplies the return value.
- `EXEC NAME(...)`: scans stored BASIC text for the matching `PROC`, binds `%`/`$` actuals to formals, pushes a four-entry return stack in bridge state, and resumes at the routine body. `FUNC` is expression-only; `EXEC FUNC(...)` is rejected.
- `CALL` remains reserved for a future non-returning named transfer and is not implemented.

Native flow-control forms:

- `REPEAT ... UNTIL expr`: post-test loop, nested four deep. Overflow reports
  `?RB ERROR 35`; `UNTIL` without an active `REPEAT` reports `?RB ERROR 36`.
- `LABEL NAME`: stored-program marker.
- `JUMP NAME`: scans the stored program for `LABEL NAME`, then resumes there.
  Missing labels report `?RB ERROR 39`. Numeric `GOTO` remains BASIC ROM.

## Known V1 Boundaries

- No private command token: stored lines remain visible `COMMAND(...)` text, so
  regular BASIC `LIST` shows the command text.
- A tiny crunch hook delegates to ROM first, then normalizes tokenized
  `THEN EXEC ...` and `THEN JUMP ...` to colon-prefixed statements so BASIC's
  existing statement dispatcher reaches the `$0308` execute hook. Descriptor
  command statements after `THEN` should write the colon explicitly. String,
  `REM`, and `DATA` text are left alone.
- Raw stored-program `RUN` is supported through the `$0308` execute hook; the
  relocated BASIC sentinel byte at `BASIC_START-1` must stay zero.
- Command lookup is linear over descriptor pages in bank `$44`: one 256-byte page is fetched into `$C500`, eight descriptors are scanned locally, and the matched descriptor is copied into `$C480`.
- String input uses BASIC ROM string expression evaluation and then stages up to
  64 bytes. Numeric inputs support nested ReadyBASIC expression terms in the
  tested command and `FUNC` actual forms, including `ZADD16(1,ADDI(2,3))`-style
  integer cases and `FADD(1.5,FADD(2.25,3.25))` float cases.
- V1 integer arrays are explicit base element plus count, e.g. `A%(0),N`.
- Native routine V1 formals support `%`, `$`, and plain C64 BASIC float
  variables. There are no arrays, locals, or by-reference parameters. `FUNC`
  returns through `RET`, `RET%`, or `RET$` and has one returned value.
- Native routine definitions should be placed after `END`; fall-through into
  `PROC`/`FUNC` is invalid in V1.
- `REPEAT`/`UNTIL` and `LABEL`/`JUMP` are resident language features. They are
  stored as readable BASIC text and handled through the execute hook.
- The persistent typed heap currently suballocates 48KB inside bank `$44`; handle type `1` is a byte buffer and type `2` is a screen text+color buffer. Future large/long-lived data can allocate extra REU banks and record those banks in the same REU-backed handle directory.

## Current Flow And Error Introspection

The current design includes resident flow control and error introspection:

- `REPEAT` / `UNTIL expr`: post-test loops, nested four deep.
- `LABEL name` / `JUMP name`: named stored-program transfer without changing
  numeric `GOTO`.
- `ERRCODE()` / `ERRLINE()` and statement output forms return the last
  ReadyBASIC runtime error code and line.

Measured current layout: `BASIC_START=$2AC1`; BASIC owns `$2AC1-$9FFF`, for
`30013` formula empty free bytes. `ENTRY` is `$1000-$11FF` (`512` bytes),
`RESIDENT` is `$1200-$2ABF` (`6336` bytes), `HIDDEN` is `$A000-$A7E8`
(`2025` bytes), `BRIDGE` is `$C000-$C1FE` (`511` bytes), `LOWPACK` is `$07DC`
(`2012` bytes), `SLOTPACK2` is `$0759` (`1881` bytes), `OVL1PACK` is `$0272`
(`626` bytes), `OVL2PACK` is `$006D` (`109` bytes), `OVL3PACK` is `$079A`
(`1946` bytes), and `bin/readybasic.prg` is `28674` bytes because `CMDPACK2`
extends the cold-load seed image through `$7FFF`. BASIC free bytes remain
unchanged because the seed image is reclaimed before BASIC initialization.

## Current Nested-Term Support

ReadyBASIC keeps descriptor-backed commands in the same REU layout and uses
resident parsing/return handling for selected nested expression forms:

- Command and `FUNC` returns can be consumed by the tested BASIC ROM wrappers
  `ABS(ADDI(1,6)-10)` and `LEFT$(GREET("READY"),2)`.
- Numeric actuals for commands and `FUNC` calls accept one extra wrapper pair,
  as in `ADDI(1,(2+4))`, `ZADD16(1,(2+4))`, and `ADDI((1+2),(3+4))`.
- Fully recursive ReadyBASIC terms inside other ReadyBASIC actual lists remain
  future work.

## Current Float-Term Support

ReadyBASIC supports plain numeric float parameters and returns for commands and
native `FUNC`, and preserves parser state around nested command/`FUNC` calls so
selected calls work as BASIC expression terms inside ROM functions, arithmetic,
string concatenation, and other ReadyBASIC calls.

Proven forms include `ABS(FADD(1.2,2.3)-3)`, `ADDI(1,ADDI(2,3))`,
`FADD(1.5,FADD(2.25,3.25))`, `LEFT$(GREET("READY")+"!",3)`, and
`LEFT$(UPPER(GREET("ready")),2)`. `FADD` is computed by resident code and keeps
only a one-byte low-overlay stub because the actual calculation calls BASIC ROM
float helpers.

## Current Expression-Style Dispatch

ReadyBASIC installs an additional eval-vector hook at `$030A/$030B`. The hook
recognizes a small allow-list of expression-safe command calls and selected
numeric/string `FUNC` calls.

- Command expressions: `ZECHO1()`, `ZADD16(a,b)`, `ZHIDDENRAM(s$)`,
  `ZSUMNUMARRAY(a%(0),n)`, `BUFNEW(n)`, `ZTEMPSCRATCH(n)`, and `SCRCAP()`
  return integers or handles; `UPPER(s$)` and `LOWER(s$)` return strings.
- Parenthesized routine syntax: `PROC NAME(P%,S$)`, `FUNC NAME(S$)`, and
  `EXEC NAME(actuals...)` for non-empty argument lists.
- Zero-argument routines use `EXEC NAME`; empty parentheses were omitted to keep
  resident code smaller.
- `FUNC` returns use `RET expr`, with optional `RET% expr` and `RET$ expr`
  markers to make the return type explicit. Expression `FUNC` calls scan the
  routine body, execute simple scalar assignments before `RET`, and then
  evaluate the return expression.
