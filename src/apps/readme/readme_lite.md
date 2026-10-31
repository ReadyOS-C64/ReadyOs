## Using ReadyOS 0.5

Pick an app in the launcher with the
cursor keys and press RETURN.

CTRL+B returns to the launcher.
F2 and F4 switch between apps.
A suspended app keeps its place in
the REU. Save files to disk before
you reset or turn off the C64.

In this guide, cursor keys turn the
page. HOME returns to this page.

Next: launcher keys, files and tools,
ReadyShell, ReadyBASIC, disk setup
and OS bridges.
---
## Launcher keys

Disk launcher: F3 preloads an app.
F5 browses manifests. Select an `app.*`
manifest to add its launcher entry.
F7 unloads the selected app and
releases its REU storage.

CTRL+1 through CTRL+9 switch to the
apps assigned to those slots. In apps
with the shared hotkey handler, use
CTRL+SHIFT+number to bind a slot.
On an emulator the shifted digit may
appear as a symbol, such as CTRL+!.

Use REU Viewer to see which apps
hold banks when memory runs low.
---
## Files and everyday apps

Editor edits text and uses the shared
clipboard. Simple Files browses two
directories and displays SEQ text.

QuickNotes keeps notes in the REU.
Clipboard Manager previews, imports
and clears clipboard history.

Task List keeps an indented outline.
Calendar 26 tracks appointments.
Dizzy organizes tasks on a board.
Simple Cells stores spreadsheet grids
and formulas. Calc Plus evaluates
expressions and keeps a history.

---
## ReadyShell

Type HELP for command help. Read the
command guide one page at a time:
CAT "rshelp" | MORE

On a C64 keyboard, type ! to enter
the pipe character. Try:
LST | TOP 3 | SEL "NAME"

LST reads a directory. TOP limits
the rows. SEL returns the names.
To list SEQ files on drive 9:
`LST "*", 9, "SEQ"`

CAT reads a text file. PUT replaces
its contents. ADD appends a line.
---
## ReadyBASIC

Launch ReadyBASIC before loading a
BASIC example. On the regular D81:
LOAD "RBGFX02",8
RUN

On the Ultimate D81 pair, examples
are on drive 9. Use ,9 in LOAD.
Modules and demo media load from 8.

For a language example try RBPROC1.
For sprites try RBGFX13. RBSND01
plays SID sound. The release guide
lists the examples on your disks.

See docs/readybasic_reference.html
for listings and command syntax.
---
## Tools and games

Hex View displays memory as bytes
and PETSCII. System Info reports your
machine, drives and REU.

### ReadyIRC
Connect to an IRC server using the
Ultimate TCP interface. Use /join
to enter a channel.

### UCI Tester
Inspect Ultimate DOS, network and
device responses. Ultimate Zip makes
and extracts ZIP files on the device.

Resume a game from the launcher.
---
## Disk and cartridge boot

Enable the REU before starting.
Mount the boot disk on drive 8.
`LOAD"*",8`
RUN

For a dual-disk boot set, mount its
second disk on 9 before starting.
The release folder lists the drive
types and any optional swap disk.

EasyFlash: mount readyos_data.d64 on
8, attach the CRT, then reset. During
preload the border alternates yellow
and brown/orange. Restart ReadyOS to
recover an unloaded cartridge app.
---
## Ultimate disk setup

Mount the first D81 on drive 8.
Before first boot, type:
LOAD"SETUP",8,1
RUN

Select the boot image on Ultimate
storage and save. SETUP records its
host path for fast loading. Repeat
after moving or renaming the image.
Boot PREBOOT after leaving SETUP.

On stock Ultimate, disk 2 holds
BASIC examples and optional bridge
manifests. Browse it on device 9.
---
## Switching operating systems

For OS switching, choose the matching
bridge SKU and enable a 16 MiB REU.
Install the native companion from
that release folder on your OS disk.

C64OS 1.09: use Bridge Setup to cap
C64OS at 8-32 banks and restart.
The dedicated ReadyOS boot skips 39.

GDOS64 and GEOS 2.0: use the matching
desk accessory and skip-71 image.

Stock Ultimate skips zero banks. Its
optional apps show the required setup.
Each bridge README covers native disk
placement, resume and recovery.
