#pragma once

#include <Arduino.h>

// Eight AS5600 magnetic encoders behind a TCA9548A I2C mux.
//
//   TCA9548A on the pad bus: Wire, GPIO 10/11
//   one AS5600 per mux channel, all at 0x36 (fixed by the part)
//
// The mux address is set by the A0..A2 straps and therefore lives anywhere in
// 0x70..0x77. This board straps A0 high, so it answers at 0x71 - it used to be
// hardcoded as 0x70, and the probe failed at boot: found stayed 0, the poll
// returned immediately, and the ENCMODE dials never left their placeholder
// values. ENC_MUX_ADDR is only the strap the board is designed for;
// initEncoders() falls back across the rest of the range, so re-strapping the
// mux can no longer switch the encoders off.
//
// The mux sits on the same bus as the four MPR121s (0x5A..0x5D), which are
// wired to the main bus rather than behind a channel, so they stay reachable
// while a channel is open. Every sweep closes the mux again (channel 0 written
// as a mask of zero) so nothing is left selected between polls.
//
// Which mux channel sits behind which panel knob is board routing, not index
// order - see ENC_PORT[] in Encoders.cpp (knob 1 lands on channel 5, knob 2 on
// 7, ...). Reading channel == index mapped every knob onto a different dial.
// SC01_BASE_A5600 does the same with its MUX_PORTS_TCA1..3 tables.
//
// Angles come back as ANGLE (0x0E), the 12-bit value scaled by ZPOS/MPOS, which
// with factory settings is the raw 0..4095. Dividing by 32 gives the 0..127 a
// MIDI CC wants; whether a dial reads the right way round depends on which face
// of the magnet points at the chip, so a dial that counts backwards is a magnet
// orientation problem, not a wiring one - flip the magnet or invert the value
// in the one place it is computed (pollEncoders).

#define NUM_ENCS 8

#define ENC_MUX_ADDR 0x71  // this board's strap (A0 high), not a fixed truth
#define ENC_MUX_ADDR_LO 0x70  // full TCA9548A address range, probed in fallback
#define ENC_MUX_ADDR_HI 0x77
#define ENC_AS5600_ADDR 0x36
#define ENC_POLL_MS 10

#define AS5600_STATUS 0x0B
#define AS5600_ANGLE 0x0E
#define AS5600_AGC 0x1A
#define AS5600_MAGNITUDE 0x1B

// STATUS (0x0B) bits, in the high byte a 2-byte read of 0x0B returns:
// MD magnet detected, ML field too weak, MH field too strong.
#define AS5600_MD 0x20
#define AS5600_ML 0x10
#define AS5600_MH 0x08

extern uint16_t encRaw[NUM_ENCS];    // last 12-bit angle
extern uint8_t encValue[NUM_ENCS];   // same angle as a 0..127 CC value
extern bool encOk[NUM_ENCS];         // channel answered during bring-up

void initEncoders();                 // locate the mux, then probe every channel
void pollEncoders();                 // read all eight, cheap enough for the main loop
uint8_t encodersFound();             // channels that answered, 0..8
bool encMuxPresent();                // did a TCA9548A with sensors behind it answer
uint8_t encMuxAddr();                // the address that answered, 0 if none
void encPrintDiag();                 // per-channel angle, AGC, magnet status
