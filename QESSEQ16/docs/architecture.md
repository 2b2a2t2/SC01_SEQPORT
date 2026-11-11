<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](README.md) · [Cheat sheet](cheatsheet.md) · [Glossary](glossary.md)_

# Software Architecture

## Libraries Used
- **FastLED** — WS2812 LED control
- **U8g2lib** — SSD1306 OLED driver
- **Encoder** (PJRC) — Quadrature encoder reading
- **Adafruit_MPR121** — Capacitive touch controllers
- **uClock** — MIDI clock/transport (24 PPQN external, 96 PPQN internal)
- **Teensyduino USB MIDI** — USB-MIDI device (note on/off, CC, clock, start/stop/continue)
- **MIDI Library** (FortySevenEffects, bundled with Teensyduino) — the 5-pin DIN note input on Serial2

## Module Overview

| File | Responsibility |
|------|----------------|
| `QESSEQ16.ino` | Setup, main loop, timing diagnostics |
| `Buttons.h/cpp` | Touch handling, debounce, LED color hook, pad groups |
| `TouchChips.h/cpp` | MPR121 init, IRQ-driven polling |
| `Display.h/cpp` | OLED rendering (header + main screen), paged I2C write |
| `Encoders.h/cpp` | 8 encoders, delta queue, position tracking |
| `Modes.h/cpp` | Mode state machine (ENC/SEQ/KEY, variants 1–4), LED animation |
| `Bindings.h/cpp` | Mode→control mapping table, momentary overlays |
| `Events.h/cpp` | Event loop: button + encoder dispatch per binding |
| `SeqClock.h/cpp` | uClock integration, playhead, sync (INT/EXT), tempo, track types |
| `Keyboard.h/cpp` | Keyboard pads → MIDI notes, octave, channel, pending chords |
| `Chords.h/cpp` | KEYMODE2 chord engine: quality, extensions, FScale, latch, KeyLock |
| `Gen.h/cpp` + `bjorklund.h` | GNR8 generative page (SEQMODE, MONO+BASS): FILL/ACC/GLIDE/TIE knobs, option sub-pages (TONES/RANGE/LOW/ALGO), per-step OCT/BRW/PROB modifiers, FOLLOW note mapping (near/root/deg/ivl), Bjorklund/Euclidean placement. LEAD page (SEQMODE, MONO+LEAD): contour melody generator (ARCH/DESC/WALK, live reshape + per-contour cache) with CT/GD follow-mapping knobs, re-resolved onto the feeder chord via FOLLOW + MAP ct/gd |
| `Key4.h/cpp` + `genre_data.h` | KEYMODE4 genre engine: 18-genre transition tables, ranked chord proposals, write-to-focus, EMB embellish menu (passing chords + substitution; data generated into `genre_data.h`, method in `GENRE_DATA.md`) |
| `Skank.h/cpp` | SKANK walking-voicing derive (SEQMODE, POLY track with TYPE = SKANK): `deriveSkankChord()` — anchor inversion (TES) walked by MOT/LIN·PP·RND, thinning, in-scale extension roll, genre-matrix bar-end substitution (via `Key4.cpp` `k4BorrowSample()`); page knobs + per-step locks live in `Keyboard.cpp` |

## Event Flow (Main Loop)

```cpp
loop() {
  pollSeqClock();        // MIDI clock pump + playhead advance (highest priority)
  pollTouchChips();      // Read MPR121 IRQs → handleTouches()
  btnUpdate();           // Debounce, hold/double detection
  pollEncoders();        // Read encoder deltas → queue
  pollModeLeds();        // Animation (pulse/blink/playback blink)

  handleButtonEvents();  // Dispatch per current binding (mode+variant+overlays)
  handleEncoderEvents(); // Dispatch per current binding

  pollSeqClock();        // Drain ticks queued during event handling
  displayUpdate();       // Paged OLED write (non-blocking)

  // Serial commands: "recal" / "diag" (see Serial Diagnostics)
}
```

`seqDiagNote()` timestamps each section; worst-case >3 ms reported at 1 Hz.

## Key Constants

```cpp
// Keyboard (Keyboard.cpp)
KEY_OCT_MIN  = -2
KEY_OCT_MAX  = 8
KEY_OCT_DEF  = 3      // C3 = 60
KEY_VEL      = 100
STEP_COUNT   = 16
STEP_MAX     = 64     // 4 bars × 16 steps max per track
CHORD_MAX    = 8      // notes per chord
SEQ_CH_COUNT = 16     // sequences = MIDI channels 1..16

// Chord Engine (Chords.cpp / Chords.h)
CHORD_VEL    = 100
CHORD_N      = 8      // max simultaneous notes
KEY_SCALE_COUNT = 54  // scale table size

// Encoders (Encoders.cpp)
// Every list/small-range knob (octave, scale, root, genre, phrase, drum
// genre/variation/line, chord bass-spread-density, swing style) steps
// through encStep(): 4 counts = one quadrature detent on the Teensy, 12 on
// the SC01 Plus, whose detentless AS5600 knobs give 96 counts a turn.
ENC_STEP_COUNTS = 4 (Teensy) / 12 (SC01 Plus)

// Touch (TouchChips.h)
TOUCH_THRESH = 50     // press threshold
TOUCH_REL    = 30     // release threshold (hysteresis)

// Sequencer (Keyboard.cpp)
static uint8_t chStep[16]          // playhead per track (0..63)
static uint8_t chStepShadow[16]    // global position (ignores loops)
static bool    loopWasOn[16]       // previous loop state (edge detect)
```
