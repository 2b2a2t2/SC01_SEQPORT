# Glossary

The vocabulary the rest of the docs assume. Terms are grouped, not alphabetical, because
the groups are what make the instrument legible.

## Structure

**Mode family** — one of the three top-level behaviours: `ENC` (knobs as CC), `SEQ`
(sequencer), `KEY` (keyboard). Chosen with the MODE pad.

**Variant** — one of four sub-behaviours inside a family. `SEQ 3` is "sequencer, variant 3".
Variants are independent: each family numbers its own 1–4.

**Momentary variant** — variant 3 everywhere. You enter it by **holding** the mode pad and
leave it by releasing. `KEY 3` restores the previous family *and* variant; `ENC 3` / `SEQ 3`
come back into their own family, on the variant the hold started from — or variant 1 if the
hold began in another family. It is a look, not a place.

**Locked variant** — `ENC 4`, the channel picker. It stays until you leave it with a click.

**Track** — one of 16 sequencer lanes, each bound to its own MIDI channel (1–16). A track
has a type, a length, a pattern and its own settings.

**Sequence** — the pattern a track plays: up to **64 steps** (four 16-step bars, `LEN` per
track), edited one 16-step window at a time. "Track" is the lane, "sequence" is its
content; the docs say "track" when they mean the whole lane.

**Track type** — `DRUM`, `MONO` or `POLY`. MONO and POLY carry a sub-type (the "transform"
or "type" knob): MONO is `ROOT` / `BASS` / `ARP` / `STRUM` / `LEAD`, POLY is `STEP` /
`STRUM` / `HAND` / `HARM` / `SKANK`.

**Pattern length (`LEN`)** — how many steps the track actually plays, up to 64. Different
lengths per track are the point: that is how polimeter and polyrhythm happen.

## Binding machinery

**Binding set** — the lookup that says what the encoders, controls, step pads and keys do
right now. Four groups: encoder, control, step, key. Defined in `Bindings.cpp`.

**Overlay** — a temporary binding set pushed over the base one for one group, cleared on
release. There is exactly **one overlay slot per group**, so an overlay cannot stack on an
overlay.

**Page** — an encoder page owned by a track type, opened with **M2** and identified by the
pad's hue: GNR8 on a bass track, LEAD on a lead track, COMP on POLY/STEP, and so on.

**Radio** — a set of options where exactly one is armed and arming one takes over the
whole row. The step-edit radio (TIE / GLIDE / ACCENT / PROB) and the F row are radios.

**Arm** — a click that makes an F key or an attribute current until it is clicked off.
Armed keys burn bright.

**Sub-page** — what the encoder row becomes while an F is armed. F1 on the ARP page is the
ORDER sub-page, F4 is OPTIONS.

## Sound and harmony

**Pending chord** — the notes currently held (or arriving over MIDI) that the instrument
treats as one chord: what the pads play, what gets recorded, what followers read.

**Feeder / follower** — a *feeder* is a track whose notes another track reads; the reader
is the *follower*. Set with `SEQ 3 → INCH` (input channel) and `FOLLOW` on. One level only:
a follower is never itself read.

**FOLLOW** — the listener switch. With it on, the track re-derives its notes from the
feeder's timeline at playback instead of playing its stored steps.

**Stored vs derived** — a *stored* step is what the pattern holds; a *derived* note is
computed at send time (LEAD's MAP resolve, SKANK's walk, HARM's live follow). Derived
notes never rewrite the pattern.

**FScale** — force-to-scale: inbound notes are snapped into the current KEY + SCALE
instead of being played raw.

**Latch** — `KEY 2 → ENC8`: the chord keeps sounding after the fingers lift, until the next
chord replaces it.

**Latch vs lock** — a *latch* holds a sound; a *lock* pins a value. SKANK's per-step locks
override that step's knobs; KEY 3's ENCOD/KBOARD **locks** tie the encoder channel and the
keyboard channel to the sequencer's.

**Borrow (`BRW`)** — replace a mapped note with a tone borrowed from the incoming chord,
instead of using the generated pitch.

**Guide tone** — the 3rd and 7th of a chord: the two notes that carry its colour. `GD`
expands that pool.

**Tessitura (`TES`)** — the inversion a walking voicing is anchored to.

**Comp** — re-strike a step's chord several times inside the step (`HITS`, 0–8).

**Accent** — a per-step velocity lift, stamped by the ACCENT target and by the drum page's
`MOT` mask. The accent velocity is **per track** (`seqAccVel[16]`, boot 101), not a global
setting.

## Time and persistence

**Swing ratio** — `50 + (AMT/100) × (max − 50)`, where `max` comes from the selected style
table (MPC60, MPC3K, SP1200, LINN, TR808, TR909, SHUFFLE, TRIPLET).

**EXT IN** — `OFF` / `ON` / `THRU` / `LIVE`: external notes ignored; fed to the pending chord
(what `REC` captures); forwarded out on the track's channel; or **played live through the
track's engine** while the chord is held. `LIVE` does not record.

**SRC** — `USB` / `DIN` / `BOTH`: which ports a track accepts external notes from.

**Detent** — one click of an encoder = **4 counts**. Wide dials use 1 count = 1 unit, so a
detent is 4 units (4 % on a 0–100 % dial).

**EEPROM block** — the saved settings region, one block per subsystem, each with a byte
size that grows when fields are added. An older block loads with defaults for the new
fields; that cascade is the compatibility promise.

**`SEQ 4 + REC` / `ENC 4 + REC`** — the save gesture. There is no autosave of settings.

## Display

**Header band** — the top 16 px of the OLED, 128 px / **21 characters**, mode on the left,
transient message on the right.

**Announce** — a transient 2.5 s message naming the parameter and its value in words.

**Legend** — the control-button label row under the encoder grid. ENCMODE 1/2/4 draw none;
the dials take the band.

## Panel codes

The OLED prints 3–6 character codes where the full word will not fit; the header band always
names the parameter in words. This is the key to every code the panel can show. `--` in a
legend means the knob has no idle binding.

**Shared SEQ rows**

| Code | In words |
|---|---|
| `OCT` | octave |
| `LEN` | length — the steps the track plays |
| `INCH` | input channel — the follower's feeder |
| `SPEED` | clock division |
| `SWING` / `AMT` | swing style / amount |
| `SRC` | external source (`USB` / `DIN` / `BOTH`) |
| `EXT IN` | external input mode |
| `GATE` / `VEL` / `PROB` | gate length / velocity / probability |
| `LISTEN` | what a live record pass listens to (`CHORD` / `NOTE`) |
| `PLAY` | playback source for stored steps (`GATE` / `RECORDED`) |
| `OVERDUB` | whether a fresh gesture stacks onto a step (`OFF` / `ON`) |
| `SHIFT` | pattern rotate |
| `TRANS` | transpose |
| `KLOCK` | key lock — fixes the TRANS offset to the scale |
| `MAP` | drum kit mapping (`CUSTOM` / `GM`) |
| `HITS` | comp stabs per step (0–8) |
| `BRW` | borrow a tone from the incoming chord |

**KEY 2 / 3 / 4**

| Code | In words |
|---|---|
| `FSCALE` | force to scale |
| `BASS` / `SPRD` / `DENS` | chord bass note / spread / density |
| `LATCH` | latch — the chord survives the release |
| `KLOCK` | key lock — the chord is the scale's diatonic triad |
| `PERF` | performance switch (state only — see below) |
| `ENCOD` / `KBOARD` | encoder / keyboard channel link |
| `GENRE` | genre (18 tables) |
| `MOOD` | mood bias (DARK/COOL/MIXED/WARM/LIGHT) |
| `TENS` | tension — how wide the borrowed-degree window opens |
| `MOTION` | motion — how hard the transition matrix pulls |
| `CAD` | cadence (`V-I` / `IV-I` / `V-vi` / `ii-V`) |
| `CPX` | complexity — how faithfully chord *qualities* follow the genre |
| `ERA` | era — the same weighting for *inversions* |
| `SURP` | surprise — the chance a proposal rolls into a secondary dominant |
| `EMB` | embellish — the menu and its mode (OFF / PASS / P+S) |
| `PLACE` | passing-chord placement (DIST / ODD) |
| `PHRASE` | phrase template (FREE / AABB / ABAB / ABAC / AABA) |
| `SUB` / `INV` / `PAS` / `COL` | substitution probability / inversion bias / passing amount / colour amount |
| `DEG` / `VOICE` | scale-degree shift / voice spread |

`PERF` (`KEY 2` → hold `PREV`, ENC6) is a two-state switch kept for parity with the reference
instrument: it stores ON/OFF and changes no behaviour.

Codes that already spell their word need no key: `SCALE`, `KEY`, `TRACK`, `TYPE`, `FOLLOW`,
`BPM`, `MODE`, `SPAN`, `SKIP`, `TONES`, `LOW`, `ALGO`, `RANGE`, `CONTOUR`, `HOLD`. `CC1`–`CC36`
print the MIDI control-change number itself.

**SKANK and DRUM pages**

| Code | In words |
|---|---|
| `TES` | tessitura — the walk's anchor inversion |
| `MOT` | motion — how far the voicing walks from the anchor |
| `SHP` | shape of the walk (`LIN` / `PP` / `RND`) |
| `THN` | thinning — drop the lowest tones |
| `EXT` | extension — the chance of a colour tone |
| `TES%` / `NOT%` | tessitura randomizer / note randomizer |
| `VAR` | variation — the bar-end fill (`off` / `fill4` / `fill8`) |
| `FILL` | fill amount |

**GNR8 and ARP pages**

| Code | In words |
|---|---|
| `FILL` | note density |
| `ACC` | accent |
| `GLIDE` / `TIE` | glide / tie probability |
| `RNG` | range |
| `BRW` | borrow from the incoming chord |
| `ORD` / `MOT` / `GEN` / `OPT` | order / motion / generative / options sub-pages |
| `INST` | instrument (`3F1B` = 3 from the bottom, `4F2B` = 4 from the bottom) |

