#!/usr/bin/env python3
"""
Temporary EasyFlash probe scenario runner.

Runs a small matrix of VICE scenarios against the xefprobe CRT while always
mounting the placeholder drive-8 D64. Intended for repeated debugging of the
EasyFlash + REU coexistence question.
"""

from __future__ import annotations

import argparse
import shutil
import re
import select
import socket
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STOP_SUCCESS = 0xC7F8
STOP_FAIL = 0xC7FA


def ensure_dir(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def connect_with_retry(host: str, port: int, timeout_seconds: float) -> socket.socket:
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
    raise RuntimeError("failed to connect to VICE remote monitor")


def read_until_quiet(sock: socket.socket,
                     quiet_seconds: float = 0.20,
                     timeout_seconds: float = 5.0) -> str:
    deadline = time.time() + timeout_seconds
    chunks: list[bytes] = []
    last_data_at = time.time()
    while time.time() < deadline:
        wait = max(0.05, min(0.2, deadline - time.time()))
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


def send_command(sock: socket.socket, command: str, timeout_seconds: float = 3.0) -> str:
    sock.sendall(command.encode("ascii") + b"\n")
    return read_until_quiet(sock, timeout_seconds=timeout_seconds)


def wait_for_any(sock: socket.socket, needles: list[str], timeout_seconds: float) -> str:
    deadline = time.time() + timeout_seconds
    parts: list[str] = []
    while time.time() < deadline:
        chunk = read_until_quiet(sock, quiet_seconds=0.15, timeout_seconds=1.0)
        if chunk:
            parts.append(chunk)
            text = "".join(parts)
            for needle in needles:
                if needle in text:
                    return text
    raise RuntimeError(f"timed out waiting for any of {needles!r}")


def region_bytes(text: str, start: int, length: int) -> bytes:
    dumps: dict[int, bytes] = {}
    for line in text.splitlines():
        match = re.match(r"^>?\s*[.>]?C:([0-9a-fA-F]{4})\s+(.*)$", line)
        if not match:
            continue
        addr = int(match.group(1), 16)
        values = re.findall(r"\b([0-9a-fA-F]{2})\b", match.group(2))
        if values:
            dumps[addr] = bytes(int(value, 16) for value in values)

    out = bytearray()
    cursor = start
    while len(out) < length:
        chunk = dumps.get(cursor)
        if not chunk:
            break
        out.extend(chunk)
        cursor += len(chunk)
    return bytes(out[:length])


def scenario_args(mode: str) -> list[str]:
    args: list[str] = []
    if mode == "iocollision2":
        args.extend(["-iocollision", "2"])
    elif mode == "detach32":
        args.append("+cartreset")
    return args


def run_probe(vice_cmd: str,
              crt_path: Path,
              d64_path: Path,
              mode: str,
              remote_addr: str,
              vice_stdout_path: Path,
              transcript_path: Path) -> int:
    host, port_text = remote_addr.rsplit(":", 1)
    port = int(port_text)
    ensure_dir(vice_stdout_path)
    ensure_dir(transcript_path)
    runtime_crt = transcript_path.with_suffix(".runtime.crt")
    shutil.copyfile(crt_path, runtime_crt)

    vice_args = [
        vice_cmd,
        "-console",
        "-default",
        "+sound",
        "-warp",
        *scenario_args(mode),
        "-reu",
        "-reusize", "16384",
        "-cartcrt", str(runtime_crt),
        "-drive8type", "1541",
        "-devicebackend8", "0",
        "+busdevice8",
        "-8", str(d64_path),
        "-remotemonitor",
        "-remotemonitoraddress", remote_addr,
        "-initbreak", "0xe000",
        "-limitcycles", "30000000",
    ]

    transcript: list[str] = []
    exit_code = 1
    with vice_stdout_path.open("w", encoding="utf-8", errors="replace") as vice_out:
        proc = subprocess.Popen(
            vice_args,
            cwd=str(ROOT),
            stdout=vice_out,
            stderr=subprocess.STDOUT,
            text=True,
        )
        sock: socket.socket | None = None
        try:
            sock = connect_with_retry(host, port, timeout_seconds=15.0)
            with sock:
                sock.setblocking(False)
                transcript.append(read_until_quiet(sock, timeout_seconds=4.0))
                transcript.append(send_command(sock, f"break ${STOP_SUCCESS:04x}"))
                transcript.append(send_command(sock, f"break ${STOP_FAIL:04x}"))
                if mode == "detach32":
                    transcript.append(send_command(sock, "break $1000"))
                sock.sendall(b"x\n")
                if mode == "detach32":
                    transcript.append(
                        wait_for_any(
                            sock,
                            ["C:$1000", "C:1000", "C:$1000  ", ".C:1000"],
                            timeout_seconds=12.0,
                        )
                    )
                    transcript.append(send_command(sock, "r"))
                    transcript.append(send_command(sock, "m $c7a0 $c7ef"))
                    transcript.append(send_command(sock, "detach 20", timeout_seconds=4.0))
                    sock.sendall(b"x\n")
                transcript.append(
                    wait_for_any(
                        sock,
                        [
                            f"C:${STOP_SUCCESS:04x}", f"C:{STOP_SUCCESS:04x}",
                            f"C:${STOP_SUCCESS:04X}", f"C:{STOP_SUCCESS:04X}",
                            f"C:${STOP_FAIL:04x}", f"C:{STOP_FAIL:04x}",
                            f"C:${STOP_FAIL:04X}", f"C:{STOP_FAIL:04X}",
                        ],
                        timeout_seconds=20.0,
                    )
                )
                transcript.append(send_command(sock, "r"))
                transcript.append(send_command(sock, "m $0400 $04ff"))
                transcript.append(send_command(sock, "m $c7a0 $c7ef"))
                transcript.append(send_command(sock, "m $c7e8 $c7ef"))
                transcript.append(send_command(sock, "q"))
                exit_code = 0
        except Exception as exc:
            transcript.append(f"\n[scenario-exception] {exc!r}\n")
            if sock is not None:
                try:
                    transcript.append(send_command(sock, "stop"))
                except Exception:
                    pass
                try:
                    transcript.append(send_command(sock, "r"))
                    transcript.append(send_command(sock, "m $0400 $04ff"))
                    transcript.append(send_command(sock, "m $c7a0 $c7ef"))
                    transcript.append(send_command(sock, "m $c7e8 $c7ef"))
                    transcript.append(send_command(sock, "q"))
                except Exception:
                    pass
        finally:
            transcript_path.write_text("".join(transcript), encoding="utf-8")
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.terminate()
                try:
                    proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=5)
    return exit_code


def print_summary(transcript_path: Path) -> None:
    text = transcript_path.read_text(encoding="utf-8", errors="replace")
    screen = region_bytes(text, 0x0400, 0x100)
    ring = region_bytes(text, 0xC7A0, 0x50)
    results = region_bytes(text, 0xC7E8, 8)

    final_trap = "unknown"
    stop_hits = re.findall(r"#\d+\s+\(Stop on\s+exec\s+([0-9a-fA-F]{4})\)", text)
    if stop_hits:
        last = int(stop_hits[-1], 16)
        if last == STOP_SUCCESS:
            final_trap = "success"
        elif last == STOP_FAIL:
            final_trap = "fail"

    print(f"final_trap: {final_trap}")
    if ring:
        printable = "".join(chr(b) if 32 <= b <= 126 else "." for b in ring if b)
        print(f"debug_ring: {printable}")
    if results:
        print(f"result_bytes: {results.hex(' ')}")
    if screen:
        print(f"screen_0400: {screen[:40].hex(' ')}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--vice", default="x64sc")
    ap.add_argument("--crt", required=True)
    ap.add_argument("--d64", required=True)
    ap.add_argument("--mode", choices=["normal", "iocollision2", "detach32"], default="normal")
    ap.add_argument("--remote-addr", default="127.0.0.1:6513")
    ap.add_argument("--transcript", required=True)
    ap.add_argument("--vice-stdout", required=True)
    args = ap.parse_args()

    rc = run_probe(
        vice_cmd=args.vice,
        crt_path=Path(args.crt),
        d64_path=Path(args.d64),
        mode=args.mode,
        remote_addr=args.remote_addr,
        vice_stdout_path=Path(args.vice_stdout),
        transcript_path=Path(args.transcript),
    )
    print_summary(Path(args.transcript))
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
