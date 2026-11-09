# Transpose a track without leaving the scale

**Goal** — move a part up or down in semitones, with the line staying diatonic.

**Before you start** — a non-DRUM track with steps in it.

## Steps

1. In `SEQUENCER 1` (any variant), hold **PREV**. The encoder row swaps to
   `SHIFT · TRANS · KLOCK`.
2. **ENC2 = TRANS**: 1 count = 1 semitone, clamped **−12…+12**, no wrap. The header reads
   `Transpose +k` / `Transpose −k`.
3. **ENC3 = KLOCK** on (clockwise saturates at `Scale Lock on`): the transposed note is
   snapped back to the nearest in-scale pitch, down first then up, so the part stays
   diatonic under the current KEY + SCALE.
4. Release **PREV**. The track keeps the transposition.

## What you should hear

The part moves in pitch while the stored notes stay untouched — TRANS is a playback
transform, applied after the octave and the range fold. With KLOCK on, no transposed note
lands outside the scale; with it off you get the chromatic shift.

## Variations

- **ENC1 = SHIFT** rotates the whole pattern (4 counts = 1 step) — a real data move, so it
  is destructive to the pattern and works on every track type, DRUM included. One
  continuous turn is one undo burst: **M3 hold** restores the pre-rotation pattern.
- Hold a **step pad** for a per-step `TRANS` offset (−12…+12). It is playback-only, never
  probability-rolled, and moves with SHIFT.
- Both TRANS and KLOCK are per track and survive `SEQ 4 + REC`.

## Traps

- **DRUM tracks ignore TRANS** (their "notes" are drum-map ids) and never lock.
- `Chrom` (chromatic) makes KLOCK a no-op — there is nothing to snap to.
- KLOCK is a two-state switch with no wrap: a continuous turn saturates rather than
  flipping back and forth.
- Generators bake raw notes, so a transposition moves the whole generated result; comp
  stabs, HARM live-follow and FOLLOW-resolved notes pick it up at send time.

See [Sequencer](../modes/seqmode.md) and [Bindings](../reference/bindings.md).
