#include "Config.h"

#ifdef use_SC01Plus

#include "Screen.h"
#include "PanelSC01.h"
#include "SeqClock.h"   // seqStepBreath(): the paint loop hands the beat through

#include <math.h>
#include <stdio.h>
#include <string.h>

// The single panel instance, owned here (PanelSC01.h declares it).
PanelSC01 tft;

/*****************************************************************************
 SC01 Plus (480x320 ST7796) - the layout SC01_SEQPORT.ino was designed on, so
 the ported firmware uses the same screen as the bring-up sketch instead of a
 128x64 picture scaled into the middle of the panel:

   OLED 128x64                  SC01 Plus 480x320
   ------------------------------------------------------------------
   y  0..15  header strip   ->  y   0..33   header (34 px, accent rule)
   (no OLED counterpart)    ->  y  34..67   ENC / SEQ / KEY mode row
   y 16..47  two knob rows  ->  y  68..255  4 columns x 2 rows, 120x94
   y 48..63  control legend ->  y 256..319  5 + 4 slots

 Fonts: the OLED's 6x12 header font maps to font 4 (26 px) and its 4x6 label
 font to font 2 (16 px); the panel uses exactly those two sizes - 26 px for the
 header and the mode row, 16 px for the whole knob grid (labels AND values,
 including the number inside a dial ring) and every legend label - so nothing
 changes size from one cell to the next and a value never swaps face as it
 grows.  The measured widths are why: the widest caption is 13 characters and
 the widest legend 11.

 Everything is immediate mode, as on the Teensy: the scr*() calls record what
 a cell should say and scrUpdate() draws only what changed, so a knob turn
 repaints one dial and not the whole band.
*****************************************************************************/

// ---------------------------------------------------------------------------
// Palette (RGB565) - from SC01_SEQPORT/Display.cpp
// ---------------------------------------------------------------------------
static const uint16_t COL_BG = 0x0000;      // page background
static const uint16_t COL_BAND = 0x18E3;    // header + legend strips
static const uint16_t COL_GRID = 0x2965;    // cell separators
static const uint16_t COL_DIM = 0x8C71;     // captions, legend
static const uint16_t COL_TEXT = 0xFFFF;    // values, header
static const uint16_t COL_RING = 0x630C;    // dial ring
static const uint16_t COL_ACCENT = 0x07FF;  // transient text, dial pointer

// ---------------------------------------------------------------------------
// Type: one family, two fixed sizes, and nothing shrinks to fit.
//
// The panel used to mix FreeSans with the bitmap fonts and let every over-wide
// string drop a size, which is why ACCENT and PROBABILITY - the same legend row
// - came out in different fonts and sizes.  The measured widths decide the two
// sizes here: at 9 pt FreeSans a capital is ~11 px, so PROBABILITY is 118 px and
// INPUT CHANNEL 144 px, wider than the 96 px legend slot and the 120 px cell
// they have to sit in.  The 16 px bitmap face advances ~8 px per character, so
// the widest caption (13) is 104 px inside a 108 px box and the widest legend
// (11) is 88 px inside its slot, with the same face and size for both.
// ---------------------------------------------------------------------------
#define FONT_BIG   4   // 26 px: header and mode row only
#define FONT_SMALL 2   // 16 px: everything in the knob grid - labels AND
                       // values - plus the legend labels.  The grid has one
                       // size so a cell never changes face when its value does

// ---------------------------------------------------------------------------
// Layout: header 0..33, mode row 34..67, knob band 68..255, legend 256..319
// ---------------------------------------------------------------------------
#define PANEL_W       480
#define PANEL_H       320
#define HEADER_H       34
#define MODE_Y       HEADER_H
#define MODE_H         34
#define MAIN_Y       (MODE_Y + MODE_H)          // 68
#define CELL_W       (PANEL_W / 4)              // 120
#define CELL_H         94
#define LEGEND_Y     (MAIN_Y + CELL_H * 2)      // 256
#define LEGEND_SLOT_H ((PANEL_H - LEGEND_Y) / 2)  // 32, one per legend row
// The legend rows are justified from the measured label widths (see
// drawLegendRow), so there is no fixed column width any more.
#define PAD_X           6                       // cell left inset
#define TEXT_W       (CELL_W - PAD_X * 2)       // 108: 13 characters at 16 px
#define LABEL_Y        24                       // caption top inside a cell
#define VALUE_Y        44                       // value top inside a cell

// Dial geometry inside a cell (ring outer radius 26, caption in the notch)
#define DIAL_CY         44
#define DIAL_R          26
#define DIAL_CAPTION_Y  82
// Region repainted when only a dial's value changed: ring, pointer at its
// farthest (R+4 plus the 2 px stroke) and the value text. From the cell centre
// that is 32 px sideways/up and 23 px down, so it stops short of the cell's
// left edge, its top edge and the caption: the grid lines and the label are
// never erased by an encoder step.
#define DIAL_CLR        (DIAL_R + 6)    // 32
#define DIAL_CLR_DOWN   (DIAL_R - 3)    // 23

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
struct Cell {
  char label[18];   // caption / CC number / device parameter name
  char value[16];   // value text (unused in dial mode)
  bool dial;
  uint8_t dialValue;
};

static Cell cells[SCR_CELLS];
static char legendText[SCR_LEGENDS][18];
static bool legendSel[SCR_LEGENDS];   // the armed F1..F4 radio / open M menu

static char headerLeft[24] = "";
static char headerRight[40] = "";
static bool headerTransient = false;

static char modeLabel[3][10] = { "ENCODER", "SEQUENCER", "KEYBOARD" };
static uint8_t activeMode = SCR_MODE_NONE;

static bool headerDirty = true;
static bool modeDirty = true;
static bool mainDirty = true;
// One flag per cell and per legend slot: a single band-wide flag repainted all
// eight cells on every value write, which on a panel with no framebuffer reads
// as a flash - the live dials write four times a second.
static bool cellDirty[SCR_CELLS];
static bool dialDirty[SCR_CELLS];   // only the dial moved, not the whole cell
static bool legendDirty;            // the whole strip: justified, so one row moves together

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void copyText(char* dst, size_t cap, const char* src) {
  if (!src) src = "";
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

// ---------------------------------------------------------------------------
// Full names
// The label vocabulary comes from the 128x64 firmware, where a caption had
// 32 px and had to be an abbreviation.  A cell here is 120x94, so the panel
// spells the tokens out instead of sharing the short spelling - Bindings.cpp
// and Display.cpp keep the tables the OLED needs, and this table is applied
// where the caption is drawn.  Only real words are expanded: MIDI's own terms
// (CC, BPM), the physical key names (M1..M4, F1..F4, PREV, NEXT), the chord
// names (sus4, m7, 9) and tokens whose meaning is not certain (MOD, SD) are
// left exactly as they are.
// ---------------------------------------------------------------------------
struct FullName { const char* from; const char* to; };
static const FullName kFullNames[] = {
  // SEQUENCER rows
  { "OCT",    "OCTAVE" },
  { "LEN",    "LENGTH" },
  { "INCH",   "INPUT CHANNEL" },
  { "AMT",    "AMOUNT" },
  { "EXT IN", "EXT INPUT" },
  { "SRC",    "SOURCE" },
  { "VEL",    "VELOCITY" },
  { "PROB",   "PROBABILITY" },
  { "BPM",    "TEMPO" },
  { "TRANS",  "TRANSPOSE" },
  { "KLOCK",  "KEY LOCK" },
  // KEYMODE 2 / 3 / 4
  { "SPRD",   "SPREAD" },
  { "DENS",   "DENSITY" },
  { "FSCALE", "FORCE SCALE" },
  { "ENCOD",  "ENCODER" },
  { "KBOARD", "KEYBOARD" },
  { "PERF",   "PERFORMANCE" },
  { "TENS",   "TENSION" },
  { "CAD",    "CADENCE" },
  { "CPX",    "COMPLEXITY" },
  { "SURP",   "SURPRISE" },
  { "SUB",    "SUBSTITUTION" },
  { "INV",    "INVERSION" },
  { "PAS",    "PASSING" },
  { "COL",    "COLOR" },
  { "EMB",    "EMBED" },
  { "DEG",    "DEGREE" },
  { "GRAV",   "GRAVITY" },
  { "MODTGT", "MOD TARGET" },
  { "MODLEN", "MOD LENGTH" },
  { "CADSTR", "CAD STRENGTH" },
  { "CADWIN", "CAD WINDOW" },
  { "DIAT",   "DIATONIC" },
  { "UNRES",  "UNRESOLVED" },
  { "PARAL",  "PARALLEL" },
  { "DOUBL",  "DOUBLE" },
  { "CMNTN",  "COMMON TONE" },
  { "ANTE",   "ANTECEDENT" },
  { "VAR",    "VARIANT" },
  // ARP / GNR8 / LEAD / SKANK pages
  { "RNG",    "RANGE" },
  { "ALGO",   "ALGORITHM" },
  { "BRW",    "BORROW" },
  { "ACC",    "ACCENT" },
  { "GEN",    "GENERATE" },
  { "GNR8",   "GENERATE" },
  { "ARP",    "ARPEGGIO" },
  { "ORD",    "ORDER" },
  { "MOT",    "MOTION" },
  { "OPT",    "OPTIONS" },
  { "OPTS",   "OPTIONS" },
  { "CT",     "CHORD TONE" },
  { "GD",     "GUIDE TONE" },
  { "TES",    "TESSITURA" },
  { "TES%",   "TESSITURA %" },
  { "NOT%",   "NOTE %" },
  { "SHP",    "SHAPE" },
  { "THN",    "THINNING" },
  { "EXT",    "EXTENSION" },
  { "FX",     "EFFECTS" },
  // ENCABLETONMODE navigation
  { "BANK<",  "PREV BANK" },
  { "BANK>",  "NEXT BANK" },
  { "DEV<",   "PREV DEVICE" },
  { "DEV>",   "NEXT DEVICE" },
};

// The full spelling for a caption/legend token, or the token itself.
static const char* fullName(const char* s) {
  if (!s) return "";
  for (uint8_t i = 0; i < sizeof(kFullNames) / sizeof(kFullNames[0]); i++)
    if (strcmp(s, kFullNames[i].from) == 0) return kFullNames[i].to;
  return s;
}

// One text run at the grid's single size.  Labels and values share it: a long
// value used to drop from the 26 px face to the 16 px one while its neighbours
// kept theirs, so a cell changed font as a knob turned.  Nothing shrinks to fit
// here - the widest label and value a cell can carry both measure under 108 px
// at 16 px.
static void drawText(const char* s, int x, int y, uint16_t color, uint16_t bg) {
  if (!s || !s[0]) return;
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  tft.setTextFont(FONT_SMALL);
  tft.setTextColor(color, bg);
  tft.drawString(s, x, y);
}

// ---------------------------------------------------------------------------
// CC dial: 270-degree arc open at 6 o'clock, 0 at the lower-left end and 127
// at the lower-right one, a pointer across the rim and the value inside the
// ring.  Angles here are screen degrees: 0 = 3 o'clock, growing clockwise, so
// the ring runs 135 -> 405 and the bottom quarter (45..135) stays empty.
// ---------------------------------------------------------------------------
static void drawDial(int cellX, int cellY, uint8_t val) {
  const float D2R = 0.0174533f;
  const int cx = cellX + CELL_W / 2;
  const int cy = cellY + DIAL_CY;

  // The ring is 272 two-pixel drawLine calls.  Batched into one bus
  // transaction they cost a fraction of what one transaction each did - the
  // difference is loop() time, and loop() time is sequencer time.
  tft.startWrite();
  for (int a = 135; a <= 405; a += 2) {
    const float a0 = (float)a * D2R;
    const float a1 = (float)(a + 2) * D2R;
    for (int r = DIAL_R - 2; r <= DIAL_R - 1; r++) {
      tft.drawLine(cx + (int)lroundf(cosf(a0) * r), cy + (int)lroundf(sinf(a0) * r),
                   cx + (int)lroundf(cosf(a1) * r), cy + (int)lroundf(sinf(a1) * r),
                   COL_RING);
    }
  }

  const float av = (135.0f + 270.0f * (float)val / 127.0f) * D2R;
  const int xi = cx + (int)lroundf(cosf(av) * (DIAL_R - 6));
  const int yi = cy + (int)lroundf(sinf(av) * (DIAL_R - 6));
  const int xo = cx + (int)lroundf(cosf(av) * (DIAL_R + 4));
  const int yo = cy + (int)lroundf(sinf(av) * (DIAL_R + 4));
  const int px = (int)lroundf(cosf(av + 1.5708f));
  const int py = (int)lroundf(sinf(av + 1.5708f));
  tft.drawLine(xi, yi, xo, yo, COL_ACCENT);
  tft.drawLine(xi + px, yi + py, xo + px, yo + py, COL_ACCENT);
  tft.endWrite();

  char v[4];
  snprintf(v, sizeof(v), "%u", (unsigned)val);
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  tft.setTextFont(FONT_SMALL);  // the grid's one size: the caption's own face
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(v, cx, cy);
}

// ---------------------------------------------------------------------------
// Bands
// ---------------------------------------------------------------------------
static void drawHeader() {
  tft.fillRect(0, 0, PANEL_W, HEADER_H - 2, COL_BAND);
  tft.fillRect(0, HEADER_H - 2, PANEL_W, 2, COL_ACCENT);

  tft.setTextDatum(lgfx::textdatum_t::top_left);
  tft.setTextFont(FONT_BIG);
  int leftW = 0;
  if (headerLeft[0]) {
    tft.setTextColor(COL_TEXT, COL_BAND);
    tft.drawString(headerLeft, 12, 4);
    leftW = tft.textWidth(headerLeft);
  }

  const char* right = headerRight;
  if (!right[0]) return;

  // A temporary message owns the right side; a long one takes the whole strip
  // and the left label is then not drawn underneath it.
  int avail = PANEL_W - 24 - (leftW ? leftW + 16 : 0);
  tft.setTextFont(FONT_BIG);
  int noteY = 4;
  if (tft.textWidth(right) > avail) {
    tft.setTextFont(FONT_SMALL);
    noteY = 9;  // the caption size sits a little lower in the band
  }
  tft.setTextColor(headerTransient ? COL_ACCENT : COL_DIM, COL_BAND);
  tft.setTextDatum(lgfx::textdatum_t::top_right);
  tft.drawString(right, PANEL_W - 12, noteY);
}

// Mode row: three always-visible mode buttons, the active one filled.
static void drawModes() {
  tft.fillRect(0, MODE_Y, PANEL_W, MODE_H, COL_BG);
  tft.drawFastHLine(0, MODE_Y + MODE_H - 1, PANEL_W, COL_GRID);

  const int bw = PANEL_W / 3;  // 160
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  for (uint8_t i = 0; i < 3; i++) {
    const int bx = i * bw + 4;
    const int by = MODE_Y + 4;
    const int bwv = bw - 8;
    const int bh = MODE_H - 8;
    const bool on = (i == activeMode);
    tft.fillRoundRect(bx, by, bwv, bh, 6, on ? COL_ACCENT : COL_BAND);
    tft.setTextColor(on ? COL_BG : COL_DIM, on ? COL_ACCENT : COL_BAND);
    tft.setTextFont(FONT_BIG);
    tft.drawString(modeLabel[i], bx + bwv / 2, by + bh / 2);
  }
  tft.setTextFont(FONT_SMALL);  // the panel's default size for later draws
}

static int cellX(uint8_t i) { return (i & 3) * CELL_W; }
static int cellY(uint8_t i) { return MAIN_Y + (i >> 2) * CELL_H; }

// Repaint one knob cell and nothing else. The fill covers the separators that
// run along this cell's left and top edges, so those two get drawn back.
static void drawCell(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);
  const uint8_t col = i & 3;
  const uint8_t row = i >> 2;

  tft.fillRect(x, y, CELL_W, CELL_H, COL_BG);
  if (cells[i].dial) {
    drawDial(x, y, cells[i].dialValue);
    tft.setTextDatum(lgfx::textdatum_t::middle_center);
    tft.setTextFont(FONT_SMALL);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(cells[i].label, x + CELL_W / 2, y + DIAL_CAPTION_Y);
  } else {
    drawText(cells[i].label, x + PAD_X, y + LABEL_Y, COL_DIM, COL_BG);
    drawText(cells[i].value, x + PAD_X, y + VALUE_Y, COL_TEXT, COL_BG);
  }
  if (col > 0) tft.drawFastVLine(x, y + 8, CELL_H - 16, COL_GRID);
  if (row > 0) tft.drawFastHLine(x, y, CELL_W, COL_GRID);
}

// Repaint just the dial of a cell whose value moved. drawCell() would fill the
// whole cell first, which erases the grid lines along its edges and its
// caption and redraws them a moment later - the outline flickers on every
// encoder step. This touches neither.
static void drawDialOnly(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);
  const int cx = x + CELL_W / 2;
  const int cy = y + DIAL_CY;
  tft.fillRect(cx - DIAL_CLR, cy - DIAL_CLR, DIAL_CLR * 2 + 1,
               DIAL_CLR + DIAL_CLR_DOWN + 1, COL_BG);
  drawDial(x, y, cells[i].dialValue);
}

// ---------------------------------------------------------------------------
// Control legend: the same 5 + 4 labels as the OLED strip, justified.
//
// Fixed 96 px slots gave PROBABILITY (11 characters) 4 px of air and TIE (3)
// 34 px, and once the labels were spelled out the long ones ran into their
// neighbours.  Each row is now laid out from the measured widths: one gap
// before the first label, between every pair and after the last, so every
// label gets the same breathing room whatever its length.  The positions depend
// on the text, so a change repaints the whole strip (2 rows, ~2 ms) rather than
// one slot - it only happens on a mode or page change, never on a knob turn.
//
// One label can be ARMED: the chosen F radio, or the M button whose menu is
// open.  It is filled with the accent and printed dark - the same treatment the
// mode row gives the active mode - so the band says what the pads already say,
// since a chosen F pad burns white (Modes.cpp).  The fill does not move any
// label: the layout still comes from the measured widths alone.
// ---------------------------------------------------------------------------
static void drawLegendRow(uint8_t first, uint8_t count, int yCenter) {
  int w[5];
  int sum = 0;
  tft.setTextFont(FONT_SMALL);
  for (uint8_t k = 0; k < count; k++) {
    const char* s = legendText[first + k];
    w[k] = s[0] ? (int)tft.textWidth(s) : 0;
    sum += w[k];
  }
  int gap = (PANEL_W - sum) / (count + 1);
  if (gap < 8) gap = 8;   // tightest row (the ABLETON bank/device pair)
  int x = gap;
  tft.setTextDatum(lgfx::textdatum_t::middle_left);
  for (uint8_t k = 0; k < count; k++) {
    if (legendText[first + k][0]) {
      if (legendSel[first + k]) {
        tft.fillRoundRect(x - 7, yCenter - 13, w[k] + 14, 26, 6, COL_ACCENT);
        tft.setTextColor(COL_BG, COL_ACCENT);
        tft.drawString(legendText[first + k], x, yCenter);
        tft.setTextColor(COL_DIM, COL_BAND);
      } else {
        tft.drawString(legendText[first + k], x, yCenter);
      }
    }
    x += w[k] + gap;
  }
}

static void drawLegendBand() {
  tft.fillRect(0, LEGEND_Y, PANEL_W, PANEL_H - LEGEND_Y, COL_BAND);
  tft.setTextColor(COL_DIM, COL_BAND);
  drawLegendRow(0, 5, LEGEND_Y + LEGEND_SLOT_H / 2);
  drawLegendRow(5, 4, LEGEND_Y + LEGEND_SLOT_H + LEGEND_SLOT_H / 2);
}

// ---------------------------------------------------------------------------
// Screen interface
// ---------------------------------------------------------------------------
void scrBegin() {
  tft.init();
  tft.setRotation(3);  // landscape 480x320
  tft.setBrightness(255);
  tft.setTextWrap(false);
  tft.fillScreen(COL_BG);
  headerDirty = true;
  modeDirty = true;
  mainDirty = true;
}

// Backlight: the PWM light on GPIO 45 takes 0..255, the page speaks percent
void scrSetBrightness(uint8_t percent) {
  if (percent > 100) percent = 100;
  tft.setBrightness((uint8_t)((uint16_t)percent * 255 / 100));
}

// Header tap: a touch that STARTS inside the header band, edge-detected so a
// held finger is one tap and a drag that began elsewhere never counts.  The
// I2C read is throttled - this runs in loop() - and a missing touch
// controller (getTouch() returns 0) simply never taps.
bool scrTapHeader() {
  static bool down = false;
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < 30) return false;
  lastMs = now;
  uint16_t x = 0, y = 0;
  bool touching = tft.getTouch(&x, &y) > 0;
  bool tap = touching && !down && (int)y < HEADER_H;
  down = touching;
  return tap;
}

void scrClear() {
  for (uint8_t i = 0; i < SCR_CELLS; i++) {
    cells[i].label[0] = '\0';
    cells[i].value[0] = '\0';
    cells[i].dial = false;
    cells[i].dialValue = 0;
    cellDirty[i] = false;
    dialDirty[i] = false;
  }
  for (uint8_t i = 0; i < SCR_LEGENDS; i++) {
    legendText[i][0] = '\0';
    legendSel[i] = false;
  }
  legendDirty = false;
  mainDirty = true;   // one band repaint draws the blank model back
}

// Nothing to blank: there is no framebuffer.  The model holds what is on the
// panel, so the comparisons in the setters below are what decide how little is
// repainted - blanking it here would mark all eight cells and every legend
// slot dirty and repaint the whole band on a single knob turn.
void scrFrameBegin() {}

void scrHeader(const char* left, const char* right, bool transient) {
  char l[sizeof(headerLeft)];
  char r[sizeof(headerRight)];
  copyText(l, sizeof(l), left);
  copyText(r, sizeof(r), right);
  if (strcmp(l, headerLeft) == 0 && strcmp(r, headerRight) == 0 &&
      transient == headerTransient)
    return;
  memcpy(headerLeft, l, sizeof(l));
  memcpy(headerRight, r, sizeof(r));
  headerTransient = transient;
  headerDirty = true;
}

void scrModeRow(uint8_t active) {
  if (active >= 3) active = SCR_MODE_NONE;
  if (active == activeMode) return;
  activeMode = active;
  modeDirty = true;
}

void scrCellLabel(uint8_t i, const char* text) {
  if (i >= SCR_CELLS) return;
  char buf[sizeof(cells[i].label)];
  copyText(buf, sizeof(buf), fullName(text));
  // a text label replaces a dial (same rule as the value setter)
  if (!cells[i].dial && strcmp(buf, cells[i].label) == 0) return;
  memcpy(cells[i].label, buf, sizeof(buf));
  cells[i].dial = false;
  cellDirty[i] = true;
}

void scrCellLabelSmall(uint8_t i, const char* text) {
  // The panel has room for the caption font, so a device short name and an
  // ordinary label are drawn alike here.
  scrCellLabel(i, text);
}

void scrCellValue(uint8_t i, const char* text) {
  if (i >= SCR_CELLS) return;
  char buf[sizeof(cells[i].value)];
  copyText(buf, sizeof(buf), text);
  if (!cells[i].dial && strcmp(buf, cells[i].value) == 0) return;
  memcpy(cells[i].value, buf, sizeof(buf));
  cells[i].dial = false;
  cellDirty[i] = true;
}

void scrDial(uint8_t i, const char* caption, uint8_t value) {
  if (i >= SCR_CELLS) return;
  if (value > 127) value = 127;
  char buf[sizeof(cells[i].label)];
  copyText(buf, sizeof(buf), fullName(caption));
  const bool dialOnly = cells[i].dial && strcmp(buf, cells[i].label) == 0;
  if (dialOnly && value == cells[i].dialValue) return;
  memcpy(cells[i].label, buf, sizeof(buf));
  cells[i].value[0] = '\0';
  cells[i].dial = true;
  cells[i].dialValue = value;
  // Same caption, already a dial: only the pointer moved, so mark just the
  // dial. A caption change (or a text cell becoming a dial) needs the cell.
  if (dialOnly) dialDirty[i] = true;
  else cellDirty[i] = true;
}

void scrLegend(uint8_t i, const char* text) {
  if (i >= SCR_LEGENDS) return;
  char buf[sizeof(legendText[i])];
  copyText(buf, sizeof(buf), fullName(text));
  if (strcmp(buf, legendText[i]) == 0) return;
  memcpy(legendText[i], buf, sizeof(buf));
  legendDirty = true;
}

// Armed label: filled in the band.  Only the flag changes here - the strip is
// justified, so the whole band has to be repainted for one label to grow a
// background (it happens on a page change, not on a knob turn).
void scrLegendSel(uint8_t i, bool on) {
  if (i >= SCR_LEGENDS) return;
  if (legendSel[i] == on) return;
  legendSel[i] = on;
  legendDirty = true;
}

// Paint every stale region in this pass, so the panel stays immediate - but hand
// the sequencer through between regions (seqStepBreath).  A mode change makes
// the mode row, all eight cells and the legend stale at once, which is tens of
// milliseconds of SPI (one cell's fill alone is 22 KB).  loop() is what advances
// the sequencer's steps, so the step that came due inside that frame used to
// wait for the last region: switching ENCODER <-> SEQUENCER nudged the clock for
// one beat.  Painting one region per pass fixed the beat and made the screen
// crawl, so the frame goes out in one pass again and the dispatch in between is
// what keeps the beat on time (one region of latency, not one frame).
void scrUpdate() {
  if (modeDirty) {
    drawModes();
    modeDirty = false;
    seqStepBreath();
  }
  if (mainDirty) {
    // No band-wide fill: the eight cell repaints cover MAIN_Y..255 completely
    // and each one draws its own separators back
    for (uint8_t i = 0; i < SCR_CELLS; i++) {
      cellDirty[i] = true;
      dialDirty[i] = false;
    }
    legendDirty = true;
    mainDirty = false;
  }
  for (uint8_t i = 0; i < SCR_CELLS; i++) {
    if (cellDirty[i]) {
      drawCell(i);
      cellDirty[i] = false;
      dialDirty[i] = false;  // the full repaint drew the dial too
      seqStepBreath();
    } else if (dialDirty[i]) {
      drawDialOnly(i);
      dialDirty[i] = false;
      seqStepBreath();
    }
  }
  if (legendDirty) {
    drawLegendBand();
    legendDirty = false;
    seqStepBreath();
  }
  if (headerDirty) {
    drawHeader();
    headerDirty = false;
  }
}

#endif // use_SC01Plus
