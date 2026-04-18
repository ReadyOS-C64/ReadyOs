# Calendar 26 REL Format

`cal26` stores event data in `cal26.rel` and UI/config state in
`cal26cfg.rel`. Both files use REL records with fixed-width ASCII decimal
fields and space-filled unused bytes.

- `cal26.rel`: 64-byte records
- `cal26cfg.rel`: 32-byte records

Record numbers are 1-based.

## `cal26.rel`

`cal26.rel` is split into three regions:

- Record `1`: superblock
- Records `2..366`: one day-index record for each day of 2026
- Records `367+`: event records allocated on demand

The app is fixed to year `2026`, so the day index table always covers day of
year `1..365`.

## Superblock

Record `1` stores:

- Bytes `0..3`: ASCII magic `c26e`
- Byte `4`: version, currently ASCII `1`
- Bytes `5..9`: free-list head record number, 5 decimal digits
- Bytes `10..14`: next never-used event record number, 5 decimal digits
- Bytes `15..19`: checksum, 5 decimal digits

The checksum is the sum of bytes `0..14` of the superblock record before the
checksum field. On load, `cal26` verifies the magic, version, and checksum.

Semantics:

- `free_head = 0` means there are no reusable deleted event slots.
- `next_record` starts at `367` and grows as new event records are allocated.
- Deleted event records are recycled through the free list.

## Day Index Records

Records `2..366` map one day-of-year to its linked list of events.

For day `N`, the index record is at record `1 + N`.

Layout:

- Byte `0`: record type, ASCII `i`
- Bytes `1..3`: day-of-year, 3 decimal digits
- Bytes `6..10`: head event record number, 5 decimal digits
- Bytes `11..15`: tail event record number, 5 decimal digits
- Bytes `16..20`: event count for the day, 5 decimal digits

Semantics:

- `head = 0` and `tail = 0` mean the day has no events.
- New events are appended to the tail of the day's list.
- The app validates that each day record's stored day matches its record
  position during load.

## Event Records

Event records start at record `367`.

Layout:

- Byte `0`: record type, ASCII `e`
- Byte `1`: done flag, ASCII `0` or `1`
- Byte `2`: deleted flag, ASCII `0` or `1`
- Bytes `3..5`: owning day-of-year, 3 decimal digits
- Bytes `6..10`: previous event record number, 5 decimal digits
- Bytes `11..15`: next event record number, 5 decimal digits
- Bytes `16..17`: text length, 2 decimal digits
- Bytes `18..63`: raw event text, up to 46 bytes

Current flag bits in the in-memory model:

- `0x01`: done
- `0x02`: deleted

Semantics:

- `prev` and `next` link active events into a doubly linked list per day.
- `0` in `prev` or `next` means no previous or next record.
- Text is stored without a trailing NUL byte.
- Deleted records are written back with the deleted flag set and their `next`
  field reused as the free-list pointer to the next reusable slot.

## Allocation Model

`cal26` does not compact the file. Instead:

- creating an event first consumes `free_head` if a deleted record is available
- otherwise it writes to `next_record` and then increments `next_record`
- deleting an event pushes that record onto the free list

This means record numbers are stable for live events until deletion, and the
file can grow over time even if later deletions free slots for reuse.

## `cal26cfg.rel`

The config file uses one 32-byte record at record `1`.

- Bytes `0..3`: ASCII magic `c26c`
- Byte `4`: version, currently ASCII `1`
- Bytes `5..7`: `today` day-of-year, 3 decimal digits
- Byte `8`: week-start flag, ASCII `0` or `1`

Current behavior:

- `today` is clamped to the valid `1..365` range on load
- `0` is the default week-start setting used by the app
- if the config record is missing or invalid, `cal26` recreates it using the
  compile-time current day and default week-start `0`

Unlike the main events REL file, `cal26cfg.rel` does not store a checksum.
