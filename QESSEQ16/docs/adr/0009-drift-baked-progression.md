# 0009 — DRIFT writes its derived progression

**Status** — Accepted

## Context

DRIFT is a POLY track type that answers one chord progression with a second one. It needs the
feeder's progression as a whole (which chords, in which order, for how long), so it reads the
feeder's **stored** chords rather than its sounding notes. ADR 0005 says derived notes are
computed at send time and never written back, because writing would make the derivation
destructive, fight undo, and let a follower's pattern drift whenever the feeder moved.

A send-time engine would also store nothing, and then a third track would have nothing to read:
`A → DRIFT B → DRIFT C` would be impossible, because B would only exist while it was playing.

## Decision

DRIFT **writes** its answer into its own steps, under three conditions:

- The write is explicit (`M3` tap, or `F4` APPLY for the in-place re-harmonisation), never a side
  effect of playback, and it takes the shared one-level step snapshot first, so `M3` hold restores
  the pattern that was there.
- The automatic update that follows a feeder edit touches **only steps the track already uses**
  (never activating one) and **skips steps whose chord did not move**, so it is idempotent and
  converges instead of churning. The one exception is `RHYTHM = BLOCK`: that setting defines a
  block as one chord plus rests, so the update may empty a step inside a block — it still never
  turns an empty step into a chord.
- It reads the feeder's **stored** steps, so the result is a function of stored data on both
  sides, not of send-time state.

The rest of ADR 0005 still holds: LEAD, SKANK and the HARM live follow stay send-time, and
nothing else writes on the player's behalf.

## Consequences

- **Good**: the derived progression is visible on the pads, editable by hand, undoable, and
  readable by another track — chains work, and a DRIFT track can even follow itself.
- **Good**: playback needs no special path; a DRIFT track is just a POLY track whose steps were
  written by an engine, which keeps the send pipeline (FX, FScale, comp, mute, gate) shared.
- **Cost**: DRIFT is a *second* kind of derivation to document. The pads show stored notes, so a
  track whose feeder has changed but whose file has not — impossible here — would be the only
  way for the two to disagree; the automatic update closes that window.
- **Good**: the rhythm of the write is a setting, not an assumption. `BLOCK` (one chord per block),
  `FILL` (the HARM bake's roll) and `STAB` (every step) all write through the same path, so the
  stored pattern always says exactly which steps sound.
- **Cost**: the answer is a snapshot. A feeder edited while DRIFT is muted still updates, but a
  DRIFT track cannot answer a feeder's *performance* (probability misses, arp walks, a SKANK
  voicing) — only what the feeder stores. That is deliberate: it is what makes chains stable.
- **Cost**: the type byte is shared with the MONO transform, so DRIFT takes value `6` after
  SKANK's `5`; `4` stays reserved for a stale LEAD. An older build reading `6` shows `STEP`.
