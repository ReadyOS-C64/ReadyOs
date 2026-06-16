# ReadyBASIC Graphics Phase 1 Lessons

- Keep graphics state command-owned. Do not alter ReadyBASIC language paths.
- Bank D graphics must not use `$C000-$C9FF`; ReadyBASIC and ReadyOS own that
  area for bridge, shared frames, REU metadata, and shim ABI.
- The existing static checker froze old proof payload sizes, so graphics work
  must update it to validate ranges and budgets instead of exact legacy sizes.
- Descriptor lookup does not need BASIC ROM and can live under hidden RAM; this
  is useful resident-pressure relief without changing language behavior.
- Phase 1 can expose the REU surface handle ABI before full offscreen drawing,
  but docs/tests must be explicit that `GFXTARGET(0)` is the implemented draw
  target and `GFXBLIT(H%)` is currently handle validation.
- Launcher-cycle hotkey verification can hang at preboot `LOADING LAUNCHER...`
  even after the default hotkey probe passes. Keep this separated from graphics
  command verification unless a focused change touches launcher/profile boot.
- Visual ReadyOS-context screenshots matter for graphics commands. The
  command-only probe passed before screenshots showed blank output because VIC
  mode registers were being clobbered after the command handler returned.
- Keep demos inside the already-supported language surface. C64 BASIC rejects
  `FOR X%` style loop variables here, and ReadyBASIC intentionally does not
  add command-after-`THEN` behavior for Phase 1 graphics.
- Bitmap screen RAM color bytes are part of visibility, not just clearing.
  Clearing bitmap memory while setting foreground equal to background can make
  correct plotted pixels look blank.
- Sprite demos need explicit pattern data. `SPRSET`/`SPRMOVE` prove VIC state,
  but `SPRROW` makes the pixels visible and auditable from BASIC source.
- Slot-2 overlay descriptors must describe the linked payload location, not
  assume every overlay begins at `$B800`. Otherwise absolute references inside
  the overlay resolve against the wrong runtime address.
