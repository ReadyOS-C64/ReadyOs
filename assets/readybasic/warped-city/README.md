# Warped City — prepared second background

Status: **integrated into both demo sources and the D81 build profile** as a
blue/purple city contrast to the red space background in Orbital Echoes.
The canonical asset remains Koala; the build losslessly packs it as RKC1 for
the existing `MCFILE` command. See the demo's current verification notes for
which disk profiles have been rebuilt and physically tested.

## Source and permission

- Artwork: **Warped City**, **Luis Zuno / Ansimuz**.
- Artist's publication: <https://opengameart.org/content/warped-city>
- Posted January 29, 2019; source/license checked September 11, 2026.
- Publication license: **CC0 1.0 Universal**,
  <https://creativecommons.org/publicdomain/zero/1.0/>.
- Original archive:
  <https://opengameart.org/sites/default/files/warped_city_files.zip>
- The archive's unmodified `public-license.txt` is preserved under `source/`.
  It explicitly declares the artwork public domain, available for personal or
  commercial use, with credit appreciated but not required.
- Original ZIP SHA-256:
  `cf0e69a203206f529adbaf1f82d4c5f165ca9cdb49d3995ec88d135b37e40e3e`.
- **The archive also contains music under separate attribution terms. None of
  that music is used, converted, or included in this asset package.** Continue
  using the existing Shiru tune.

Only the artist's environment artwork is used. There are no people, sexual
images, game characters, or logos added to the prepared scene. Only the three
used source PNGs and original license notice are included. The downloaded ZIP
is preserved locally in the ignored
`build/readybasic-media-tools/warped-city-source.zip` for provenance, not committed
or shipped. It is not needed to regenerate the resource.

Artist and source attribution are included in both demo sources. Expanded credit:

```basic
REM WARPED CITY ART: LUIS ZUNO/ANSIMUZ
REM CC0 - OPENGAMEART.ORG
REM C64 ADAPTATION: TILED/CROPPED/QUANTIZED
```

## Prepared files

- `rb.warp.koa`: **10,003 bytes**, uncompressed Koala with `$6000` load prefix,
  8,000 bitmap bytes, 1,000 screen bytes, 1,000 color-RAM bytes, and background
  index **0**. The build packages its lossless RKC1 counterpart as **SEQ**
  `rb.warp`, alongside `rb.neon`.
- `background-preview.png`: a **640×400 nearest-neighbor decode of the actual
  resource bytes**, showing its 160×200 logical multicolor pixels at 2:1 width.
- `composition-preview.png`: the pre-quantization 320×200 source composition.
- `prepare.py`: reproducible offline converter; requires Pillow and numpy.
- `checksums.json`: SHA-256 source/output provenance and format audit results.

The adaptation tiles the supplied sky, selects one planet from its alternate
sky layer, fits the complete foreground-building width into the lower 136
display rows, boosts exposure 2.4×, and chooses C64 palette colors per 4×8
logical-pixel cell. The top third remains predominantly sky for the READY wave.
Fine signs/window details necessarily simplify at C64 resolution; this is not
a lossless rendering of the original art.

Conversion follows the palette and weighted squared RGB error metric in
`build_support/prepare_readybasic_neon.py`. Every cell has the common black
background plus three distinct local colors. Unused local slots receive
contrasting colors for additive line drawing, as in the existing background.

Regenerate from the preserved source PNGs:

```sh
python3 assets/readybasic/warped-city/prepare.py
```

Validation decodes the output bytes, checks its header/length, all color-RAM
nibbles, shared black background, and four distinct palette slots in all 1,000
cells. The preview contains 11 of the 16 C64 colors. This is an offline format
check and visual review, **not a hardware test**.

## Integration constraints

- A second uncompressed Koala needs **40 disk blocks**. The recent Ultimate
  build had only one free block, so an additive image cannot simply be appended;
  both pictures are now losslessly packed to 9,141 bytes combined instead.
- Cache a second typed multicolor surface using the established REU API, then
  alternate cached surfaces without disk I/O during music. Each surface uses
  40 REU pages (10,240 bytes). Do not reuse the same cache handle for both.
- Both pictures use global background **0**, useful because `GFXSYNC`/`GFXBLIT`
  cache bitmap, screen, and color data but not the global background register.
- Load/cache both pictures before starting music, while disk access is safe;
  the current loaders reject an active music lifetime.
- Preserve the Space reset semantics (restore the current picture) and Q/M
  cleanup semantics: both exits release both image handles.
