# ReadyBASIC Future Goals

This document captures intended ReadyBASIC directions that are not part of the
current implemented design. The current source of truth remains
`readybasic.s` plus `READYBASIC_CURRENT_DESIGN.md`; items here are planning
notes for future implementation work.

## REU-Backed Handle System

ReadyBASIC now has a REU-backed handle descriptor system with 128 live handles.
RAM holds only enough scratch space to fetch, validate, and update one
descriptor at a time; the canonical descriptor directory and allocator bitmap
live in REU so the larger handle count does not consume scarce resident or
bridge RAM.

The implemented heap is a 48KB typed area in ReadyBASIC's loader-assigned core
bank. Future growth should keep this REU-first shape while adding richer descriptors,
optional extra banks for very large resources, and stale-handle protection.

Handles should be typed from the start rather than treated as generic memory
blocks. Some future handles will represent plain byte buffers, but others will
represent structured C64 resources such as text screen buffers, text-plus-color
screen buffers, bitmap or graphics screen buffers, variable character sets,
sprites, sprite sheets, file/cache objects, or command-private working sets.
Each command family should validate the handle type it accepts, so a screen
command cannot accidentally consume a sprite-sheet handle and a memory command
cannot silently reinterpret a structured graphics buffer.

Useful descriptor fields would likely include allocation state, type, flags,
REU bank, REU offset or page, byte length, and a generation/version byte. The
generation byte would let ReadyBASIC reject stale handles after free/reuse while
still keeping the BASIC-visible handle value compact.

## Command Signatures

The current signature parser is intentionally small and resident-code driven.
Future work should consider a compact REU-backed signature table so commands can
share parameter shapes without adding one resident parser case per new
command. The goal is not a large dynamic parser, but a small data-driven layer
for common forms such as numeric inputs, string inputs, array base/count pairs,
typed handles, and optional output targets.

Native `PROC`/`FUNC` now covers reusable BASIC-level routines with `%`, `$`, and
plain C64 BASIC float formals, `EXEC`, `RET`, and `ENDP`. `FUNC` definitions
have input formals only and return directly as BASIC expressions; `EXEC` is for
`PROC`. Future routine work should build from that implemented base:
possible follow-ups are `CALL` for non-returning named transfer, fall-through
skipping of definitions, richer parameter types, and optionally a small formal
metadata cache if repeated `EXEC` scans become too slow.

The expression-style work proved a useful split: expression-safe command
returns, string/numeric `FUNC` expression returns, and `FUNC` bodies with simple
assignments before `RET` are viable. The nested-term work proved the
specific ROM-consumer forms `ABS(ADDI(1,6)-10)` and
`LEFT$(GREET("READY"),2)` plus one-wrapper numeric actuals such as
`ADDI(1,(2+4))`. The float-term work then added plain C64 BASIC float
formals/returns and tested nested ReadyBASIC terms inside ROM functions,
arithmetic, string concatenation, and other ReadyBASIC actual lists. Future
work can now focus on richer in-body statements, a data-driven signature parser,
and broader command coverage rather than treating float/nested term support as
unproven.

## Command Naming

Future public command names should be screened against C64 BASIC tokenization
before implementation. Avoid names that contain BASIC keywords, functions,
operators, pseudo-variables, or short token names as substrings. In practice,
that means avoiding embedded words such as `SAVE`, `LOAD`, `RUN`, `LIST`, `NEW`,
`PRINT`, `INPUT`, `DATA`, `REM`, `SYS`, `FN`, `FRE`, and `PI`. Prefer a short
synonym over adding another resident parser exception.

Every new name should have direct and stored-program probes that cover `LIST`,
`RUN`, colon chains, expression use after `IF ... THEN`, explicit-colon command
statements after `THEN`, and normalized `THEN EXEC`/`THEN JUMP`. This catches
tokenizer surprises before the command becomes part of the user-facing
vocabulary.

## Resource-Oriented Commands

Future command families should favor stable handles for long-lived resources
instead of copying large values through BASIC variables. BASIC should keep small
integers and short strings visible to the user, while REU stores larger command
state and resource data. This keeps the BASIC workspace readable and compact
while allowing richer graphics, text, storage, and tool workflows.

## Module Catalog And Residency

The current module/submodule design proves fixed-address assembler payloads,
three 2KB under-ROM submodule slots, two built-in modules, overlay rotation, and
a disk-module loader command that lives in module 2 rather than resident code.
Future work should fill in the richer assigned core-bank `$2000-$3FFF` catalog that
the current implementation reserves:

- per-slot live records with module id, submodule id, overlay id, generation or
  checksum, slot mask, payload bank, and payload offset;
- command-name collision policy for disk-loaded modules;
- optional unload/replace policy for disk modules;
- additional payload banks when the assigned ReadyBASIC code bank does not have enough free space;
- a compact way to enumerate loaded modules from BASIC for diagnostics.

The design should keep the resident rule intact: resident code dispatches and
performs ROM-facing parse/commit work, while module policy such as disk loading,
overlay choice, and command-family helpers lives in under-ROM module code.

## Graphics Memory Goals

Graphics commands now exist as descriptor-backed built-in modules and overlays.
Future graphics work should keep the same proportional memory thinking:

- keep BASIC workspace pressure low by storing large buffers, sprite sheets,
  character sets, and bitmap resources in REU-backed handles;
- use VIC bank D and memory behind ROM only in ways compatible with the ReadyOS
  shim and the app contract;
- never use `$C800-$C9FF` unless deliberately calling the ReadyOS shim ABI;
- keep `$C600-$C7FF` unused unless the custom ReadyBASIC assembler/linker shape
  and all lifecycle tests are deliberately revised; it is app-private now;
- make graphics modes explicit about which C64 RAM ranges they claim while
  active, especially screen RAM, color RAM, character sets, and bitmap pages.

Implemented graphics already cover immediate bitmap/tile primitives, sprites,
polling input, polygon and convex fill helpers, retained display lists, REU
surface handles, charset/tileset/tilemap handles, and multicolor bitmap cell
semantics. Future work should focus on resource-file loading into REU, richer
asset packaging, more efficient dirty-region blits, and any new commands that
fit the existing overlay budgets.
