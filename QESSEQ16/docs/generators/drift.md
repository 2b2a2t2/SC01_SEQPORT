_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# DRIFT (POLY, TYPE = DRIFT)
- **DRIFT = a second progression derived from an existing one**: a POLY+DRIFT track reads the
  **FOLLOW feeder's stored chords** — not its sounding notes — and cuts them into **blocks**: one
  chord plus how many steps it lasts. Consecutive equal chords merge, an empty feeder step keeps
  the chord before it going, and a chord outside the current key keeps its real root (the grammar
  only judges its in-key neighbours, so borrowed chords do not turn into nonsense). The blocks are
  fitted onto the DRIFT track's own pattern length and answered, then **written into its own
  steps** (`Drift.cpp`). Playback is the normal POLY path over those steps, so the pads show the
  derived progression, you can edit it by hand, and a third track can read it.
- **M3 tap derives and writes**, `M3` hold undoes (the shared one-level step snapshot). Under
  `RHYTHM = FILL` the write uses the track's own **FILL** (and `ALGO = EUCLID` for placement)
  exactly like the HARM bake, so a rejected step becomes a rest. **Editing the feeder** re-derives
  only the steps the track already uses — the steps that are on stay on, and a step whose chord did
  not move is left alone. **The feeder's speed never matters**: the map counts steps, not time.
- **M2 opens the DRIFT encoder page** (the click announces `Drift`, M2 breathes **violet**). Three
  F banks, each owning `ENC1..4`; `F4` is the **APPLY** gesture:
  - **F1 BASE** — `ENC1 = GENRE` (the grammar `REPLY` draws from), `ENC2 = FILL` (how many of the
    track's steps get a chord when `M3` writes, under `RHYTHM = FILL`), `ENC3 = HOW` (`SAME` /
    `WALK` / `REPLY`), `ENC4 = ANCHOR` (0–100 %: how often `REPLY` keeps the feeder's own chord
    instead of replacing it)
  - **F2 VOICE** — `SHAPE` (Close / Open / Drop) · `SIZE` (Triad / 7th / 9th) · `VOICE` (0–3
    octaves) · `INV` (`Nat` = nearest voice leading, or `1st`/`2nd`/`3rd` forced). These are the
    DRIFT track's own values; the HARM page's bank is a different set of knobs
  - **F3 FIT** — `ENC1 = SPAN` (`AUTO` keeps the feeder's rhythm of chord changes by scaling each
    block to the track's length; `1`–`8` forces that many steps per block and starts the
    progression again when the track is longer) · `ENC2 = READ` (`CHRD` decodes each feeder step's
    whole chord; `ROOT` takes its lowest note as the root and the quality from the current scale) ·
    `ENC3 = RHYTHM` (`BLOCK` / `FILL` / `STAB`, below)
  - A turn on any of these knobs re-derives the steps already in use, so the page is audible while
    it is open; `RHYTHM` is the exception (it decides which steps carry a chord, so it re-lays the
    pattern and takes the undo snapshot). Opening the page drops the `M1` edit radio (one
    radio, like ARP/BASS/LEAD/HARM/COMP/SKANK), and a channel / `TRACK` / `TYPE` / mode change
    closes it; `M2` again closes the page.
- **Three ways to answer the feeder**:
  - **SAME** — every step of a block plays the feeder's chord, re-voiced. Clear, no surprises; the
    way to hear the mapping.
  - **WALK** — the block's own chord on its first step, then chords that lead into the **next**
    block: on the last step of a block with room for three, the **dominant** of the next chord,
    otherwise a passing chord one scale step below it. This is what makes the answer move.
  - **REPLY** — the block's chord is a **related chord** instead of the feeder's: a root drawn
    through the current genre's transition table (the same 18-genre grammar KEYMODE4, HARM and
    SKANK use), biased toward the relative, the subdominant and the neighbours. `ANCHOR` is the
    chance to keep the feeder chord anyway, and the **last block of every cycle always comes home**
    to it, so each phrase ends where the feeder is. Inside a block, `REPLY` moves like `WALK`.
    The draw is **deterministic** (a hash of the block, the chord and a per-track seed), so the
    automatic update after a feeder edit produces the same answer; **`M3` bumps the seed**, so
    every write is a fresh answer.
- **`RHYTHM` decides which steps of a block carry a chord** (`F3 FIT`, `ENC3`), because `POLY` holds
  a chord only for its own gate and **cannot sustain across steps** — a block that re-strikes every
  step reads as a pile of stabs rather than one chord with air after it:
  - **BLOCK** — the chord lands on the **first step of each block only** and every other step is
    left empty. One chord per block, ringing for the step's gate; this is the setting that makes
    the derived progression read as harmony. A block that comes round again (a short feeder under
    `SPAN`) re-strikes on its head. `WALK`/`REPLY`'s chords inside a block have nowhere to go, so
    under `BLOCK` only the head chord is heard.
  - **FILL** — the default, and the old behaviour: the track's own `FILL%` (and `ALGO = EUCLID`)
    rolls which steps of the block land, exactly the HARM bake's rhythm. Scattered chords, more
    motion, `FILL 0` = nothing but rests.
  - **STAB** — every step of the pattern gets a chord: a hard rhythmic re-strike of the harmony
    from start to finish.
  `FILL` and `ALGO` only matter under `FILL`; `BLOCK` and `STAB` ignore them, because which steps
  land is the point of the setting. Changing `RHYTHM` **re-lays the pattern immediately** — it is a
  real write, so it takes the undo snapshot (`M3` hold restores the previous rhythm), and it keeps
  the current `REPLY` answer instead of re-rolling it, so `BLOCK` / `FILL` / `STAB` can be compared
  on the same progression. The other page knobs re-derive the steps in use without a snapshot. An
  automatic re-derive under `BLOCK` also empties the steps inside a block (it still never
  *activates* a step the player cleared), so a block stays one chord plus rests when a feeder edit
  moves its boundaries.
- **`F4 = APPLY` re-harmonises in place** — the pass a feeder edit runs, on demand. Every step the
  track **already uses** takes the chord the engine places there (the block's answer, `WALK` /
  `REPLY`'s lead, the current voicing), and **its activation and rhythm are left exactly as they
  are**: no step is switched on or off, a `BLOCK` track does not empty its block interiors, and no
  `FILL` roll happens. That is the workflow it exists for: program or record a rhythm first, then
  press `F4` to fit chords to the feeder's progression — and press it again after editing the
  feeder instead of waiting for the automatic pass. It keeps the current `REPLY` answer (no seed
  bump) and takes the undo snapshot, so `M3` hold restores the chords from before the press. The
  header reports `Fitted N Steps` (`No Feeder` when the feeder has no chords); the page must be
  open.
- **Voicing** happens in the track's own register (its `KEYMODE` octave + 24, the HARM convention),
  one chord per filled step. With `INV = Nat` each chord takes the inversion nearest the previous
  one.
- **Chains**: because the result is written, `A → DRIFT B → DRIFT C` works — B's stored chords are
  what C reads. `FOLLOW OFF` makes a DRIFT track read **its own** chords instead of the feeder, so
  it can answer itself. A DRIFT track **never mirror-follows**: unlike STEP/STRUM/HAND its active
  steps gate the rhythm but the notes come from its own written steps. A **SKANK** feeder's walked
  voicing is invisible to DRIFT: DRIFT reads stored chords, not send-time notes.
- **Timing, by example**: a 4-step feeder at `1/64` lasts 256 ticks = 16 bars, so each feeder chord
  lasts 4 bars. With the DRIFT track at **speed 1** (one bar per loop) and `SPAN = AUTO`, the four
  blocks get four steps each: the derived loop runs 4× per feeder chord and 16× per feeder
  progression — coherent, deliberately not locked to the feeder's clock. For the **time-locked**
  version leave the pattern at 16 steps, set `SPAN = 4` and the track's own speed to **1/16** (one
  step per bar): each derived chord then lasts exactly 4 bars, the same 16-bar arc. No new clock
  rule is involved — only the per-track speed that already exists.
- **Saved/settings**: the page knobs are **RAM only** (like HARM's), the written pattern is RAM
  only like every pattern, and an older build that reads type value `6` shows the track as `STEP`.
  `M3` hold restores the pattern from before the write.
- **LEDs**: **M2 = violet** (dim while the page is closed, breathing while it is open; touch still
  lights it white); **M3** carries the slow hue cycle of the generator button.

See [HARM](harm.md) for the inverse direction (bass line → chords), [SKANK](skank.md) for
send-time voicing, and [Concepts](../concepts.md) for stored vs derived notes.
