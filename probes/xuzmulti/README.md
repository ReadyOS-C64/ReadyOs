# xuzmulti physical ReadyOS diagnostic

`xuzmulti` proves that one Ultimate DOS output stream can remain open across
seven bounded ReadyOS jobs while its writer state and complete central catalog
live in separate allocator-owned REU banks. The deterministic archive contains
nested directory records, four Deflate files, and one Store file:

1. `ROOT/`
2. `ROOT/EMPTY.BIN`
3. `ROOT/REPEAT.BIN`
4. `ROOT/SUB/`
5. `ROOT/SUB/RANDOM.BIN`
6. `ROOT/REPEAT.STO`
7. `CROSS.BIN`

The Terminal-owned runner creates a unique owner-marked folder below
`USB1/READYOS_UZIP_TEST/XUZMULTI-*`, boots ReadyOS itself on a physical C64
Ultimate, and downloads `MULTI.ZIP`. A strict byte oracle verifies local
headers, signed descriptors, methods, offsets, the central directory, EOCD,
CRC/size metadata, exact Deflate decoding, Store bytes, entry order, and Python
`zipfile` extraction. No emulator is launched or used.

Launch the first physical proof with:

```sh
XUZMULTI_SPEED_MHZ=16 XUZMULTI_QUIET_S=900 \
  /bin/bash build_support/start_xuzmulti_c64u_terminal.sh
```

The printed status file must contain zero. The owned test root is preserved as
evidence, and the runner restores the original Ultimate CPU configuration.
