#pragma once

#include <Arduino.h>

#define NUM_ENCODERS 8

void initEncoders();
void pollEncoders();                              // call every loop
bool popEncEvent(uint8_t &idx, int16_t &delta);   // drain deltas
long encPosition(uint8_t idx);                    // current accumulated position

// Shared drain for every knob whose value is a percentage or another wide
// range: 1 count = 1 unit - the same rate as the ENCMODE CC knobs - so a
// 0-100% sweep is about one turn (4 units per detent), and a spin, which the
// Encoder library delivers as a bigger delta per poll, is worth up to 4x
// that.  `res` is kept for the call sites' shape; nothing carries below one
// count any more.
int16_t encDrain(int16_t counts, int16_t *res);

// ENCMODE3: MIDI channel of the encoder CCs (1..16, default 1); selected
// on the step-pad channel overlay (yellow theme like KEY3/SEQ3)
void encSetChannel(uint8_t ch);
uint8_t encChannel();
