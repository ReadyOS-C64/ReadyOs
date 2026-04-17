# ReadyShell RSV1 Value Format

`STV` writes ReadyShell values in a compact binary container with the ASCII
magic `RSV1`. `LDV` reads the same format back and rebuilds scalar values,
strings, arrays, and objects in the ReadyShell REU value arena.

This is the on-disk format for ReadyShell value snapshots, not a text format.

## Container Header

- Bytes `0..3`: ASCII magic `RSV1`
- Bytes `4..5`: payload length, little-endian `u16`
- Bytes `6..`: serialized root value payload

`LDV` validates both the magic and the exact payload length before decoding.
There is no checksum or footer; the file length must be exactly `6 + payload`.

## Root Value Behavior

There are two common write shapes:

- Direct `STV <expr>, <file>` writes one serialized root value.
- Pipeline `... | STV <file>` writes a root array whose items are the emitted
  pipeline values.

Examples:

- `STV 42, "answer"` stores one root `U16` value.
- `LST | TOP 3 | STV "dir3"` stores one root array containing three objects.

On load:

- Direct `LDV "answer"` returns the root value as-is.
- When `LDV` is used as a pipeline source and the root value is an array, it
  emits the array items one by one.

## Value Encoding

Each value starts with a one-byte tag.

Current serialized tags:

- `0x00`: `FALSE`
- `0xFF`: `TRUE`
- `0x09`: unsigned 16-bit integer
- `0x02`: string
- `0x06`: array
- `0x08`: object

Pointer-backed runtime variants such as string, array, and object pointers are
flattened to their plain serialized forms before writing.

There is no separate on-disk "custom type" tag. Custom structured values are
stored as normal `OBJECT` values with whatever property names and nested values
the shell provides.

## Scalar Forms

### Boolean

- `FALSE` is a single byte `0x00`
- `TRUE` is a single byte `0xFF`

### Unsigned 16-bit Integer

- Byte `0`: tag `0x09`
- Bytes `1..2`: value as little-endian `u16`

### String

- Byte `0`: tag `0x02`
- Byte `1`: string length `0..255`
- Bytes `2..`: raw string bytes

Strings are stored as byte strings with no trailing NUL in the file.

## Byte Layout Pictures

The diagrams below show the exact byte sequence shape in a markdown-safe ASCII
form. Hex byte values are shown in brackets.

### Container

```text
+--------+--------+--------+--------+--------+--------+-------------------+
| [52] R | [53] S | [56] V | [31] 1 | len lo | len hi | serialized value  |
+--------+--------+--------+--------+--------+--------+-------------------+
  byte 0   byte 1   byte 2   byte 3   byte 4   byte 5   byte 6...
```

### FALSE

```text
payload:
+--------+
| [00]   |
+--------+
  FALSE
```

### TRUE

```text
payload:
+--------+
| [FF]   |
+--------+
  TRUE
```

### Unsigned 16-bit Integer

Example: `42`

```text
payload:
+--------+--------+--------+
| [09]   | [2A]   | [00]   |
+--------+--------+--------+
  U16      low      high
```

### String

Example: `"HEY"`

```text
payload:
+--------+--------+--------+--------+--------+
| [02]   | [03]   | [48] H | [45] E | [59] Y |
+--------+--------+--------+--------+--------+
  STR      len=3    data     data     data
```

### Array

Example: `1,2,3`

```text
payload:
+--------+--------+--------+-----------------------------------------------+
| [06]   | [03]   | [00]   | item1 | item2 | item3                         |
+--------+--------+--------+-----------------------------------------------+
  ARRAY    count lo count hi

item1 = [09] [01] [00]
item2 = [09] [02] [00]
item3 = [09] [03] [00]
```

### Nested Array

Example: `"HEAD", (1,2), "TAIL"`

```text
payload:
+--------+--------+--------+-----------------------------------------------+
| [06]   | [03]   | [00]   | item1 | item2 | item3                         |
+--------+--------+--------+-----------------------------------------------+
  ARRAY    count=3

item1: [02] [04] [48] [45] [41] [44]
       STR   len   H    E    A    D

item2: [06] [02] [00] [09] [01] [00] [09] [02] [00]
       ARRAY count=2   U16 1            U16 2

item3: [02] [04] [54] [41] [49] [4C]
       STR   len   T    A    I    L
```

### Object

Example: `{NAME:"BOOT",BLOCKS:12}`

```text
payload:
+--------+--------+-----------------------------------------------+
| [08]   | [02]   | prop1-name | prop1-value | prop2-name | ...   |
+--------+--------+-----------------------------------------------+
  OBJECT   prop count=2

prop1-name:
  [02] [04] [4E] [41] [4D] [45]
   STR  len   N    A    M    E

prop1-value:
  [02] [04] [42] [4F] [4F] [54]
   STR  len   B    O    O    T

prop2-name:
  [02] [06] [42] [4C] [4F] [43] [4B] [53]
   STR  len   B    L    O    C    K    S

prop2-value:
  [09] [0C] [00]
   U16   12
```

### Custom Structured Object

There is no dedicated custom-type opcode. A custom structure is just an object
with user-chosen property names and nested values.

Example:

```text
{
  KIND: "SNAP",
  ITEMS: ("A","B"),
  META: {COUNT:2, OK:TRUE}
}
```

ASCII view:

```text
OBJECT
+-- "KIND"  -> STR "SNAP"
+-- "ITEMS" -> ARRAY
|             +-- STR "A"
|             +-- STR "B"
+-- "META"  -> OBJECT
              +-- "COUNT" -> U16 2
              +-- "OK"    -> TRUE
```

## Arrays

- Byte `0`: tag `0x06`
- Bytes `1..2`: item count as little-endian `u16`
- Then each item, serialized recursively in order

Array order on disk is the same order seen in the pipeline or expression.

## Objects

- Byte `0`: tag `0x08`
- Byte `1`: property count `0..255`
- Then for each property, in stored order:
  - serialized property name as a string value
  - serialized property value

Property names are stored as normal serialized strings, not as fixed-size
fields.

## Practical Examples

### Direct Scalar Save

`STV 42, "answer"` writes:

- `RSV1`
- payload length `3`
- root value:
  - tag `0x09`
  - bytes `0x2A 0x00`

### Direct Object Save

`STV DRVI, "driveinfo"` writes:

- `RSV1`
- payload length for one root object
- root object with properties such as `DRIVE` and `DISKNAME`

### Direct Array Save

`STV 1,2,3, "nums"` writes:

- `RSV1`
- payload length for one root array
- root array tag `0x06`
- count `3`
- three serialized `U16` values

### Direct Nested Array Save

`STV "HEAD",(1,2),"TAIL", "nested"` writes:

- `RSV1`
- payload length for one root array
- a root array whose second item is itself a serialized array

### Pipeline Save

`LST | TOP 2 | STV "dir2"` writes:

- `RSV1`
- payload length for one root array
- root array tag `0x06`
- count `2`
- two serialized directory-entry objects

### Custom Structured Save

Any command or script that builds a structured object saves as an `OBJECT`
value with recursively serialized properties.

Example shape:

```text
{
  KIND:"SNAP",
  ITEMS:("A","B"),
  META:{COUNT:2,OK:TRUE}
}
```

This still uses only the standard serialized tags: `OBJECT`, `ARRAY`, `STR`,
`U16`, and `TRUE`.

## Limits And Notes

- Total serialized payload is limited to `65535` bytes by the 16-bit length
  field.
- Serialized strings are limited to `255` bytes each.
- Serialized object property count is limited to `255`.
- Serialized arrays use a 16-bit item count.
- `FLOAT` values are not part of the current `STV`/`LDV` serialized format.

## Implementation Notes

- Generic serializer/deserializer: `src/apps/readyshellpoc/core/rs_serialize.c`
- Direct value command path: `src/apps/readyshellpoc/core/rs_cmd_stv_c64.c`
- Load/validate path: `src/apps/readyshellpoc/core/rs_cmd_ldv_c64.c`

The REU-backed runtime value arena is an internal execution detail. The `RSV1`
file only stores the flattened value tree needed to rebuild that runtime state.
