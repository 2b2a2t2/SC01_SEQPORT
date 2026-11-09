# Swing one track only

**Goal** — a shuffled hat against a straight kick, without moving the kick.

**Before you start** — two tracks playing, one of them DRUM.

## Steps

1. `SEQUENCER 4` is the timing screen. **ENC1 = SWING** picks the style for the **edit
   track**: `MPC60`, `MPC3K`, `SP1200`, `LINN`, `TR808`, `TR909`, `SHUFFLE`, `TRIPLET`.
2. **ENC2 = AMT** sets how much. The header shows the **effective ratio**, e.g. `Swing 62%`.
3. Switch the edit track (`SEQUENCER 3 → ENC6 = TRACK`) and leave its swing `OFF`.

## What you should hear

The hats land late on the off-beats while the kick stays on the grid. The ratio is
`50 + (AMT/100) × (styleMax − 50)`, so AMT 0 is straight and AMT 100 is that machine's full
swing; the style caps how far the swing can shove the off-beat.

## Variations

- Each style pairs a grid (16th for the MPCs, SP1200, LINN, 808, 909; 8th pairs for SHUFFLE
  and TRIPLET), which is why `TRIPLET` at the same AMT sounds deeper than `LINN`.
- Swing is per track and saved with `SEQ 4 + REC`, so a whole set can keep its feel.
- Both knobs are clamped, no wrap — a turn past the end stays there.

## Traps

- The swing row only lives on the **plain** track view. An open page (GNR8, ARP, DRUM…)
  owns those slots, so close the page first.
- Swung steps are **deferred**, not shifted: the on-steps stay on the grid. The queue is
  cleared on transport start/stop, so a swung track never leaks a stray step into a fresh
  run.

See [Swing & external input](../modes/timing.md).
