#pragma once

#include <stdint.h>

// DEVICE SETTINGS - the page the header band opens (tap it on the panel; the
// Teensy's SSD1306 has no touchscreen, so there it never opens).
//
// Three knobs, and only three:
//   ENC1  LED brightness    0..100 % (the strip runs at FastLED brightness,
//                            boot 10 % = the 25/255 the strip has always used)
//   ENC2  screen brightness 0..100 % (the panel's PWM backlight, boot 100 %)
//   ENC5  clock source      INTERNAL (master, the firmware generates the
//                            clock) / EXTERNAL (slave, follow incoming USB
//                            MIDI clock) - the same switch as SEQMODE3 ENC4
//
// The two brightness values are stored in EEPROM above both save blocks
// (SEQ at 160..1557, ENC CC at 1600..2178) and survive a power cycle; the
// clock source stays RAM like the SEQMODE3 knob it mirrors.

bool settingsOpen();

// call every loop: reads the header tap that opens / closes the page.  A mode
// change also closes it, so the page can never outlive the screen it covers
void settingsTouchPoll();

// one encoder turn while the page is up: 0 = LED brightness, 1 = screen
// brightness, 4 = clock source; every other knob is idle
void settingsAdjust(uint8_t ei, int16_t counts);

// the cell text for the page (Display.cpp): 0..7 = the eight cells, "" for
// the idle ones
void settingValueText(uint8_t cell, char* buf, uint8_t n);

// boot: load the stored brightness values and apply them
void settingsBegin();
