# Changelog

User-visible changes, newest first. The firmware has **no tagged releases** yet, so the
de-facto version marker is the size of the saved-settings block (see
[Compatibility](#compatibility)).

## Unreleased

### SEQMODE3 channel picker

- **Only `LENGTH` and `SPEED` take the step pads back from the channel selector** — those two
  change what the step row shows, so the pads return to the sequencer as before. Dialling
  `OCT`, `FOLLOW`, `INPUT CHANNEL`, `TRACK`, `TYPE` or `TEMPO` now leaves the yellow picker up,
  so several channels can be picked in one hold (`modes/seqmode.md`).

### Ableton Live bridge

- **A clip press with no link now says `Link Down Check MIDI`** instead of repeating
  `Live Link Off`, which read like the overlay closing; the overlay's own strings are
  `Live Mode On` / `Live Mode Off`. `modes/link.md` now separates the two pairs and names the
  surface's missing **Output** port as the usual reason a loaded script never answers.

- **`ENCABLETONMODE` — tap `NEXT` in ENCMODE and the instrument becomes a Live controller** —
  a latched overlay (like an ARP page, not a fourth mode family) with a companion remote
  script in `Remote Scripts/Teensy2048/`. The pads become the selected track's 16 **clip
  slots** (bright green playing · blinking green triggered · dim green has a clip · dark blue
  empty · dim red armed): press an empty one to **create and fill** it from that channel's
  Teensy pattern, a filled one to **launch**, a playing one to **stop**, double-press to
  **re-fill**. A fill is `patLen × 0.25` beats with notes at the stored pitch **+ 12 × octave
  + transpose**, duration `0.25 × gate%`, TIE ringing through the following empty steps,
  GLIDE +0.02 beat, velocity accent-aware and PROB as the note's probability. **PREV** (hold)
  turns the pads into the 16 **tracks**, **M3** swaps **F1–F4** to `FILL · SCENE · STOP ·
  ALL`, **F1/F2** page the parameter bank and **F3/F4** step the device. The knobs auto-map to
  the selected device's eight parameters on the **new CC block 64–71** — ENCMODE's CC 17–20 /
  CC 1–36 map, its dials and its per-CC-per-channel memory stay **byte-identical**, so every
  MIDI-learn map keeps working; a track with no device shows its mixer instead. Traffic is
  SysEx only (`F0 7D 37 32 34 38 … F7`, ≤ 32 bytes, chunked pattern dumps, HELLO retried
  every 200 ms for the first 5 s and then every second, with **any** frame back — not just the
  `ACK` — counting as the link being up), so a port that also carries Ableton's clock is
  unaffected; state-only, never clock. The companion script ships as-is and is never edited
  here: the phone app loads the same files, so bridge behaviour is fixed in firmware.
  `Link.h/.cpp` (new), `Keyboard`, `Bindings`, `Modes`, `Events`, `Display`, `SeqClock`,
  `Teensy2048.ino`, `Remote Scripts/Teensy2048/` (new), docs (`modes/link.md` new); patterns
  and overlay state are RAM only — **no EEPROM block change**.

### Sequencer

- **Layer panel: `MUTE` never has to be re-armed, and its key breathes** - the hold-`M2`
  panel opened on `MUTE` already, but tapping `C#` disarmed it and an erase cleared the arm,
  so a layer-by-layer pass kept dropping back to the report. `MUTE` is the **resting
  selection** now: `C#` only repeats `Mute layer?`, a white key mutes/unmutes and the arm
  stays, and `ERASE` hands the row back to `MUTE` after its confirm (a second `D#` also
  goes back to mute instead of nothing). The selected `MUTE` key is a **dim orange that
  breathes** - the panel was painted once when it opened, so the colour never moved; a
  30 fps repaint now runs while the row is up - and selected `ERASE` stays a static **dim
  dark red**. Muted layers themselves read dim red as before.

- **Overdub layers + the hold-`M2` layer panel** — every note a record pass writes now
  remembers **which pass wrote it**: up to **4 layers per track** (RAM only, like the
  patterns; DRUM steps stay untagged). A pass writes the **first layer that holds no notes
  anywhere**, so pre-existing content counts as layer 1, the first pass over a written
  pattern lands in layer 2, and a full set **stacks into layer 4**. **Holding `M2`** in
  SEQMODE 1/2/4 (non-DRUM edit track) opens a keyboard-row panel in the **M2 button's
  hue**: white **C D E F = layers 1–4** (bright = holds notes, dim = empty, dim red =
  muted), the black keys carry **C# = mute layer · D# = erase layer · F# = undo · G# =
  redo · A# = reset layers** — `MUTE` is armed whenever the panel opens and a white key
  confirms on that layer, `D#` arms erase for one confirm (see the note above),
  undo/redo/reset fire at once.
  **Mute** gates playback per layer (an all-muted step rests; a follower filters with the
  feeder's mask), **erase** deletes the layer's notes from every step (the rest compact,
  emptied steps lose their attributes, mirrors/HARM re-derive), **reset** flattens all
  layers back into one. Undo/redo walk **layer commands only** on an **8-deep** stack per
  direction and switch the edit channel to whatever they restore. The panel swallows its
  keys (press/release/click) so nothing leaks to the instrument, and closes on every M2
  release even across a mid-hold mode switch. `Keyboard` (`stepLay`/`layMute`/`recLay`,
  playback filter, overlay + stacks), `Events`, `Modes`; docs (`modes/seqmode.md`); no
  EEPROM change.

- **`OVERDUB` now works while `FOLLOW` is ON** - a follower re-resolves every step's
  pitches from the feeder, so notes stacked by a record pass were thrown away on the
  next lap and the knob only shaped what you heard with FOLLOW off. A step the pass
  stacked while `OVERDUB` = `ON` now **claims** itself: it plays its **own** stored
  notes exactly as it would with FOLLOW off (engine, FX, FScale, the PLAYBACK knob and
  the per-step `OCT`/`TRANS` all still apply), while every other step of the track keeps
  following the feeder. A plain record gesture (`OVERDUB` = `OFF`) never claims - it
  still gates the rhythm and the feeder still supplies its pitches, as before - and
  FREEZE plus the live HARM / SKANK refreshes lose to a claim. The flag rides in
  `stepAtt`, so a rotate, copy/paste and the undo snapshot carry it while a fresh
  gesture, a generator re-roll or erasing the step clears it; it is masked out of the
  armed-attribute LED pulse so a claimed step cannot light an FX target it does not
  carry. Patterns are RAM only, so no EEPROM change.

- **Live record gains `LISTEN` / `PLAY` / `OVERDUB` on the M1 step-edit row** (SEQMODE 1
  and 2, ENC2/3/4, per track, saved with SEQMODE4 + REC). `LISTEN` picks what the pass
  stores: `CHORD` (the gesture, as before) or `NOTE`, which writes the **live arp's own
  output** onto the step it sounds on — one note per step — instead of a single chord on
  the step where the gesture started; on a MONO track the gesture's key edges stop
  recording while `NOTE` is set. `PLAY` picks how stored steps sound: `GATE` (through the
  engine, as before) or `RECORDED`, which plays the stored notes **verbatim, all at once**
  with the engine bypassed (octave and per-step `OCT`/`TRANS` still apply; step FX, FScale,
  strum/HAND and COMP shaping are skipped). `OVERDUB` picks what a fresh gesture does:
  `OFF` replaces the step (as before), `ON` stacks notes onto it — and a MONO step that
  then holds several notes plays **all of them at once**. All three are clamped (4 counts =
  1 state, no wrap); a DRUM track shows `--`, and only the gesture you play feeds the
  recorder, never the track's own playback.

- **ENCMODE: the step pads are the channel picker in plain ENCMODE, not ENCMODE4** -
  the locked picker moved off the double-click variant, so entering ENCMODE now hands the
  16 pads to the encoder's MIDI channels (left header `CHANNEL SELECT`, bright yellow =
  the selected one, a press sets the channel and repaints the dials). **ENCMODE4 keeps the
  sequencer on the pads** - playhead, step states and triggers - so the CC dials can be
  worked while the pattern runs. Variant 3 is still the momentary hold picker, and
  ENCMODE4 + REC still saves the 36x16 CC values and channel.

- **ARP: the MODE knob now carries all nine walk orders** - `OUTIN` / `INOUT` /
  `CONV` / `DIV` existed only on the F1 `ORDER` knob, so with `ORDER = MODE` (the
  default) they were unreachable and the MODE knob stopped at `RANDOM2`. The enum
  grew past `RANDOM2` (`SEQ_ARP_OUTIN` .. `SEQ_ARP_DIV`), the knob and the value
  column follow it, and an older save's mode 0..4 keeps its exact sound. The ORDER
  knob's list is unchanged, so `RANDOM` / `RANDOM2` stay on MODE and `PLAY` still
  follows MODE for its shape.

- **SEQ 3: hold a bar pad for that bar's `OCT` / `TRANS`** - the keyboard's C D E F row
  already picks the 16-step edit window (and loops it on a second press); now the pad
  also arms an encoder row for **that bar**: `ENC1` = `OCT` (-2..+2), `ENC2` = `TRANS`
  (-12..+12 st), header `Bar 3 Transpose +5`. The stored chords are untouched - the
  shift folds into the track's own octave/`TRANS` at playback, so a `KLOCK` track
  snaps the transposed bar back into scale instead of stepping outside it. DRUM tracks
  refuse both (`Bar 2 Drum Track`): their steps are drum-map ids, not pitches. One bar
  at a time (the last pad pressed); the row drops when the last C-F pad lets go.

### Keyboard

- **KEYMODE4: the TONALITY page is four banks of eight** - `M2` opened a single
  four-knob row (OCT/SCALE/KEY/FSCALE), which is a shortcut, not tonality
  control. It now has four banks and `F1`-`F4` pick one while the page is up
  (the open bank's pad burns white, the other three drift through a slow dim
  hue cycle). Bank 1 TONAL CENTER (`KEY SCALE GRAV MOD MODTGT MODLEN FSCALE
  CADSTR`), 2 CHROMATICISM (`BORROW PREP SD MIX DIAT UNRES SURP STRICT`),
  3 VOICE LEADING (`PARAL BASS LEAP DOUBL CMNTN TOP LEAD SPREAD`), 4 STRUCTURE
  (`RATE ELIDE CADWIN CELL VAR ANTE MOTIF PHRASE`) - 26 new generator controls
  plus the six mirrors (KEY/SCALE/FSCALE/SURP/VOICE-LEAD/PHRASE), every one
  defaulting to a neutral value so the page never changes the sound until a
  knob is turned. Tonal gravity, modulation plan/target/span, cadence
  strictness, borrow budget, preparation, secondary-dominant density, modal
  mixture, diatonic bias, unresolved cost, functional strictness, parallel
  strictness, bass continuity, leap limit, doubling rule, common-tone weight,
  top-voice weight, register spread, harmonic rhythm, phrase overlap, cadence
  window, cell reuse, the variation operator, antecedent relation and motif
  length all reach the scoring, the apply pass or the variation pass.
  `Key4.cpp/.h`, `Bindings.cpp/.h` (four new sets), `Events.cpp`, `Display.cpp`,
  `Modes.cpp`.
- **KEYMODE4: M2 is a TONALITY menu, auto-advance moved to F4** - the M2 button
  carried the auto-advance toggle, which blocked the encoder-page slot the other
  modes use for their menus. `F4` **tap** now toggles auto-advance (a long press
  is still the audition-only hold), and `M2` toggles a sticky **TONALITY** page:
  `OCT / SCALE / KEY / FSCALE` on ENC1-4, the four knobs the PREV overlay otherwise
  hides behind a hold, `--` on the rest, `TONALITY` in the header's left slot. The
  M2 pad is a **slow hue cycle** - four times slower than the M3 APPLY marker, dim
  while the page is closed and brighter while it is open (KEYMODE4 now repaints at
  30 fps so both markers actually move; the M3 cycle was being sampled once per
  repaint before). The page dies with the mode, like the EMB row. `Bindings.cpp/.h`
  (`ENC_KEY4_MENU`), `Events.cpp`, `Display.cpp`, `Key4.cpp/.h`, `Modes.cpp`.

- **The keyboard on a `DRUM` track is a drum pad row** - when the keyboard channel
  points at a DRUM sequence the twelve pads stop being pitches: the scale filter, the
  KEY/SCALE colors and the `KEY 2` / `KEY 4` chord engines all stay out of it, and the
  row sits in one **dim OrangeRed**, blinking **dim green** on the pad whose note the
  playhead is sounding and reading **dim orange** on a voice muted in the ENCMODE drum
  overlay (mute wins over the blink; touch still flashes white). The keyboard **octave
  snaps to 1** on landing there - the octave the kit's own notes sit on (`C1` = 36
  through `B1` = 47) -and the octave is **per track**: every track keeps its own KEYMODE octave (default 3, a DRUM row defaulting to 1), so the snap and any knob turn on one track never move another, while the octave knob still transposes the drum row. Pressing a
  pad also **picks that drum voice** for the step edit, the direct form of hold `M1` +
  step N (`C` = part 1, `C#` = part 2 … `D#2` = part 16).

- **KEY 4 phrase templates are per-bar, so `AABB` / `ABAB` / `AABA` differ** - the
  restating templates used to copy the first half verbatim into the second, which made
  all three produce the same `AA` pattern and left only `ABAC` distinguishable. They
  are a four-letter map over bars now: a bar whose letter has already played replays
  the first bar that carried it, so `AABB` runs one cell for bars 1-2 and a repeated
  second cell for 3-4, `ABAB` alternates, `AABA` returns to the opening cell for its
  last bar, and `ABAC` repeats just bar 3 from bar 1. The replay goes through the
  normal write path, so the chain advances with it and the next new bar answers the
  repeated material; each bar's planned landing and any locked step are never
  overwritten, which keeps the cadence arc intact.

- **KEY 4: `PHRASE` promoted to the encoder row, `ERA` moved to the EMB page** - the phrase
  template decides whether the pattern has an arc at all, so it no longer sits two gestures
  deep (`NEXT` tap, then `ENC3`): it takes `ENC7`, the slot `ERA` used, and reads out by name
  (`FREE`/`AABB`/...). `ERA` keeps its value and now lives at the EMB page's `ENC3`, where a
  turn regenerates the unlocked steps immediately instead of waiting for the next row turn.
- **KEY 4 phrase templates** - `FREE` / `AABB` / `ABAB` / `ABAC` / `AABA` (EMB page
  `ENC3`) give the progression generator a **goal**: the cadence the knob selected is
  planned for the last bar, every earlier bar end is derived **backward** from it, and
  the forward pass scores those landings x8. `FREE` (the boot default) keeps the old
  causal chain. After M3/F1 the header says which layer decided the phrase end
  (`Plan V-vi PLAN|CAD|GENR`) - arc-level only, the scoring terms are not attributed.
- **Named readouts for two KEY 4 knobs** - `MOOD` reads DARK/COOL/MIXED/WARM/LIGHT and
  `CAD` its cadence pair (`V-I`/`IV-I`/`V-vi`/`ii-V`). The `CADENCE` knob is now a
  **four-band selector** (HALF `ii-V` under 25, PLAGAL `IV-I` under 50, AUTHENTIC `V-I`
  under 75, DECEPTIVE `V-vi` above): a seeded weighted roll left the type unchanged
  while the knob moved, which read as a dead knob. Its continuous value still scales the
  cadence terms.

- **KEY 3 `ENCOD` / `KBOARD` boot `locked`** — the encoder channel and the keyboard
  channel follow the SEQ edit channel (the hub) from power-up, instead of each keeping
  its own channel until you turn the link on. Turn the knob CCW to go back to `free`.
  A save written before this change holds no explicit choice, so it boots locked too;
  the first turn of the knob records yours.

### Documentation

- `README.md` split into a one-page landing page and a `docs/` tree — the old page was
  103,568 characters on 599 lines with no table of contents. Prose was moved verbatim; see
  [ADR 0008](docs/adr/0008-docs-split.md).
- New pages: [start here](docs/start-here.md), [glossary](docs/glossary.md),
  [cheat sheet](docs/cheatsheet.md), [panel map](docs/panel-map.md),
  [concepts](docs/concepts.md), [feature status](docs/status.md),
  [recipes](docs/recipes/index.md), [FAQ](docs/faq.md),
  [limitations](docs/limitations.md), [style guide](docs/style-guide.md),
  [bindings reference](docs/reference/bindings.md) and eight [ADRs](docs/adr/README.md).
- `AGENTS.md` now points at `docs/` as the reference instead of `README.md`.

No firmware behaviour changed in this entry.

## Feature timeline

The first commit whose diff mentions each surface, with the commit subject quoted as it was
written. Subjects often describe the broad change rather than the feature, so read the date
as "this is when it appeared", not as a release note.

| Date | Commit subject | Surface |
|---|---|---|
| 2026-09-25 | `first up` | 48-pad MPR121 scan, OLED, sequencer skeleton, USB-MIDI |
| 2026-09-28 | `scales update` | scale table and the chord engine (`Chords.cpp`) |
| 2026-11-01 | `Swing, pattern edit` | per-track swing style and amount |
| 2026-09-30 | `mono channel implementation` | per-channel step edit; the channel variants (`SEQ 3`/`4`, `ENC 3`/`4`) |
| 2026-10-03 | `64 steps` | 64-step patterns, bar loop, follow |
| 2026-10-06 | `chord progressions` | KEY 4 progression engine, 18 genre tables |
| 2026-10-07 | `Strumming` | strummed chords and the HAND strum zone |
| 2026-10-09 | `bassline generator` | GNR8 page (MONO/BASS) |
| 2026-10-20 | `minor bugs` | HARM harmonizer |
| 2026-10-21 | `MONO / HARM engine` | LEAD generator page |
| 2026-10-22 | `skank track engine` | SKANK walking voicings and COMP stabs |
| 2026-10-25 | `drum patterns` | 18 preset DRUM styles |
| 2026-10-26 | `more arp options` | ARP sub-pages; encoder dial grid; 21-character header lint |
| 2026-11-01 | `live input` | `EXT IN = LIVE`, `SRC = DIN/BOTH` |

## Compatibility

Saved settings are **backward-tolerant**: a block written by an older build loads, with the
fields it did not have left at their defaults. Fields are appended, never inserted. The
block size tells you which build wrote it.

| SEQ block | Added |
|---|---|
| 260 | earliest saved layout |
| 388 | GNR8 page parameters |
| 404 | LEAD contour |
| 420 | COMP `HITS` per track |
| 436 | TRANS offset |
| 532 | SKANK knobs `TES`…`SUB` |
| 564 | SKANK randomizers `TES%` / `NOT%` |
| 567 | KEY 3 `ENCOD` / `KBOARD` link flags |
| 616 | per-channel mode memory (KEY variant, F bank + page group) |
| 649 | LEAD CT blend, GD depth |
| 665 | per-track `Klock` (7-rung speed ladder) |
| 666 | 9-rung speed ladder marker |
| 923 | DRUM page state: genre, VAR, lane lines, per-lane FILL/MOT |
| 940 | DRUM kit map |
| 1262 | ARP sub-page parameters |
| 1296 | per-track swing style and amount |
| 1330 | per-track `EXT IN` and `SRC` |
| 1380 | per-track live-record options (`LISTEN` / `PLAYBACK` / `OVERDUB`) |

`ENC 4 + REC` is the exception: an older block is **ignored** and defaults stay, because a
half-loaded CC map is worse than a clean one.

## Release-note template

```markdown
## <date> — <what a player can now do>

### Play
- The three or four things that are new or different at the panel, in player language.

### Compatibility
- SEQ block: <old> → <new> bytes. An older save loads with <defaults>.
- Any gesture that changed, and what it was before.

### Known issues
- Open items from TODO.md that a player can hit, with the workaround.
```

Lead with the behaviour, not the files. Never claim a fix that was only compiled.
