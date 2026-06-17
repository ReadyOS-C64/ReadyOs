# Implementation Log

- Added linker segments `OVL4PACK` and `OVL5PACK` for built-in slot-2 replacement overlays.
- Added submodules `GFXDL` and `GFXTILE`, stashed at code-bank offsets `$6800` and `$7000`.
- Added typed REU handle kinds for display lists, charsets, tilesets, and tilemaps.
- Implemented `DLMAKE`, `DLCLR`, `DLPLOT`, `DLLINE`, `DLRECT`, `DLFRECT`, and `DLDRAW`.
- Implemented `CHRMAKE`, `CHRROW`, `CHRUSE`, `TSMAKE`, `TSSET`, `TMMAKE`, `TMSET`, `TMDRAW`, `MCELL`, and `MCBG`.
- Fixed `TMDRAW` map handle preservation: the first version saved map bank/page in `RF_RECT_BUF`, which overlaps the tileset fetch buffer.
- Kept constructor command names as `*MAKE` because embedded `NEW` is unsafe under the unchanged BASIC tokenizer path.
- Added `GFXTGT` as a stored-program-safe descriptor alias for the existing `GFXTARGET` command id/worker after `GFXTARGET(H%)` tokenized badly in a `petcat` demo.
- Added `rbgfx26_mode_matrix.bas` to exercise immediate primitives across `HIRES`, `MBITMAP`, `TILE`, and `MTILE`.
- Added `rbgfx27_target_blit.bas` to exercise `GFXSURF`, `GFXTGT`, `GFXSYNC`, `GFXBLIT`, and visible-target restore.
