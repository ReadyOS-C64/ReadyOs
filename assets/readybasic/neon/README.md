# Orbital Echoes assets

Background: **Space Background**, by **Luis Zuno / Ansimuz**, published March
13, 2015 under **CC0 1.0 Universal**. The original downloaded archive and its
license notice are preserved here.

- Source: https://opengameart.org/content/space-background-3
- Download: https://opengameart.org/sites/default/files/space_background_pack.zip
- License: https://creativecommons.org/publicdomain/zero/1.0/
- Retrieved: 2026-09-11. Exact hashes are in `checksums.json`.

The C64 adaptation composites the supplied layers, adjusts exposure and
quantizes to 160x200 logical pixels with one global background and three local
colors per 4x8 cell. Unused local slots receive contrasting colors for the
additive line effect. `rb.neon.koa` is a standard 10003-byte uncompressed Koala
file, stored as a SEQ entry on the demo disk. Preview PNGs are decoded from the
same palette/pixel assignments used by the binary resources.

The five **READY** glyphs are original pixel-grid artwork for this demo: heavy
chamfered shapes, a cyan/white split face and a blue extrusion. No font is used.
`rb.ready.rbr` contains an RBR1 header and five 64-byte multicolor sprites at
$CA00-$CB3F. Shared colors are white (1) and blue (6), individual color cyan (3).

Offline regeneration: `python3 build_support/prepare_readybasic_neon.py`
(requires Pillow and numpy). Normal builds use the committed binary resources
and do not download art or require those packages. Source code for the glyphs
and conversion is preserved in that script.

The BASIC demo also credits the art and **Summer Vacation** by Shiru, CC BY 3.0.
Music licensing and relocation details are in `../music/README.md`. The line
motion is an original sine-driven interpretation of classic kinetic C64 demos;
no Tubular Bells code, music or graphics are copied.
