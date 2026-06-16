# ReadyBASIC Graphics Phase 1 State

Objective: implement Phase 1 graphics as ReadyBASIC commands only, without
changing language/runtime behavior.

Design source: `src/apps/readybasic/READYBASIC_GRAPHICS_COMMAND_DESIGN.md`.

Current status:

- Phase 1 command-only implementation is in place.
- Old `agentworking` top-level artifacts archived under
  `agentworking/archive-pre-readybasic-gfx-phase1-20260615/`.
- `bin/readybasic.prg` builds with Phase 1 graphics command descriptors and
  built-in payloads.
- Graphics module payloads are grouped into non-default built-in packs:
  `GFXCORE`, `GFXPRIM`, `GFXSPR`, and `INPUTEV`.
- Twelve `rbgfx*.bas` demos exist and compile to `$2AC1` PRGs.
- Static checker validates graphics modules, Bank D guardrails, and flexible
  pack budgets while preserving the hard ReadyBASIC memory guards.
- Current map: `RESIDENT` `$1200-$2ABA`, `HIDDEN` `$A000-$A790`,
  `LOWPACK` `$A800-$AEFC`, `SLOTPACK1` `$B000-$B452`,
  `SLOTPACK2` `$B800-$BB62`, `OVL1PACK` `$BB63-$BCBA`, and
  `OVL2PACK` `$BCBB-$BD27`.

Known limits:

- `GFXTARGET(0)` is the implemented draw target. `GFXSURF` creates typed REU
  handles and `GFXBLIT(H%)` validates them, but offscreen draw/blit is not
  pixel-complete in Phase 1.
- `PNT(X,Y,V%)` is the preferred Phase 1 point-read spelling. `POINT` remains
  registered, but the short alias avoids a BASIC parser/name corner in probes.
- `LINE` is a simple endpoint stepper, not a full Bresenham implementation.
- Multicolor bitmap/tile modes set VIC mode/layout state, but primitives are
  still Phase 1 one-bit/cell-oriented behavior.
- Input is polling-only; no IRQ sampler was added.
