#include "Encoders.h"

#include <string.h>
#include <Wire.h>

uint16_t encRaw[NUM_ENCS];
uint8_t encValue[NUM_ENCS];
bool encOk[NUM_ENCS];

static uint8_t found = 0;
static uint8_t muxAddr = 0;  // TCA9548A that answered, 0 = none on the bus

// Mux channel behind each panel position, measured on this board: entry i is
// the channel that knob i+1 is wired to, so reading ENC_PORT[i] puts knob i+1
// on dial i+1. The routing is NOT in channel order - treating channel == index
// made every knob drive a different dial (knob 1 moved dial 6, knob 2 moved
// dial 8, knob 3 moved dial 7, ...). This is the same idea as
// MUX_PORTS_TCA1..3 in SC01_BASE_A5600, which never assumes pot index == mux
// channel either. To re-map a knob, edit here, not the display.
//
// Measured on this board: knobs 1..4 -> channels 5,7,6,4 and knob 8 -> channel
// 1 were right; knobs 5,6,7 came back one place out (knob 5 drove dial 6,
// knob 6 dial 7, knob 7 dial 5), so their three entries are rotated here.
static const uint8_t ENC_PORT[NUM_ENCS] = {5, 7, 6, 4, 2, 3, 0, 1};

// The mux control register is a channel mask: write 1 << channel to connect it,
// 0 to disconnect everything.
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
// adopting one that has no sensor behind it would trade this failure for the
// next one. So a candidate is only taken once an AS5600 answers on one of its
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

    for (uint8_t ch = 0; ch < NUM_ENCS; ch++) {
      if (!muxSelect(addr, ch)) continue;
      if (probe(ENC_AS5600_ADDR)) {
        muxNone(addr);
        return addr;
      }
    }
    muxNone(addr);
  }
  return 0;
}

void initEncoders() {
  found = 0;
  muxAddr = 0;
  memset(encOk, 0, sizeof(encOk));
  for (uint8_t i = 0; i < NUM_ENCS; i++) {
    encRaw[i] = 0;
    encValue[i] = 0;
  }

  muxAddr = findMuxWithEncoder();
  if (muxAddr == 0) {
    Serial.println("no TCA9548A with an AS5600 behind it in 0x70..0x77 - encoders disabled");
    return;
  }
  Serial.printf("TCA9548A at 0x%02X\n", muxAddr);

  for (uint8_t ch = 0; ch < NUM_ENCS; ch++) {
    if (!muxSelect(muxAddr, ENC_PORT[ch])) continue;
    if (!probe(ENC_AS5600_ADDR)) {
      Serial.printf("AS5600 missing behind panel encoder %u (mux channel %u)\n",
                    (unsigned)(ch + 1), (unsigned)ENC_PORT[ch]);
      continue;
    }
    // A chip that answers but cannot see a magnet still counts as present; the
    // message is the difference between "not connected" and "magnet too far".
    // readReg16 fetches 0x0B and 0x0C together, so STATUS is the high byte and
    // the low byte is RAW ANGLE(11:8) - testing that one reports on the knob's
    // position instead of on the magnet. MD = bit 5, ML = 4, MH = 3.
    uint16_t status = 0;
    if (readReg16(ENC_AS5600_ADDR, AS5600_STATUS, &status)) {
      const uint8_t st = (uint8_t)(status >> 8);
      if (!(st & AS5600_MD))
        Serial.printf("no magnet behind panel encoder %u (ch%u), status 0x%02X%s%s\n",
                      (unsigned)(ch + 1), (unsigned)ENC_PORT[ch], st,
                      (st & AS5600_ML) ? " ML" : "",
                      (st & AS5600_MH) ? " MH" : "");
    }
    encOk[ch] = true;
    found++;
  }
  muxNone(muxAddr);

  Serial.printf("encoders %u/%u\n", (unsigned)found, (unsigned)NUM_ENCS);
}

void pollEncoders() {
  if (found == 0) return;

  for (uint8_t ch = 0; ch < NUM_ENCS; ch++) {
    if (!encOk[ch]) continue;
    if (!muxSelect(muxAddr, ENC_PORT[ch])) continue;
    uint16_t angle = 0;
    if (readReg16(ENC_AS5600_ADDR, AS5600_ANGLE, &angle)) {
      angle &= 0x0FFF;
      encRaw[ch] = angle;
      encValue[ch] = (uint8_t)(angle >> 5);
    }
  }
  muxNone(muxAddr);
}

uint8_t encodersFound() { return found; }

bool encMuxPresent() { return muxAddr != 0; }

uint8_t encMuxAddr() { return muxAddr; }

void encPrintDiag() {
  Serial.printf("encoders: %u/%u present, mux @0x%02X\n", (unsigned)found,
                (unsigned)NUM_ENCS, (unsigned)muxAddr);
  if (muxAddr == 0) return;

  for (uint8_t ch = 0; ch < NUM_ENCS; ch++) {
    if (!encOk[ch]) {
      Serial.printf("  enc%u -> ch%u  --\n", (unsigned)(ch + 1),
                    (unsigned)ENC_PORT[ch]);
      continue;
    }
    muxSelect(muxAddr, ENC_PORT[ch]);
    const uint16_t angle = encRaw[ch];
    uint16_t status = 0;
    uint16_t agc = 0;
    const bool statusOk = readReg16(ENC_AS5600_ADDR, AS5600_STATUS, &status);
    const bool agcOk = readReg16(ENC_AS5600_ADDR, AS5600_AGC, &agc);
    // AGC is one byte at 0x1A and this read also picks up 0x1B, so the value
    // that belongs to AGC is the high byte - and STATUS, read from 0x0B, sits
    // in the high byte for the same reason (0x0C is RAW ANGLE). A failed read
    // is called out instead of being printed as a plausible-looking zero.
    if (!statusOk || !agcOk) {
      Serial.printf("  enc%u -> ch%u  angle %4u  I2C READ FAILED\n",
                    (unsigned)(ch + 1), (unsigned)ENC_PORT[ch], (unsigned)angle);
      continue;
    }
    const uint8_t st = (uint8_t)(status >> 8);
    Serial.printf(
                  "  enc%u -> ch%u  angle %4u  cc %3u  agc %3u  status 0x%02X  magnet %s%s%s\n",
                  (unsigned)(ch + 1), (unsigned)ENC_PORT[ch], (unsigned)angle,
                  (unsigned)encValue[ch], (unsigned)(agc >> 8), (unsigned)st,
                  (st & AS5600_MD) ? "ok" : "MISSING",
                  (st & AS5600_ML) ? " tooWeak" : "",
                  (st & AS5600_MH) ? " tooStrong" : "");
  }
  muxNone(muxAddr);
}
