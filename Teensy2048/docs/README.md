# Teensy2048 documentation

These docs are organised by **what you are trying to do**, not by source file. Each page
is short, starts with a one-line orientation, and links sideways instead of repeating
its neighbours.

## Where do I find…?

| I need… | Page |
|---|---|
| To get sound out in the first five minutes | [Start here](start-here.md) |
| A worked example for a musical result | [Recipes](recipes/index.md) |
| The binding of a specific encoder in a specific mode | [Bindings](reference/bindings.md) or the [cheat sheet](cheatsheet.md) |
| To drive Ableton Live from the pads and knobs | [ENCABLETONMODE](modes/link.md) |
| To know whether a feature is safe to rely on | [Feature status](status.md) |
| The meaning of "feeder", "page", "radio", "variant" | [Glossary](glossary.md) |
| Pins, pad numbering, LED order | [Hardware](hardware.md) |
| Module responsibilities and the main loop | [Architecture](architecture.md) |
| Why a design is the way it is | [ADRs](adr/README.md) |
| A symptom and its fix | [FAQ](faq.md) |
| To compile or flash | [Build & flash](build.md) |
| To add a mode, scale, chord quality or genre | [Extending](extending.md) |
| Why something is deliberately missing | [Limitations](limitations.md) |
| How to write docs that match these | [Style guide](style-guide.md) |

## The map

```
docs/
├── start-here.md          guided first session
├── status.md              feature × status × date
├── cheatsheet.md          one printable page
├── glossary.md            vocabulary
├── concepts.md            the eight ideas the firmware is built on
├── panel-map.md           labelled panel + mode/variant diagram
├── limitations.md         known limits, non-goals
├── style-guide.md         how these pages are written
├── faq.md                 symptom → cause → fix
├── hardware.md            pins, pads, LEDs
├── architecture.md        modules, loop, constants, EEPROM
├── build.md               toolchain and flashing
├── extending.md           extension points
├── modes/
│   ├── README.md          mode system, variants, binding matrix
│   ├── keymode.md         KEY 1–4 (chromatic, chords, channels, genres)
│   ├── seqmode.md         SEQMODE 1–4 (tracks, steps, record, edit)
│   ├── timing.md          swing and external input
│   ├── encmode.md         ENCMODE 1–4 (CC banks, dials)
│   ├── link.md            ENCABLETONMODE — the Ableton bridge overlay (NEXT)
│   └── drum.md            DRUM part sequencing
├── generators/
│   ├── arp.md             arpeggiator + 4 sub-pages
│   ├── gnr8.md            bassline generator
│   ├── lead.md            melody generator
│   ├── comp.md            chord stabs
│   ├── harm.md            progression inference
│   ├── skank.md           walking voicings
│   ├── hand-strum.md      strummed pads (POLY TYPE = HAND)
│   └── drum-styles.md     18 preset drum styles
├── reference/
│   ├── bindings.md        every encoder/control/pad set
│   ├── display.md         OLED layout and header rules
│   ├── leds.md            LED semantics and page hues
│   ├── midi-io.md         transport, ports, channels
│   └── diagnostics.md     serial commands
├── recipes/
│   └── index.md           10–15 worked examples
└── adr/
    └── README.md          decision records
```

## How these docs are written

- **One H1 per page**, headings at most three levels deep.
- **Tables for parameter reference**, prose for explanation.
- **Links instead of repetition** — the binding lives in one place, the cheat sheet points at it.
- **Behaviour, not implementation** — a page says what a knob does and what you hear; code
  lives in [Architecture](architecture.md) and [Extending](extending.md).
- Vocabulary is fixed by the [glossary](glossary.md). If a page needs a new term, the term
  is added there first.

The full set of rules, including the tone and the do-not list, is in the
[style guide](style-guide.md).
