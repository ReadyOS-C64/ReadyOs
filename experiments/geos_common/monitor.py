"""Small binary-monitor client for executing the port's assembly in VICE."""

from __future__ import annotations

import socket
import struct
import subprocess
import time
from collections import deque
from pathlib import Path


class Monitor:
    def __init__(self, sock, timeout=20):
        self.sock = sock
        self.timeout = timeout
        self.sequence = 0
        self.events = deque()

    def exact(self, size):
        result = bytearray()
        while len(result) < size:
            chunk = self.sock.recv(size - len(result))
            if not chunk:
                raise RuntimeError("VICE monitor disconnected")
            result.extend(chunk)
        return bytes(result)

    def receive(self):
        header = self.exact(12)
        if header[:2] != b"\x02\x02":
            raise RuntimeError("invalid VICE binary-monitor response")
        size = struct.unpack_from("<I", header, 2)[0]
        return header[6], header[7], struct.unpack_from("<I", header, 8)[0], self.exact(size)

    def command(self, kind, body=b""):
        if kind == 0xAA:
            # A new execution interval must not consume a previous stop at
            # the same reusable harness checkpoint.
            self.events.clear()
        self.sequence += 1
        self.sock.sendall(b"\x02\x02" + struct.pack("<II", len(body), self.sequence)
                          + bytes([kind]) + body)
        while True:
            response, error, sequence, payload = self.receive()
            if sequence != self.sequence:
                self.events.append((response, error, sequence, payload))
                continue
            if error:
                raise RuntimeError(f"VICE command {kind:02x}: error {error:02x}")
            return payload

    def write(self, address, data):
        data = bytes(data)
        if data:
            self.command(2, b"\0" + struct.pack("<HHBH", address, address + len(data)-1, 0, 0) + data)

    def read(self, address, length):
        payload = self.command(1, b"\0" + struct.pack("<HHBH", address, address + length-1, 0, 0))
        count = struct.unpack_from("<H", payload)[0]
        if count != length:
            raise RuntimeError("short VICE memory response")
        return payload[2:]

    def checkpoint(self, address):
        self.command(0x12, struct.pack("<HHBBBB", address, address, 1, 1, 4, 1))

    def wait(self, address):
        deadline = time.monotonic() + self.timeout
        resumed = False
        while time.monotonic() < deadline:
            kind, error, sequence, body = self.events.popleft() if self.events else self.receive()
            if error:
                raise RuntimeError(f"VICE asynchronous error {error}")
            if sequence == 0xFFFFFFFF:
                if kind == 0x63:
                    resumed = True
                if kind == 0x61:
                    raise RuntimeError(f"VICE CPU jammed at {int.from_bytes(body[:2], 'little'):04x}")
                if resumed and kind == 0x62 and int.from_bytes(body[:2], "little") == address:
                    return
        raise TimeoutError(f"VICE did not reach {address:04x}")

