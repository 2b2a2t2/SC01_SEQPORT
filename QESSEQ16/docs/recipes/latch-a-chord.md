# Latch a chord

**Goal** — a chord that keeps sounding after you lift your fingers, replaced only when you
play the next one.

**Before you start** — nothing. This is the fastest way to hear the instrument.

## Steps

1. Tap **MODE** until the header reads `KEYBOARD 2`. (Tap the lit mode pad again from
   `KEYBOARD 1`.)
2. Play a pad. You hear a triad, not a single note — KEY 2 is the chord engine.
3. Turn **ENC8** (`LATCH`) up.
4. Play a chord and let go. It sustains.
5. Play another. The first is cut cleanly and the second takes over — no pile-up.
6. **ENC4** (`FSCALE`) on snaps every chord tone into the current key and scale, so a
   voicing can never put an out-of-scale note on the wire.
7. **ENC5** (`BASS`) and **ENC6** (`SPRD`) move the bass note and open the voicing;
   **ENC7** (`DENS`) drops or adds upper voices.

## What you should hear

A pad chord that behaves like a held organ chord: one voice group at a time, no sustain
pedal, no stuck notes when you change mode.

## Variations

- Hold **M1**, **M2**, **M3** or **NEXT** while playing to override the quality
  (maj / min / dim / sus) for that press. With KeyLock on, a held quality button wins over
  the scale's diatonic triad — the root still snaps to the scale.
- **F1–F4** toggle the extensions (6, m7, M7, 9) on top of the quality.
- Turn **LATCH** off and the pads behave like a normal keyboard again.

## Why it works this way

Chord tones are computed from the current key and scale at press time, and `FSCALE`
re-snaps them at send time as well — including sequencer playback, which is why changing
the scale recolours a recorded progression live. See
[Keyboard modes](../modes/keymode.md) and the [glossary](../glossary.md) entry for
*force-to-scale*.
