# Cheat sheet

One page. Print it. Everything here is expanded in [Bindings](reference/bindings.md).

## Mode families

Tap **ENC**, **SEQ** or **KEY** to pick the family; tapping a family you are not in recalls
the variant it was last left on. Inside a family the lit pad **toggles `1 ↔ 2`** (and
`4 → 1`), a **double-tap** selects `4`, and **holding** it enters `3`. The header shows the
family and variant in words: `SEQUENCER 3`.

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **ENC** | CC 17–20 + F-bank | shared SEQ controls | channel select (momentary) | channel picker (locked) |
| **SEQ** | track + steps | FX / step row | track config, POLY type | swing, external in, save |
| **KEY** | chromatic | chords + latch | channel select + links | genre progression |

**Variant 3 is momentary**: hold the mode pad to enter it, release to leave. `KEY 3` returns
to the previous family and variant; `ENC 3` / `SEQ 3` always return into their own family,
on the variant the hold started from (or variant 1 if it began elsewhere). The other
variants are sticky.

## Encoder rows

Labels exactly as the OLED prints them; `--` means no idle binding (the knob only comes
alive under its page or radio).

| Set | ENC1 | ENC2 | ENC3 | ENC4 | ENC5 | ENC6 | ENC7 | ENC8 |
|---|---|---|---|---|---|---|---|---|
| **SEQ 1** | OCT | LISTEN | PLAY | OVERDUB | GATE | VEL | ACC | PROB |
| **SEQ 2** | OCT | LISTEN | PLAY | OVERDUB | GATE | VEL | ACC | PROB |
| **SEQ 3** | OCT | LEN | FOLLOW | SPEED | INCH | TRACK | TYPE | BPM |
| **SEQ 4** | SWING | AMT | EXT IN | SRC | GATE | VEL | ACC | PROB |
| **ENC 1–4** | CC17 | CC18 | CC19 | CC20 | CC1… | CC2… | CC3… | CC4… |
| **KEY 1** | OCT | SCALE | KEY | -- | -- | -- | -- | -- |
| **KEY 2** | OCT | SCALE | KEY | FSCALE | BASS | SPRD | DENS | LATCH |
| **KEY 3** | OCT | SCALE | ENCOD | KBOARD | -- | -- | -- | -- |
| **KEY 4** | GENRE | MOOD | TENS | MOTION | CAD | CPX | PHRASE | SURP |

`ENC3` in **SEQ 3** reads `PAD` (`LIVE` / `SELECT`) on a DRUM track instead of `FOLLOW`,
and `ENC6` reads `RATCH` (`2` / `3` / `4`) while the DRUM ratchet target is armed — a tap
stamps that count on the step's **selected drum part**; only that part re-triggers.

`ENC 5–8` in ENCMODE print the live CC number of the armed bank: `CC1–CC4` in page group 1,
`CC21–CC24` in group 2.

### Rows that appear while something is held or armed

| Situation | ENC1–8 |
|---|---|
| **PREV held** (SEQ) | SHIFT · TRANS · KLOCK · -- · -- · -- · -- · -- |
| **Step held** (SEQ) | TRANS · -- · -- · -- · -- · -- · -- · -- |
| **Step held**, POLY + SKANK | TES · MOT · SHP · THN · EXT · SUB · TES% · TRANS |
| **PREV held**, KEY 2 | OCT · SCALE · KEY · LATCH · KLOCK · PERF · -- · -- |
| **PREV held**, KEY 4 | OCT · SCALE · KEY · FSCALE · SUB · INV · PAS · COL |
| **EMB menu**, KEY 4 | EMB · PLACE · PHRASE · -- · -- · -- · -- · -- |
| **M2 menu** 1 TONAL, KEY 4 | KEY · SCALE · GRAV · MOD · MODTGT · MODLEN · FSCALE · CADSTR |
| **M2 menu** 2 CHROMA, KEY 4 | BORROW · PREP · SD · MIX · DIAT · UNRES · SURP · STRICT |
| **M2 menu** 3 VOICE, KEY 4 | PARAL · BASS · LEAP · DOUBL · CMNTN · TOP · LEAD · SPREAD |
| **M2 menu** 4 ARC, KEY 4 | RATE · ELIDE · CADWIN · CELL · VAR · ANTE · MOTIF · PHRASE |
| **Step held**, KEY 4 | OCT · DEG · INV · VOICE · -- · -- · -- · -- |
| **Bar pad held**, SEQ 3 | OCT · TRANS · -- · -- · -- · -- · -- · -- |

## Gestures

| Gesture | Effect |
|---|---|
| Tap a step pad | Toggle the step (or the selected drum part) |
| Hold a step | Arm a span start (TIE chain on a bass track) or open the held-step row |
| Tap an **F** | Arm that sub-page; the whole encoder row becomes its knobs |
| Tap the armed F again | Release, back to the plain row |
| Tap **M1** | Step-edit radio (targets TIE / GLIDE / ACCENT / PROB — DRUM adds RATCHET on **F2**, POLY adds FX / TYPE — and lets ENC2–4 turn `LISTEN` / `PLAY` / `OVERDUB`) |
| Tap **M2** | The track-type page (GNR8, LEAD, ARP, COMP, HARM, SKANK, DRUM styles, strum ADDS) |
| Hold **M2** | Overdub-layer panel on the keyboard row: C D E F = layers 1–4, C# = mute (always armed) / D# = erase (white key confirms), F# / G# / A# = undo / redo / reset layers |
| Tap **M3** | Generate (page open) / re-roll (drum page) / page group (ENCMODE) |
| Hold **M3** | Undo one level (generators, drum page) |
| Hold **PREV** | Swap the encoder row to SHIFT / TRANS |
| Hold **M1** on a DRUM track | Orange part-select overlay |
| **REC** in SEQ 4 / ENC 4 | Save per-channel memory / all CC values to EEPROM |

M1 and M2 are **mutually exclusive**: opening a page turns the edit radio off, and
pressing M1 from the page closes it.

## ENCABLETONMODE (NEXT, inside ENCMODE)

Latched overlay, RAM only. Needs the companion remote script (`Remote Scripts/Teensy2048/`).

| | |
|---|---|
| **NEXT** | `Live Mode On` / `Live Mode Off` |
| Pads | the selected track's 16 clip slots — press empty = create + fill, filled = launch, playing = stop, double = re-fill |
| **PREV** hold | pads = the 16 tracks (press = select in Live) |
| **M3** | F row: `BANK< BANK> DEV< DEV>` ↔ `FILL SCENE STOP ALL` |
| **F1/F2 · F3/F4** | parameter bank · device |
| ENC1–8 | the current bank's eight Live parameters, on **CC 64–71** |

Pad colours: bright green playing · blinking green triggered · dim green has a clip ·
dark blue empty · dim red armed.

## Encoder feel

One detent = 4 counts. Wide dials (every 0–100 %, and CC values) move **1 unit per count**,
so **one detent = 4 %** and a full sweep is about one turn; a fast spin is worth up to 4×
that. Small enums move **1 step per detent**. **Nothing wraps** — dials stop at both ends.

## Pad colours

| Colour | Meaning |
|---|---|
| Green | Playhead |
| Dim green / dim blue | Active step / empty step |
| Yellow | Channel overlay (variant 3, locked ENC 4) |
| Orange | DRUM part overlay |
| White / red | KEY 4 focus, AUDITION / WRITE |
| Amber | KEY 4 locked step (bright = both) |
| Violet | HAND option on, or a picked GNR8 step |
| White (touch) | Pad is being touched — wins over everything |

Page hues (dim = page closed, breathing = open): **indigo** DRUM · **pink** ARP ·
**blue** GNR8 · **sea green** LEAD · **lavender** strum · **aqua** HARM ·
**pale sky** COMP (lit only while open) · **spring green** SKANK.
Red, orange and yellow are reserved; do not expect a track hue there.

## Header messages

The strip is 128 px / **21 characters**. It names the parameter and its value in words
(`Pattern Length 16`), never the 4-character main-screen code (`LEN`). Messages are 2.5 s.
A message too wide is pinned to the left edge rather than running off-screen.
`tools/header_lint.py` fails the build of a message wider than the band.

## Ten things that trip people up

1. **Variant 3 is momentary** — entered by *holding* the pad. A plain tap only toggles
   1 ↔ 2, and variant 4 needs a double-tap.
2. **The step-edit radio and an M2 page cannot be open together.**
3. **Patterns are RAM only** — rotate, undo and generate all touch `seqNotes`, never EEPROM.
4. **`SEQ 4 → SRC`** decides which port external notes are accepted from; USB only by default.
5. **DIN carries notes only** — no clock, no transport.
6. **Per-step GNR8 modifiers (OCT / BRW / PROB) are RAM only**, unlike the eight page knobs.
7. **An armed page dies with the page** — changing channel, TRACK, TYPE or mode resets it.
8. **Older EEPROM saves load with defaults**, they are not rejected (except ENCMODE4).
9. **USB Type is MIDI**, so there is no serial tty — the board only appears as a Teensy port.
10. **48 pads, 42 LEDs**: pads 42–47 have no LED.
