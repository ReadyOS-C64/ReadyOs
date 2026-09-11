# Demo music attribution

“Summer Vacation” (2010), by Shiru (the SID header says `Shiru 10'10`).
Licensed by the composer under **Creative Commons Attribution 3.0 Unported**.

- Composer and permission: https://shiru.untergrund.net/music.shtml
- License: https://creativecommons.org/licenses/by/3.0/
- Original archive: https://shiru.untergrund.net/files/mus/music_sid.zip
- Retrieved 2026-09-10. Archive SHA-256:
  `636c6e00054c48cb5ac52609355e3156e97a8b890607800232c6938beb284b87`.

The composer's permission covers original songs, not covers/collaborations.
This is listed as an original song. The original archive is preserved unchanged,
including editable GoatTracker `.sng` files. Its other original songs are
potential future demo material; only Summer Vacation is integrated and tested.

Modification: `summer-9200.sid` is relocated from `$1000-$1a40` to
`$9200-$9c40`, with zero-page workspace constrained to `$fc-$fd`. The composition
and credits are unchanged. No endorsement by Shiru is implied.

Relocation uses Linus Åkesson's MIT-licensed Sidreloc 1.0:
https://linusakesson.net/software/sidreloc/index.php

Command: `sidreloc -p 92 -r 10-1a -z fc-fd -s -t 0 summer_vacation.sid summer-9200.sid`

100,000 frames: **0 bad pitches, 0 bad pulse widths**, exit 0. This is dynamic
comparison, not a proof that arbitrary SID code is safe. A reproducible helper
is in `build_support/prepare_readybasic_music.py`.

Relocated SHA-256:
`aa70c84615f8cadfae1edd1dc60a5250fd84c2cc412542059c509345544062aa`.
