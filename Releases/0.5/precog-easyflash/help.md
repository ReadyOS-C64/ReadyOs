# ReadyOS EasyFlash

- attach `readyos_easyflash.crt` as an easyflash cartridge
- mount `readyos_data.d64` on drive `8`
- enable REU with at least `1MB`; `8MB` or `16MB` is recommended where available
- reset to start the cartridge. Yellow and orange border phases show apps being copied into REU. Select an app with RETURN and use Ctrl+B to reach the launcher.
- if an app snapshot preloaded from the cartridge is unloaded from REU, ReadyOS cannot load it again from the cartridge until you restart ReadyOS

## Companion disk

- the CRT uses its cartridge-bank layout and is not governed by floppy directory order
- the companion `readyos_data.d64` writes ordinary SEQ/USR data before REL data

## Start in VICE

```sh
x64sc -reu -reusize 16384 -cartcrt readyos_easyflash.crt -drive8type 1541 -devicebackend8 0 +busdevice8 -8 readyos_data.d64
```

## Apps and requirements

Every app uses the ReadyOS REU snapshot system. Additional requirements:

- **editor** — text editor with clipboard.
- **readyshell (beta)** — command shell poc scaffold (portable; own REU overlays/state).
- **simple files** — dual pane file manager and seq viewer.
- **clipboard** — manage clipboard items (portable; shared REU clipboard/history).
- **ready basic (beta)** — beta basic v2 bridge with modules (portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate).
- **calendar 26** — calendar for 2026 with appointments.
- **task list** — outliner with notes and search.
- **reu viewer** — view reu memory usage (portable; inspects shared REU allocation records).
- **system info** — system, reu, and ultimate status (portable diagnostics; Ultimate details available only on Ultimate).
- **quicknotes** — reu-backed note editor (portable; own REU note storage).
- **calc plus** — keyboard-first expression calculator.
- **hex viewer** — browse memory in hex format.
- **simple cells (alpha)** — spreadsheet with formulas and colors.
- **2048 game** — 2048 tile puzzle.
- **sidetris** — sideways tetris.
- **deminer** — minesweeper-style puzzle.
- **dizzy kanban** — fizzy inspired kanban app.
- **readyirc** — ultimate tcp irc client (Ultimate-only TCP; own REU scrollback).
- **uci tester** — ultimate command interface lab (Ultimate-only command lab; no separate app-owned REU workspace).
- **read.me** — readyos, ultimate buddy, precog poc os.

ReadyBASIC's built-in commands are preloaded with the runtime. The companion data D64 does not package the disk-module/example/media collection; use the regular or Ultimate D81 for the new media demos. Cartridge preload is separate from Ultimate DOS DMA and needs no D81 host-path setup.
