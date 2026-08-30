# xuzread

`xuzread` is the focused physical bridge from ZIP inspection to extraction.
It is a diagnostic build of the real ReadyOS `uzip` app and is always launched
through PREBOOT/BOOT/launcher on a physical C64 Ultimate.

The fixture contains seven nested Store/method-8 entries, signed descriptors,
and a 716-byte EOCD comment that crosses the reader's 512-byte backward-scan
boundary. While the `$B000` parser overlay is active, every random read uses
resident Ultimate SEEK plus direct transfer into an allocator-owned REU bank.
All central records are then round-tripped through a second owned catalog bank.
The probe performs no destination mutation.

Only the Terminal-owned C64U runner is functional authority. There is no VICE
plan or emulator target for this probe.
