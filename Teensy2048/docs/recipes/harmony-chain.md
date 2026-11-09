# The full chain: bass → HARM → chords → LEAD

**Goal** — one bassline that produces a harmonised chord track, which in turn re-harmonises
a melody. Change the bass, and everything downstream follows.

**Before you start** — four tracks: one MONO/BASS, one POLY/HARM, one POLY/STEP, one
MONO/LEAD. Set types on `SEQUENCER 3` (`ENC6 = TRACK`, `ENC7 = TYPE`).

## Steps

1. **Bass.** On the MONO/BASS track, press **M2** (blue) and generate with **M3**. See
   [bassline from a progression](bassline-from-progression.md).
2. **Harmony.** On the POLY/HARM track, press **M2** (aqua). `SEQUENCER 3 → ENC5 = INCH` to
   the bass track's channel. Set **ENC1 = GENRE** to the grammar you want, **ENC2 = FILL**
   to control how many steps get a chord, **ENC3 = SRC** to `BASS`. Press **M3** to bake.
3. **Chords into sound.** Leave the HARM track on `SEQUENCER 1` and press **PLAY** — a HARM
   track plays like STEP, so the baked triads sound as block chords.
4. **Melody.** On the MONO/LEAD track, `ENC5 = INCH` to the HARM track, **ENC3 = FOLLOW**
   on. Press **M2** (sea green), set **ENC2 = CONTOUR**, **ENC5 = CT** (chord-tone vs scale
   blend), **ENC6 = GD** (guide-tone depth), then **M3** to generate the skeleton.
5. **MAP.** On the bass track's GNR8 page, **F2 → ENC3 = `ct`** (chord tone whose octave
   follows the contour) or `gd` (3rd/7th only). MAP is global: one setting serves the chain.
6. Play a different bass note or regenerate the bass. The HARM track re-bakes before the
   next draw, and the LEAD track re-resolves onto the new chord.

## What you should hear

A bassline, block chords that agree with it, and a melody whose register rides the contour
while its pitches lock to the harmony. Nothing is copied between tracks: each one reads the
one before it.

## Variations

- `HOLD` on the HARM page keeps a repeated bass note on the same chord instead of
  re-voicing it.
- `SKANK` on a POLY track derives a walking voicing at send time instead of baking; point a
  follower at it and the follower hears the **walked** notes.
- `FOLLOW = FREEZE` latches the chord that was sounding when you turned the knob, so a
  follower can hold a harmony while the feeder keeps moving.

## Traps

- FOLLOW is **one level and non-recursive**. Point each reader at a real track; do not
  expect A→B→C to mirror through B.
- Mute is an **output gate only**: a muted feeder still feeds.
- Erasing the feeder leaves a baked HARM pattern alone (M3 re-derives manually).

See [HARM](../generators/harm.md), [LEAD](../generators/lead.md),
[GNR8](../generators/gnr8.md) and [Concepts](../concepts.md).
