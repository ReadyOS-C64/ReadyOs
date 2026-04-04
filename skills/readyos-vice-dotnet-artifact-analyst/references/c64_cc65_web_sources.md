# C64 / cc65 Web Research Sources

Use these primary references when evaluating C64 memory mapping, IRQ behavior, and cc65 C/asm bridge assumptions.

## cc65
- cc65 internals and ABI details: https://cc65.github.io/doc/cc65-intern.html
- cc65 user guide: https://cc65.github.io/doc/cc65.html

## VICE
- VICE monitor reference: https://vice-emu.sourceforge.io/vice_12.html
- VICE resources and monitor behavior details: https://vice-emu.sourceforge.io/vice_13.html

## C64 memory mapping
- C64 Programmer's Reference ($0001 / memory configuration context):
  https://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_5/page_260.html

## Usage note
- Prefer these primary docs over secondary forum summaries.
- For every hypothesis touching map/IRQ/calling convention, cite at least one of these sources in the investigation notes.

## Key reminders from sources
- cc65 default calling convention is `fastcall`; variadic functions always use `cdecl`.
- For C-callable asm functions in cc65, C-stack restoration is callee responsibility.
- `Y` may be clobbered by called functions; caller must not rely on it after calls.
- `A/X/sreg` may be clobbered except where needed for return value.
