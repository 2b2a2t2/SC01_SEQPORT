<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](README.md) · [Cheat sheet](cheatsheet.md) · [Glossary](glossary.md)_

# Building & Flashing

1. Install Teensyduino (Arduino IDE or VS Code + PlatformIO)
2. Install libraries: FastLED, U8g2, Encoder, Adafruit_MPR121, uClock
3. Open `Teensy2048.ino`
4. Select **Teensy 4.1**, **USB Type: MIDI**
5. Compile & upload

**arduino-cli** — no IDE, no copy step, works on the checked-out tree:

```sh
./tools/fw.sh build     # compile only
./tools/fw.sh upload    # compile, then flash the attached Teensy 4.1
```

`tools/fw.sh` locates `arduino-cli` (PATH → Arduino IDE bundle → `$ARDUINO_CLI`),
collects the library roots that hold the five required libraries (sketchbook
`~/Documents/Arduino/libraries` plus any `$SKETCHBOOK/*/libraries` providing
FastLED/Encoder), builds into `${TMPDIR:-/tmp}/t2048build`, and flashes via the
**Teensy port** reported by `arduino-cli board list`. Overrides: `ARDUINO_CLI`,
`T2048_LIBS` (colon-separated roots), `T2048_BUILD`, `T2048_PORT`.

Because USB Type is MIDI the board has no serial tty: it appears only under
"Teensy Ports" (`usb:…`), and a `/dev/cu.usbmodem*` port left over from an older
non-MIDI build disappears after flashing — that is success, not a disconnect.

`tools/header_lint.py` guards the OLED header text: it fails on any
`headerShow()`/`headerAnnounce()` message wider than the 21-character band and on
the shorthand codes the strip must not use again (`PROB`, `mot`, `klock`, `GNR8`,
…), and it lists the snprintf-built messages whose worst case would overflow.
Run it after touching a message (exit status 1 = findings).

**No-compile fallback** — an image built elsewhere can be flashed without the toolchain: drag `Teensy2048.ino.hex` onto the Teensy Loader window and press its button, or `teensy_loader_cli -mmcu=imxrt1062 -w Teensy2048.ino.hex -v`. The loader only writes bytes, it does not build, so the `.hex` must come from the commit you mean to run. This also needs the board on a USB port of the machine running the command: a cloud/remote session can compile and lint the tree but has no USB, so the flash itself always happens on the local checkout.
