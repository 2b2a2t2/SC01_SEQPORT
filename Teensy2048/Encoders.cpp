#include "Config.h"
#include "Encoders.h"
#include "Events.h"    // encPageRecall - per-channel F bank + page group

#include <string.h>

#ifdef use_SC01Plus
#include <Wire.h>      // AS5600 knobs behind a TCA9548A mux, on the pad bus
#else
#include <Encoder.h>
#endif

#ifdef use_Teensy

Encoder enc1(40, 39);
Encoder enc2(36, 35);
Encoder enc3(34, 33);
Encoder enc4(31, 32);
Encoder enc5(38, 37);
Encoder enc6(26, 25);
Encoder enc7(27, 28);
Encoder enc8(29, 30);

static Encoder* encoders[NUM_ENCODERS] = {
  &enc1, &enc2, &enc3, &enc4, &enc5, &enc6, &enc7, &enc8
};
static long encPrev[NUM_ENCODERS];

#else // use_SC01Plus -------------------------------------------------------
// Eight AS5600 magnetic knobs, one per mux channel, all at 0x36 (fixed by
// the part), reached through a TCA9548A on the pad bus (Wire = GPIO 10/11).
// Ported from SC01_SEQPORT/Encoders.cpp, then converted from the 0..127 CC
// value it exposes into the accumulated counts the Encoder library hands the
// rest of this module, so every caller keeps its Teensy semantics.
//
// The mux address comes from the A0..A2 straps: this board answers at 0x71,
// but a re-strapped board must not silently switch the encoders off, so
// initEncoders() falls back across the whole 0x70..0x77 range and only
// adopts a candidate once an AS5600 answers behind one of its channels.
//
// ENC_PORT[] is the measured knob -> mux channel routing; it is NOT in
// channel order (knob 1 is on channel 5, knob 2 on 7, ...), so re-mapping a
// knob means editing this table, not the display.
#define ENC_MUX_ADDR      0x71   // this board's strap (A0 high)
#define ENC_MUX_ADDR_LO   0x70   // full TCA9548A range, probed in fallback
#define ENC_MUX_ADDR_HI   0x77
#define ENC_AS5600_ADDR   0x36
#define AS5600_ANGLE      0x0E   // 12-bit angle, scaled by ZPOS/MPOS

// One revolution is 4096 angle units -> 96 counts here, so one detent is
// 4 counts exactly like the quadrature knobs on the Teensy (encDrain and
// every "4 counts = 1 unit" caller keep their rate).  ENC_INVERT is +1 or
// -1: flip it if a knob counts the wrong way round (magnet orientation).
#define ENC_COUNTS_PER_REV 96
#define ENC_INVERT 1

static const uint8_t ENC_PORT[NUM_ENCODERS] = {5, 7, 6, 4, 2, 3, 0, 1};

static uint8_t muxAddr = 0;             // the TCA9548A that answered, 0 = none
static bool encOk[NUM_ENCODERS];         // channel answered during bring-up
static long encPos[NUM_ENCODERS];        // accumulated counts (encPosition)
static long encRemain[NUM_ENCODERS];     // fraction of a count, in 4096ths
static uint16_t encAngle[NUM_ENCODERS];  // last raw angle, for the wrap test
static bool encAngleSeen[NUM_ENCODERS];

// The mux control register is a channel mask: write 1 << channel to connect
// it, 0 to disconnect everything (every sweep leaves the mux closed, so
// nothing stays selected between polls).
static bool muxSelect(uint8_t addr, uint8_t ch) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(1u << ch));
  return Wire.endTransmission() == 0;
}

static void muxNone(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)0);
  Wire.endTransmission();
}

static bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static bool readReg16(uint8_t addr, uint8_t reg, uint16_t* out) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // repeated start
  if (Wire.requestFrom((uint8_t)addr, (uint8_t)2) != 2) return false;
  const uint8_t hi = Wire.read();
  const uint8_t lo = Wire.read();
  *out = ((uint16_t)hi << 8) | lo;
  return true;
}

// An address only proves something is sitting there, not that it is the mux -
// adopting one with no sensor behind it would trade this failure for the next
// one.  A candidate is only taken once an AS5600 answers on one of its
// channels, which also makes a mis-strapped board self-correcting.
static uint8_t findMuxWithEncoder() {
  const uint8_t candidates[] = {
      ENC_MUX_ADDR, ENC_MUX_ADDR_LO, 0x72, 0x73,
      0x74, 0x75, 0x76, ENC_MUX_ADDR_HI};

  for (uint8_t i = 0; i < sizeof(candidates); i++) {
    const uint8_t addr = candidates[i];
    bool seen = false;  // the designed strap is also inside the range
    for (uint8_t j = 0; j < i; j++)
      if (candidates[j] == addr) seen = true;
    if (seen || !probe(addr)) continue;

    for (uint8_t ch = 0; ch < NUM_ENCODERS; ch++) {
      if (!muxSelect(addr, ENC_PORT[ch])) continue;
      if (probe(ENC_AS5600_ADDR)) {
        muxNone(addr);
        return addr;
      }
    }
    muxNone(addr);
  }
  return 0;
}

#endif // use_SC01Plus ------------------------------------------------------

#define ENCQ_SIZE 16
struct EncEvent { uint8_t idx; int16_t delta; };
static EncEvent encq[ENCQ_SIZE];
static uint8_t encqHead = 0, encqTail = 0;

static void pushEncEvent(uint8_t idx, int16_t delta) {
  uint8_t next = (encqHead + 1) % ENCQ_SIZE;
  if (next == encqTail) return;
  encq[encqHead].idx = idx;
  encq[encqHead].delta = delta;
  encqHead = next;
}

bool popEncEvent(uint8_t &idx, int16_t &delta) {
  if (encqTail == encqHead) return false;
  idx = encq[encqTail].idx;
  delta = encq[encqTail].delta;
  encqTail = (encqTail + 1) % ENCQ_SIZE;
  return true;
}

void initEncoders() {
#ifdef use_SC01Plus
  muxAddr = 0;
  memset(encOk, 0, sizeof(encOk));
  memset(encPos, 0, sizeof(encPos));
  memset(encRemain, 0, sizeof(encRemain));
  memset(encAngle, 0, sizeof(encAngle));
  memset(encAngleSeen, 0, sizeof(encAngleSeen));

  muxAddr = findMuxWithEncoder();
  if (muxAddr == 0) {
    Serial.println("no TCA9548A with an AS5600 behind it in 0x70..0x77 - encoders disabled");
    return;
  }
  Serial.printf("TCA9548A at 0x%02X\n", muxAddr);

  uint8_t found = 0;
  for (uint8_t ch = 0; ch < NUM_ENCODERS; ch++) {
    if (!muxSelect(muxAddr, ENC_PORT[ch])) continue;
    if (!probe(ENC_AS5600_ADDR)) {
      Serial.printf("AS5600 missing behind panel encoder %u (mux channel %u)\n",
                    (unsigned)(ch + 1), (unsigned)ENC_PORT[ch]);
      continue;
    }
    encOk[ch] = true;
    found++;
  }
  muxNone(muxAddr);
  Serial.printf("encoders %u/%u\n", (unsigned)found, (unsigned)NUM_ENCODERS);
#else
  for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
    encPrev[i] = encoders[i]->read();
  }
#endif
}

void pollEncoders() {
#ifdef use_SC01Plus
  if (muxAddr == 0) return;

  for (uint8_t ch = 0; ch < NUM_ENCODERS; ch++) {
    if (!encOk[ch]) continue;
    if (!muxSelect(muxAddr, ENC_PORT[ch])) continue;
    uint16_t angle = 0;
    if (!readReg16(ENC_AS5600_ADDR, AS5600_ANGLE, &angle)) continue;
    angle &= 0x0FFF;
    if (!encAngleSeen[ch]) {           // first reading: take it as the origin
      encAngleSeen[ch] = true;
      encAngle[ch] = angle;
      continue;
    }
    // wrap-aware: the 12-bit angle crosses 4095 -> 0 mid-turn, so the delta
    // has to be folded into -2048..2047 before it means anything
    int32_t d = (int32_t)angle - (int32_t)encAngle[ch];
    if (d > 2048) d -= 4096;
    else if (d < -2048) d += 4096;
    encAngle[ch] = angle;
    // scale by accumulating the fraction: 96 counts per 4096 angle units, so
    // the leftover stays in encRemain instead of rounding a slow turn away
    // one poll at a time
    encRemain[ch] += d * ENC_COUNTS_PER_REV;      // 4096ths of a count
    const long step = encRemain[ch] / 4096;
    encRemain[ch] -= step * 4096;
    const long pos = encPos[ch] + step * ENC_INVERT;
    long delta = pos - encPos[ch];
    if (delta == 0) continue;
    encPos[ch] = pos;
    if (delta > 32767) delta = 32767;
    if (delta < -32767) delta = -32767;
    pushEncEvent(ch, (int16_t)delta);
  }
  muxNone(muxAddr);
#else
  for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
    long pos = encoders[i]->read();
    long d = pos - encPrev[i];
    if (d != 0) {
      encPrev[i] = pos;
      if (d > 32767) d = 32767;
      if (d < -32767) d = -32767;
      pushEncEvent(i, (int16_t)d);
    }
  }
#endif
}

long encPosition(uint8_t idx) {
  if (idx >= NUM_ENCODERS) return 0;
#ifdef use_SC01Plus
  return encPos[idx];
#else
  return encoders[idx]->read();
#endif
}

// 0-100% dials: 1 COUNT = 1 unit, the same rate the ENCMODE CC knobs use,
// so a full sweep is about one turn instead of four (the old 4-counts rate
// was unreadable on the hardware - the polls come far faster than a detent,
// so a speed heuristic never saw more than one count and could not help).
// A spin still gets more: the library batches a fast turn into a bigger
// delta, so the step grows with the counts this poll saw.  `res` is kept
// only for the call sites' shape - there is nothing to carry below one
// count any more.
int16_t encDrain(int16_t counts, int16_t *res) {
  (void)res;
  if (counts > 64) counts = 64;            // a wild spin: no need to amplify
  if (counts < -64) counts = -64;
  int16_t a = (counts < 0) ? (int16_t)-counts : counts;
  // thresholds are in counts and one detent is 4, so a normal single-detent
  // turn never amplifies: 1 detent per poll = 1x, two = 2x, and so on
  int16_t mul = (a <= 4) ? 1 : (a <= 8) ? 2 : (a <= 16) ? 3 : 4;
  return (int16_t)(counts * mul);
}

// ENCMODE3: channel of the encoder CCs
static uint8_t encCh = 1;

void encSetChannel(uint8_t ch) {
  if (ch < 1 || ch > 16 || ch == encCh) return;
  encCh = ch;
  encPageRecall();   // the new channel takes its own last F bank + group
}

uint8_t encChannel() { return encCh; }
