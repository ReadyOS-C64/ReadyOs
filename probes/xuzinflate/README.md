# xuzinflate

Standalone physical-C64 Ultimate probe for the bounded streaming RFC 1951
assembly inflater. It uses Ultimate DOS targets 1 and 2 concurrently, reads
compressed input in 512-byte chunks, retains the exact `$3000-$AFFF` 32K
dictionary in C64 RAM, and streams decoded bytes through a 508-byte output
stage. No whole compressed file or whole decoded file is staged in RAM or REU.

The probe keeps the assembly/C boundary and its fixed scratch above `$B000`,
reserves a standalone 3K cc65 software stack at `$C400-$CFFF`, and records a
stack watermark plus a 256-byte red zone below `$C400`. The physical result is
rejected if the stack reaches the job image or either dictionary guard changes.

One run requires stored, fixed-Huffman, and dynamic-Huffman streams, including
more than 32K of decoded output and overlapping back-references. It also checks
an empty member and bounded rejection of truncated, trailing, invalid-block,
invalid stored-length inputs, impossible/reserved distances, reserved length
symbols, invalid dynamic trees/repeats, and both output-size bounds. The host
independently compares every positive output byte and CRC. Functional evidence comes only from physical C64
Ultimate automation; no emulator runner exists. Host execution supplies only
fixture construction and independent byte/CRC oracles.

Every run owns a unique marked root under `/USB1/READYOS_UZIP_TEST`. The root
and all evidence are preserved after the run.
