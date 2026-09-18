# Resource-load reliability investigation (2026-09-11)

This is a chronological investigation record. Current loaders now suppress
sprite DMA during transfers and MCFILE hides the unfinished bitmap; subsequent
findings below record their tests, not a promise of rollback or universally
bounded KERNAL I/O. See the [current media contract](readybasic_reference.md)
and [demo walkthrough](readybasic_orbital_echoes.md). During hardware disk loading,
observe video until visibly complete before reading any RAM or decoded screen;
a fixed wait alone is not completion evidence. After interruption, boot afresh.

## Observations, not yet a diagnosis

The user reproduced a partial `AD` display, a slowly filled bitmap without its
final colors, and a stalled startup on the physical C64 Ultimate. The same source
had passed one earlier hardware run. No ReadyBASIC source changed during the
subsequent video-recording attempt. That single passing run did not establish
repeatable cold-start reliability.

`RBSND08` enables sprites before `SPRFILE`, `MCFILE`, and `MUSTUNE`. Its early
`ORBITAL SHOW RUNNING` text precedes all three loads; it is not a completion flag.
Until the animation starts, the sprite coordinates are not the final READY
coordinates. Koala stores bitmap bytes before both palettes, so partial loading
also explains the temporarily wrong colors without proving palette corruption.

## Source research

- Commodore KERNAL source, `serial4.0.s`, `ACPTR` (`$EE13` onward): serial reads
  disable IRQs, sample clock/data in tight loops, and execute CLI on return.
  Several clock waits have no universal wall-clock timeout. A wrapper around
  CHRIN therefore cannot honestly promise bounded recovery from every broken
  drive/bus condition. See the [KERNAL disassembly](https://www.pagetable.com/c64ref/c64disasm/).
- Sprite DMA steals CPU cycles; it is not stopped by SEI. See Linus Akesson's
  [hardware timing explanation](https://linusakesson.net/scene/nine/explanation.php).
  The streamed-read path must explicitly manage sprite activity, not assume
  the interrupt mask protects serial timing.
- ReadyBASIC uses `$01=$36` in its native worker: KERNAL and I/O remain visible,
  and stores at `$E000-$FF3F` land in underlying RAM. The image range deliberately
  excludes CPU vectors. The existing exact-byte VICE test verifies that mapping.
  Merely writing to bitmap RAM is not evidence of a memory conflict.
- Ultimate REST raw memory access stops and resumes the C64 (`dma_load_raw_buffer`
  in the official firmware source). It is unsuitable as aggressive progress
  polling during an IEC transfer; the drive and CPU may not pause together.
- VICE fast/trap disk I/O is insufficient evidence for real IEC timing.

## Current candidate and required evidence

Suppress sprite DMA inside every media-file transfer, saving/restoring the
caller's mask around OPEN through CLOSE on success and opened-file errors.
This is deliberately internal to the loader, not a BASIC-program workaround.
Test the final package through normal ReadyOS boot, verify complete image and
sprite bytes plus live SID, repeat starts, exercise missing-file cleanup, and
leave the physical machine playing for the user. Do not promote this candidate
to a proven root cause merely because a screenshot looks correct.

Presentation and transport are separate concerns: hiding an incomplete bitmap
will improve startup presentation, but cannot by itself fix a serial stall.
Full-image rollback would require a separately owned staging surface; it must
not borrow BASIC, module-slot, shim, or arbitrary REU memory.

## Implemented safeguards and initial results

The media loaders now suppress and restore `$D015` for the entire transfer.
`MCFILE` also hides the bitmap using DEN until the full image and EOF are checked,
then restores the display mode on both success and error. This is presentation
protection, not atomic rollback of already-written bytes. These graphics commands
own VIC mode setup; preserving a separately installed raster-IRQ effect is not
part of this loader ABI. The demo sets all five initial positions explicitly.

The first sprite-guard build passed the full fast-disk VICE audit at
`logs/vice_auto_20260911_142158/manifest.json` and the physical hardware audit at
`logs/ultimate_auto_20260911_142209/manifest.json`, including complete palettes
and sprites, changing frames and SID ticks at 1/16/64 MHz, refresh and cleanup.
The user independently confirmed the physical display was working. An initial
true-drive VICE run reached live music and motion checks but its VICE process
exited during the refresh wait; that interrupted run is not counted as a pass.

The final package is 2237 bytes with a 1959-byte payload and unchanged 241-byte
music driver. It claims the same slots as before and consumes no additional BASIC
workspace. Missing-file and wrong-header tests exercise restoration of the sprite
mask, display-enable bit and logical-file table. The hardware plan now asserts
the installed music state immediately after startup rather than trusting early
text, and captures color RAM as well as the screen palette.

Final true-drive VICE run: `logs/vice_auto_20260911_142659/manifest.json`, all
100 steps passed. The original post-run selector picked an older concurrent
run's manifest; this test-tool bug is fixed with unique plan IDs/paths and a
standalone exact-run auditor. Re-running that auditor against the correct
manifest passed exact bitmap/palette/sprite comparisons, reference line drawing,
REU restoration, animation, SID, refresh and cleanup checks.

Final cold-start hardware run: `logs/ultimate_auto_20260911_142855/manifest.json`.
The independent audit passed at 1/16/64 MHz, including color RAM and the explicit
startup music-state assertion. Regular 0.5Q and Ultimate 0.5R images passed
directory ordering; extracted module and demo match the final local artifacts.
The physical image was uploaded and fully read back at
`/USB1/automation/readybasic-media/neon-e14443b1/RBe14443b1.D81`; its `apps.cfg`
contains that exact case-insensitive path with DMA enabled. SHA-256:
`02ce1f55f4729e6b59d8dd4b5adaf7d3c8737b28e61d9e4e9da9cfac9e41e46c`.

## Explicit speed limit: disk loading is not music playback

A deliberately stronger test started a fresh RUN with **Manual 16 MHz already
active**. It failed the startup assertion at MCFILE, with music state 0:
`logs/ultimate_auto_20260911_143229/manifest.json`. Do not call that configuration
supported merely because post-load animation works at 16/64 MHz.

The [official Ultimate turbo documentation](https://1541u-documentation.readthedocs.io/en/latest/config/turbo_mode.html)
states that Manual mode prohibits software speed changes, and that `$D031` is
available only in U64 Turbo Registers / Turbo Enable Bit modes. A portable
loader cannot override forced Manual turbo via those registers. Current support
therefore requires **1 MHz during disk loading**, with higher playback speeds
available after the resources are loaded. The final manual handoff uses 1 MHz
so another RUN is safe. A future automatic speed guard must cover module loading
as well as resource loading, preserve/restore programmable turbo state, and be
tested separately; silently changing global turbo-control policy is not part of
this fix. A full image buffer alone would not solve the serial timing issue.
