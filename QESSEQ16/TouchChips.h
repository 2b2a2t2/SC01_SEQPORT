#pragma once

#include "Config.h"
#include <Arduino.h>
#include <Adafruit_MPR121.h>

#define NUM_CHIPS 4

#define TOUCH_THRESH 50
#define TOUCH_REL    30

#ifdef use_SC01Plus
// The SC01 routes no IRQ line to the pads, so the chips are polled on a
// timer instead of read on an edge.  They live on the pad bus: GPIO 10/11,
// deliberately not the core's default 8/9 (two of the panel's data lines).
#define MPR_SDA       10
#define MPR_SCL       11
#define MPR_FREQ      400000
#define TOUCH_POLL_MS 20
#else
#define MPR121_IRQ1 5
#define MPR121_IRQ2 2
#define MPR121_IRQ3 4
#define MPR121_IRQ4 3
#define IRQ_HEARTBEAT_MS 50   // safety re-read when no edge arrives
#endif

extern Adafruit_MPR121 chips[NUM_CHIPS];
extern uint16_t lastTouched[NUM_CHIPS];
extern uint16_t currTouched[NUM_CHIPS];

bool initTouchChips();            // begin + autoconfig (+ IRQs on the Teensy); false if a chip missing
uint8_t touchChipsFound();        // how many of the four answered at boot
void pollTouchChips();            // read changed chips, feed handleTouches()
void touchRecalibrate();          // force re-autoconfig on all chips
void touchPrintDiag();            // print baseline & filtered data for all electrodes

