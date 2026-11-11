# Generate a bassline under a progression

**Goal** — a bass part that follows the chords instead of repeating one note.

**Before you start** — a MONO track with **TYPE = BASS**, and something to follow: a chord
track, a KEY 4 progression, or your hands.

## Steps

1. `SEQUENCER 3`: **ENC6 = TRACK** to a free track, **ENC7 = TYPE** to `MONO`, then the
   MONO transform to `BASS`. **M2** now burns **blue**.
2. Press **M2** to open the GNR8 page (`GENERATOR` in the header).
3. **ENC2** (`FILL`) to taste — 50 is a good start, 100 fills every step.
4. **ENC3** (`ACC`) and **ENC4** (`GLIDE`) set the per-note accent and glide odds.
5. Arm **F1** (`OPTS`) for the full option page: **ENC1 TONES** (all / 5 / 4 / 3),
   **ENC2 RANGE** (1–5 octaves), **ENC3 LOW** (C0/C1/C2), **ENC4 ALGO**
   (`CLASSIC` random per step, `EUCLID` for even pulses).
6. Press **M3** to generate. **M3 hold** undoes it.
7. Point the track at its harmony source: `SEQUENCER 3 → ENC5 = INCH` to the chord track's
   channel, **ENC3 = FOLLOW** on.

## What you should hear

A bassline in the current key and scale, inside the LOW..RANGE window. With FOLLOW on it
re-resolves onto each incoming chord — the line keeps its contour and the harmony moves
under it.

## Variations

- **F2**: tap a step, then **ENC2** transposes that step −2…+2 octaves. **ENC3 = MAP**
  chooses how FOLLOW maps notes (`near` · `root` · `deg` · `ivl` · `ct` · `gd`).
- **F3**: tap a step, **ENC3 = BRW** borrows tone 1–8 of the incoming chord for that step.
- **F4**: tap a step, **ENC4 = PROB** sets the chance that step's OCT/BRW modifier applies.
- **TIE span**: close the page first (!), arm **M1** and then **TIE**, hold the first step
  (`Chain From Step N`), tap the last. The span holds the start note over the rests.
- **PREV held** gives SHIFT (rotate the whole pattern) and TRANS (semitones).

## Traps

- The page and the M1 edit radio are **mutually exclusive**: opening the page turns the
  radio off, so a TIE span has to be made with the page closed.
- Per-step OCT / BRW / PROB are **RAM only** — they die with the step, while the eight page
  knobs persist with `SEQ 4 + REC`.
- `FOLLOW OFF` ignores MAP and BRW entirely.

See [GNR8](../generators/gnr8.md) and [Sequencer](../modes/seqmode.md).
