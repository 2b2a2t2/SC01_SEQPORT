#include "Display.h"

#include "PanelSC01.h"

// ---------------------------------------------------------------------------
// Palette (RGB565)
// ---------------------------------------------------------------------------
static const uint16_t COL_BG = 0x0000;      // page background
static const uint16_t COL_BAND = 0x18E3;    // header + legend strips
static const uint16_t COL_GRID = 0x2965;    // cell separators
static const uint16_t COL_DIM = 0x8C71;     // captions, legend
static const uint16_t COL_TEXT = 0xFFFF;    // values, header
static const uint16_t COL_RING = 0x630C;    // dial ring
static const uint16_t COL_ACCENT = 0x07FF;  // transient text, dial pointer

// ---------------------------------------------------------------------------
// Layout: header 0..33, mode row 34..67, knob band 68..255, legend 256..319
// ---------------------------------------------------------------------------
static const int CELL_W = SCREEN_W / 4;            // 120
static const int CELL_H = 94;
static const int MAIN_Y = MODE_Y + MODE_H;         // 68
static const int LEGEND_Y = MAIN_Y + CELL_H * 2;   // 256
static const int LEGEND_COL_W = SCREEN_W / 5;      // 96
static const int PAD_X = 10;                       // cell left inset
static const int TEXT_W = CELL_W - PAD_X * 2;      // usable text width
static const int LABEL_Y = 24;                     // caption top inside a cell
static const int VALUE_Y = 44;                     // value top inside a cell

// Dial geometry inside a cell (ring outer radius 26, caption in the notch)
static const int DIAL_CY = 44;
static const int DIAL_R = 26;
static const int DIAL_CAPTION_Y = 82;
// Region repainted when only a dial's value changed: ring, pointer at its
// farthest (R+4 plus the 2 px stroke) and the value text. From the cell centre
// that is 32 px sideways/up and 23 px down, so it stops 28 px short of the
// cell's left edge, 12 px below its top edge and 7 px above the caption: the
// grid lines and the label are never erased by an encoder step.
static const int DIAL_CLR = DIAL_R + 6;       // 32
static const int DIAL_CLR_DOWN = DIAL_R - 3;  // 23

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
struct Cell {
  char label[14];   // caption / CC number
  char value[16];   // text value (unused in dial mode)
  bool dial;
  uint8_t dialValue;
};

static Cell cells[CELL_COUNT];
static char legendText[LEGEND_COUNT][10];

static char headerLeft[24] = "";
static char headerText[40] = "";
static char headerStick[24] = "";
static char modeLabel[MODE_COUNT][10];
static uint8_t activeMode = MODE_NONE;
static bool modeDirty = true;
static unsigned long headerDeadline = 0;
static bool headerActive = false;
static bool headerStickOn = false;
static bool headerDirty = true;
// One flag per cell and per legend slot. A single band-wide flag meant every
// value write repainted all eight cells plus the whole legend strip, which on a
// panel with no framebuffer reads as a flash; the live dials write four times a
// second, so the whole grid blinked. mainDirty stays for the full repaints.
static bool cellDirty[CELL_COUNT];
static bool dialDirty[CELL_COUNT];  // only the dial moved, not the whole cell
static bool legendDirty[LEGEND_COUNT];
static bool mainDirty = true;

static bool touchDown = false;
static uint16_t touchX = 0;
static uint16_t touchY = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void copyText(char* dst, size_t cap, const char* src) {
  if (!src) src = "";
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

// One text run; drops from the 26 px font to the 16 px one when the string
// would not fit its column (so a long word stays readable, never clipped).
static void drawText(const char* s, int x, int y, int maxW, uint16_t color,
                     uint16_t bg, uint8_t font) {
  if (!s || !s[0]) return;
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  tft.setTextFont(font);
  if (font != 2 && tft.textWidth(s) > maxW) tft.setTextFont(2);
  tft.setTextColor(color, bg);
  tft.drawString(s, x, y);
}

static void drawTouchMark() {
  if (!touchDown) return;
  // Only over the knob band: a marker redrawn outside it would leave pixels
  // on the header, mode row or legend that nothing clears.
  if (touchY < MAIN_Y || touchY >= LEGEND_Y) return;
  tft.drawFastHLine((int)touchX - 12, touchY, 25, COL_ACCENT);
  tft.drawFastVLine(touchX, (int)touchY - 12, 25, COL_ACCENT);
  tft.drawCircle(touchX, touchY, 6, COL_TEXT);
}

// ---------------------------------------------------------------------------
// CC dial - port of drawCcDial(): the OLED's 9 px ring, scaled to the cell.
// A 270-degree arc open at 6 o'clock, 0 at the lower-left end and 127 at the
// lower-right one, a pointer across the rim and the value inside the ring.
// Angles here are screen degrees: 0 = 3 o'clock, growing clockwise, so the
// ring runs 135 -> 405 and the bottom quarter (45..135) stays empty.
// ---------------------------------------------------------------------------
static void drawDial(int cellX, int cellY, uint8_t val) {
  const float D2R = 0.0174533f;
  const int cx = cellX + CELL_W / 2;
  const int cy = cellY + DIAL_CY;

  // Ring: 2 px thick, drawn as 2-degree segments at radii R-2 and R-1.
  for (int a = 135; a <= 405; a += 2) {
    const float a0 = (float)a * D2R;
    const float a1 = (float)(a + 2) * D2R;
    for (int r = DIAL_R - 2; r <= DIAL_R - 1; r++) {
      tft.drawLine(cx + (int)lroundf(cosf(a0) * r), cy + (int)lroundf(sinf(a0) * r),
                   cx + (int)lroundf(cosf(a1) * r), cy + (int)lroundf(sinf(a1) * r),
                   COL_RING);
    }
  }

  // Pointer: from inside the rim to one pixel outside it, two pixels wide.
  const float av = (135.0f + 270.0f * (float)val / 127.0f) * D2R;
  const int xi = cx + (int)lroundf(cosf(av) * (DIAL_R - 6));
  const int yi = cy + (int)lroundf(sinf(av) * (DIAL_R - 6));
  const int xo = cx + (int)lroundf(cosf(av) * (DIAL_R + 4));
  const int yo = cy + (int)lroundf(sinf(av) * (DIAL_R + 4));
  const int px = (int)lroundf(cosf(av + 1.5708f));
  const int py = (int)lroundf(sinf(av + 1.5708f));
  tft.drawLine(xi, yi, xo, yo, COL_ACCENT);
  tft.drawLine(xi + px, yi + py, xo + px, yo + py, COL_ACCENT);

  // Value centred in the ring, CC number in the ring's bottom notch. Font 2
  // (16 px), not font 4: at 26 px "127" all but filled the 52 px ring.
  char v[4];
  snprintf(v, sizeof(v), "%u", (unsigned)val);
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(v, cx, cy);
}

// ---------------------------------------------------------------------------
// Bands
// ---------------------------------------------------------------------------
static void drawHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H - 2, COL_BAND);
  tft.fillRect(0, HEADER_H - 2, SCREEN_W, 2, COL_ACCENT);

  tft.setTextDatum(lgfx::textdatum_t::top_left);
  tft.setTextFont(4);
  int leftW = 0;
  if (headerLeft[0]) {
    tft.setTextColor(COL_TEXT, COL_BAND);
    tft.drawString(headerLeft, 12, 4);
    leftW = tft.textWidth(headerLeft);
  }

  const char* right = headerActive && headerText[0] ? headerText
                      : (headerStickOn ? headerStick : "");
  if (!right[0]) return;

  // A temporary message owns the right side; a long one takes the whole strip
  // and the left label is then not drawn underneath it.
  int avail = SCREEN_W - 24 - (leftW ? leftW + 16 : 0);
  tft.setTextFont(4);
  int noteY = 4;
  if (tft.textWidth(right) > avail) {
    tft.setTextFont(2);
    noteY = 9;  // centre the smaller note optically in the band
  }
  tft.setTextColor(headerActive ? COL_ACCENT : COL_DIM, COL_BAND);
  tft.setTextDatum(lgfx::textdatum_t::top_right);
  tft.drawString(right, SCREEN_W - 12, noteY);
}

// Mode row: three always-visible mode buttons, the active one filled.
static void drawModes() {
  tft.fillRect(0, MODE_Y, SCREEN_W, MODE_H, COL_BG);
  tft.drawFastHLine(0, MODE_Y + MODE_H - 1, SCREEN_W, COL_GRID);

  const int bw = SCREEN_W / MODE_COUNT;  // 160
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    const int bx = i * bw + 4;
    const int by = MODE_Y + 4;
    const int bwv = bw - 8;
    const int bh = MODE_H - 8;
    const bool on = (i == activeMode);
    tft.fillRoundRect(bx, by, bwv, bh, 6, on ? COL_ACCENT : COL_BAND);
    tft.setTextColor(on ? COL_BG : COL_DIM, on ? COL_ACCENT : COL_BAND);
    tft.setFont(&lgfx::fonts::FreeSans12pt7b);
    if (tft.textWidth(modeLabel[i]) > bwv - 12) tft.setTextFont(2);
    tft.drawString(modeLabel[i], bx + bwv / 2, by + bh / 2);
  }
  tft.setTextFont(2);  // leave a bitmap font selected for later draws
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
    tft.setTextFont(2);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(cells[i].label, x + CELL_W / 2, y + DIAL_CAPTION_Y);
  } else {
    drawText(cells[i].label, x + PAD_X, y + LABEL_Y, TEXT_W, COL_DIM, COL_BG, 2);
    drawText(cells[i].value, x + PAD_X, y + VALUE_Y, TEXT_W, COL_TEXT, COL_BG, 4);
  }
  if (col > 0) tft.drawFastVLine(x, y + 8, CELL_H - 16, COL_GRID);
  if (row > 0) tft.drawFastHLine(x, y, CELL_W, COL_GRID);
}

// Repaint just the dial of a cell whose value moved. drawCell() would fill the
// whole 120x94 cell first, which erases the grid lines along its edges and its
// caption and redraws them a moment later - the outline flickers on every
// encoder step. This touches neither: it clears the small box around the ring
// and draws the dial back into it.
static void drawDialOnly(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);
  const int cx = x + CELL_W / 2;
  const int cy = y + DIAL_CY;
  tft.fillRect(cx - DIAL_CLR, cy - DIAL_CLR, DIAL_CLR * 2 + 1,
               DIAL_CLR + DIAL_CLR_DOWN + 1, COL_BG);
  drawDial(x, y, cells[i].dialValue);
}

static const int LEGEND_SLOT_H = (SCREEN_H - LEGEND_Y) / 2;  // 32

static int legendSlotX(uint8_t i) { return (i < 5 ? i : i - 5) * LEGEND_COL_W; }
static int legendSlotY(uint8_t i) { return LEGEND_Y + (i < 5 ? 0 : LEGEND_SLOT_H); }

static void drawLegendSlot(uint8_t i) {
  tft.fillRect(legendSlotX(i), legendSlotY(i), LEGEND_COL_W, LEGEND_SLOT_H, COL_BAND);
  if (!legendText[i][0]) return;

  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  tft.setTextColor(COL_DIM, COL_BAND);
  tft.setFont(&lgfx::fonts::FreeSans12pt7b);
  if (tft.textWidth(legendText[i]) > LEGEND_COL_W - 10) tft.setTextFont(2);
  tft.drawString(legendText[i], legendSlotX(i) + LEGEND_COL_W / 2,
                 legendSlotY(i) + (i < 5 ? 16 : 14));
  tft.setTextFont(2);  // leave a bitmap font selected for later draws
}

// Marks every cell the touch marker's 25 px cross can reach, so moving or
// lifting a finger erases the old cross without repainting the whole grid.
static void dirtyCellsAround(uint16_t x, uint16_t y) {
  if (y < MAIN_Y || y >= LEGEND_Y) return;
  const int x0 = (int)x - 13;
  const int x1 = (int)x + 13;
  const int y0 = (int)y - 13;
  const int y1 = (int)y + 13;
  for (uint8_t i = 0; i < CELL_COUNT; i++) {
    const int cx = cellX(i);
    const int cy = cellY(i);
    if (x1 < cx || x0 > cx + CELL_W - 1) continue;
    if (y1 < cy || y0 > cy + CELL_H - 1) continue;
    cellDirty[i] = true;
  }
}

static void drawMain() {
  // Knob cells: 4 columns x 2 rows, caption over value (or a CC dial).
  tft.fillRect(0, MAIN_Y, SCREEN_W, CELL_H * 2, COL_BG);
  for (uint8_t i = 0; i < CELL_COUNT; i++) drawCell(i);
  cellDirty[0] = false;

  // Control legend: same 5 + 4 slots as the OLED strip, two rows.
  tft.fillRect(0, LEGEND_Y, SCREEN_W, SCREEN_H - LEGEND_Y, COL_BAND);
  for (uint8_t i = 0; i < LEGEND_COUNT; i++) {
    drawLegendSlot(i);
    legendDirty[i] = false;
  }
  for (uint8_t i = 1; i < CELL_COUNT; i++) cellDirty[i] = false;
  for (uint8_t i = 0; i < CELL_COUNT; i++) dialDirty[i] = false;

  drawTouchMark();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void initDisplay() {
  tft.init();
  tft.setRotation(3);  // landscape 480x320
  tft.setBrightness(255);
  tft.setTextWrap(false);
  tft.fillScreen(COL_BG);
  headerDirty = true;
  modeDirty = true;
  mainDirty = true;
  displayUpdate();
}

void clearHeader() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, COL_BG);
  headerDirty = true;
}

void clearMainScreen() {
  tft.fillRect(0, MAIN_Y, SCREEN_W, SCREEN_H - MAIN_Y, COL_BG);
  mainDirty = true;
}

void headerSetLeft(const char* text) {
  copyText(headerLeft, sizeof(headerLeft), text);
  headerDirty = true;
}

void headerShow(const char* text) {
  copyText(headerText, sizeof(headerText), text);
  headerDeadline = millis() + HEADER_MS;
  headerActive = true;
  headerDirty = true;
}

void headerAnnounce(const char* name, const char* value) {
  char buf[sizeof(headerText)];
  snprintf(buf, sizeof(buf), "%s %s", name, value ? value : "");
  headerShow(buf);
}

void headerAnnounceInt(const char* name, int value, const char* suffix) {
  char buf[sizeof(headerText)];
  snprintf(buf, sizeof(buf), "%s %d%s", name, value, suffix ? suffix : "");
  headerShow(buf);
}

void headerSetStick(const char* text) {
  copyText(headerStick, sizeof(headerStick), text);
  headerStickOn = headerStick[0] != '\0';
  headerDirty = true;
}

void headerStickClear() {
  if (!headerStickOn) return;
  headerStickOn = false;
  headerStick[0] = '\0';
  headerDirty = true;
}

void headerHide() {
  if (!headerActive) return;
  headerActive = false;
  headerDirty = true;
}

void displaySetCell(uint8_t i, const char* label, const char* value) {
  if (i >= CELL_COUNT) return;
  Cell next = cells[i];
  copyText(next.label, sizeof(next.label), label);
  copyText(next.value, sizeof(next.value), value);
  next.dial = false;
  if (memcmp(&next, &cells[i], sizeof(Cell)) == 0) return;
  cells[i] = next;
  cellDirty[i] = true;
}

void displaySetCellValue(uint8_t i, const char* value) {
  if (i >= CELL_COUNT) return;
  char buf[sizeof(cells[i].value)];
  copyText(buf, sizeof(buf), value);
  if (!cells[i].dial && strcmp(buf, cells[i].value) == 0) return;
  memcpy(cells[i].value, buf, sizeof(buf));
  cells[i].dial = false;  // a text value replaces a dial
  cellDirty[i] = true;
}

void displaySetCellDial(uint8_t i, const char* caption, uint8_t value) {
  if (i >= CELL_COUNT) return;
  if (value > 127) value = 127;
  Cell next = cells[i];
  copyText(next.label, sizeof(next.label), caption);
  next.value[0] = '\0';
  next.dial = true;
  next.dialValue = value;
  if (memcmp(&next, &cells[i], sizeof(Cell)) == 0) return;
  // Same caption, already a dial: only the pointer moved, so mark just the
  // dial for repaint. A caption change (or a text cell becoming a dial) still
  // needs the whole cell.
  const bool dialOnly = cells[i].dial && strcmp(next.label, cells[i].label) == 0;
  cells[i] = next;
  if (dialOnly) dialDirty[i] = true;
  else cellDirty[i] = true;
}

void displaySetLegend(uint8_t i, const char* text) {
  if (i >= LEGEND_COUNT) return;
  char buf[sizeof(legendText[0])];
  copyText(buf, sizeof(buf), text);
  if (strcmp(buf, legendText[i]) == 0) return;
  memcpy(legendText[i], buf, sizeof(buf));
  legendDirty[i] = true;
}

void displaySetTouch(bool pressed, uint16_t x, uint16_t y) {
  if (pressed == touchDown && (!pressed || (x == touchX && y == touchY))) return;
  // Repaint only the cells the old cross was drawn in - that is what erases it.
  if (touchDown) dirtyCellsAround(touchX, touchY);
  touchDown = pressed;
  touchX = x;
  touchY = y;
  if (touchDown) dirtyCellsAround(touchX, touchY);
}

void refreshMainScreen() { mainDirty = true; }

void refreshHeader() { headerDirty = true; }

void displaySetModes(const char* m0, const char* m1, const char* m2) {
  const char* src[MODE_COUNT] = {m0, m1, m2};
  bool changed = false;
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    char buf[sizeof(modeLabel[0])];
    copyText(buf, sizeof(buf), src[i]);
    if (strcmp(buf, modeLabel[i]) == 0) continue;
    memcpy(modeLabel[i], buf, sizeof(buf));
    changed = true;
  }
  if (changed) modeDirty = true;
}

void displaySetActiveMode(uint8_t index) {
  if (index >= MODE_COUNT) index = MODE_NONE;
  if (index == activeMode) return;
  activeMode = index;
  modeDirty = true;
}

void displayMarkDirty() {
  headerDirty = true;
  modeDirty = true;
  mainDirty = true;
}

void displayUpdate() {
  if (headerActive && (long)(millis() - headerDeadline) >= 0) headerHide();
  if (modeDirty) {
    drawModes();
    modeDirty = false;
  }
  if (mainDirty) {
    drawMain();
    for (uint8_t i = 0; i < CELL_COUNT; i++) cellDirty[i] = false;
    for (uint8_t i = 0; i < LEGEND_COUNT; i++) legendDirty[i] = false;
    mainDirty = false;
  } else {
    bool painted = false;
    for (uint8_t i = 0; i < LEGEND_COUNT; i++) {
      if (!legendDirty[i]) continue;
      drawLegendSlot(i);
      legendDirty[i] = false;
      painted = true;
    }
    for (uint8_t i = 0; i < CELL_COUNT; i++) {
      if (cellDirty[i]) {
        drawCell(i);
        cellDirty[i] = false;
        dialDirty[i] = false;  // the full repaint drew the dial too
        painted = true;
      } else if (dialDirty[i]) {
        drawDialOnly(i);
        dialDirty[i] = false;
        painted = true;
      }
    }
    // A cell repaint can erase part of the cross, so put it back.
    if (painted && touchDown) drawTouchMark();
  }
  if (headerDirty) {
    drawHeader();
    headerDirty = false;
  }
}
