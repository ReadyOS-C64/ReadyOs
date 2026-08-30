# xuzdeflate physical ReadyOS diagnostic

`xuzdeflate` proves the production fixed-Deflate creator before the final TUI
can invoke it. Unlike the queue-only standalone probes, this codec needs a 4K
token stream and therefore runs as a diagnostic build of the real ReadyOS
`uzip` app. Its scratch bank is allocated by `reu_alloc_owned_bank`; the probe
never guesses at an arbitrary REU address.

The shared implementation uses a 2K input block, an 8K history, 1,024 hash
heads, 8,192 previous links, and at most 64 candidates per position. It loads
the packed MATCH and EMIT images alternately at `$B000`, stages tokens in the
owned scratch bank, and keeps its packed coordinator at `$A000-$AFFF` outside
that replaceable window. A resident trampoline snapshots and restores the
exact idle `$3000-$AFFF` bytes around every case.
Every block is tokenized once and emitted as fixed Huffman only when its exact
bit cost is no larger than a stored Deflate block.

The deterministic corpus covers empty output, highly repetitive data,
incompressible data, and a repeated random 8K half which requires the full
history distance. The physical C64 writes raw RFC 1951 streams; the host
downloads each stream, decodes it independently with zlib, compares every
byte, validates CRC/size and fixed/stored block counters, and checks stack and
workspace guards.

All functional runs use the C64 Ultimate runner at 1, 16, and 64 MHz. No VICE
or other emulator invocation is permitted for this probe.
