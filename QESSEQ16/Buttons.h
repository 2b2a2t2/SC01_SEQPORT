#pragma once

#include "Config.h"
#include <Arduino.h>
#include <FastLED.h>

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

#ifdef use_SC01Plus
#define NUM_LEDS 50      // strip length; leds[] is indexed by button, see below
#define DATA_PIN 12
// The eight pixels added along the stripe, one behind each AS5600 knob: the
// LAST eight in wire order (42..49).  They belong to no pad - the 42 button
// pixels are 0..41 and the six function-less buttons carry no LED, so
// initButtons() maps those pads to -1 and the pad layer never writes here.
// Modes.cpp paints them with the active MODE family's colour, so the hardware
// says which family the knobs are driving before the screen is read.  If the
// pixels were soldered in somewhere else on the chain, only ENC_LED_FIRST
// moves.
#define ENC_LEDS 8
#define ENC_LED_FIRST (NUM_LEDS - ENC_LEDS)   // 42
#else
#define NUM_LEDS 42
#define DATA_PIN 6
#define ENC_LEDS 0
#define ENC_LED_FIRST NUM_LEDS
#endif

#define BTN_HOLD_MS   300
#define BTN_HOLD_ENC_MS 150   // ENC mode button fires HOLD sooner: the
                              // momentary ENCMODE3 comes up twice as fast
#define BTN_DOUBLE_MS 300

enum BtnEvent : uint8_t { EV_NONE=0, EV_PRESSED, EV_RELEASED, EV_HOLD, EV_CLICKED, EV_DOUBLE };
enum BtnState  : uint8_t { ST_IDLE=0, ST_PRESSED, ST_HOLD };

extern CRGB leds[NUM_LEDS];
extern int8_t ledForPad[48];

// push leds[] to the strip: a no-op wrapper around FastLED.show() on the
// Teensy, on the SC01 it also remaps button order -> strip order
void ledsShow();

extern const byte padToLedList[42];

#define STEPS_NUM_G 16
extern const byte groupSteps[16];

#define CONTROLS_NUM_G 9
extern const byte groupControls[9];

#define WHITE_KEYS_NUM_G 7
extern const byte groupWhiteKeys[7];

#define MODE_NUM_G 3
extern const byte groupMode[3];

#define TRANSPORT_NUM_G 2
extern const byte groupTransport[2];

#define BLACK_KEYS_NUM_G 5
extern const byte groupBlackKeys[5];

#define KEYBOARD_NUM_G 12
extern const byte groupKeyboard[12];

extern const char* buttonNames[42];

// optional per-pad LED color override (return defaultColor to keep normal)
typedef CRGB (*LedColorHook)(uint8_t pad, bool isTouched, CRGB defaultColor);
void setLedColorHook(LedColorHook hook);

void initButtons();
void refreshAllLeds();   // repaint every pad via hook; shows only on change
void handleTouches(uint8_t chip, uint16_t currtouched, uint16_t &lasttouched);
void btnUpdate();
bool popEvent(uint8_t &pad, uint8_t &ev);
const char* eventName(uint8_t ev);
const char* buttonName(uint8_t pad);
void printButtonEvents();
