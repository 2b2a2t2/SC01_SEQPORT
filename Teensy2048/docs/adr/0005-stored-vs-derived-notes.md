# 0005 — Stored notes vs notes derived at send time

**Status** — Accepted

## Context

Following a feeder, harmonising, walking a voicing and re-harmonising a melody all need to
change what sounds **without** changing what the player programmed. Rewriting the stored
steps would make the change destructive, would fight undo, and would mean a follower's
pattern drifted every time the feeder moved.

## Decision

A pattern stores notes. Some tracks **derive** what they play at send time:

- LEAD re-resolves a stored skeleton onto the feeder's chord via MAP.
- SKANK walks an inversion / extension / substitution per active step.
- HARM re-derives each active step's chord from the feeder's sounding note.
- COMP re-strikes the derived voicing.

Nothing derived is written back. Playback runs in passes so a follower always reads the
feeder's note for the current tick.

## Consequences

- Changing the harmony is non-destructive and reversible; a pattern stays the same object
  whether it is followed or not.
- A follower hears what the feeder **plays**, not what it stores — so mute, which gates only
  the send, must not stop the internal engine. A muted track keeps feeding.
- "Stored" and "derived" have to be documented per track type, because the pads always show
  the stored pattern, not what is sounding (HARM's live follow is the clearest case).
- Derived values that a player wants to keep per step (GNR8's OCT / BRW / PROB, SKANK's
  locks) had to become step fields — RAM only, like the rest of the step.
- Undo does not need to know about derivation: there is nothing written to undo.
