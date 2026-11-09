<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](README.md) · [Cheat sheet](cheatsheet.md) · [Glossary](glossary.md)_

# Hardware

## Core
- **Teensy 4.1** (600 MHz ARM Cortex-M7)
- **4× MPR121** I2C capacitive touch controllers (48 touch pads total)
- **42× WS2812 (NeoPixel)** LEDs for pad feedback
- **SSD1306 128×64** OLED display (I2C, hardware @ 400 kHz)
- **8× Rotary Encoders** (quadrature, no detents)

## Pin Configuration

| Function | Pins | Notes |
|----------|------|-------|
| **MPR121 #1** | I2C 0x5A, IRQ pin 5 | 12 pads |
| **MPR121 #2** | I2C 0x5B, IRQ pin 2 | 12 pads |
| **MPR121 #3** | I2C 0x5C, IRQ pin 4 | 12 pads |
| **MPR121 #4** | I2C 0x5D, IRQ pin 3 | 12 pads |
| **WS2812 Data** | Pin 6 | 42 LEDs |
| **OLED (I2C)** | SDA/SCL (pins 18/19) | Shared Wire bus @ 400 kHz |
| **Encoder 1** | 40 (A), 39 (B) | |
| **Encoder 2** | 36 (A), 35 (B) | |
| **Encoder 3** | 34 (A), 33 (B) | |
| **Encoder 4** | 31 (A), 32 (B) | |
| **Encoder 5** | 38 (A), 37 (B) | |
| **Encoder 6** | 26 (A), 25 (B) | |
| **Encoder 7** | 27 (A), 28 (B) | |
| **Encoder 8** | 29 (A), 30 (B) | |

## Pad/LED Mapping (48 pads → 42 LEDs)

The hardware has 48 touch pads but only 42 LEDs. The mapping (`padToLedList` in Buttons.cpp) routes pads to LEDs:

| Pad Range | Function | LED Indices |
|-----------|----------|-------------|
| 0–15 | Step pads (16) | 0–15 |
| 16–24 | Control buttons (9) | 16–24 |
| 25–31 | White keys (7) | 25–31 |
| 32–34 | Mode buttons (3) | 32–34 |
| 35–36 | Transport (2) | 35–36 |
| 37–41 | Black keys (5) | 37–41 |
| 42–47 | (No LEDs) | — |

**Keyboard chromatic order** (groupKeyboard): C, C#, D, D#, E, F, F#, G, G#, A, A#, B  
→ LEDs: 25, 37, 26, 38, 27, 28, 39, 29, 40, 30, 41, 31
