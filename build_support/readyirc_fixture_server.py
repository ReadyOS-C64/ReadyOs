#!/usr/bin/env python3
"""Deterministic plain-TCP IRC fixture for ReadyIRC C64 Ultimate tests."""

from __future__ import annotations

import argparse
import json
import signal
import socket
import socketserver
import threading
import time
from pathlib import Path


class FixtureState:
    def __init__(self, log_path: Path, status_path: Path, channel: str) -> None:
        self.log_path = log_path
        self.status_path = status_path
        self.channel = channel
        self.lock = threading.Lock()
        self.connections = 0
        self.registered = 0
        self.quit_count = 0
        self.exact_pong = False
        self.registrations: list[dict[str, object]] = []
        self.events: list[str] = []

    def record(self, event: str) -> None:
        with self.lock:
            stamp = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())
            self.events.append(event)
            with self.log_path.open("a", encoding="utf-8") as handle:
                handle.write(f"{stamp} {event}\n")
            payload = {
                "connections": self.connections,
                "registered": self.registered,
                "quit_count": self.quit_count,
                "exact_pong": self.exact_pong,
                "registrations": self.registrations,
                "events": self.events[-40:],
            }
            temp_path = self.status_path.with_suffix(self.status_path.suffix + ".tmp")
            temp_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            temp_path.replace(self.status_path)

    def new_connection(self) -> int:
        with self.lock:
            self.connections += 1
            number = self.connections
        self.record(f"connection {number} accepted")
        return number


class ReadyIrcHandler(socketserver.StreamRequestHandler):
    state: FixtureState

    def setup(self) -> None:
        super().setup()
        self.connection_number = self.server.state.new_connection()  # type: ignore[attr-defined]
        self.nick = "enteryournick"
        self.user = ""
        self.join_channel = ""
        self.got_user = False
        self.got_join = False
        self.registered = False
        self.send_lock = threading.Lock()
        self.closed = False

    def send_line(self, line: str) -> None:
        with self.send_lock:
            if self.closed:
                return
            try:
                self.wfile.write((line + "\r\n").encode("ascii"))
                self.wfile.flush()
                self.server.state.record(  # type: ignore[attr-defined]
                    f"connection {self.connection_number} tx {line}"
                )
            except OSError:
                self.closed = True

    def finish_registration(self) -> None:
        if self.registered or not (self.got_user and self.got_join):
            return
        self.registered = True
        with self.server.state.lock:  # type: ignore[attr-defined]
            self.server.state.registered += 1  # type: ignore[attr-defined]
            self.server.state.registrations.append(  # type: ignore[attr-defined]
                {
                    "connection": self.connection_number,
                    "nick": self.nick,
                    "user": self.user,
                    "channel": self.join_channel,
                }
            )
        self.server.state.record(  # type: ignore[attr-defined]
            f"connection {self.connection_number} registered nick={self.nick}"
        )
        self.send_line(f":Fixture.Server 001 {self.nick} :WELCOME MIXED CASE")
        self.send_line(
            f":UpperNick!user@fixture PRIVMSG {self.server.state.channel} "  # type: ignore[attr-defined]
            f":CONNECTION {self.connection_number} MIXED CASE WELCOME"
        )
        self.send_line("PING :MiXeDToken")

    def schedule_line(self, delay: float, line: str) -> None:
        timer = threading.Timer(delay, self.send_line, args=(line,))
        timer.daemon = True
        timer.start()

    def schedule_close(self, delay: float) -> None:
        def close_connection() -> None:
            self.server.state.record(  # type: ignore[attr-defined]
                f"connection {self.connection_number} forced close"
            )
            self.closed = True
            try:
                self.request.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                self.request.close()
            except OSError:
                pass

        timer = threading.Timer(delay, close_connection)
        timer.daemon = True
        timer.start()

    def handle(self) -> None:
        while not self.closed:
            raw = self.rfile.readline(1024)
            if not raw:
                break
            line = raw.decode("ascii", errors="replace").rstrip("\r\n")
            self.server.state.record(  # type: ignore[attr-defined]
                f"connection {self.connection_number} rx {line}"
            )
            upper = line.upper()
            if upper.startswith("NICK "):
                self.nick = line[5:].strip() or self.nick
            elif upper.startswith("USER "):
                self.user = line[5:].split(" ", 1)[0].strip()
                self.got_user = True
            elif upper.startswith("JOIN "):
                self.join_channel = line[5:].strip()
                self.got_join = True
            elif line == "PONG :MiXeDToken":
                with self.server.state.lock:  # type: ignore[attr-defined]
                    self.server.state.exact_pong = True  # type: ignore[attr-defined]
                self.server.state.record("exact mixed-case pong received")  # type: ignore[attr-defined]
            elif upper.startswith("QUIT "):
                with self.server.state.lock:  # type: ignore[attr-defined]
                    self.server.state.quit_count += 1  # type: ignore[attr-defined]
                self.server.state.record(  # type: ignore[attr-defined]
                    f"connection {self.connection_number} quit received"
                )
                break
            elif upper.startswith("PRIVMSG ") and " :" in line:
                message = line.split(" :", 1)[1]
                if message.lower() == "queuewhileaway":
                    self.schedule_line(
                        3.0,
                        f":QueueBot!user@fixture PRIVMSG {self.server.state.channel} "  # type: ignore[attr-defined]
                        ":QUEUED WHILE SUSPENDED",
                    )
                elif message.lower() == "dropwhileaway":
                    self.schedule_close(3.0)
                else:
                    self.send_line(
                        f":EchoBot!user@fixture PRIVMSG {self.server.state.channel} "  # type: ignore[attr-defined]
                        f":ECHO MIXED {message.upper()}"
                    )
            self.finish_registration()

    def finish(self) -> None:
        self.closed = True
        self.server.state.record(  # type: ignore[attr-defined]
            f"connection {self.connection_number} closed"
        )
        super().finish()


class FixtureServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address: tuple[str, int], state: FixtureState) -> None:
        self.state = state
        super().__init__(address, ReadyIrcHandler)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=16667)
    parser.add_argument("--channel", default="#readyostest")
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--status", type=Path, required=True)
    args = parser.parse_args()

    args.log.parent.mkdir(parents=True, exist_ok=True)
    args.status.parent.mkdir(parents=True, exist_ok=True)
    args.log.write_text("", encoding="utf-8")
    state = FixtureState(args.log, args.status, args.channel)
    server = FixtureServer((args.bind, args.port), state)

    def stop_server(_signum: int, _frame: object) -> None:
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGTERM, stop_server)
    signal.signal(signal.SIGINT, stop_server)
    state.record(f"fixture listening {args.bind}:{args.port}")
    try:
        server.serve_forever(poll_interval=0.2)
    finally:
        server.server_close()
        state.record("fixture stopped")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
