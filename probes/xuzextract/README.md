# xuzextract

`xuzextract` is the focused physical transaction probe between the proven ZIP
reader/inflater pieces and the final ReadyOS Ultimate zip UI. It is a diagnostic
build of the real `uzip` app and boots only through PREBOOT, BOOT, and launcher
on a physical C64 Ultimate.

Before creating any destination, the C64 preflights every central and local
record in both fixture archives through bounded UltimateDOS SEEK/LOAD_REU
callbacks. It then uses the production `uz_extract_member` path to:

- create two nested directories;
- extract a 12,289-byte Store member;
- extract a 34,800-byte dynamic-Deflate member;
- refuse an existing final filename without changing its sentinel bytes; and
- reject a structurally valid member whose claimed CRC is false, deleting its
  temporary sibling.

The host oracle downloads both successful files and the existing sentinel,
compares every byte, and checks every touched directory for `.uztmpXX` leaks.
The owned test root is preserved as physical evidence. Functional authority is
the Terminal-owned C64U runner; this probe has no VICE or emulator target.

Run one fresh 16 MHz candidate through
`build_support/start_xuzextract_c64u_terminal.sh`. After that exact candidate
passes, run the acceptance matrix through
`build_support/start_xuzextract_c64u_matrix_terminal.sh`; it defaults to
1, 16, and 64 MHz. The Terminal starter freezes both the matrix driver and its
per-speed runner before launching so later source edits cannot change a live
hardware run.
