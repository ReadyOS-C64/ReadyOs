# ReadyBASIC Sound Phase 1 Implementation Log

## 2026-06-17

- Added linker segment `SLOT2OVL6` / `OVL6PACK`.
- Added sound constants for SID register addresses.
- Added `RB_MODULE_SID=4`, `RB_SUBMOD_SIDCORE=23`, and `RB_CODE_SIDCORE_OFF=$7800`.
- Added `CMD_SIDCORE` descriptor macro and 16 command descriptors.
- Extended cold prestash table from five to six built-in overlays.
- Implemented `SIDCORE` direct SID commands in `OVL6PACK`.
- Tried a seven-number command parser signature for friendly `VOICE(V,F,W,A,D,S,R)`, but it grew resident code by 31 bytes and overflowed the fixed resident budget.
- Reworked `VOICE` to the faster packed SID form `VOICE(V,F,W,AD,SR)` using the existing five-number parser signature.
- Added six human-audible BASIC demos and a ReadyOS/ReadyBASIC VICE automation script.
