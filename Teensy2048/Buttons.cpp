#include "Config.h"
#include "Buttons.h"

CRGB leds[NUM_LEDS];

const byte padToLedList[42] = {
  13, 16, 14, 46, 12, 44, 38, 24, 36, 29, 41, 25,
   0, 31, 33,  1, 45, 17, 15, 42, 47, 43, 39, 37,
  40, 26, 27, 28, 34, 30, 35, 32, 10, 11,  9,  8,
   7,  5,  4,  2,  3,  6
};
int8_t ledForPad[48];

#ifdef use_SC01Plus
// The panel's pads are not wired in function order, and kLedForPad is the
// calibration that says how: the value is the physical button the pad sits at,
// and on this board button N owns strip LED N (SC01_SEQPORT/Leds.cpp).  So the
// function behind a pad *is* that number; only slots 42..47 - the six spare
// buttons at the end of the panel - have no function.
//
// ledForPad[pad] therefore holds a FUNCTION index (0..41, or -1 for a spare),
// and everything that names a button (buttonNames, group*, the mode colour
// hooks) keeps indexing through it exactly as on the Teensy.  leds[] is the
// strip in function order, which on this board is also wire order, so
// ledsShow() needs no second table.
//
// The first port built ledForPad as the inverse of the Teensy's padToLedList.
// That table describes the *Teensy's* pad wiring, so on this panel it reported
// "D#" for the first step button instead of "Step 1".
static const int8_t kLedForPad[48] = {
  32, 17, 18, 16,  0,  1, 42, 43,  // pads  0.. 7
  44, 45, 46, 47,  6, 34,  4, 20,  // pads  8..15
  35, 22,  3, 33,  5,  2, 21, 19,  // pads 16..23
  26, 10, 38, 37, 11, 25, 24,  7,  // pads 24..31
  36,  8,  9, 23, 31, 27, 12, 29,  // pads 32..39
  40, 28, 13, 41, 39, 30, 14, 15   // pads 40..47
};
// SAFE MODE skips initButtons() (the strip is one of the suspects), and the
// mode layer still repaints on boot - so until the driver exists a show is a
// no-op instead of a call into FastLED with no controller
static bool stripUp = false;
#endif

void ledsShow() {
#ifdef use_SC01Plus
  if (!stripUp) return;
#endif
  FastLED.show();
}

const byte groupSteps[16] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };
const byte groupControls[9] = { 16,17,18,19,20,21,22,23,24 };
const byte groupWhiteKeys[7] = { 25,26,27,28,29,30,31 };
const byte groupMode[3] = { 32,33,34 };
const byte groupTransport[2] = { 35,36 };
const byte groupBlackKeys[5] = { 37,38,39,40,41 };
const byte groupKeyboard[12] = { 25,37,26,38,27,28,39,29,40,30,41,31 };

const char* buttonNames[42] = {
  "Step 1", "Step 2", "Step 3", "Step 4",
  "Step 5", "Step 6", "Step 7", "Step 8",
  "Step 9", "Step 10", "Step 11", "Step 12",
  "Step 13", "Step 14", "Step 15", "Step 16",
  "PREV", "NEXT", "M1", "M2", "M3", "F1", "F2", "F3", "F4",
  "C", "D", "E", "F", "G", "A", "B",
  "ENC", "SEQ", "KEY",
  "PLAY", "REC",
  "C#", "D#", "F#", "G#", "A#"
};

#define EVQ_SIZE 16
struct ButtonEvent { uint8_t pad; uint8_t ev; };
static ButtonEvent evq[EVQ_SIZE];
static uint8_t evqHead = 0, evqTail = 0;

static BtnState btnState[48];
static unsigned long btnDownAt[48];
static unsigned long btnShortRelAt[48];
static bool btnInDoubleWin[48];

static void pushEvent(uint8_t pad, uint8_t ev) {
  uint8_t next = (evqHead + 1) % EVQ_SIZE;
  if (next == evqTail) return;
  evq[evqHead].pad = pad;
  evq[evqHead].ev = ev;
  evqHead = next;
}

bool popEvent(uint8_t &pad, uint8_t &ev) {
  if (evqTail == evqHead) return false;
  pad = evq[evqTail].pad;
  ev  = evq[evqTail].ev;
  evqTail = (evqTail + 1) % EVQ_SIZE;
  return true;
}

static void btnOnPress(uint8_t pad) {
  btnState[pad] = ST_PRESSED;
  btnDownAt[pad] = millis();
  pushEvent(pad, EV_PRESSED);
}

static void btnOnRelease(uint8_t pad) {
  bool wasHold = (btnState[pad] == ST_HOLD);
  btnState[pad] = ST_IDLE;
  unsigned long now = millis();
  pushEvent(pad, EV_RELEASED);

  if (wasHold) {
    btnInDoubleWin[pad] = false;
    return;
  }

  pushEvent(pad, EV_CLICKED);

  if (btnInDoubleWin[pad] && (now - btnShortRelAt[pad] <= BTN_DOUBLE_MS)) {
    pushEvent(pad, EV_DOUBLE);
    btnInDoubleWin[pad] = false;
  } else {
    btnInDoubleWin[pad] = true;
    btnShortRelAt[pad] = now;
  }
}

void btnUpdate() {
  unsigned long now = millis();
  for (uint8_t p = 0; p < 48; p++) {
    if (btnState[p] == ST_PRESSED && (now - btnDownAt[p]) >=
        (ledForPad[p] == 32 ? BTN_HOLD_ENC_MS : BTN_HOLD_MS)) {  // 32 = "ENC"
      btnState[p] = ST_HOLD;
      pushEvent(p, EV_HOLD);
    }
    if (btnInDoubleWin[p] && (now - btnShortRelAt[p]) > BTN_DOUBLE_MS) {
      btnInDoubleWin[p] = false;
    }
  }
}

const char* eventName(uint8_t ev) {
  switch (ev) {
    case EV_PRESSED:  return "pressed";
    case EV_RELEASED: return "released";
    case EV_HOLD:     return "hold";
    case EV_CLICKED:  return "clicked";
    case EV_DOUBLE:   return "double tap";
    default:          return "?";
  }
}

const char* buttonName(uint8_t pad) {
  int8_t led = ledForPad[pad];
  return (led >= 0) ? buttonNames[led] : "Spare";
}

static LedColorHook ledColorHook = nullptr;
static bool padTouched[48] = {false};

void setLedColorHook(LedColorHook hook) {
  ledColorHook = hook;
}

// full repaint through the LED hook (mode colors, playhead, keyboard, ...)
void refreshAllLeds() {
  bool changed = false;
  for (uint8_t p = 0; p < 48; p++) {
    int8_t led = ledForPad[p];
    if (led < 0) continue;
    CRGB c = padTouched[p] ? CRGB::White : CRGB::Black;
    if (ledColorHook) c = ledColorHook(p, padTouched[p], c);
    if (leds[led] != c) { leds[led] = c; changed = true; }
  }
  if (changed) ledsShow();
}

void initButtons() {
#ifdef use_SC01Plus
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  stripUp = true;
  // Hard ceiling on strip current: an all-white strip sags the 5 V rail fed
  // from USB and resets the board, so FastLED dims the frame instead
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
#else
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
#endif
  FastLED.setBrightness(25);
  ledsShow();
#ifdef use_SC01Plus
  // pad -> function: the physical button the pad sits at; spare slots have none
  for (uint8_t p = 0; p < 48; p++) {
    const int8_t slot = kLedForPad[p];
    ledForPad[p] = (slot < 42) ? slot : -1;
  }
#else
  for (uint8_t p = 0; p < 48; p++) ledForPad[p] = -1;
  for (uint8_t l = 0; l < 42; l++) ledForPad[padToLedList[l]] = l;
#endif
}

void handleTouches(uint8_t chip, uint16_t currtouched, uint16_t &lasttouched) {
  bool ledChanged = false;
  for (uint8_t i = 0; i < 12; i++) {
    uint8_t pad = chip * 12 + i;
    bool isTouched = currtouched & _BV(i);
    bool wasTouched = lasttouched & _BV(i);

    if (isTouched && !wasTouched) btnOnPress(pad);
    if (!isTouched && wasTouched) btnOnRelease(pad);
    padTouched[pad] = isTouched;

    int8_t led = ledForPad[pad];
    if (led >= 0) {
      CRGB c = isTouched ? CRGB::White : CRGB::Black;
      if (ledColorHook) c = ledColorHook(pad, isTouched, c);
      if (leds[led] != c) { leds[led] = c; ledChanged = true; }
    }
  }
  lasttouched = currtouched;
  if (ledChanged) ledsShow();
}

void printButtonEvents() {
  uint8_t pad, ev;
  while (popEvent(pad, ev)) {
    Serial.print(buttonName(pad));
    Serial.print(" ");
    Serial.println(eventName(ev));
  }
}
