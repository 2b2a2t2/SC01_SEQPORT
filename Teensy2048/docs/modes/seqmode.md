<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# Sequencer Overview (SEQMODE)

**Encoder feel**: one detent is **4 counts**. Knobs whose value is a wide range — every **0–100%** dial (GNR8 FILL/ACC/GLIDE/TIE, the LEAD CT blend, the ARP sub-page percentages, GATE, PROB, the eight SKANK knobs, DRUM FILL/MOT and the per-lane FILL/MOT) — go through `encDrain()` in `Encoders.cpp` at **1 count = 1 unit**, the same rate as the ENCMODE **CC knobs**, so **one detent = 4% and a full 0–100 sweep is about one turn**; a spin (the library hands a fast turn over as a bigger delta per poll) is worth up to **4×** that. Small enums (MODE, SPAN, TRACK, GENRE …) stay 1 step per detent, and the CC knobs themselves stay 1 count = 1 value. Two enums are the opposite: **FOLLOW** and the three **live-record knobs** (`LISTEN` / `PLAY` / `OVERDUB`) take **4 counts = 1 state**, so a value cannot be skipped by a fast turn.

**Playhead** advances on MIDI clock (24 PPQN external or internal master at tempo). Up to 64 steps per track (four 16-step bars, `LEN` on `SEQ 3`), 16 tracks (one per MIDI channel 1–16).

## Sequence Configuration
- **ENC1**: Playback octave shift (±5 octaves from stored notes, clamped to –2..+8 convention)
- **ENC2**: Pattern length of the edit sequence (**0–64** steps, boot default 16; 0 = silent track, per-track RAM only)
  - **Steps past the length go dark and inert** — no record, no toggle, no attribute edit, so a shortened pattern can't hide stale content. An armed **bar loop** still covers its own 16 steps, so those stay lit and editable
- **ENC3**: **FOLLOW** — three-state `OFF · ON · FREEZE` (4 counts = 1 state, **clamped at both ends — no wrap**). **OFF** = the track plays its own stored line. **ON** = its active steps still gate the rhythm, but the notes re-resolve live from the feeding track's current bar (the input channel on ENC5). **FREEZE** = keep the chord **that was sounding when you turned the knob**: `seqFollowModeSet` latches the feeder's current chord/note right then (`seqFollowFreezeCapture`) and **replays it unchanged** even as the feeder walks on — its own steps still gate *when* it plays, only *what* it plays is frozen. Leaving FREEZE (to ON or OFF) drops the latch, so every entry recaptures; if the feeder has nothing sounding at that instant the latch stays empty and the follower captures on its first frozen active step instead (also the boot path, since a restored FREEZE track starts unlatched). FREEZE counts as following for every gate that checks FOLLOW, so HARM/SKANK/arp-style listeners behave identically except that the resolved chord is held. A follower copies its feeder's timeline into a step, so while the **feeder is empty** a step press has nothing to copy and is refused with `Feeder track empty` in the header — feed the input track (or play a chord and press again) and the step lights. Survives SEQMODE4+REC (a 0/1/2 mode per track in the SEQ EEPROM block; older ON/OFF saves still load). **An OVERDUBBED step is the one exception to "the feeder supplies the pitches"**: a step the record pass stacked while `OVERDUB` was `ON` **claims** itself and plays its **own** stored notes, exactly as it would with FOLLOW off (engine, FX, FScale, the PLAYBACK knob and the per-step `OCT`/`TRANS` all still apply) — every other step of the track keeps re-resolving from the feeder. A plain record gesture (`OVERDUB` = `OFF`) never claims: it still gates the rhythm and the feeder still supplies its pitches, as before. FREEZE and the live HARM / SKANK refreshes lose to a claim too — the point of the knob is that what you played is what sounds. The claim is step content: it rides in `stepAtt`, so a rotate, copy/paste and the undo snapshot carry it, while a fresh gesture with `OVERDUB` off, a generator re-roll or erasing the step clears it (it is masked out of the armed-attribute LED pulse, so a claimed step never lights an FX target it does not carry)
- **ENC4**: Pattern **speed** of the edit sequence — step rate relative to the master clock: **1/64, 1/32, 1/16, 1/8, 1/4, 1/2, 1 (default), 2, 4** (the two slowest rungs are one step per 32 / 64 master ticks — a step can span several bars)
  - Per track (polymeter + polyrhythm): a slow track skips master ticks until its accumulator fills, a fast one plays 2–4 sub-steps inside one tick; the gate time scales with the track's own step length
  - Saved/restored with SEQMODE4+REC. Clock **SYNC** moved to **ENCMODE3+PLAY**
- **ENC5**: Input channel (1–16) — notes arriving here feed pending chord; changing backfills from that track
- **ENC6**: Track type — **DRUM** / **MONO** / **POLY**
  - Ch1 defaults DRUM, ch2–16 default POLY
  - A **DRUM** track lights **M2 indigo** in SEQMODE 1/2/4 — click it for the [DRUM pattern page](../generators/drum-styles.md). Every page track has its **own M2 hue** (DRUM indigo · ARP pink · BASS blue · LEAD sea green · STRUM/HAND lavender · HARM aqua · COMP pale sky · SKANK spring green), so the pad name needs no reading — see [page pad colors](../reference/leds.md#encoder-page-pad-colors)
- **ENC7**: MONO transform — **ROOT** (chord root), **BASS** (lowest note), **ARP** (ascending arpeggio), **STRUM** (the whole chord raked inside the step), **LEAD** (the generated melody line: its own steps gate the rhythm, the notes re-resolve onto the feeder chord via FOLLOW + MAP `ct`/`gd`)
  - Selecting **ARP** lights **M2 pink** in SEQMODE 1/2/4 — click it for the [ARP options](../generators/arp.md)
  - Selecting **BASS** lights **M2 blue** in SEQMODE 1/2/4 — click it for the [GNR8 generator](../generators/gnr8.md)
  - Selecting **LEAD** lights **M2 sea green** in SEQMODE 1/2/4 — click it for the [LEAD generator](../generators/lead.md)
  - **STRUM** sends the stored chord's notes ascending, one after another across the step (stagger ≈ 8–40 ms each, scaling with the step), every note-on cutting the one before it and the last held to the gate — a quick strum instead of one block chord. TIE, GLIDE, PROB and the gate behave as on the other types; TYPE is per track and saved with SEQMODE4+REC
- **ENC7 on a DRUM track**: a drum has no mono/poly subtype, so this slot — dead there until now — carries the kit **MAP**: `CUSTOM` (the drum engine's own kit, boot default) or `GM` (General MIDI percussion). Two-state, **no wrap** (CW = GM, CCW = CUSTOM, like KLOCK); a change re-bakes the pattern onto the new map as one undo burst. This is the only knob that sets it - the pattern row's `ENC5` is the plain gate again
- **ENC7 (POLY track)**: playback TYPE of the whole track — **STEP** (block chord per step — optionally rhythmically comped, see the [COMP page](../generators/comp.md)), **STRUM** (auto-strum: every step rakes ascending at a fixed ≈ 8–40 ms stagger scaling with the step), **HAND** (see [HAND keyboard](../generators/hand-strum.md) — steps buffer their chord silently, the keyboard strums the buffer), **HARM** (see [HARM page](../generators/harm.md) — the track is a harmonizer: M3 bakes a chord progression inferred from the FOLLOW feeder's bassline; plays like STEP), **SKANK** (see [SKANK page](../generators/skank.md) — the track derives each step's voicing live from the source chord: a walking inversion/extension/substitution engine). No wrap; value 4 is reserved and never surfaces (the MONO type byte is shared — a stale LEAD reads as STEP), per track, RAM only — a stale value normalizes to STEP at boot
- **ENC8**: Tempo 40–240 BPM (INT sync only)
- **Step-pad channel selector**: while SEQ is held into SEQMODE3 the 16 steps are the edit-channel selector (dim yellow, selected channel bright yellow). **Any encoder movement dismisses it** — the pads immediately show the normal sequencer again (playhead, recorded steps, pattern length) and stay that way until SEQ is held into SEQMODE3 once more
  - **Choosing a channel always returns to step edit**: the edit radio switches back on with **M1 (STEPS)** selected on the newly selected channel, whatever attribute it had on the previous one

## Track Behaviors
| Track | Playback | Step Recording |
|-------|----------|----------------|
| **DRUM** | Each step holds drum parts (voices C1–D#2). Playhead sends all active parts on channel. | Press step = toggle selected part (M1 hold = part-select overlay) |
| **MONO** | One note per step: root/lowest/arp of stored chord + sequence octave shift | Records pending chord; MONO also follows **input track's timeline** (last active step at/before position) |
| **POLY** | Full chord per step + sequence octave shift + per-step FX (sus/ext/color) + **PROB gate** + **ACCENT** velocity | Records pending chord; if input≠own, prefers **held gesture** (keys/latched chord/ext notes) over timeline |

**Attribute Support by Track:**
| Attribute | MONO | DRUM | POLY |
|-----------|------|------|------|
| TIE | ✓ | — | — |
| GLIDE | ✓ | — | — |
| ACCENT | ✓ | ✓ (per-voice) | ✓ (per-step, uses track accent velocity) |
| PROB | ✓ | ✓ (per-voice mask) | ✓ (per-step, re-reads track probability live) |

## Step Recording
- **Press unassigned step** → silently records current *pending chord* into that step
- **Refused press** → when there is nothing to record the header says why instead of leaving the pad dark: **`Feeder track empty`** (the track reads its input track on ENC5 and that track has not been fed yet — the usual case on a follower) or **`No notes to record`** (nothing held/pending and no timeline to mirror)
- **Hold step** → assign mode: play keyboard keys to **replace** that step's chord
- **Click active step** → disables it
- **Pending chord sources**: keyboard (when its channel matches), external MIDI on sequence's input channel

## Live Recording
- **REC** arms one record pass over the edit track's pattern from its **next 1st bar** (or the looped bar's head when a bar loop is armed); the pass covers the whole play range and then switches itself off (`rec done`)
- **LED**: dim dark red = idle, **bright red** = armed / recording (touch = white)
- The keyboard channel must match the edit track — otherwise arming is refused (`kbd ch mismatch`); changing the keyboard channel mid-record disarms
- Each gesture writes the step under the playhead: **POLY/MONO replace** (fresh gesture clears step + attributes, held notes build the chord), **DRUM overdubs** (notes are added, never removed, duplicates skipped)
  - **POLY gestures are anchored**: all notes of one gesture (first touch → all keys up) land on the step where it **started** — on a HAND step each pluck carries its real onset (ms since the first touch, u16), so a long strum replays exactly as performed across later steps/bars
- Press **REC again** = cancel (`rec off`); **transport stop** interrupts the pass and re-arms it for the next start
- **PLAY starts in SEQMODE or KEYMODE4** — start the transport there first (or feed an external clock), then switch to KEYMODE to arm and play

### Record options — `LISTEN` / `PLAY` / `OVERDUB`

Three knobs on the **M1 step-edit row** (`ENC2` / `ENC3` / `ENC4`, SEQMODE 1 and 2). They
live where the row's `GATE` / `VEL` / `PROB` live: **the step-edit radio must be armed**,
the plain track view must be showing — an open page that owns those slots (GNR8, LEAD,
HARM, COMP, SKANK, DRUM, ARP) keeps them while it is open. Each knob is **4 counts = 1
state, clamped at both ends — no wrap**, **per track**, and saved with **SEQMODE4 + REC**
(the SEQ EEPROM block grew **1330 → 1380 bytes**; a save from an older build still loads
and keeps `CHORD` / `GATE` / `OFF`).
A **DRUM** track shows `--` and does not turn them; its record behaviour is the overdub it
has always had.

- **`ENC2 LISTEN` — what a record pass listens to**
  - **`CHORD`** (default): the **gesture** — key edges, the held chord, a latched chord, notes arriving on the input channel. One write per touch, as before.
  - **`NOTE`**: the **arp's own output**. Each note the live keyboard arp sounds is written to the **step it sounds on**, one note per step, so the line you hear is the line that gets stored instead of one chord on the step where the gesture began. On a **MONO** track the gesture's key edges stop recording altogether (the arp output is the only thing written); the arp output is captured only from the gesture you play — the track's own stored-step playback is never fed back into the recorder. On POLY / DRUM the knob has no arp to listen to and is inert.
- **`ENC3 PLAY` — how stored steps play back**
  - **`GATE`** (default): notes run **through the engine** as always — MONO's ROOT / BASS / ARP / STRUM / LEAD pick, POLY's per-step FX, FScale, HAND buffering, COMP shaping.
  - **`RECORDED`**: the stored notes sound **exactly as recorded**, **all at once**, with the engine bypassed. The track's playback octave and the per-step `OCT` / `TRANS` still apply; step FX, FScale, strum/HAND onsets and COMP shaping are skipped, and a MONO track's BASS/LEAD register fold is skipped too.
- **`ENC4 OVERDUB` — what a fresh gesture does to a step**
  - **`OFF`** (default): a fresh gesture **replaces** the step (and clears its attributes) — today's behaviour.
  - **`ON`**: notes **stack** onto what the step already holds, duplicates skipped — the DRUM behaviour, now on MONO and POLY. A MONO track whose step holds several notes then plays **all of them at once** (the track stops being mono for that step) instead of the engine picking one.
  - `ON` never clears: a record pass re-prints cleanly on an empty track and stacks on a filled one.
  - **A claimed step beats FOLLOW**: while the track follows, every other step still re-resolves its pitches from the feeder, but a step stacked with `ON` keeps its own notes and plays them (see [ENC3 FOLLOW](#sequence-configuration) above) — which is why overdubbing is audible with FOLLOW on and not only with it off.

The header echoes each change full-width: `Listen Chord` / `Listen Note`,
`Playback Gate` / `Playback Recorded`, `Overdub Off` / `Overdub On`.

### Overdub layers — hold `M2`

A record pass with `OVERDUB` = `ON` stacks notes onto what is already there, but until now
there was no way to take a single pass back out again. **Layers** give every note the
identity of the pass that wrote it: up to **4 layers per track**, shown and managed from a
keyboard-row overlay opened by **holding `M2`** in SEQMODE 1/2/4 (the panel wears the
**same hue as the M2 button**, i.e. the track's `seqPageColor()`; DRUM tracks have no
layers — their steps gate per voice — so the hold does nothing there, and SEQMODE 3 keeps
its own M2 jobs).

**How a layer gets picked** — a layer is **one record pass over the whole pattern**. When
the pass starts it writes the **first layer that holds no notes anywhere** in the pattern,
so pre-existing content counts as layer 1 (the first pass over a written pattern lands in
layer 2) and a set where all four layers are full **stacks into layer 4**. Every note of
the pass carries that tag (`stepLay`, RAM only — patterns are never stored, so neither are
layers); DRUM steps stay untagged.

**The panel** (while `M2` is held — release closes it):

| Key | Job |
|---|---|
| **C · D · E · F** (white) | layers **1 · 2 · 3 · 4** — a press **confirms** the selected command on that layer: `MUTE` toggles that layer's playback, `ERASE` deletes its notes |
| **C#** | **(re)select mute layer** — the resting selection, it never disarms (`Mute layer?`) |
| **D#** | arm **erase layer**; pressing it again goes back to mute (`Erase layer?`) |
| **F#** | **undo** the last layer command (`Undone` / `Nothing to undo`) |
| **G#** | **redo** it (`Redone` / `Nothing to redo`) |
| **A#** | **reset layers** — flatten the whole track back to one layer (`Layers flattened`) |
| **G · A · B** | nothing assigned (swallowed while the panel is up) |

`MUTE` is the **resting selection**: the panel opens on it, a white key mutes or unmutes
that layer and the arm **stays** — so four layers mute in four presses, no re-arming in
between, and tapping `C#` again only repeats `Mute layer?`. `ERASE` is the one command that
has to be picked: `D#` arms it (`Erase layer?`), the next white key erases that layer, and
the row **falls back to `MUTE`** (pressing `D#` again goes back to mute without erasing).
The white keys and black commands swallow their press, release and click, so nothing leaks
to the instrument even if the overlay closes mid-hold.

- **Mute layer** gates **playback only** on this track: the layer's notes drop out of every
  step (a step whose notes are all muted **rests**), and notes already sounding ring out to
  the next step boundary — same granularity as the track mute. A follower reads the
  **feeder's** mute mask and tags, because that is where its chord came from.
- **Erase layer** deletes that layer's notes from every step of the track: the remaining
  notes compact in place (strum onsets and tags travel with them), a step that empties
  loses its attributes with it, and mirrors / HARM re-derive follow. Nothing to erase = no
  change.
- **Reset layers (flatten)** sets every tag back to layer 1 and clears the mute mask, so
  the pattern reads as a single base layer again; an already-flat track reports but does
  not change.
- **Undo / redo** walk **layer commands only** (mute, erase, flatten — never ordinary step
  edits), on an **8-deep** stack per direction. Undo and redo each move the current state
  onto the other stack and apply the top of this one, switching the edit channel to
  whatever they restore.

**LEDs**: the whole keyboard row turns into the panel — the four white keys wear the
track's page hue **bright when the layer holds notes, dim when empty, dim red when
muted**; the five black keys sit dim in the same hue except the **selected** one, which
wears its own colour — `MUTE` a **dim orange that breathes** (~1 s, the only moving light
in the row), `ERASE` a static **dim dark red**; `G A B` sit dark and the gap LEDs go black.
Touch still flashes white, and the panel wins over every inherited key colour while it is
up.

Like the patterns they tag, layers and the undo stacks live in **RAM only** — no EEPROM
change, gone on power cycle.

## Bar Loop
- **Press C/D/E/F bar button** (on step pads in SEQMODE3): first press selects that 16-step edit window; **press again on the SELECTED bar toggles its loop**
- **Loop activation**: playhead **teleports immediately** to the looped bar and starts looping it (no waiting to "walk" there)
- **Loop disable**: playhead resumes at the global position it *would have reached* without the loop (shadow position continues advancing silently during the loop)
- Each track has independent loop state and pattern length (polymeter)
- **Hold a bar to shift it**: while a C-F pad is down the encoder row becomes
  **that bar's** playback shift - `ENC1` = `OCT` (-2..+2, the step `OCT`'s range),
  `ENC2` = `TRANS` (-12..+12 st), the header echoing `Bar 3 Transpose +5`. The stored
  chords are never rewritten: the shift folds into the track's own octave/`TRANS` at
  playback, so a `KLOCK` track snaps the transposed bar back into the scale, and a
  DRUM track refuses both (`Bar 2 Drum Track`) because its steps are drum-map ids,
  not pitches. One bar is under the knobs at a time (the last pad pressed); the row
  goes when the last C-F pad lets go. RAM only, like the per-step fields

## Track Verbs
- The **five black keys** of the bar octave carry the per-track verbs - C# - D# - F# - G# - A# (pads 37-41). They are swallowed exactly like the C-F bar buttons, release included, so a press never leaks a MIDI note, and they work on **every track type**:  Their LEDs are a second row, not part of the keyboard: the length pair pulses light blue (**C# dim, D# brighter and faster** - one colour, two intensities; both pulses stay dim, well under a bar LED), copy / paste sit dim MediumSeaGreen, and follow is dim green that breathes brighter green while it is **ON**.
  - **C# = pattern length /2** (never below 1 step, so a press cannot silence a track; a track already at length 0 stays silent) and **D# = pattern length x2** (0 -> one bar of 16, clamped at 64). One press instead of a wrist turn on ENC2; the header echoes `Pattern 32 Steps`, the pads repaint, and the 16-step edit window is pulled back onto a bar that still exists (`harmFeederChanged` marks HARM feeders like the ENC2 dial does). The steps the doubling opens up are filled from the old pattern - 4 chords on steps 1-4 of a 4-step pattern repeat on 5-8, and a full bar becomes a two-bar loop - so a press never leaves the new steps empty (a length of 0 has nothing to copy; a length already at the 64 ceiling does not grow).  The copy is the same step table F# / A# move (notes, attributes, FX, modifiers, strum onsets, drum masks, SKANK locks), and the press claims the undo slot, so a M3 hold puts the shorter version back.
  - **F# = copy** the edit track's pattern into a **one-slot RAM clipboard** (`Copied 24 Steps`, `Nothing to Copy` when the track is empty); **A# = paste** it into the edit track (`Pasted 24 Steps`, `Clipboard Empty` when nothing has been copied yet). Copy and paste meet in the middle: switch channel with the step pads or TRACK with ENC6, then paste.
  - A paste writes the **whole step table** - notes and chord lengths, the attribute byte (TIE/GLIDE/ACCENT/PROB and the POLY step-TYPE override), the FX byte, the KEYMODE4 embellishment params, the per-step OCT/BRW/PROB/TRANS modifiers, the HAND strum onsets, the DRUM per-voice accent/probability masks and the SKANK locks - over all 64 steps, and the track's **pattern length** and **speed** with them - a pasted line loops at the length it was written for and runs at the rate it was built at. What stays is the destination's **sound** (FILL/MOT, genre, kit map, TRANS, swing, channel), so the same phrase plays on a different instrument; it claims the one-level undo slot, so **M3 hold** brings the replaced pattern back (SKANK locks, length and speed included). The clipboard lives in RAM like the patterns it holds - it does not survive power.
  - **G# = bar follow**: that track's 16-step edit window **chases the playhead** - the bar moves with the green dot instead of staying on the bar you picked, so the pads always show what is sounding; switching it on jumps the window onto the bar playing right now (`Bar Follow On` / `Bar Follow Off`). Per track, RAM only, and the C-F bar buttons still pick a bar (the playhead then re-grabs it).
  - **The row stays on duty**: the moment you pick a bar or press a verb, the keyboard **commits** to that track's row - the C-F bars and the black-key verbs keep working after SEQ is released, and in every mode, so you can walk bar to bar (and copy, paste, half, double) without re-holding SEQ each time. The pads stay swallowed while it is on (nothing sounds from them) and the row keeps its colours. It is let go only by selecting the keyboard's own track on **KEYMODE3** (`Channel N`) - that gesture is what says "be an instrument again".

## Step Edit Radio
- **M1 (edit)**: off = pads record steps, on = pads edit attributes on ACTIVE steps (PROB arms the gate; ENC8 sets the **track** probability that gate re-reads live every hit)
  - **MONO**: click cycles OFF → STEPS → OFF; **F1–F4** pick TIE / GLIDE / ACCENT / PROB; ENC5/6 shape gate/velocity — turning the radio **on** closes the GNR8 page if it is open (mutually exclusive with M2 GNR8)
  - **DRUM/POLY**: quick M1 tap toggles edit (on reuses ACCENT/PROB, default ACCENT); **F3 = ACCENT, F4 = PROB** (click selected again = off); M1 **hold** on DRUM still opens the part-select; ENC8 = track probability
- **POLY step FX (F1 = `FX`)**: on a POLY track with the radio on, **F1** switches the target to **FX** (the control legend reads `FX`, the ENC7 slot reads `FX` and shows the current type; F1 again closes the radio). **ENC7** cycles the selected type with wrap: **sus2 · sus4 · 6 · m7 · M7 · 9 · color** — the header names every turn. Tap a step pad to toggle the selected FX on that step; the yellow attribute pulse shows only the steps carrying the **currently selected** type, and **F1 blinks while the track holds any FX**. **sus2/sus4 are mutually exclusive** (arming one clears the other); the extension types can stack with them. FX live only with the pattern (RAM, by design — patterns are never stored)
  - **Playback (POLY only — MONO/DRUM steps are never transformed)**: **sus2** replaces the chord's third with a second, **sus4** with a fourth; **6 / m7 / M7 / 9** add the extension on top of the stored triad; **color** appends a quiet ghost note a **half step below the root** (half velocity, half delay, sent raw — a chromatic color tone that bypasses the scale snap). FX are applied at send time — the stored notes stay untouched
- **POLY step TYPE (F2 = `TYPE`)**: on a POLY track with the radio on, **F2** switches the target to **TYPE** (legend `TYPE`, ENC7 slot reads `TYPE` + the armed value; F2 again closes the radio). **ENC7** cycles the armed value with wrap: **STEP · STRUM · HAND**. Tap a step pad to **stamp** that override onto the step; **tap again = clear** back to inherit-the-track (`… type inherit`). The yellow pulse shows the steps carrying an override and **F2 blinks** while any step of the track has one. The override wins over the track default (ENC7 in SEQMODE3) for that step's playback only
  - **step TYPE playback**: **STEP** = the plain block chord (as always); **STRUM** = auto-strum — that step rakes ascending at ≈ 8–40 ms stagger scaling with the step; **HAND** = the step buffers silently for the strum — an explicitly recorded gesture (stored timings) still replays as performed (see [HAND strum](../generators/hand-strum.md)), regardless of the track default
- **LED**: M1 bright white while no attribute is selected, selected F-pad white (on POLY all four F-pads are radio pads: FX / TYPE / ACC / PROB — DRUM and MONO follow their own subsets), all radio pads carry a dim dark-blue background (F-pads blink green while the playhead reads an attributed step)
- **Accent velocity (per track, boot 101, 1–127)**: an accented step sends this instead of a hard-wired 127 — applies to MONO, POLY and the DRUM step/per-voice accent alike (the other notes keep their own velocity)
  - **Hold F3** in SEQMODE 1/2/4: the ENC6 row switches **VEL → ACC** and ENC6 edits the edit track's accent velocity (header shows `Accent Velocity`); release restores ENC6 to VEL
- **DRUM ENC7 = `MAPPING`** (SEQMODE3 on a DRUM track - the TYPE slot, which a drum has no use for): the **kit map** switch — **`CUSTOM`** (the default, see [DRUM Part Sequencing](drum.md): C1 bass drum, C#1 hand clap, D1/D#1 hats, E1 snare, F1–G1 congas, G#1–A#1 toms, C2 cow bell, C#2 claves, D2 maracas, D#2 cymbal) or **`GM`** (General MIDI percussion: 36 kick, 37 rim, 38 snare, 39 clap, 40 e.snare, 41/43 floor toms, 42/44/46 hats, 45/47/48/50 toms, 49 crash, 51 ride). Two-state, **no wrap** (CW = `GM`, CCW = `CUSTOM`, saturating like KLOCK/ENCOD); the header echoes `Kit Map CUSTOM` / `Kit Map GM` and the OLED shows `MAP` with the current value. Switching **re-bakes the track's steps onto the new map** (one undo burst), so a preset pattern lands on the right sounds of either kit. Per track, saved with SEQMODE4+REC. The map lives on SEQMODE3 only: on the pattern row a DRUM track's `ENC5` is its gate, as before
- **Probability (per track, boot 100, 0–100%)**: ENC8 edits the edit track's own playback probability. Every step carrying **PROB** re-reads it on each advance, so a turn changes the odds from the very next hit — no re-toggling the attribute. On DRUM the mask still picks *which* parts are gated, only the % is per track
  - Saved/restored with SEQMODE4+REC
  - F1–F4 in SEQMODE are radio keys only — they no longer trigger chord extensions there (DRUM/POLY with the radio off used to leak into the KEY behaviour); the one extra job is POLY's **F1 = FX target** and **F2 = TYPE target** above

## Shift / Transpose
- **PREV (hold) = SHIFT/TRANS encoder row** — on **any** SEQMODE variant (GNR8, ARP, LEAD, HARM, COMP and the plain track view alike; the previous GNR8-only rotate row now lives here):
  - **ENC1 = SHIFT** — rotates the whole pattern **4 counts = 1 step** (CW = content delays one step, wrapping at the pattern length — notes, attributes, FX, strum timings, drum masks **and the per-step OCT/BRW/PROB/TRANS values** all move together). The header shows the rotation position (`Pattern Start N`); a continuous turn is one undo burst (M3 hold restores the pre-rotation pattern). **SHIFT is a real data move** (inherently destructive to the pattern) and works on every track type including DRUM.
  - **ENC2 = TRANS** — the edit track's **semitone transpose** of its playback, **1 count = 1 st**, clamped **−12…+12**, no wrap; header `Transpose +k` / `Transpose −k`. TRANS is **non-destructive**: the stored steps never change, the playback offset is applied after the fold/octave shaping (the same property ENC1's octave shift has). **DRUM tracks ignore it** (the turn does nothing). Generators bake *raw* notes (the offset then moves the whole result), comp stabs / HARM live-follow / FOLLOW-resolved notes pick it up at send time, and it survives SEQMODE4+REC (per-track field of the extended SEQ EEPROM block, backward-tolerant like the rest).
  - **ENC3 = KLOCK** — per-track **scale-lock** for the TRANS offset, a **two-state switch with no wrap** exactly like the KEYMODE3 `ENCOD`/`KBOARD` switches: **CW = `Scale Lock on`**, **CCW = `Scale Lock off`**, so a continuous turn saturates at the end instead of flipping back and forth on every count (the header/value column only repaint when the flag actually changes). When **ON** the transposed playback note is snapped back to the **nearest in-scale pitch** (down first, then up), so the −12…+12 semitone knob stays **diatonic** under the current KEY + SCALE. Non-destructive like TRANS (stored steps untouched; the snap is a playback-time transform), applied *after* the octave + TRANS offset on every playback path (POLY, MONO incl. STRUM, and FOLLOW-resolved notes). Chromatic (scale `Chrom`) is a no-op. **DRUM tracks never lock** (their "notes" are drum-map ids). Turning the scale recolors the transposed line live. Survives SEQMODE4+REC (per-track flag of the extended SEQ EEPROM block).
- **Hold a step pad = per-step TRANS overlay** (non-DRUM tracks): the encoder row swaps to `TRANS` on **ENC1** and the header announces `Step N Transpose +k` — **1 count = 1 st**, same −12…+12 clamp. It is a **playback-only modifier like the step's OCT** (the stored chord stays a clean root-shape for the generators), **never probability-rolled**, moves with **SHIFT** (rotate), resets with generate/erase, and needs no undo (release just closes the overlay; the step keeps its offset until the next step refresh). Assign mode keeps working while the step is held; release closes the overlay (a PREV pressed mid-hold keeps its own row). Per-step values are **RAM only** like every other step field.
- Saved/restored with SEQMODE4+REC: the per-track **TRANS** offset arrived with the 436-byte extension and **Klock** closed the 665-byte block; the SEQ EEPROM block is **1380 bytes** today, and a save of any earlier size (1330 · 1296 · 1262 · 940 · 923 · 666 · 665 · 649 · 616 · 567 · 564 · 532 · 436 · 420 · 404 · 388 · 260 bytes) still loads — TRANS = 0, Klock = off, and speed indices written before the ladder grew to nine rungs are lifted two rungs so a saved **1/16** still boots as 1/16.

## Cross-Sequence Sync
When a track's step chords are written, all MONO/POLY sequences **listening to that track's channel** re-derive their active steps from the source timeline (single-level, non-recursive). This enables "follow the leader" arrangements. A POLY track with **TYPE = HARM** never mirrors — it **re-bakes** instead (see the [HARM generative page](../generators/harm.md)).
