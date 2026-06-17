# ReadyBASIC Sound Command Design

## Phase 1 Summary

ReadyBASIC Sound Phase 1 is command-only SID support. It does not change
ReadyBASIC language syntax, tokenization, control flow, native expression rules,
`BASIC_START`, or BASIC free bytes.

The implementation adds built-in module id `4`, submodule `SIDCORE` id `23`, as
a slot-2 replacement overlay. `SIDCORE` is stored in the ReadyBASIC assigned
code REU bank at offset `$7800`, loaded from cold-only `OVL6PACK`, and fetched
to `$B800-$BA0D` when a sound command runs.

There is no IRQ music engine in Phase 1. Commands either write SID registers
immediately or, in the case of `SOUND`/`SND`, perform a simple blocking tone.
This keeps all persistent behavior out of resident ReadyBASIC RAM.

## Design Rules

- Use SID-native concepts: voice, frequency, waveform/control, pulse width,
  ADSR, volume, and filter.
- Keep voice number explicit. Hidden current-voice state would be shorter to
  type, but it is less clear in stored programs and harder to reason about.
- Reuse existing parser signatures. A trial `VOICE(V,F,W,A,D,S,R)` signature
  grew the fixed resident area by 31 bytes, so Phase 1 uses the packed SID form
  `VOICE(V,F,W,AD,SR)`.
- Prefer fast hardware-shaped commands for loops. `ADSR` is friendlier for
  setup; `VOICE` is the compact 1 MHz path.
- Do not install interrupts for playback yet.

## SID Register Mapping

SID lives at `$D400-$D418`.

Each voice has seven registers:

| Register role | Voice 1 | Voice 2 | Voice 3 |
|---|---:|---:|---:|
| Frequency low/high | `$D400/$D401` | `$D407/$D408` | `$D40E/$D40F` |
| Pulse width low/high | `$D402/$D403` | `$D409/$D40A` | `$D410/$D411` |
| Control/wave/gate | `$D404` | `$D40B` | `$D412` |
| Attack/decay | `$D405` | `$D40C` | `$D413` |
| Sustain/release | `$D406` | `$D40D` | `$D414` |

Filter and volume use:

| Register | Role |
|---|---|
| `$D415/$D416` | 11-bit filter cutoff |
| `$D417` | resonance and voice/external routing |
| `$D418` | filter mode high nibble and master volume low nibble |

Wave/control bits:

| Bit | Decimal | Meaning |
|---:|---:|---|
| 0 | 1 | Gate |
| 1 | 2 | Sync |
| 2 | 4 | Ring modulation |
| 3 | 8 | Test |
| 4 | 16 | Triangle |
| 5 | 32 | Saw |
| 6 | 64 | Pulse |
| 7 | 128 | Noise |

## Commands

| Command | Syntax | Behavior |
|---|---|---|
| `SIDCLR` / `SILENCE` | `SIDCLR()` | Clears SID registers `$D400-$D418`. |
| `VOL` | `VOL(VOL)` | Sets master volume `0..15`, preserving filter mode bits in `$D418`. |
| `FREQ` | `FREQ(VOICE,FREQ16)` | Writes raw 16-bit SID frequency for voice `1..3`. |
| `NOTE` | `NOTE(VOICE,NOTE,OCT)` | Sets frequency from `NOTE=0..11` (`C..B`) and octave `0..7`, using a compact PAL C0 table shifted by octave. It does not gate the voice. |
| `PULSE` | `PULSE(VOICE,WIDTH)` | Writes 12-bit pulse width `0..4095`. |
| `ADSR` / `ENV` | `ADSR(VOICE,A,D,S,R)` | Writes attack, decay, sustain, and release nibbles `0..15`. |
| `WAVE` / `CTRL` | `WAVE(VOICE,MASK)` | Writes the SID voice control byte directly. Use bit `1` for gate, or use `GATE`. |
| `GATE` | `GATE(VOICE,ON)` | Sets or clears bit 0 in the current voice control register. |
| `VOICE` | `VOICE(VOICE,FREQ16,WAVE,AD,SR)` | Fast packed setup. Writes frequency, packed attack/decay byte, packed sustain/release byte, and control/wave byte. |
| `FILTER` / `FILT` | `FILTER(CUTOFF,RES,ROUTE,MODE)` | Writes cutoff `0..2047`, resonance `0..15`, route bitmask, and mode bitmask. |
| `SOUND` / `SND` | `SOUND(VOICE,FREQ16,DURATION,WAVE)` | Blocking helper. Writes frequency, gates `WAVE` on, waits for `DURATION` spin-delay units, then gates off. |

`FILTER` mode uses a logical low-nibble mask before shifting into `$D418`:

| Mode bit | Meaning |
|---:|---|
| 1 | Low-pass |
| 2 | Band-pass |
| 4 | High-pass |
| 8 | Voice 3 off |

`FILTER` route is written to the low nibble of `$D417`; bit 0 routes voice 1,
bit 1 routes voice 2, bit 2 routes voice 3, and bit 3 routes external input.

## Examples

Simple blocking tones:

```basic
10 SIDCLR():VOL(15):ADSR(1,0,5,12,3)
20 SOUND(1,4455,45,16):REM TRIANGLE C4
30 SOUND(1,4455,45,32):REM SAW C4
40 PULSE(1,2048):SOUND(1,4455,45,64):REM PULSE C4
50 SOUND(1,4455,45,128):REM NOISE
60 SILENCE()
```

Manual voice control:

```basic
10 SIDCLR():VOL(15)
20 ADSR(1,0,9,12,6):PULSE(1,3072):FREQ(1,4455)
30 WAVE(1,64):GATE(1,1):ZPAUSE(70):GATE(1,0)
```

Packed fast path:

```basic
10 SIDCLR():VOL(15):PULSE(1,2048)
20 VOICE(1,4455,65,9,195)
30 ZPAUSE(80):CTRL(1,64)
```

Here `65` is pulse plus gate, `9` is packed attack/decay `$09`, and `195` is
packed sustain/release `$C3`.

Filter example:

```basic
10 SIDCLR():VOL(15):ADSR(1,0,9,15,4)
20 FREQ(1,2230):WAVE(1,33)
30 FOR C=150 TO 950 STEP 200:FILTER(C,8,1,1):ZPAUSE(35):NEXT
40 FILTER(700,12,1,2):ZPAUSE(90)
50 FILTER(700,12,1,4):ZPAUSE(90)
60 SILENCE()
```

## Demos

The Phase 1 demos are built with `petcat -w2 -l 2ac1` and run inside
ReadyBASIC under ReadyOS:

- `rbsnd01_sid_basics.bas`: triangle, saw, pulse, and noise.
- `rbsnd02_voice_state.bas`: explicit ADSR, pulse width, frequency, wave, and gate.
- `rbsnd03_notes.bas`: chromatic `NOTE` command.
- `rbsnd04_filter.bas`: filter route/mode/cutoff changes.
- `rbsnd05_voice_batch.bas`: packed `VOICE` fast path.
- `rbsnd06_three_voice.bas`: three SID voices.

Automation target:

```sh
make readybasic-sound-phase1-vice
```

The automation verifies ReadyOS boot, ReadyBASIC load, stored BASIC demo loading,
listing, running, and absence of BASIC error prompts. It cannot prove audio
quality; the demos print the expected listening result before each step.

## Phase 2 Direction

Likely next sound work:

- String/MML-style `PLAY(...)`.
- REU-backed tune handles.
- Polling `SNDSTEP()` playback before considering IRQ playback.
- Optional PAL/NTSC note-clock selection.
- More compact sound-effect macros that compile into REU resources before BASIC
  begins running.
