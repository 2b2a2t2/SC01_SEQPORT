# Bindings

Every combination of mode family and variant resolves to four binding sets — one for the
**encoders**, one for the **control buttons**, one for the **step pads**, one for the
**keyboard pads**. This page is the reference for what lands where; the tables live in
`Bindings.cpp`.

## How resolution works

```
modeBase (ENC/SEQ/KEY) × modeVariant (1-4)  →  base binding set
overlay per group (encoder / control / step / key)  →  pushed over the base
```

There is **exactly one overlay slot per group**, so an overlay never stacks. An overlay is
cleared on release (or when the armed page closes), which is why a held key's row always
comes back to what it covered.

`Labels` are printed on the OLED; `--` means the knob has no idle binding and only comes
alive under its page or radio. Every code is expanded in the
[glossary's panel-codes table](../glossary.md#panel-codes).

## Encoders — base sets

| Mode | ENC1 | ENC2 | ENC3 | ENC4 | ENC5 | ENC6 | ENC7 | ENC8 |
|---|---|---|---|---|---|---|---|---|
| **SEQ 1** | OCT | LISTEN | PLAY | OVERDUB | GATE | VEL | -- | PROB |
| **SEQ 2** | OCT | LISTEN | PLAY | OVERDUB | GATE | VEL | -- | PROB |
| **SEQ 3** | OCT | LEN | FOLLOW | SPEED | INCH | TRACK | TYPE | BPM |
| **SEQ 4** | SWING | AMT | EXT IN | SRC | GATE | VEL | -- | PROB |
| **ENC 1** | CC17 | CC18 | CC19 | CC20 | CC1 | CC2 | CC3 | CC4 |
| **ENC 2** | CC17 | CC18 | CC19 | CC20 | CC1 | CC2 | CC3 | CC4 |
| **ENC 3** | CC17 | CC18 | CC19 | CC20 | CC1 | CC2 | CC3 | CC4 |
| **ENC 4** | CC17 | CC18 | CC19 | CC20 | CC1 | CC2 | CC3 | CC4 |
| **KEY 1** | OCT | SCALE | KEY | -- | -- | -- | -- | -- |
| **KEY 2** | OCT | SCALE | KEY | FSCALE | BASS | SPRD | DENS | LATCH |
| **KEY 3** | OCT | SCALE | ENCOD | KBOARD | -- | -- | -- | -- |
| **KEY 4** | GENRE | MOOD | TENS | MOTION | CAD | CPX | PHRASE | SURP |

In ENCMODE, `ENC5–8` print the live CC number of the armed bank: `CC1–CC4` in page group 1,
`CC21–CC24` in group 2.

## Encoders — overlay sets

| Set | When | ENC1–8 |
|---|---|---|
| **SEQ ROT** | `PREV` held (any SEQ variant) | SHIFT · TRANS · KLOCK · -- · -- · -- · -- · -- |
| **SEQ STEP** | a step pad held (non-DRUM) | TRANS · -- · -- · -- · -- · -- · -- · -- |
| **SEQ SKANK STEP** | a step held on POLY + SKANK | TES · MOT · SHP · THN · EXT · SUB · TES% · TRANS |
| **SEQ BAR** | a C–F bar pad held (SEQ 3 keyboard row) | OCT · TRANS · -- · -- · -- · -- · -- · -- |
| **KEY 2 PREV** | `PREV` held in KEY 2 | OCT · SCALE · KEY · LATCH · KLOCK · PERF · -- · -- |
| **KEY 4 PREV** | `PREV` held in KEY 4 | OCT · SCALE · KEY · FSCALE · SUB · INV · PAS · COL |
| **KEY 4 EMB** | `NEXT` tapped in KEY 4 | EMB · PLACE · ERA · -- · -- · -- · -- · -- |
| **KEY 4 STEP** | a step pad held in KEY 4 | OCT · DEG · INV · VOICE · -- · -- · -- · -- |
| **ENCABLETON** | `NEXT` tapped in ENCMODE | P1 · P2 · P3 · P4 · P5 · P6 · P7 · P8 |

`ENCABLETON` ([page](../modes/link.md)) is the one overlay that takes **three** groups at
once: the knobs (`ENC_ABLETON`, labels `P1–P8` until Live names them), the control row
(`CTRL_ABLETON`: `TRACKS · LIVE · DRUM · MUTE · CLIPS`, with `BANK< BANK> DEV< DEV>` or — on
the M3 action set — `FILL SCENE STOP ALL` on `F1–F4`) and the pads (`STEP_ABLETON`: the
selected track's 16 clip slots, or the 16 tracks with `PREV` held). Its knobs send
**CC 64–71**, not the base set's CC 17–20 / CC 1–36, so the ENCMODE dials and their
per-CC-per-channel memory are untouched.

Page rows (ARP sub-pages, GNR8 option radios, the strum ADDS row, the DRUM knob sets,
COMP/HARM/SKANK/LEAD pages) are themselves overlays: they take the whole encoder row while
armed, and return the base row when released or when the page closes. Their knobs are
documented on each page — see [Generators](../generators/arp.md).

## Step pads

| Mode / variant | Pads 0–15 do |
|---|---|
| **SEQ 1 / 2 / 4** | Toggle steps; green playhead, dim green active, dim blue empty. `M1` armed → edit the selected attribute on active steps |
| **SEQ 3** | Channel selector (dim yellow, selected bright yellow), until any encoder moves |
| **SEQ 3, bar row** | `C–F` pick the 16-step edit window; pressing the selected bar again toggles its loop |
| **SEQ 3, black keys** | Track verbs: `C#` length ÷2, `D#` length ×2, `F#` copy, `A#` paste, `G#` bar follow |
| **SEQ, DRUM track, `M1` held** | Orange part-select overlay; a pad picks the drum part, the normal view then toggles it |
| **SEQ, page open** | Belong to the page (step pick for GNR8 mod pages, focus in KEY 4) |
| **ENC 3** | Channel overlay (momentary, yellow) |
| **ENC 4** | Locked channel picker: 16 pads = the encoder's 16 MIDI channels |

## Keyboard pads

| Mode | Pads 25–41 do |
|---|---|
| **KEY 1** | Chromatic keyboard, in-scale pads only; octave/scale/key on the encoders |
| **KEY 2** | Chord pads; `M1/M2/M3/NEXT` pick quality, `F1–F4` toggle extensions |
| **KEY 3** | Channel selector on the step pads; keyboard unchanged |
| **KEY 4** | Scored degree map: hue by pitch class, brightness by rank, orange = borrowed; press to audition and chain |
| **POLY + HAND** | White keys are strings (pink → seagreen gradient), black keys are option toggles (violet on) |
| **SEQ, keyboard row committed** | The `C–F` bars and black-key verbs keep working in every mode until the keyboard's own track is selected on KEY 3 |

## Changing a binding

1. Add or extend the `EncSet` / `CtrlSet` / `StepSet` / `KeySet` enum in `Bindings.h`.
2. Add the row to the corresponding names/labels table in `Bindings.cpp`.
3. Point the mode/variant row at it in `table[3][4]`.
4. Add the dispatch in `Events.cpp` and, if the row is visible, the labels in `Display.cpp`.

The full procedure is in [Extending](../extending.md).
