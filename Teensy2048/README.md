# Teensy2048

A Teensy 4.1 performance controller: **48 touch pads**, **8 encoders**, a 128×64 OLED,
**42 addressable LEDs** and **USB-MIDI** — played as a keyboard, a 16-track sequencer,
or a bank of MIDI CC knobs.

One firmware, three mode families. Everything is live: no menus to save, no patterns to
load, no computer required.

---

## What it is for

| If you want to… | Start with |
|---|---|
| Play chords and scales on the pads | [Keyboard modes](docs/modes/keymode.md) |
| Program a 16-track pattern and sync it to a DAW | [Sequencer](docs/modes/seqmode.md) |
| Generate basslines, leads and harmonies from a chord progression | [Generators](docs/generators/gnr8.md) |
| Use the eight knobs as MIDI CC controllers | [Encoder mode](docs/modes/encmode.md) |
| Drive Ableton Live's clips and device knobs | [ENCABLETONMODE](docs/modes/link.md) |
| Build or reflash the firmware | [Build & flash](docs/build.md) |

## Hardware at a glance

| Part | Detail |
|---|---|
| MCU | Teensy 4.1, USB Type **MIDI** |
| Pads | 48 capacitive pads on 4× MPR121 (I²C), 12 per chip |
| Encoders | 8 quadrature encoders, 4 counts per detent |
| Display | SSD1306 128×64 OLED over I²C |
| LEDs | 42 WS2812 on one strip (48 pads, 6 unmapped) |
| MIDI | USB-MIDI device + 5-pin DIN **input** (notes only) |

Pins, the pad→LED order and the pad map: [docs/hardware.md](docs/hardware.md).

## The three mode families

The three mode pads — **ENC**, **SEQ**, **KEY** — pick the family, and a family you return to
recalls the variant it was last left on. Each family has **four variants**: a tap on the lit
pad toggles **1 ↔ 2**, a **double-tap** selects **4**, and a **hold** enters the momentary
**3**.

| Family | Variant 1 | Variant 2 | Variant 3 | Variant 4 |
|---|---|---|---|---|
| **ENC** | CC 17–20 + F-bank CCs | shared SEQ controls | channel select (momentary) | channel picker (locked) |
| **SEQ** | track + steps | FX / step row | track config (+POLY type) | timing, input, save |
| **KEY** | chromatic keyboard | chords + latch | channel select (+locks) | genre progression engine |

Full semantics, including what is momentary and what is locked:
[docs/modes/README.md](docs/modes/README.md).

## Documentation

The docs are split by what you are trying to do, not by source file.

- **[Start here](docs/start-here.md)** — pick a path in five minutes.
- **[Docs home](docs/README.md)** — the complete map of every page.
- **[Cheat sheet](docs/cheatsheet.md)** — one printable page of bindings and gestures.
- **[Glossary](docs/glossary.md)** — feeder, follow, page, radio, variant, latch…
- **[Feature status](docs/status.md)** — what is stable, what is still moving.
- **[Recipes](docs/recipes/index.md)** — worked examples from "four-on-the-floor" to "bass → HARM → LEAD".
- **[FAQ & troubleshooting](docs/faq.md)** — symptom → cause → fix.
- **[Architecture](docs/architecture.md)** — modules, event flow, EEPROM layout.
- **[Decisions (ADRs)](docs/adr/README.md)** — why the firmware is shaped this way.

## Build & flash

```sh
./tools/fw.sh build     # compile only
./tools/fw.sh upload    # compile, then flash the attached Teensy 4.1
```

Requires Teensyduino plus FastLED, U8g2, Encoder, Adafruit_MPR121 and uClock. Details,
the raw `arduino-cli` equivalent and the no-toolchain `.hex` fallback:
[docs/build.md](docs/build.md). After touching an OLED header message, run
`tools/header_lint.py`.

There is no automated test suite: verification is compiling plus on-hardware behaviour.

## License

MIT — see [LICENSE](LICENSE).
