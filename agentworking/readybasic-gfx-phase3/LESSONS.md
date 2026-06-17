# ReadyBASIC Graphics Phase 3 Lessons

- Resident parser bytes are now the scarcest resource. Preserve
  `BASIC_START=$2AC1` first, even when that means command spelling is less
  elegant than the original design.
- Same-name overloads are expensive in this command spine because lookup returns
  one descriptor and the parser must distinguish array-base syntax from scalar
  numeric handle syntax.
- `GFXPOLY` belongs in its own replacement overlay. Putting polygon bodies into
  `GFXPRIM` would consume the remaining primitive headroom and make future
  primitive fixes harder.
- Document implementation-level approximations plainly. Current filled polygons
  are convex fan fills; a true scanline fill should be a future overlay-growth
  task, not silently implied by the command name.
- ReadyOS-context VICE automation remains the real graphics proof. All Phase 3
  demos are loaded and run from inside ReadyBASIC after ReadyOS launches it.
- Demo text should avoid command-looking words in visible strings and even in
  casual REM text. Existing command hook behavior can scan surprising places, so
  the Phase 3 demos use neutral labels and done markers.
- Do not keep polygon loop state in command frame fields reused by primitive
  drawing. The line worker mutates current-point fields; polygon count and index
  need separate scratch bytes.
- Screenshot demos should hold graphics mode with a `GET` loop and let VICE
  automation press SPACE after capture. Pause-only demos can return to text
  before the harness captures, especially when command waits are byte-sized or
  intentionally short.
