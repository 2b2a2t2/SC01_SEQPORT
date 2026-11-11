# Concepts

Eight ideas explain almost every behaviour in this instrument. If a page elsewhere seems
arbitrary, one of these is the reason.

## 1. Three families, four variants, one pad

Mode is a **family** (`ENC`, `SEQ`, `KEY`) crossed with a **variant** (1–4). The family
decides what the hardware is *for*; the variant decides which screen and which bindings are
live. Twelve combinations, one physical panel, no menus. See [Modes](modes/README.md).

## 2. Variant 3 is a place you visit, not a place you live

Holding the mode pad enters variant 3; releasing it leaves the momentary screen — `ENC 3` and
`SEQ 3` back into their own family, `KEY 3` back to whatever family it came from. This exists so
a rarely used screen (channel select, track config) never costs you your working state.
Everything else is sticky. `ENC 4` is the exception: it is a locked picker because its
whole job is to be left open while you address 16 channels.

## 3. Bindings are data, not code

The firmware resolves what an encoder, control, step pad or key does through four small
tables (`EncSet`, `CtrlSet`, `StepSet`, `KeySet`) picked by mode + variant. An **overlay**
pushes one of those tables aside for one group, temporarily. That is why every page,
held-key row and CC bank is one table entry rather than a branch in the event handler —
and why adding a mode is a table row, not a rewrite. See [Extending](extending.md).

## 4. One radio at a time

The step-edit radio (TIE / GLIDE / ACCENT / PROB) and any **M2 page** are mutually
exclusive. A page needs the encoder row and the step pads for its own purposes; the radio
needs them for attributes. The instrument refuses to guess, so opening one closes the
other. The same rule repeats inside pages: arming one F takes the whole row, and the armed
page dies when the page that owns it closes.

## 5. Stored notes vs derived notes

A pattern stores notes. Some tracks then **derive** what they actually play at send time:
LEAD resolves its skeleton onto the feeder's chord, SKANK walks a voicing, HARM re-derives
from the feeder's sounding note. Derived notes never rewrite the pattern, so changing the
harmony is non-destructive and reversible — and a pattern stays the same object whether it
is being followed or not.

The corollary: a follower hears what the feeder *plays*, not what the feeder *stored*.
Mute is an output gate only, so a muted feeder still feeds.

## 6. Feeder and follower

Any track can be read by another: point the reader's input channel at the feeder and turn
`FOLLOW` on. One level, non-recursive — A can follow B, but A is never read by C *through*
B. Chains are built by pointing each reader at a real track, not by daisy-chaining
listeners.

## 7. Three tiers of state

| Tier | What | Why |
|---|---|---|
| **Step content** | Notes, TIE/GLIDE/ACCENT/PROB, GNR8 OCT/BRW/PROB, SKANK locks | It is the music; it moves with rotate and dies with the step |
| **Track settings** | Length, follow, input channel, type, generator knobs | You want them back after a power cycle |
| **Global** | Key, scale, BPM, swing, accent velocity, chord settings | They belong to the instrument, not to a pattern |

Step content is RAM only. Track settings persist per channel with `SEQ 4 + REC`. Global
settings persist in their own blocks. Regenerating a part clears the step content of the
steps it writes — that is intended, and M3-hold is the way back.

## 8. Undo is a snapshot of one gesture, not a history

Generators, the drum page and the rotate knob each take a one-level snapshot: M3-hold
restores the pattern as it was before the last generate, re-roll or rotation burst. A
continuous knob turn counts as **one** gesture. There is no undo stack, so undo is a way
back from a mistake, not a way to explore.

## Where to go next

- How these ideas appear on the panel: [Panel map](panel-map.md)
- Which behaviours are settled: [Feature status](status.md)
- Why each decision was made: [ADRs](adr/README.md)
