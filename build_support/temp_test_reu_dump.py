#!/usr/bin/env python3
"""
Temporary standalone REU test dumper.

Runs test_reu.prg under VICE with REU enabled, always mounts an optional drive-8
D64, and uses the remote monitor to stop at a deterministic address before
dumping screen and REU registers.
"""

from __future__ import annotations

import argparse
import select
import socket
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


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
                     quiet_seconds: float = 0.2,
                     timeout_seconds: float = 4.0) -> str:
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


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--vice", default="x64sc")
    ap.add_argument("--prg", required=True)
    ap.add_argument("--d64")
    ap.add_argument("--break-addr", type=lambda s: int(s, 0), default=0x1055)
    ap.add_argument("--initbreak", default="0xfce2")
    ap.add_argument("--remote-addr", default="127.0.0.1:6520")
    ap.add_argument("--transcript", required=True)
    ap.add_argument("--vice-stdout", required=True)
    args = ap.parse_args()

    host, port_text = args.remote_addr.rsplit(":", 1)
    port = int(port_text)
    transcript_path = Path(args.transcript)
    vice_stdout_path = Path(args.vice_stdout)
    transcript_path.parent.mkdir(parents=True, exist_ok=True)
    vice_stdout_path.parent.mkdir(parents=True, exist_ok=True)

    vice_args = [
        args.vice,
        "-console",
        "-default",
        "+sound",
        "-warp",
        "-reu",
        "-reusize", "16384",
        "-autostartprgmode", "1",
        "-autostart", args.prg,
        "-remotemonitor",
        "-remotemonitoraddress", args.remote_addr,
        "-initbreak", args.initbreak,
        "-limitcycles", "120000000",
    ]
    if args.d64:
        vice_args.extend(
            [
                "-drive8type", "1541",
                "-devicebackend8", "0",
                "+busdevice8",
                "-8", args.d64,
            ]
        )

    transcript: list[str] = []
    with vice_stdout_path.open("w", encoding="utf-8", errors="replace") as vice_out:
        proc = subprocess.Popen(
            vice_args,
            cwd=str(ROOT),
            stdout=vice_out,
            stderr=subprocess.STDOUT,
            text=True,
        )
        sock: socket.socket | None = None
        exit_code = 1
        try:
            sock = connect_with_retry(host, port, timeout_seconds=15.0)
            with sock:
                sock.setblocking(False)
                transcript.append(read_until_quiet(sock, timeout_seconds=4.0))
                transcript.append(send_command(sock, f"break ${args.break_addr:04x}"))
                sock.sendall(b"x\n")
                transcript.append(
                    wait_for_any(
                        sock,
                        [
                            f"C:${args.break_addr:04x}", f"C:{args.break_addr:04x}",
                            f"C:${args.break_addr:04X}", f"C:{args.break_addr:04X}",
                        ],
                        timeout_seconds=15.0,
                    )
                )
                transcript.append(send_command(sock, "r"))
                transcript.append(send_command(sock, "m $0400 $05ff"))
                transcript.append(send_command(sock, "m $d800 $dbff"))
                transcript.append(send_command(sock, "m $df00 $df08"))
                transcript.append(send_command(sock, "m $c000 $c41f"))
                transcript.append(send_command(sock, "q"))
                exit_code = 0
        except Exception as exc:
            transcript.append(f"\n[test-reu-exception] {exc!r}\n")
            if sock is not None:
                try:
                    transcript.append(send_command(sock, "stop"))
                except Exception:
                    pass
                try:
                    transcript.append(send_command(sock, "r"))
                    transcript.append(send_command(sock, "m $0400 $05ff"))
                    transcript.append(send_command(sock, "m $d800 $dbff"))
                    transcript.append(send_command(sock, "m $df00 $df08"))
                    transcript.append(send_command(sock, "m $c000 $c41f"))
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


if __name__ == "__main__":
    raise SystemExit(main())
