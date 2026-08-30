# xuzzip8 physical ReadyOS diagnostic

`xuzzip8` proves the production method-8 creation boundary on a physical C64
Ultimate before the final TUI can create user archives.

For each deterministic source it opens a new archive through Ultimate DOS,
uses the Store image for the local method-8 header, alternates the promoted
MATCH and EMIT images for raw RFC 1951 output, then reloads Store for the
signed descriptor, method-aware central record, EOCD, and close. Only the
small writer/record state is staged in allocator-owned `uzwk`; source and ZIP
bytes remain streamed.

The Terminal-owned runner uses a unique owner-marked directory below
`USB1/READYOS_UZIP_TEST/XUZZIP8-*`, preserves that directory, runs ReadyOS
itself at a read-back 1, 16, or 64 MHz, and downloads all four ZIPs. The host
oracle checks their byte-level headers/descriptors/central directory and exact
Python `zipfile` extraction. It never launches or uses VICE.

Launch one physical run with:

```sh
XUZZIP8_SPEED_MHZ=16 XUZZIP8_QUIET_S=120 \
  /bin/bash build_support/start_xuzzip8_c64u_terminal.sh
```

The starter prints unique `/tmp/xuzzip8_c64u_*.log` and `.status` paths. A
zero status is valid only when the persistent C64 result, downloaded archive
oracle, live-speed readback, owned-bank release, stack watermark, and exact CPU
setting restoration all pass.
