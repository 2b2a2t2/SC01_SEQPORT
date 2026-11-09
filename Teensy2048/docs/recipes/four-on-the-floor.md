# Four-on-the-floor in 30 seconds

**Goal** — a usable house beat without programming a single step.

**Before you start** — a DRUM track. Channel 1 defaults to DRUM; on any other channel set
`SEQUENCER 3 → ENC6 = TRACK` to that track and `ENC7 = TYPE` to `DRUM`.

## Steps

1. Tap **MODE** to `SEQUENCER 1`, on the DRUM track.
2. Press **M2**. The pad is **indigo** — that is the DRUM pattern page. The header reads
   `DRUM PATTERN`.
3. **ENC1** (`GENRE`) to `HOUSE`. The pattern is baked immediately; no M3 press needed.
4. **ENC3** (`FILL %`) up: the hats go from an eighth-note backbone to full sixteenths.
5. **ENC4** (`MOT %`) up for more accent variation — this rides the same per-voice accent
   mask the ACCENT attribute edits.
6. **ENC2** (`VAR`) to `fill4` for a snare-and-tom run over the last beat.
7. Press **M3** to re-roll the ghosts and accents from the same settings. **M3 hold** undoes
   the last re-roll or knob burst.
8. Press **PLAY**.

## What you should hear

A four-on-the-floor kick with claps and congas, hats that fill in as you turn FILL, and a
fill every bar if you left `VAR` on.

## Variations

- The F row gives four knob sets: **F1** groove (genre/VAR/FILL/MOT), **F2** the four lane
  lines (`A` / `B` / `OFF`), **F3** per-lane FILL, **F4** per-lane MOT. The master knobs
  write all four lanes; the per-lane knobs trim them.
- `SEQUENCER 3 → ENC6` stays on the track; **ENC7 = `MAP`** switches the kit between
  `CUSTOM` and `GM`, and re-bakes the pattern onto the new map.
- The 18 styles are independent of the KEY 4 progression genre: a jazz progression can run
  over a trap beat on purpose.

## Why it works this way

The styles are authored one-bar patterns with a core line and a ghost layer per lane, so
FILL and MOT are recombinations rather than randomisation. See
[DRUM preset styles](../generators/drum-styles.md) and
[DRUM part sequencing](../modes/drum.md).
