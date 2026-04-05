# Simple Cells SEQ Format

`Simple Cells` stores workbooks as binary SEQ files with a compact `SCLS1`
header. The format is optimized for low-RAM load/save on the C64.

Current workbook size in this build is `18` rows by `10` columns (`A`..`J`).

## File layout

Bytes are little-endian where multi-byte values appear.

### Header

- `0..4`: ASCII magic `SCLS1`
- `5`: format version, currently `1`
- `6`: row count
- `7`: column count
- `8`: active row (0-based)
- `9`: active column (0-based)
- `10`: first visible column (0-based)

### Column table

For each column:

- `width` (`1` byte)
- `type` (`1` byte)

Current column type codes:

- `0`: general
- `1`: text
- `2`: integer
- `3`: float2
- `4`: currency

### Cell records

Only populated cells are stored.

Each record contains:

- `row` (`1` byte)
- `col` (`1` byte)
- `flags` (`1` byte)
  - bit `0`: raw cell content starts with `=` and should be treated as a formula
- `color` (`1` byte)
  - `0`: default cell color
  - otherwise C64 color value used for the cell text
- `raw_len` (`1` byte)
- `raw bytes` (`raw_len` bytes)

Record stream terminates with a single byte `0xFF` in place of the next `row`.

## Notes

- Rows and columns are stored 0-based in the file, while the UI shows rows
  starting at `1` and columns starting at `A`.
- Files with unknown magic, unsupported version, invalid bounds, or truncated
  records are rejected during load.
- Formula results and cached display text are not stored; they are rebuilt after
  load or resume.
