#include "Config.h"

#ifdef use_Teensy

#include "Screen.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/*****************************************************************************
 Teensy 4.1 / SSD1306 128x64 - the layout every screen in this firmware was
 designed on, drawn exactly as Display.cpp used to draw it inline:

   y  0..15   header strip (6x12): left label, right note
   y 16..47   eight label/value cells, 4 columns x 2 rows (32 px columns)
              row 1: label y22, value y30   row 2: label y38, value y46
   y 48..63   nine control legend slots: five on top (y54), F1..F4 below (y62)
              ENCMODE's dial page puts eight CC dials in the same band

 It only moved here so the same content logic can drive the 480x320 panel
 (ScreenPanel.cpp) without a second copy of the layout code.
*****************************************************************************/

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#define SCR_W       128
#define SCR_H        64
#define SCR_HEADER_H 16
#define SCR_COL_W    32

static bool bufferDirty = true;   // something changed since the last send
static bool sending = false;      // a frame is going out, one page per call
static uint8_t sendPage = 0;

static void mark() { bufferDirty = true; }

// The 128x64 header has no backlight dimmer and no touchscreen: the settings
// page that the panel's header tap opens cannot be reached here, so these two
// exist only to keep the interface one shape for both panels
void scrSetBrightness(uint8_t percent) { (void)percent; }
bool scrTapHeader() { return false; }

void scrBegin() {
  u8g2.begin();
  Wire.setClock(400000);
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, SCR_W, SCR_H);
  u8g2.setDrawColor(1);
  mark();
}

void scrClear() {
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, SCR_HEADER_H, SCR_W, SCR_H - SCR_HEADER_H);
  u8g2.setDrawColor(1);
  mark();
}

// Framebuffer backend: a new frame starts from a blank buffer.
void scrFrameBegin() {
  scrClear();
}

void scrHeader(const char* left, const char* right, bool transient) {
  if (!left) left = "";
  if (!right) right = "";
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, SCR_W, SCR_HEADER_H);
  u8g2.setDrawColor(1);

  if (transient && right[0]) {
    // a long message is pinned left so its start stays readable
    int x = SCR_W - (int)u8g2.getStrWidth(right) - 2;
    if (x < 2) x = 2;
    u8g2.drawStr(x, 13, right);
  } else if (right[0]) {
    int r = SCR_W - (int)u8g2.getStrWidth(right) - 2;
    if (r < 0) r = 0;
    u8g2.drawStr(r, 13, right);
    // the left label yields to the persistent right text
    if (left[0] && u8g2.getStrWidth(left) + 1 <= r) u8g2.drawStr(0, 13, left);
  } else if (left[0]) {
    u8g2.drawStr(2, 13, left);
  }
  mark();
}

void scrModeRow(uint8_t) {}   // 128x64: the family is named in the header

static int cellX(uint8_t i) { return (i & 3) * SCR_COL_W + 1; }
static int cellLabelY(uint8_t i) { return (i >> 2) ? 38 : 22; }
static int cellValueY(uint8_t i) { return (i >> 2) ? 46 : 30; }

void scrCellLabel(uint8_t i, const char* text) {
  if (i >= SCR_CELLS || !text) return;
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cellX(i), cellLabelY(i), text);
  mark();
}

void scrCellLabelSmall(uint8_t i, const char* text) {
  if (i >= SCR_CELLS || !text) return;
  u8g2.setFont(u8g2_font_micro_tr);   // ENCABLETONMODE's device short names
  u8g2.drawStr(cellX(i), cellLabelY(i), text);
  mark();
}

void scrCellValue(uint8_t i, const char* text) {
  if (i >= SCR_CELLS || !text) return;
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(cellX(i), cellValueY(i), text);
  mark();
}

// one CC knob: 270-degree ring open at 6 o'clock, a pointer at the value and
// the number itself in the middle.  u8g2's Andres arc takes 0..255 with 0 at
// 3 o'clock, so the bottom quarter is 160..224 and 224..160 draws the 270
// degrees wanted here
static void drawCcDial(uint8_t cx, uint8_t cy, uint8_t val) {
  u8g2.drawArc(cx, cy, 9, 224, 160);
  const float d2r = 0.0174533f;
  float av = (225.0f + 270.0f * (float)val / 127.0f) * d2r;
  int xo = (int)cx + (int)lroundf(sinf(av) * 10.0f);
  int yo = (int)cy - (int)lroundf(cosf(av) * 10.0f);
  int xi = (int)cx + (int)lroundf(sinf(av) * 7.0f);
  int yi = (int)cy - (int)lroundf(cosf(av) * 7.0f);
  int px = (int)lroundf(cosf(av));   // across the tick: (cos, sin) is the
  int py = (int)lroundf(sinf(av));   // radial direction turned 90 degrees
  if (py > 0) { px = -px; py = -py; }   // "up" = away from the caption
  u8g2.drawLine(xi, yi, xo, yo);
  u8g2.drawLine(xi + px, yi + py, xo + px, yo + py);
  char v[4];
  snprintf(v, sizeof(v), "%u", (unsigned)val);
  int w = (int)u8g2.getStrWidth(v);
  int half = (int)(u8g2.getAscent() - u8g2.getDescent()) / 2;
  u8g2.drawStr((int)cx - w / 2, (int)cy + half, v);
}

void scrDial(uint8_t i, const char* caption, uint8_t value) {
  if (i >= SCR_CELLS) return;
  const uint8_t col = i & 3;
  const uint8_t row = i >> 2;
  const uint8_t cx = (uint8_t)(col * SCR_COL_W + SCR_COL_W / 2);
  const uint8_t cy = row ? 50 : 26;
  drawCcDial(cx, cy, value > 127 ? 127 : value);
  if (caption && caption[0]) {
    u8g2.setFont(u8g2_font_micro_tr);
    u8g2.drawStr((int)cx - (int)u8g2.getStrWidth(caption) / 2,
                 row ? 63 : 39, caption);
  }
  mark();
}

// Level bar (ENCMODE4 FLAVOR = BARS): a 28 x 4 track across the cell, filled to
// the value, the number over it and the CC caption under it - the same two
// baselines the label/value cells use, so the row keeps the dial page's shape.
void scrCellBar(uint8_t i, const char* caption, uint8_t value) {
  if (i >= SCR_CELLS) return;
  if (value > 127) value = 127;
  const uint8_t row = i >> 2;
  const int x = cellX(i) + 1;
  const int y = row ? 43 : 19;

  u8g2.setDrawColor(1);
  u8g2.drawFrame(x, y, 28, 4);
  const int w = (int)value * 26 / 127;
  if (w > 0) u8g2.drawBox(x + 1, y + 1, w, 2);

  char v[4];
  snprintf(v, sizeof(v), "%u", (unsigned)value);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(x + 14 - (int)u8g2.getStrWidth(v) / 2, row ? 55 : 31, v);
  if (caption && caption[0]) {
    u8g2.setFont(u8g2_font_micro_tr);
    u8g2.drawStr(x + 14 - (int)u8g2.getStrWidth(caption) / 2, row ? 63 : 39,
                 caption);
  }
  mark();
}

// Exponential fall from 1 at u = 0 to 0 at u = 1 (see the panel's expFall).
static float expFallOled(float u) {
  const float k = 0.0183156f;   // e^-4
  if (u <= 0.0f) return 1.0f;
  if (u >= 1.0f) return 0.0f;
  return (expf(-4.0f * u) - k) / (1.0f - k);
}

// Envelope segments, same placement as the panel's: laid out left to right, so
// the release's end moves with the release knob and stops short of the edge.
#define ENV_EDGE_OLED 0.88f
static void envSegmentsOled(const uint8_t v[4], float* a, float* decayEnd,
                            float* relStart, float* relEnd) {
  *a = 0.05f + 0.30f * ((float)v[0] / 127.0f);
  const float d = 0.04f + 0.16f * ((float)v[1] / 127.0f);
  *decayEnd = *a + d;

  *relStart = *decayEnd + 0.12f;
  *relEnd = *relStart + 0.05f + 0.16f * ((float)v[3] / 127.0f);
  if (*relEnd > ENV_EDGE_OLED) {
    *relEnd = ENV_EDGE_OLED;
    if (*relStart > *relEnd - 0.02f) *relStart = *relEnd - 0.02f;
  }
}

// The ADSR level at x (0..1 across the row): attack rises to full over its own
// slice, decay bends down to the sustain level, sustain holds, release bends
// down to zero.  Same curve as the panel's, in this screen's own geometry.
// Negative = past the end of the envelope.
static float envLevelOled(const uint8_t v[4], float x) {
  float a, decayEnd, relStart, relEnd;
  envSegmentsOled(v, &a, &decayEnd, &relStart, &relEnd);
  const float s = (float)v[2] / 127.0f;

  if (x <= 0.0f) return 0.0f;
  if (x < a) return a > 0.0f ? x / a : 1.0f;
  if (x < decayEnd) return s + (1.0f - s) * expFallOled((x - a) / (decayEnd - a));
  if (x < relStart) return s;
  if (x < relEnd) return s * expFallOled((x - relStart) / (relEnd - relStart));
  return -1.0f;
}

// LPF24 response for the row's left half, mirroring the panel's: the corner
// follows the cutoff value, the peak the resonance one.
static float filterLevelOled(float f, uint8_t cutoff, uint8_t res) {
  const float dec = f * 3.0f;                                 // log axis
  const float f0 = 0.25f + 2.10f * ((float)cutoff / 127.0f);  // corner, decades
  const float r = (float)res / 127.0f;
  const float dd = dec - f0;
  const float x = powf(10.0f, dd < 0.3f ? dd : 0.3f);
  const float x8 = x * x * x * x * x * x * x * x;
  const float x16 = x8 * x8;
  const float mag = 1.0f / sqrtf(1.0f + x16 * x16 * x16);     // brick wall
  const float width = (dd < 0.0f) ? (0.10f + 0.08f * r) : (0.03f + 0.02f * r);
  const float d = dd / width;
  // same shelf / bump / headroom as the panel's
  float lvl = 0.55f * mag + 0.42f * r * expf(-d * d);
  if (lvl > 0.97f) lvl = 0.97f;
  return lvl;
}

// The envelope row (ENCMODE4 FLAVOR = ENVELOPE), split in two on the 128 px
// strip as well: the filter (LPF24), a gap, then the ADSR.  Only the ADSR
// carries bars and CC numbers - the filter's two values are the dials above it.
// The strip is a framebuffer, so each curve is drawn once rather than per cell.
void scrEnvRow(const char* const envCaps[4], const uint8_t envVals[4],
               uint8_t cutoff, uint8_t resonance) {
  uint8_t v[4];
  for (uint8_t k = 0; k < 4; k++) v[k] = envVals[k] > 127 ? 127 : envVals[k];

  const int half = SCR_W / 2;   // 64: filter | envelope
  const int fltW = half - 16;   // 48: the filter, then a 16 px gap
  const int top = 36;           // curve band: 36..48
  const int h = 13;

  // left: the LPF24, no readout under it.  The curve stops where the response
  // runs out (no zero line trailing towards the envelope)
  int prevY = top + h - 1;
  bool fltJoined = false;
  for (int px = 0; px < fltW; px++) {
    const float f = (float)px / (float)(fltW - 1);
    const float lvl = filterLevelOled(f, cutoff, resonance);
    if (lvl < 0.015f) {
      fltJoined = false;
      continue;
    }
    const int py = top + (int)lroundf((1.0f - lvl) * (float)(h - 1));
    if (fltJoined) u8g2.drawLine(px - 1, prevY, px, py);
    prevY = py;
    fltJoined = true;
  }

  // right half: the ADSR, ending before the edge instead of on it
  prevY = top + h - 1;
  bool envJoined = false;
  for (int px = half; px < SCR_W; px++) {
    const float t = (float)(px - half) / (float)(half - 1);
    const float lvl = envLevelOled(v, t);
    if (lvl < 0.0f) {
      envJoined = false;
      continue;
    }
    const int py = top + (int)lroundf((1.0f - lvl) * (float)(h - 1));
    if (envJoined) u8g2.drawLine(px - 1, prevY, px, py);
    prevY = py;
    envJoined = true;
  }

  // readouts: the ADSR's four only - the filter's numbers are the first two
  // columns' dials above it
  for (uint8_t k = 0; k < 4; k++) {
    const int x = half + (int)k * (half / 4) + 1;
    const int w = half / 4 - 2;
    u8g2.drawFrame(x, 51, w, 3);
    const int fill = (int)v[k] * (w - 2) / 127;
    if (fill > 0) u8g2.drawBox(x + 1, 52, fill, 1);
  }

  // captions: the ADSR's four CC numbers
  u8g2.setFont(u8g2_font_micro_tr);
  for (uint8_t k = 0; k < 4; k++) {
    if (!envCaps[k] || !envCaps[k][0]) continue;
    const int x = half + (int)k * (half / 4) + (half / 8);
    u8g2.drawStr(x - (int)u8g2.getStrWidth(envCaps[k]) / 2, 63, envCaps[k]);
  }
  mark();
}

// The LFO row (FLAVOR = LFO), laid out like the envelope row: the FILTER on the
// left (the same LPF24, same 48 px), a 16 px blank band, then the still
// waveform on the right half with its four readouts along the bottom - DEPTH
// 75% / RATE 0.9HZ / SHAPE TRI / SYNC HZ.  Same mapping as the panel's
// (Screen.h owns it): depth sets the height, rate the cycles, shape the
// waveform, sync how they line up.
void scrLfoRow(const char* const lfoCaps[4], const char* const lfoVals[4],
               uint8_t depth, uint8_t rate, uint8_t shape, uint8_t sync,
               uint8_t cutoff, uint8_t resonance) {
  const int half = SCR_W / 2;   // 64: filter | LFO
  const int fltW = half - 16;   // 48: the filter, then the blank band
  const int top = 36;           // curve band: 36..48, as the envelope's
  const int h = 13;
  const int mid = top + h / 2;

  // left: the same LPF24 the envelope row draws, no readout under it
  int prevY = top + h - 1;
  bool fltJoined = false;
  for (int px = 0; px < fltW; px++) {
    const float f = (float)px / (float)(fltW - 1);
    const float lvl = filterLevelOled(f, cutoff, resonance);
    if (lvl < 0.015f) {
      fltJoined = false;
      continue;
    }
    const int py = top + (int)lroundf((1.0f - lvl) * (float)(h - 1));
    if (fltJoined) u8g2.drawLine(px - 1, prevY, px, py);
    prevY = py;
    fltJoined = true;
  }

  // right: the LFO across 64..127
  const uint8_t sy = lfoSync(sync);
  const float amp = ((float)lfoClamp7(depth) / 127.0f) * (float)(h / 2);
  const int lfoW = SCR_W - half;       // 64 px of LFO

  // beat-locked: a rule per cycle boundary, drawn before the curve.  TRIG gets
  // one at the end of its one shot, so the flat line after it reads as the end
  // of the burst rather than as a missing curve.
  if (sy == 1) {
    float cycles = roundf(lfoCycles(rate));
    if (cycles < 1.0f) cycles = 1.0f;
    for (uint8_t c = 1; (float)c < cycles; c++) {
      const int tx = half + (int)lroundf((float)c / cycles * (float)(lfoW - 1));
      u8g2.drawLine(tx, top, tx, top + h - 1);
    }
  } else if (sy == 2) {
    const int tx = half + (int)lroundf(lfoTrigSpan(rate) * (float)(lfoW - 1));
    u8g2.drawLine(tx, top, tx, top + h - 1);
  }

  u8g2.drawLine(half, mid, SCR_W - 1, mid);   // the zero line under the curve

  prevY = mid;
  bool joined = false;
  for (int px = half; px < SCR_W; px++) {
    const float u = (float)(px - half) / (float)(lfoW - 1);
    const float v = lfoSyncedWave(u, rate, shape, sync);
    const int py = mid - (int)lroundf(v * amp);
    if (joined) u8g2.drawLine(px - 1, prevY, px, py);
    prevY = py;
    joined = true;
  }

  // readouts: caption (micro) on one line, value (4x6) under it, per column.
  // The panel puts both on one line; 128 px cannot - four label+value pairs need
  // ~150 px even in the micro face - so the strip keeps the caption above the
  // value in its four 32 px columns.
  for (uint8_t k = 0; k < 4; k++) {
    const int x = (int)k * SCR_COL_W + 1;
    const int w = SCR_COL_W - 2;
    u8g2.setFont(u8g2_font_micro_tr);
    if (lfoCaps[k] && lfoCaps[k][0]) u8g2.drawStr(x, 56, lfoCaps[k]);
    u8g2.setFont(u8g2_font_4x6_tr);
    if (lfoVals[k] && lfoVals[k][0])
      u8g2.drawStr(x + w - (int)u8g2.getStrWidth(lfoVals[k]), 63, lfoVals[k]);
  }
  mark();
}

void scrLegend(uint8_t i, const char* text) {
  if (i >= SCR_LEGENDS || !text) return;
  u8g2.setFont(u8g2_font_4x6_tr);
  const int x = (i < 5 ? i : i - 5) * 25 + 1;
  u8g2.drawStr(x, i < 5 ? 54 : 62, text);
  mark();
}

// The 128x64 strip is 25 px per slot at 4x6: a filled label there would have
// to lose the text it is pointing at.  The pads show the armed control.
void scrLegendSel(uint8_t i, bool on) {
  (void)i;
  (void)on;
}

// Paged write: one 128-byte page (~4ms @400kHz) per call so the MIDI pump runs
// between pages - a monolithic sendBuffer stalls ~37ms, longer than one MIDI
// clock at 120BPM, and the burst poisons uClock's external interval.
static void sendOledPage(uint8_t page) {
  uint8_t addr = u8g2.getU8x8()->i2c_address >> 1;  // u8x8 stores 8-bit (0x78)
  uint8_t cmds[3] = { (uint8_t)(0xB0 | page), 0x00, 0x10 };  // page, col 0
  Wire.beginTransmission(addr);
  Wire.write(0x00);            // control byte: commands follow
  Wire.write(cmds, 3);
  Wire.endTransmission();
  Wire.beginTransmission(addr);
  Wire.write(0x40);            // control byte: GDDRAM data follows
  Wire.write(u8g2.getBufferPtr() + (uint16_t)page * 128, 128);
  Wire.endTransmission();
}

void scrUpdate() {
  if (!sending) {
    if (!bufferDirty) return;
    sending = true;
    sendPage = 0;
    bufferDirty = false;
  }
  sendOledPage(sendPage);
  if (++sendPage >= 8) sending = false;
}

#endif // use_Teensy
