#pragma once

#include <Arduino.h>
#include <Adafruit_MPR121.h>

// Four MPR121 capacitive controllers on the board's second I2C bus, 12 pads
// each. Port of Teensy2048's TouchChips module with the bus changed for this
// board; four chips at 0x5A..0x5D is the same 48-pad layout as the Teensy, so
// the pad numbering lines up with the original again.
//
// The Teensy drove an IRQ line per chip. Those wires are not part of this
// build, so the status registers are polled instead - at 400 kHz three 2-byte
// reads cost well under a millisecond.

#define NUM_CHIPS 4
#define PADS_PER_CHIP 12
#define NUM_PADS (NUM_CHIPS * PADS_PER_CHIP)  // 48

#define TOUCH_THRESH 50
#define TOUCH_REL 30
#define TOUCH_POLL_MS 20  // poll cadence when no IRQ line is available

// Second I2C bus. The panel's FT5x06 keeps its own bus on 6/5 (PanelSC01.h).
#define MPR_SDA 10
#define MPR_SCL 11
#define MPR_FREQ 400000

extern Adafruit_MPR121 chips[NUM_CHIPS];
extern uint16_t currTouched[NUM_CHIPS];

bool initTouchChips();   // bus + autoconfig; false if a chip did not answer
uint8_t touchChipsFound();  // 0..NUM_CHIPS that answered
void pollTouchChips();   // re-read touch status, prints pad transitions
bool padDown(uint8_t pad);  // pad 0..35
uint8_t padsDown();         // how many pads are currently touched
void touchRecalibrate();    // force autoconfig on every chip
void touchPrintDiag();      // baselines + filtered data for all electrodes
