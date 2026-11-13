# Loop a bar while you edit another

**Goal** — keep four bars playing while you rewrite a single bar of a different track.

**Before you start** — patterns on at least two tracks.

## Steps

1. `SEQUENCER 3`, on the track you want to work on. This **commits the keyboard row** to
   that track: the white keys `C–F` become bar buttons and the black keys carry the verbs.
   The row keeps working in every mode until you give it back.
2. Tap `C`–`F` to select the 16-step edit window. Tap the **selected** bar again to toggle
   its loop. The playhead **teleports** to the looped bar immediately — no waiting to walk
   there.
3. Edit steps in `SEQUENCER 1`. The loop keeps running.
4. Use the verbs: **`C#`** pattern length ÷2, **`D#`** ×2 (the new steps are filled from the
   old pattern), **`F#`** copy the track, **`A#`** paste it here, **`G#`** bar follow (the
   window chases the playhead).
5. Switch track with `SEQUENCER 3 → ENC6`, paste, and keep going.
6. To give the keyboard back: tap the `KEYBOARD` mode button (the latch drops at once), or
   select the keyboard's own track on `KEYBOARD 3`.

## What you should hear

The looped bar repeating under everything, with your edits appearing on the next pass.
Disabling the loop resumes at the position the transport *would* have reached — the shadow
position kept advancing while you were looping.

## Variations

- **Copy/paste is the whole step table**: notes, attributes, FX, GNR8 modifiers, strum
  onsets, drum masks, SKANK locks, plus length and speed. The destination keeps its *sound*
  (kit, FILL/MOT, genre, swing), so the same phrase plays on a different instrument.
- Paste claims the one-level undo slot — **M3 hold** brings the replaced pattern back.

## Traps

- Pressing a bar button or a verb **swallows the pads**: nothing sounds from them while the
  row is committed. That is deliberate, so a bar press never leaks a MIDI note.
- Bar loop is per track; each track keeps its own loop state and pattern length.

See [Sequencer](../modes/seqmode.md).
