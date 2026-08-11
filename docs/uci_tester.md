# ReadyOS UCI Tester User Guide

The ReadyOS **UCI Tester** is an interactive laboratory for the Ultimate
Command Interface available on Ultimate-II+, Ultimate 64, Ultimate 64 Elite,
and Commodore 64 Ultimate systems. It can discover the mapped UCI registers,
send structured commands to the documented targets, show decoded or raw
responses, and retain returned socket and HTTP handles for the next command.

The app is intended for development and diagnosis. Several commands can alter
files, mounts, network state, drive power, the REU, or the machine itself.
Read the command flag at the top of the screen before pressing F5.

## Before You Start

1. Enable **Command Interface** in the Ultimate configuration.
2. Boot ReadyOS on real Ultimate hardware and launch **UCI Tester** from the
   launcher.
3. Start with **Transport → Detect**. A normal result shows a base such as
   `$DF1C`. If the header says `base none`, check the Ultimate configuration.
4. Select **Transport → Protocol norms** for a concise in-app copy of the
   transport rules described below.

The HTTP target requires Ultimate firmware 3.15 or later. Target availability
and command details can vary with firmware.

## Screen and Keys

The screen contains a target list, a command list, an editable form, and a
five-line output viewport. The output keeps 30 lines of history.

| Key | Action |
|---|---|
| F1 | Focus the target list |
| F3 | Focus the command form |
| F5 or Return | Execute the selected command |
| F6 | Focus output; Up/Down scroll its history |
| F7 | Toggle decoded and raw response display |
| F8 | Open the selectable example list |
| Up/Down | Move in the focused list, form, or output |
| Left/Right | Move between target/command panes or change numeric fields |
| Delete | Remove the final character from a text field |
| Home | Reset a numeric field to its minimum |
| F2/F4 | Switch ReadyOS apps |
| Ctrl+B, Left Arrow, or Run/Stop | Return to the launcher |

Numeric fields are displayed in hexadecimal. Text and raw-byte fields are
edited as text. Raw bytes may be separated by spaces or commas and may use a
`$` prefix, for example `04 01`, `$04,$01`, or `04,01`.

## Prefilled Examples

Press F8, use Up/Down, and press Return to load an example. Loading does not
run anything: it selects the target and command, prefills the form, and leaves
you at a review step. Press F5 only after checking the values.

The built-in examples are:

- **Detect UCI interface** — locate the register block safely.
- **Explain UCI protocol** — load the in-app protocol guide.
- **Identify Ultimate DOS** — query target `$01` safely.
- **Show current DOS path** — inspect the independent DOS 1 working path.
- **Inspect ReadyOS image** — prefill File Stat for `/usb1/readyos.d81`.
- **Show network address** — query IP, netmask, and gateway for interface zero.
- **Open TCP example.com** — prefill `example.com` on port 80.
- **Prepare HTTP GET** — create a reusable GET header for `example.com/`.
- **Query last HTTP body** — query the root of the most recently returned
  HTTP body object.
- **Raw Control identify** — send the raw bytes `04 01`.

Examples are safe starting points, not macros: each one prefills a single
command. Multi-command workflows remain explicit so you can inspect every
handle and status response.

## The UCI Transport Rules

UCI is an asynchronous state machine. It must never be paced by an assumed
number of C64 instructions or by fixed delays.

1. Start only when the interface is idle and the pending `CMD_BUSY`,
   `DATA_ACC`, and `ABORT_P` bits are clear.
2. Write the complete command, then issue `PUSH_CMD` once.
3. Do not interpret an immediate `IDLE` sample after the push as completion.
   It means the Ultimate may not have observed the asynchronous push yet.
4. Wait for `DATA_LAST` or `DATA_MORE`.
5. While `DATA_AV` or `STAT_AV` is set, read the corresponding queue.
6. Issue `DATA_ACC` only after both availability bits are clear.
7. `DATA_ACC` is asynchronous. After `DATA_MORE`, do not mistake an immediate
   unchanged `DATA_MORE` sample for the next block. Require the documented
   transition back through `COMMAND_BUSY` before consuming the next block.
   After `DATA_LAST`, wait for a fully quiescent idle state.
8. Treat `ERROR` as a transport/state error. Target success or failure is
   reported separately in the status-data queue.
9. Treat `ABORT` as asynchronous and wait for fully quiet idle before sending
   again. `ABORT_P` means a request is already pending, so keep servicing the
   interface instead of re-issuing `ABORT` on every poll.
10. Use counters only as failure bounds. Do not reset them indefinitely while
    servicing a stuck flag, bound drain loops above the queue capacities, and
    ensure state waits remain long enough at the fastest supported CPU speed.

The hardware queues hold 896 command bytes, 896 response-data bytes per
block, and 256 status bytes. UCI Tester captures those full queue sizes. It
continues draining larger multi-block responses and marks captured output as
truncated rather than leaving the interface wedged.

## Understanding Results

The output begins with the selected command, its target-status response, and
the captured data length. A binary status prefix is shown explicitly, for
example `stat $00: OK`. HTTP firmware commonly returns printable three-digit
status text instead.

Decoded mode recognizes text, handles, 16- and 32-bit values, MAC/IP data,
directory entries, file information, framed socket reads, HTTP response-handle
pairs, HTTP values, and SoftwareIEC filename results. Press F7 whenever you
need the exact bytes.

The header's `st $xx` value is the live transport register, not the target
status message. If a command fails:

- **timeout flag set** means the required state transition did not arrive
  within the failure bound;
- **UCI transport error flag set** means the state machine rejected the
  transfer, commonly because a command was pushed while it was not idle;
- a normal target error such as `FILE NOT FOUND` appears in the status-data
  output and does not itself mean the transport is broken.

## Real-World Workflows

### Confirm UCI and Discover Targets

1. Run **Transport → Detect**.
2. Run **Transport → Read ID**; the low seven bits should identify UCI.
3. Run **Identify** on DOS 1, Network, Control, SoftwareIEC, and HTTP as
   appropriate for the installed firmware.

This is the safest first test after a firmware or hardware configuration
change.

### List a Directory with Ultimate DOS

DOS 1 and DOS 2 are independent sessions, each with its own current directory
and open file.

1. Select **DOS 1 → Change dir**, enter `/usb1`, and run it.
2. Run **Get path** to confirm the selected directory.
3. Run **Open dir**.
4. Run **Read dir** repeatedly. Each call returns one attribute byte followed
   by the filename. Stop when target status reports the end condition.

Use decoded mode for readable names and F7 raw mode when inspecting FAT
attribute bits.

### Inspect and Read a File

1. Press F8 and load **Inspect ReadyOS image**, adjust the path if necessary,
   then run File Stat.
2. Select **Open file**, set attribute `$01` for read-only access, enter a
   filename, and run it.
3. Select **File info** to inspect size and timestamp.
4. Select **Read data**, set a modest length such as `$0040`, and run it.
5. Select **Close file** when finished.

For writes, the Ultimate DOS attribute byte is significant: `$02` opens an
existing file for writing, `$04` creates a new file, and `$08` allows replacing
an existing file. The common create/overwrite combination is `$0E`. Test write
operations on disposable files.

### Inspect Network Configuration

1. Press F8 and load **Show network address**.
2. Leave interface `$00` selected and run it.
3. Decoded output shows the IP address, netmask, and gateway.

`Set IP` is intentionally marked as a write. Its raw field contains interface,
address, mask, and gateway bytes; do not run it casually on a remotely managed
Ultimate.

### Open and Close a TCP Socket

1. Press F8 and load **Open TCP example.com**.
2. Review port `$0050` (80) and host `example.com`, then run it.
3. The returned one-byte socket handle is remembered automatically.
4. Selecting Read Socket, Write Socket, or Close Socket prefills that handle.
5. Always run **Close socket** when the experiment is complete.

The Network target's Read Socket response starts with a two-byte little-endian
payload length. Decoded mode removes that framing and displays the payload.

### Perform an HTTP GET

HTTP resources survive a C64 reset, so begin by running **HTTP → Free all**.

1. Press F8 and load **Prepare HTTP GET**.
2. Run Header Create. The request-header handle is remembered.
3. Select **Exchange obj**. The remembered header is prefilled and body `$FF`
   means “no request body.” Run it.
4. A successful exchange returns two handles: response header and response
   body. Both are remembered.
5. Select **Header list**, use index `$00`, and run it to view the complete
   response header.
6. Press F8 and load **Query last HTTP body**, then run it. Its empty path
   queries the root value of the remembered body object.
7. Free individual handles when done, or run **Free all**.

Use **Exchange raw** when the response body should be returned directly rather
than parsed into an HTTP body object. Long network requests can legitimately
take seconds; the tester waits on UCI state transitions rather than inserting
CPU-speed-dependent delays.

### Compare Structured and Raw Commands

Run **Control → Identify**, then press F8 and load **Raw Control identify**.
Both send target `$04`, command `$01`. The raw form is useful for testing a new
firmware command before adding a structured catalog entry. Prefer structured
commands for routine use because they document byte order and field widths.

## Commands That Need Extra Care

- Delete, rename, copy, create-directory, mount, unmount, and write commands
  alter storage or drive state.
- REU load/save commands can replace REU contents or files.
- Control Freeze and Reboot interrupt ReadyOS; Reboot has no UCI response
  because it resets the interface itself.
- Drive enable/disable changes emulated hardware state.
- SoftwareIEC partition changes alter virtual filesystem mappings.
- HTTP and socket resources should be freed or closed after use.
- The destructive EasyFlash erase command is deliberately not exposed in the
  structured catalog. It can still be investigated through Raw Bytes by an
  expert with disposable media and a recovery plan.

## Source and Compatibility Notes

The catalog and transport behavior are based on the official Ultimate
documentation:

- [UCI core architecture and register protocol](https://github.com/GideonZ/1541u-documentation/blob/master/uci/core_uci_architecture.rst)
- [Ultimate DOS target](https://github.com/GideonZ/1541u-documentation/blob/master/uci/ultimate_dos_target.rst)
- [Network target](https://github.com/GideonZ/1541u-documentation/blob/master/uci/network_target.rst)
- [Control target](https://github.com/GideonZ/1541u-documentation/blob/master/uci/control_target.rst)
- [SoftwareIEC target](https://github.com/GideonZ/1541u-documentation/blob/master/uci/software_iec_target.rst)
- [HTTP target](https://github.com/GideonZ/1541u-documentation/blob/master/uci/http_target.rst)

For Ultimate-specific regressions, validate on physical hardware at both normal
and accelerated CPU speeds. VICE does not implement the Ultimate hardware UCI
service and cannot prove this transport correct.

ReadyOS developers can run the focused 16 MHz physical-hardware smoke suite
with:

```sh
/bin/bash build_support/run_ucitest_c64u_smoke.sh
```

Repeat the same plan at normal CPU speed with:

```sh
UCITEST_C64U_SPEED_MHZ=1 /bin/bash build_support/run_ucitest_c64u_smoke.sh
```

Run it from the Terminal-owned/background shell required by `AGENTS.md`. The
suite boots the regular D81 build, exercises detection, protocol guidance, DOS,
network, and example prefilling, then clears REU state and reboots the Ultimate.

Before any ReadyOS profile build, the source-contract guard checks every UCI
transport and its app-level call sites for known asynchronous-state-machine
regressions. It can also be run directly:

```sh
python3 build_support/verify_uci_protocol_contract.py
```

For a release-quality physical-hardware check, run the complete matrix from a
Terminal-owned/background shell:

```sh
UCI_C64U_MATRIX_OUT_DIR="$PWD/logs/uci-matrix" \
  /bin/bash build_support/run_uci_c64u_matrix.sh
```

The matrix exercises the standalone DMA probe, the production launcher timing
transport, System Info, UCI Tester, and ReadyIRC at both 1 and 16 MHz. Both
disk-image probes upload a fresh uniquely named D81 for every run and embed
that exact basename in the probe; reusing or replacing a recently mounted
generic filename can corrupt a later boot or make Ultimate DOS return a
misleading `84,NO FILE` for files that are present in the image.
