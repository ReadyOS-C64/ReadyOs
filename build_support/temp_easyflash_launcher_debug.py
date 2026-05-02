#!/usr/bin/env python3
"""
Temporary EasyFlash launcher debug helper.

Supports two diagnostic flows:
- emit a simple VICE monitor script and parse a resulting monitor log
- launch an EasyFlash VICE remote-monitor session, break in the CRT launcher,
  and dump the launcher trace ring plus key screen/VIC state
"""

from __future__ import annotations

import argparse
import os
import re
import select
import socket
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VICE_STDIO_LOG = ROOT / "logs" / "temp_easyflash_launcher_debug_vice.out"
DEFAULT_REMOTE_LOG = ROOT / "logs" / "temp_easyflash_launcher_debug_remote.log"

RAM_RING_START = 0xC7A0
RAM_RING_END = 0xC7DF
RAM_RING_DATA_LEN = 0x3F


def write_monitor(path: Path, break_addr: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "\n".join(
            [
                f"break ${break_addr:04x}",
                "x",
                "m $c7a0 $c7df",
                "m $c7e0 $c7ff",
                "m $c820 $c83f",
                "q",
                "",
            ]
        ),
        encoding="ascii",
    )


def write_dump_monitor(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "\n".join(
            [
                "r",
                "m $0400 $04ff",
                "m $d800 $d8ff",
                "m $d011 $d018",
                "m $dd00 $dd00",
                "m $c7a0 $c7df",
                "m $c820 $c83f",
                "q",
                "",
            ]
        ),
        encoding="ascii",
    )


def parse_monitor_log(path: Path) -> dict[int, bytes]:
    dumps: dict[int, bytes] = {}
    text = path.read_text(encoding="utf-8", errors="replace")
    for line in text.splitlines():
        match = re.match(r"^>C:([0-9a-fA-F]{4})\s+(.*)$", line)
        if not match:
            continue
        addr = int(match.group(1), 16)
        values = re.findall(r"\b([0-9a-fA-F]{2})\b", match.group(2))
        if values:
            dumps[addr] = bytes(int(value, 16) for value in values)
    return dumps


def parse_monitor_text(text: str) -> dict[int, bytes]:
    dumps: dict[int, bytes] = {}
    for line in text.splitlines():
        match = re.match(r"^>?\s*[.>]?C:([0-9a-fA-F]{4})\s+(.*)$", line)
        if not match:
            continue
        addr = int(match.group(1), 16)
        values = re.findall(r"\b([0-9a-fA-F]{2})\b", match.group(2))
        if values:
            dumps[addr] = bytes(int(value, 16) for value in values)
    return dumps


def region_bytes(dumps: dict[int, bytes], start: int, length: int) -> bytes:
    out = bytearray()
    cursor = start
    while len(out) < length:
        chunk = dumps.get(cursor)
        if not chunk:
            break
        out.extend(chunk)
        cursor += len(chunk)
    return bytes(out[:length])


def render_ascii(data: bytes) -> str:
    chars: list[str] = []
    for value in data:
        if 32 <= value <= 126:
            chars.append(chr(value))
        elif value == 0:
            chars.append(".")
        else:
            chars.append(f"<{value:02x}>")
    return "".join(chars)


def render_ring_trace(data: bytes, head: int) -> str:
    if not data:
        return ""
    head %= len(data)
    ordered = data[head:] + data[:head]
    chars: list[str] = []
    for value in ordered:
        if value == 0:
            continue
        if 32 <= value <= 126:
            chars.append(chr(value))
        else:
            chars.append(f"<{value:02x}>")
    return "".join(chars)


def dump_ring(log_path: Path) -> int:
    text = log_path.read_text(encoding="utf-8", errors="replace")
    dumps = parse_monitor_text(text)
    ring = region_bytes(dumps, RAM_RING_START, RAM_RING_END - RAM_RING_START + 1)
    if len(ring) != (RAM_RING_END - RAM_RING_START + 1):
        print(f"error: missing RAM ring dump in {log_path}", file=sys.stderr)
        return 1

    data = ring[:RAM_RING_DATA_LEN]
    head = ring[RAM_RING_DATA_LEN]
    print(f"head: {head}")
    print(f"data(hex): {data.hex(' ')}")
    print(f"data(ascii): {render_ascii(data)}")
    print(f"trace(ordered): {render_ring_trace(data, head)}")
    return 0


def read_until_quiet(sock: socket.socket, quiet_seconds: float = 0.35, timeout_seconds: float = 10.0) -> str:
    deadline = time.time() + timeout_seconds
    last_data_at = time.time()
    chunks: list[bytes] = []
    while time.time() < deadline:
        wait = max(0.05, min(0.25, deadline - time.time()))
        ready, _, _ = select.select([sock], [], [], wait)
        if ready:
            chunk = sock.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
            last_data_at = time.time()
            continue
        if chunks and (time.time() - last_data_at) >= quiet_seconds:
            break
    return b"".join(chunks).decode("utf-8", errors="replace")


def send_command(sock: socket.socket, command: str, *, quiet_seconds: float = 0.35, timeout_seconds: float = 10.0) -> str:
    sock.sendall(command.encode("ascii") + b"\n")
    return read_until_quiet(sock, quiet_seconds=quiet_seconds, timeout_seconds=timeout_seconds)


def wait_for_text(sock: socket.socket, needle: str, *, timeout_seconds: float = 20.0) -> str:
    deadline = time.time() + timeout_seconds
    chunks: list[str] = []
    while time.time() < deadline:
        chunk = read_until_quiet(sock, quiet_seconds=0.2, timeout_seconds=1.0)
        if chunk:
            chunks.append(chunk)
            if needle in "".join(chunks):
                return "".join(chunks)
    raise RuntimeError(f"timed out waiting for monitor text: {needle!r}")


def wait_for_any_text(sock: socket.socket, needles: list[str], *, timeout_seconds: float = 20.0) -> str:
    deadline = time.time() + timeout_seconds
    chunks: list[str] = []
    while time.time() < deadline:
        chunk = read_until_quiet(sock, quiet_seconds=0.2, timeout_seconds=1.0)
        if chunk:
            chunks.append(chunk)
            combined = "".join(chunks)
            for needle in needles:
                if needle in combined:
                    return combined
    raise RuntimeError(f"timed out waiting for monitor text: {needles!r}")


def dump_region(sock: socket.socket, start: int, end: int) -> str:
    return send_command(sock, f"m ${start:04x} ${end:04x}")


def dump_registers(sock: socket.socket) -> str:
    return send_command(sock, "r")


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def connect_with_retry(host: str, port: int, *, timeout_seconds: float = 15.0) -> socket.socket:
    deadline = time.time() + timeout_seconds
    last_error: OSError | None = None
    while time.time() < deadline:
        try:
            return socket.create_connection((host, port), timeout=2.0)
        except OSError as exc:
            last_error = exc
            time.sleep(0.2)
    if last_error is not None:
        raise last_error
    raise RuntimeError("remote monitor connection failed")


def run_remote_session(vice_cmd: str,
                       crt_path: Path,
                       d64_path: Path,
                       break_addr: int | None,
                       init_break_addr: int | None,
                       remote_addr: str,
                       stdio_log: Path,
                       remote_log: Path,
                       limit_cycles: int,
                       startup_wait: float) -> int:
    host, port_text = remote_addr.rsplit(":", 1)
    port = int(port_text)
    ensure_parent(stdio_log)
    ensure_parent(remote_log)

    vice_args = [
        vice_cmd,
        "-console",
        "-default",
        "+sound",
        "-warp",
        "-reu",
        "-reusize", "16384",
        "-cartcrt", str(crt_path),
        "-drive8type", "1541",
        "-devicebackend8", "0",
        "+busdevice8",
        "-8", str(d64_path),
        "-remotemonitor",
        "-remotemonitoraddress", remote_addr,
        "-monlog",
        "-monlogname", str(remote_log),
        "-limitcycles", str(limit_cycles),
    ]
    if init_break_addr is not None:
        vice_args.extend(["-initbreak", f"0x{init_break_addr:04x}"])
    with stdio_log.open("w", encoding="utf-8", errors="replace") as vice_out:
        proc = subprocess.Popen(
            vice_args,
            cwd=str(ROOT),
            stdout=vice_out,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            sock = connect_with_retry(host, port, timeout_seconds=15.0)
        except OSError:
            proc.terminate()
            proc.wait(timeout=5)
            raise

        transcript_parts: list[str] = []
        failure_text = ""
        try:
            with sock:
                sock.setblocking(False)
                initial_text = read_until_quiet(sock, quiet_seconds=0.5, timeout_seconds=10.0)
                transcript_parts.append(initial_text)
                if init_break_addr is not None:
                    try:
                        transcript_parts.append(
                            wait_for_any_text(
                                sock,
                                [
                                    f"BREAK: 1  C:${init_break_addr:04x}",
                                    f"BREAK: 2  C:${init_break_addr:04x}",
                                    f".C:{init_break_addr:04x}",
                                ],
                                timeout_seconds=max(5.0, startup_wait + 5.0),
                            )
                        )
                    except RuntimeError:
                        time.sleep(startup_wait)
                        transcript_parts.append(
                            f"\n[debug-helper-note] initbreak text not observed for ${init_break_addr:04x}; attempting live dump anyway\n"
                        )
                elif break_addr is None:
                    transcript_parts.append(send_command(sock, "x",
                                                         quiet_seconds=0.2,
                                                         timeout_seconds=5.0))
                    time.sleep(startup_wait)
                    transcript_parts.append(read_until_quiet(sock, quiet_seconds=0.2, timeout_seconds=2.0))
                else:
                    transcript_parts.append(send_command(sock, f"break ${break_addr:04x}",
                                                         quiet_seconds=0.2,
                                                         timeout_seconds=5.0))
                    sock.sendall(b"x\n")
                    transcript_parts.append(
                        wait_for_any_text(
                            sock,
                            [
                                f"BREAK: 1  C:${break_addr:04x}",
                                f"BREAK: 2  C:${break_addr:04x}",
                                f".C:{break_addr:04x}",
                            ],
                            timeout_seconds=max(5.0, startup_wait + 5.0),
                        )
                    )
                transcript_parts.append(dump_registers(sock))
                transcript_parts.append(dump_region(sock, 0x0100, 0x01FF))
                transcript_parts.append(dump_region(sock, 0x0800, 0x083F))
                transcript_parts.append(dump_region(sock, 0x1000, 0x103F))
                transcript_parts.append(dump_region(sock, 0x0400, 0x04FF))
                transcript_parts.append(dump_region(sock, 0xD800, 0xD8FF))
                transcript_parts.append(dump_region(sock, 0xD011, 0xD018))
                transcript_parts.append(dump_region(sock, 0xDD00, 0xDD00))
                transcript_parts.append(dump_region(sock, RAM_RING_START, RAM_RING_END))
                transcript_parts.append(dump_region(sock, 0xC820, 0xC83F))
                transcript_parts.append(send_command(sock, "q", quiet_seconds=0.2, timeout_seconds=5.0))
        except Exception as exc:
            failure_text = f"\n[debug-helper-exception] {exc!r}\n"
        finally:
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=5)

    transcript = "".join(transcript_parts) + failure_text
    remote_log.write_text(transcript, encoding="utf-8")
    if failure_text:
        return 1
    return 0


def summarize_session(log_path: Path) -> int:
    text = log_path.read_text(encoding="utf-8", errors="replace")
    dumps = parse_monitor_text(text)
    ring = region_bytes(dumps, RAM_RING_START, RAM_RING_END - RAM_RING_START + 1)
    if len(ring) == (RAM_RING_END - RAM_RING_START + 1):
        data = ring[:RAM_RING_DATA_LEN]
        head = ring[RAM_RING_DATA_LEN]
        print(f"launcher ring head: {head}")
        print(f"launcher ring ordered trace: {render_ring_trace(data, head)}")
    else:
        print("launcher ring: missing")

    screen = region_bytes(dumps, 0x0400, 64)
    if screen:
        print(f"screen $0400: {screen.hex(' ')}")
        print(f"screen ascii: {render_ascii(screen)}")

    launcher = region_bytes(dumps, 0x1000, 64)
    if launcher:
        print(f"launcher $1000: {launcher.hex(' ')}")

    handoff = region_bytes(dumps, 0x0800, 64)
    if handoff:
        print(f"handoff $0800: {handoff.hex(' ')}")

    stack = region_bytes(dumps, 0x0100, 64)
    if stack:
        print(f"stack $0100: {stack.hex(' ')}")

    vic = region_bytes(dumps, 0xD011, 8)
    if vic:
        print(f"vic $d011-$d018: {vic.hex(' ')}")
    cia2 = region_bytes(dumps, 0xDD00, 1)
    if cia2:
        print(f"cia2 $dd00: {cia2[0]:02x}")
    return 0


def trace_disk_calls(vice_cmd: str,
                     crt_path: Path,
                     d64_path: Path,
                     break_addr: int,
                     remote_addr: str,
                     stdio_log: Path,
                     remote_log: Path,
                     limit_cycles: int) -> int:
    host, port_text = remote_addr.rsplit(":", 1)
    port = int(port_text)
    ensure_parent(stdio_log)
    ensure_parent(remote_log)

    vice_args = [
        vice_cmd,
        "-console",
        "-default",
        "+sound",
        "-warp",
        "-reu",
        "-reusize", "16384",
        "-cartcrt", str(crt_path),
        "-drive8type", "1541",
        "-devicebackend8", "0",
        "+busdevice8",
        "-8", str(d64_path),
        "-remotemonitor",
        "-remotemonitoraddress", remote_addr,
        "-monlog",
        "-monlogname", str(remote_log),
        "-limitcycles", str(limit_cycles),
        "-initbreak", f"0x{break_addr:04x}",
    ]

    with stdio_log.open("w", encoding="utf-8", errors="replace") as vice_out:
        proc = subprocess.Popen(
            vice_args,
            cwd=str(ROOT),
            stdout=vice_out,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            sock = connect_with_retry(host, port, timeout_seconds=15.0)
        except OSError:
            proc.terminate()
            proc.wait(timeout=5)
            raise

        transcript_parts: list[str] = []
        try:
            with sock:
                sock.setblocking(False)
                transcript_parts.append(read_until_quiet(sock, quiet_seconds=0.5, timeout_seconds=10.0))
                transcript_parts.append(dump_registers(sock))
                transcript_parts.append(send_command(sock, "break $ffba"))
                transcript_parts.append(send_command(sock, "break $ffbd"))
                transcript_parts.append(send_command(sock, "break $ffc0"))
                transcript_parts.append(send_command(sock, "break $ffcf"))
                transcript_parts.append(send_command(sock, "break $ffd5"))
                transcript_parts.append(send_command(sock, "break $c80d"))
                transcript_parts.append(send_command(sock, "x", quiet_seconds=0.2, timeout_seconds=8.0))
                transcript_parts.append(read_until_quiet(sock, quiet_seconds=0.4, timeout_seconds=8.0))
                transcript_parts.append(dump_registers(sock))
                transcript_parts.append(dump_region(sock, 0x0400, 0x047F))
                transcript_parts.append(dump_region(sock, RAM_RING_START, RAM_RING_END))
                transcript_parts.append(send_command(sock, "q", quiet_seconds=0.2, timeout_seconds=5.0))
        finally:
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=5)

    transcript = "".join(transcript_parts)
    remote_log.write_text(transcript, encoding="utf-8")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    write_ap = sub.add_parser("write-monitor")
    write_ap.add_argument("--output", required=True)
    write_ap.add_argument("--break-addr", required=True, type=lambda s: int(s, 0))

    dump_monitor_ap = sub.add_parser("write-dump-monitor")
    dump_monitor_ap.add_argument("--output", required=True)

    dump_ap = sub.add_parser("dump-ring")
    dump_ap.add_argument("--log", required=True)

    remote_ap = sub.add_parser("remote-session")
    remote_ap.add_argument("--vice", default=os.environ.get("VICE", "x64sc"))
    remote_ap.add_argument("--crt", required=True)
    remote_ap.add_argument("--d64", required=True)
    remote_ap.add_argument("--break-addr", type=lambda s: int(s, 0))
    remote_ap.add_argument("--init-break-addr", type=lambda s: int(s, 0))
    remote_ap.add_argument("--remote-addr", default="127.0.0.1:6510")
    remote_ap.add_argument("--stdio-log", default=str(DEFAULT_VICE_STDIO_LOG))
    remote_ap.add_argument("--remote-log", default=str(DEFAULT_REMOTE_LOG))
    remote_ap.add_argument("--limitcycles", type=int, default=30000000)
    remote_ap.add_argument("--startup-wait", type=float, default=2.0)

    summarize_ap = sub.add_parser("summarize-session")
    summarize_ap.add_argument("--log", required=True)

    trace_ap = sub.add_parser("trace-disk-calls")
    trace_ap.add_argument("--vice", default=os.environ.get("VICE", "x64sc"))
    trace_ap.add_argument("--crt", required=True)
    trace_ap.add_argument("--d64", required=True)
    trace_ap.add_argument("--break-addr", required=True, type=lambda s: int(s, 0))
    trace_ap.add_argument("--remote-addr", default="127.0.0.1:6510")
    trace_ap.add_argument("--stdio-log", default=str(DEFAULT_VICE_STDIO_LOG))
    trace_ap.add_argument("--remote-log", default=str(DEFAULT_REMOTE_LOG))
    trace_ap.add_argument("--limitcycles", type=int, default=30000000)

    args = ap.parse_args()
    if args.cmd == "write-monitor":
        write_monitor(Path(args.output), args.break_addr)
        return 0
    if args.cmd == "write-dump-monitor":
        write_dump_monitor(Path(args.output))
        return 0
    if args.cmd == "dump-ring":
        return dump_ring(Path(args.log))
    if args.cmd == "remote-session":
        return run_remote_session(
            vice_cmd=args.vice,
            crt_path=Path(args.crt),
            d64_path=Path(args.d64),
            break_addr=args.break_addr,
            init_break_addr=args.init_break_addr,
            remote_addr=args.remote_addr,
            stdio_log=Path(args.stdio_log),
            remote_log=Path(args.remote_log),
            limit_cycles=args.limitcycles,
            startup_wait=args.startup_wait,
        )
    if args.cmd == "summarize-session":
        return summarize_session(Path(args.log))
    if args.cmd == "trace-disk-calls":
        return trace_disk_calls(
            vice_cmd=args.vice,
            crt_path=Path(args.crt),
            d64_path=Path(args.d64),
            break_addr=args.break_addr,
            remote_addr=args.remote_addr,
            stdio_log=Path(args.stdio_log),
            remote_log=Path(args.remote_log),
            limit_cycles=args.limitcycles,
        )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
