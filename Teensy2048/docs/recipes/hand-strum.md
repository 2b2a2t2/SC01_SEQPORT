# Strum a chord by hand

**Goal** — a strum whose timing is yours, not a fixed rake.

**Before you start** — a POLY track with **TYPE = HAND** (`SEQUENCER 3 → ENC7`). Its **M2**
pad is **lavender**: the strum ADDS row.

## Steps

1. `SEQUENCER 3`: select the track, **ENC6 = POLY**, **ENC7 = `HAND`**.
2. Play its **M2** pad to reach the strum options.
3. Switch to `KEYBOARD 1`. The keyboard is now a fretboard: white keys are strings, lit in
   a pink → seagreen gradient left to right; black keys are option toggles (violet = on).
   Silent strings dim almost to black.
4. Pluck with white keys. The chord built by the option keys is what sounds.
5. To capture it: `SEQUENCER 1`, arm **REC** (bright red), start **PLAY**, and perform the
   strum over the bar. The gesture lands on the step under the playhead.

## What you should hear

A strum with real string order and spacing. Recorded, it replays **as performed**: each
pluck keeps its onset in milliseconds from the first touch, so a long slow strum across a
bar is reproduced rather than quantised into a rake.

## Variations

- A step's **TYPE** can be overridden per step (M1 radio → `F2 TYPE`, then tap the step):
  `STEP` block chord, `STRUM` fixed rake, `HAND` buffered gesture.
- **SKANK** is where COMP stabs pay off: on a SKANK track `HITS` re-strikes what the walk
  produced. A HAND step keeps its own strum.

## Traps

- HAND steps **buffer silently** — if a step makes no sound on its own, that is the point;
  the keyboard is what plays it.
- The gradient and option keys win over the KEY 2 chord tint and the KEY 4 proposal
  rainbow while HAND is live.

See [HAND strum](../generators/hand-strum.md) and [LED feedback](../reference/leds.md).
