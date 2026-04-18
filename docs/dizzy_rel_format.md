# Dizzy REL Format

`dizzy` stores its board in `dizzy.rel` and its UI state in `dizzycfg.rel`.
Both files are Commodore REL files with fixed-size records:

- `dizzy.rel`: 64-byte records
- `dizzycfg.rel`: 32-byte records

Within each record, numeric fields are stored as fixed-width ASCII decimal.
Unused bytes are space-filled. Record numbers are 1-based.

## `dizzy.rel`

The board file always uses 97 records:

- Record `1`: board superblock
- Records `2..97`: card slots for card IDs `1..96`

Card ID `N` always lives in record `1 + N`.

## Board Superblock

Record `1` stores:

- Byte `0`: ASCII `d`
- Byte `1`: ASCII `z`
- Byte `2`: ASCII `r`
- Byte `3`: ASCII `b`
- Byte `4`: version, currently ASCII `1`
- Bytes `5..6`: used-card count, 2 decimal digits
- Bytes `7..11`: payload checksum, 5 decimal digits
- Bytes `12..13`: max card count, 2 decimal digits, currently `96`
- Byte `14`: column count, 1 decimal digit, currently `3`

The checksum is not a raw byte sum of the REL file. It is recomputed from the
logical board model by summing:

- each column's card count
- for every used card: ID, column, ordinal, flags, created day, due day,
  snooze day, and title bytes

The loader verifies both the stored used count and this checksum after reading
all card records.

## Card Records

Each card slot record stores one stable card ID.

- Byte `0`: record type, ASCII `C`
- Byte `1`: used flag, ASCII `0` or `1`
- Bytes `2..3`: card ID, 2 decimal digits

If byte `1` is `0`, the slot is unused and the remaining fields are ignored.

Used card fields:

- Byte `4`: column index, 1 decimal digit
  - `0`: `NOT NOW`
  - `1`: `MAYBE?`
  - `2`: `DONE`
- Bytes `5..6`: ordinal within the column, 2 decimal digits
- Bytes `7..8`: flags, 2 decimal digits
- Bytes `9..11`: created day-of-year, 3 decimal digits
- Bytes `12..14`: due day-of-year, 3 decimal digits
- Bytes `15..17`: snooze-until day-of-year, 3 decimal digits
- Bytes `18..19`: title length, 2 decimal digits
- Bytes `20..63`: raw title bytes, up to 44 bytes

Current flag bits:

- `0x01`: done
- `0x02`: archived
- `0x04`: snoozed
- `0x08`: subscribed

Notes:

- `due_day` and `snooze_day` use `0` when unset.
- Titles are stored without a trailing NUL byte.
- Load reconstructs each column by placing used cards into the stored
  `column + ordinal` positions.

## `dizzycfg.rel`

The config file uses one 32-byte record at record `1`.

- Bytes `0..3`: ASCII magic `dzcf`
- Byte `4`: version, currently ASCII `1`
- Byte `5`: selected column, 1 decimal digit
- Bytes `6..7`: selected visible index for column `0`, 2 decimal digits
- Bytes `8..9`: selected visible index for column `1`, 2 decimal digits
- Bytes `10..11`: selected visible index for column `2`, 2 decimal digits
- Byte `12`: view mode, 1 decimal digit
- Bytes `13..17`: config checksum, 5 decimal digits

Current view modes:

- `1`: single expanded column
- `2`: double expanded columns

The config checksum is computed from:

- selected column
- selected index for each column
- view mode

If the config record is missing, malformed, or has a bad checksum, `dizzy`
recreates it with defaults:

- selected column `1`
- selected indexes `0,0,0`
- view mode `1`
