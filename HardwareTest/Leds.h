#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "TouchChips.h"

// WS2812B strip, data on GPIO 12. Port of Teensy2048's Buttons module's LED
// half. The Teensy drove 42 LEDs and the first 42 here are the same strip; the
// board now carries 50, so the last 8 are spare (they show in the startup
// sweep, nothing maps to them yet). The pad-event, hold and double-tap logic is
// not ported yet - this is bring-up, so a touched pad simply lights its LED.

#define NUM_LEDS 50
#define LED_PIN 12
#define LED_BRIGHTNESS 24  // 25 on the Teensy; current is also capped in Leds.cpp

extern CRGB leds[NUM_LEDS];

// Pad -> LED, calibrated on the panel (see kLedForPad in Leds.cpp): the table
// names the strip index of the LED sitting at each button, so button N lights
// LED N. Replaces the identity mapping the port shipped with, which assumed
// pad order matched strip order - it does not.
extern int8_t ledForPad[NUM_PADS];

void initLeds();
void ledStartupTest();   // one sweep, so a dead or misordered LED shows up
void ledPadFeedback();   // touched pad = lit, released = dark
