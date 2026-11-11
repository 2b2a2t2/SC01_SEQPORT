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
