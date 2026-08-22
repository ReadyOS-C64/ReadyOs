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
immediately or, in the case of `SOUND`, perform a simple blocking tone.
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
- Command names used in stored BASIC demos must be BASIC-token-safe and should
  be written lowercase in host `.bas` sources before `petcat`. This beta build
  does not register duplicate legacy spellings, so demos use `sidrst`,
  `sidoff`, `frq`, and `pitch` and avoid names that contain BASIC V2 tokens
  such as `CLR`, `LEN`, `FRE`, and `NOT`.

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
| `SIDRST` / `SIDOFF` | `SIDRST()` | Reset/off commands. Clears SID registers `$D400-$D418`. |
| `VOL` | `VOL(VOL)` | Sets master volume `0..15`, preserving filter mode bits in `$D418`. |
| `FRQ` | `FRQ(VOICE,FREQ16)` | Writes raw 16-bit SID frequency for voice `1..3`. |
| `PITCH` | `PITCH(VOICE,SEMI,OCT)` | Sets frequency from semitone `0..11` (`C..B`) and octave `0..7`, using a compact PAL C0 table shifted by octave. It does not gate the voice. |
| `PULSE` | `PULSE(VOICE,WIDTH)` | Writes 12-bit pulse width `0..4095`. |
| `ADSR` | `ADSR(VOICE,A,D,S,R)` | Writes attack, decay, sustain, and release nibbles `0..15`. |
| `WAVE` | `WAVE(VOICE,MASK)` | Writes the SID voice control byte directly. Use bit `1` for gate, or use `GATE`. |
| `GATE` | `GATE(VOICE,ON)` | Sets or clears bit 0 in the current voice control register. |
| `VOICE` | `VOICE(VOICE,FREQ16,WAVE,AD,SR)` | Fast packed setup. Writes frequency, packed attack/decay byte, packed sustain/release byte, and control/wave byte. |
| `FILTER` | `FILTER(CUTOFF,RES,ROUTE,MODE)` | Writes cutoff `0..2047`, resonance `0..15`, route bitmask, and mode bitmask. |
| `SOUND` | `SOUND(VOICE,FREQ16,DURATION,WAVE)` | Blocking helper. Writes frequency, gates `WAVE` on, waits for `DURATION` spin-delay units, then gates off. |

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
10 sidrst():vol(15):adsr(1,0,5,12,3)
20 sound(1,4455,45,16):rem triangle c4
30 sound(1,4455,45,32):rem saw c4
40 pulse(1,2048):sound(1,4455,45,64):rem pulse c4
50 sound(1,4455,45,128):rem noise
60 sidoff()
```

Manual voice control:

```basic
10 sidrst():vol(15)
20 adsr(1,0,9,12,6):pulse(1,3072):frq(1,4455)
30 wave(1,64):gate(1,1):zpause(70):gate(1,0)
```

Packed fast path:

```basic
10 sidrst():vol(15):pulse(1,2048)
20 voice(1,4455,65,9,195)
30 zpause(80):wave(1,64)
```

Here `65` is pulse plus gate, `9` is packed attack/decay `$09`, and `195` is
packed sustain/release `$C3`.

Filter example:

```basic
10 sidrst():vol(15):adsr(1,0,9,15,4)
20 frq(1,2230):wave(1,33)
30 for c=150 to 950 step 200:filter(c,8,1,1):zpause(35):next
40 filter(700,12,1,2):zpause(90)
50 filter(700,12,1,4):zpause(90)
60 sidoff()
```

## Demos

The Phase 1 demos are built with `petcat -w2 -l 2ac1` and run inside
ReadyBASIC under ReadyOS:

- `rbsnd01_sid_basics.bas`: triangle, saw, pulse, and noise.
- `rbsnd02_voice_state.bas`: explicit ADSR, pulse width, frequency, wave, and gate.
- `rbsnd03_notes.bas`: chromatic `PITCH` command.
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
