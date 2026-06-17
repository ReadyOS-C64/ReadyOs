# ReadyBASIC Graphics Phase 5 Lessons

- A VICE automation step passing is not enough for graphics work. The screenshot has to be visually inspected or pixel-counted.
- `GFXMODE("TILE")` was returning text mode because the four-character parser branch checked only the leading `T`. The fix is command parser logic, not ReadyBASIC language behavior.
- Running overlay code with BASIC ROM hidden is not enough for graphics; VIC/CIA/color/REU paths also need I/O visibility at the right moments.
- `$36` is appropriate for ordinary under-ROM command execution: BASIC ROM hidden, KERNAL and I/O visible.
- REU graphics DMA that touches Bank D bitmap RAM should use `$35` while programming/transferring, because it keeps the needed RAM visible without hiding the REU registers.
- Store graphics mode as command state instead of reverse-engineering it from live VIC/CIA registers on every draw. It is faster and avoids ReadyOS context surprises.
- Keep demos small, but make them visually unambiguous. Blank-screen tests are not tests.

