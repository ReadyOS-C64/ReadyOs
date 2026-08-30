# xuzstore

Standalone physical-C64 Ultimate probe for the production-shared method-0 ZIP
writer and Store parser/extractor. The C64 creates a streamed archive, closes
and reopens it, then extracts all files through the second Ultimate DOS target
into a separate tree while checking CRCs. It may run directly only as a named
development probe. Functional evidence comes from C64 Ultimate automation and
an independent host ZIP/byte oracle; no VICE runner exists.

The same run also extracts a deterministic host-created Store archive whose
local headers contain final CRC/sizes and whose entries have no data
descriptors. This keeps the proof from depending only on uZIP's own writer.
It then requires bounded rejection of truncated EOCD data, multi-disk metadata,
`..` traversal, and corrupted file data before reporting physical success.

Every run is compiled for one unique marked root under
`/USB1/READYOS_UZIP_TEST`. The runner preserves that root, its archive, and its
extracted tree. The host validates the archive with Python's ZIP implementation
and compares every C64-extracted byte with the deterministic fixtures.
