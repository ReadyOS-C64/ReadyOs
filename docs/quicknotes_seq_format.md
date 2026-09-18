# Quicknotes SEQ Format

`quicknotes` stores one notebook per SEQ file.

The format is compact and versioned so the C64 app can stream it directly while
keeping note bodies in REU.

Titles and note text are **PETSCII**, stored as C64 bytes without ASCII or
UTF-8 conversion. Binary counts and lengths are not character-encoded digits.

## Header

- Bytes `0..3`: PETSCII magic `QNTS`: hex `D1 CE D4 D3` (the cc65 C64 uppercase literals, not ASCII `51 4E 54 53`)
- Byte `4`: version, currently `1`
- Byte `5`: note count (`1..50`)
- Byte `6`: active note index (`0..note_count-1`)
- Byte `7`: reserved, currently `0`

## Note Records

For each note:

- Bytes `0..19`: PETSCII title, fixed 20 bytes, NUL-padded if shorter
- Byte `20`: line count (`1..50`)
- Then for each stored line:
  - Byte `0`: line length (`0..29`)
  - Bytes `1..N`: raw PETSCII line bytes

## Semantics

- Titles are stored separately from note body text.
- Note order in the file is the visible order in the left pane.
- Only the used lines and used bytes are written; blank tail rows are omitted.
- On load, `quicknotes` rebuilds its REU-backed note storage and fetches only the
  active note into C64 RAM for editing.
