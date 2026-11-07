#include "Leds.h"

CRGB leds[NUM_LEDS];
int8_t ledForPad[NUM_PADS];

// Pad -> LED: index = pad (0-based), value = the strip index of the LED that
// sits at that button, so button N lights LED N. Derived by inverting the
// observed button -> pad pairs (recorded under the old identity mapping).
// A permutation of 0..47, so every pad owns exactly one LED and only strip
// ends 48/49 are unowned. This table is the one place to fix if the strip moves.
static const int8_t kLedForPad[NUM_PADS] = {
  32, 17, 18, 16,  0,  1, 42, 43,  // pads  0.. 7
  44, 45, 46, 47,  6, 34,  4, 20,  // pads  8..15
  35, 22,  3, 33,  5,  2, 21, 19,  // pads 16..23
  26, 10, 38, 37, 11, 25, 24,  7,  // pads 24..31
  36,  8,  9, 23, 31, 27, 12, 29,  // pads 32..39
  40, 28, 13, 41, 39, 30, 14, 15   // pads 40..47
};

void initLeds() {
  for (uint8_t p = 0; p < NUM_PADS; p++) ledForPad[p] = kLedForPad[p];
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
  // Hard ceiling on strip current: all 42 LEDs white is ~2.5 A, which sags a
  // 5 V rail fed from USB and resets the board. FastLED then dims the frame
  // instead of asking for power that is not there.
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear(true);
}

// Walk one LED at a time in blue, then a white flash. A gap in the sweep is a
// wiring fault; a sweep that runs the wrong way means the strip's first LED is
// at the other end, which is what the pad -> LED table has to match.
void ledStartupTest() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Blue;
    FastLED.show();
    delay(10);
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}

void ledPadFeedback() {
  bool changed = false;
  for (uint8_t p = 0; p < NUM_PADS; p++) {
    const int8_t led = ledForPad[p];
    if (led < 0 || led >= NUM_LEDS) continue;
    const CRGB want = padDown(p) ? CRGB::White : CRGB::Black;
    if (leds[led] != want) {
      leds[led] = want;
      changed = true;
    }
  }
  if (changed) FastLED.show();
}
