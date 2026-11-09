<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# Mode System

Three mode families, each with 4 variants (1–4). Variant 3 is **momentary hold**. Variant 4 is **blinking** (double-click to enter) — and in the ENC family variant 4 is the **locked channel picker**: its step pads select the encoder's MIDI channel for the whole visit, no hold needed (see [ENCMODE4](encmode.md)). On release of the held button, **ENCMODE3 always exits into ENCMODE** and **SEQMODE3 always exits into SEQMODE** — a hold started inside that family restores that variant, a hold started anywhere else lands on that family's variant 1; only **KEYMODE3** restores the previous base/variant wherever it came from.

**Per-channel mode memory**: each **keyboard channel** remembers the KEY variant last latched on it — switching the keyboard channel (its own variant-3 overlay, or a locked channel-link change) recalls that channel's variant live (KEYMODE2 for one track, KEYMODE4 for another) and the next KEYMODE entry lands on it. Each **encoder channel** likewise remembers its last **F bank + M3 page group** — switching the encoder channel or re-entering ENCMODE recalls that channel's page instead of resetting to F1. Both memories **persist with SEQMODE4+REC** (the 616-byte SEQ EEPROM block; older saves load with all channels at the defaults — KEYMODE1, F1, group 1).

| Mode Button | Family | Variants |
|-------------|--------|----------|
| **ENC** (LED 32) | Encoder/CC mode | 1: shared SEQ controls, 2: ENC2, 3: ENC3 (momentary), 4: ENC4 (**locked** channel picker) |
| **SEQ** (LED 33) | Sequencer mode | 1–4: SEQ1–SEQ4 |
| **KEY** (LED 34) | Keyboard mode | 1–4: KEY1–KEY4 |

**KEYMODE2** = chord mode (diatonic auto-chords, quality/extension buttons, latch, KeyLock).  
Chord behavior is **inherited** by SEQMODE/ENCMODE via `keyBehaviorVariant()`.

**KEYMODE4** = genre mode (double-click KEY): an 18-genre progression engine ranks the best next chords on the keyboard, pressing one auditions it and writes it to the focused step (see [Keyboard modes](keymode.md)).

## Binding Matrix (Bindings.cpp)

Each mode/variant combination selects an `EncSet`, `CtrlSet`, `StepSet`, `KeySet` that defines what the 8 encoders, 9 control buttons, 16 step pads, and 12 keyboard pads do.

**Example (SEQMODE3):**
- ENC1: Octave shift of edit sequence
- ENC2: Pattern length (0–64 steps)
- ENC3: FOLLOW OFF/ON/FREEZE (three-state; FREEZE holds the last incoming chord)
- ENC4: Pattern speed (1/64 … 1 … 4, default 1)
- ENC5: Input channel (1–16)
- ENC6: Track type (DRUM/MONO/POLY)
- ENC7: Mono type (ROOT/BASS/ARP/STRUM/LEAD) — MONO only; Poly type (STEP/STRUM/HAND/HARM/SKANK) — POLY only
- ENC8: Tempo (40–240 BPM)

## Pages in this folder

- [Keyboard modes](keymode.md) — KEY 1–4
- [Sequencer](seqmode.md) — SEQMODE 1–4: tracks, steps, record, edit
- [Swing & external input](timing.md) — SEQMODE 4
- [Encoder / CC mode](encmode.md) — ENCMODE 1–4
- [DRUM part sequencing](drum.md) — parts and kit maps
