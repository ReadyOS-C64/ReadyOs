# ReadyBASIC Graphics Phase 2 Lessons

- Keep command names token-safe in demo source. PETCAT can split embedded BASIC
  tokens inside longer names, so visible demos should prefer aliases such as
  `SPRSIZE`, `SPRMUL`, `SPRMCO`, and `SPRCOL`.
- Do not hide sprite pixel setup. `rbgfx16` uses `SPRROW` to make the sprite
  bitmap auditable in BASIC source before testing movement and VIC controls.
- Split staged demo commands onto separate lines when debugging parser issues.
  It makes VICE screenshot failures point at the exact command.
- Slot-2 payload budget is now tighter. `GFXPRIM` is `$0515` bytes and the
  `GFXSPR` overlay is `$0272` bytes; larger commands should either replace
  approximations carefully or move into a separate overlay.
- ReadyOS-context screenshots are the real proof for graphics work. Direct PRG
  loads are not representative of ReadyBASIC running under ReadyOS.
