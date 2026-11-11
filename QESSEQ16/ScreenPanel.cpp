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
   y 16..47  two knob rows  ->  y  68..217  4 columns x 2 rows, 120x75
   y 48..63  control legend ->  y 218..319  3 rows of 34: PREV/NEXT, then 3 + 4

 Fonts: the panel draws in Nasalization, embedded as a compact bitmap font
 (16 px for the header, 10 px for everything else - see the type block further
 down).  The OLED's own fonts are still the fallback if that load fails: font 4
 (26 px) for the header, font 2 (16 px) for the rest.  One grid size either
 way: the mode row, the whole knob grid (labels AND values, including the
 number inside a dial ring) and every legend label share a face, so a mode
 button comes out the same size as a legend button and nothing changes size
 from one cell to the next.  The measured widths are why: the widest caption is
 13 characters and the widest legend 11 - which is where the button width comes
 from, and what pins the grid face to 10 px.

 A cell reads caption over value: the caption at y+10, the value at y+32, and a
 value too wide for the 108 px column is wrapped onto a second line at y+48
 instead of being abbreviated or allowed to run into the next cell - so a scale
 name is read as "Harmonic Minor" over two lines, not as "HarmMin".

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
#define FONT_BIG   4   // 26 px: the header only
#define FONT_SMALL 2   // 16 px: the mode row, everything in the knob grid -
                       // labels AND values - and every legend button.  One size
                       // so a cell never changes face when its value does

// ---------------------------------------------------------------------------
// Type face: Nasalization, the brand face, instead of the built-in bitmap
// fonts above - which stay as the fallback if the font ever fails to load.
//
// The OTF cannot be drawn by this panel, so it was converted to LovyanGFX's
// compact bitmap font (head/cmap/loca/glyf, 1 bpp) and embedded in flash by
// fonts/nasa_fonts.h; PointerWrapper feeds the bytes to lgfx::BFFfont, so the
// board needs no filesystem and no SD card to have its type.  Two sizes, the
// two the layout was built around:
//
//   16 px  the header band
//   10 px  the mode row, every cell caption and value, every legend button
//
// 10 px is not a preference, it is what the fixed-width furniture allows: a
// legend button is 104 px and its widest label (PROBABILITY) measures 85 px at
// 10 px but 102 px at 12 px, and a caption gets 108 px, where INPUT CHANNEL is
// 98 px at 10 px but 118 px at 12 px - the 12 px cut truncates it.  Nasalization
// is a wide face; the bitmap font it replaces advanced 8 px per character,
// which is the assumption the measurements in the comments above were made on.
//
// loadFont() parses the tables into RAM once, from scrBegin().  A failed load
// leaves the built-in faces selected, so a bad font can never blank the panel.
// ---------------------------------------------------------------------------
#include "fonts/nasa_fonts.h"

static lgfx::BFFfont nasaHeader;
static lgfx::BFFfont nasaGrid;
static lgfx::PointerWrapper nasaHeaderSrc(kFontNasa16, sizeof(kFontNasa16));
static lgfx::PointerWrapper nasaGridSrc(kFontNasa10, sizeof(kFontNasa10));
static bool nasaUp = false;

static void scrLoadFonts() {
  if (nasaUp) return;
  const bool h = nasaHeader.loadFont(&nasaHeaderSrc);
  const bool g = nasaGrid.loadFont(&nasaGridSrc);
  nasaUp = h && g;
}

// Select the header face / the grid face, or the built-in one as a fallback.
static void useHeader() {
  if (nasaUp) tft.setFont(&nasaHeader); else tft.setTextFont(FONT_BIG);
}
static void useGrid() {
  if (nasaUp) tft.setFont(&nasaGrid); else tft.setTextFont(FONT_SMALL);
}

// ---------------------------------------------------------------------------
// Layout: header 0..33, mode row 34..67, knob band 68..217, legend 218..319
// ---------------------------------------------------------------------------
#define PANEL_W       480
#define PANEL_H       320
#define HEADER_H       34
#define MODE_Y       HEADER_H
#define MODE_H         34
#define MAIN_Y       (MODE_Y + MODE_H)          // 68
#define CELL_W       (PANEL_W / 4)              // 120
// The legend grew a third row: PREV / NEXT get one of their own above the two
// control rows.  Each row is a 34 px band - the mode row's own height - so all
// four button rows on the panel are identical, and the 3 x 34 px that costs
// the bottom is what the knob band pays: 75 px a row instead of 94, with every
// position inside a cell derived from that.  The row split is slots 0-1 / 2-4
// / 5-8, which is the OLED's "PREV NEXT M1 M2 M3" row minus its first two.
#define LEGEND_ROWS     3
#define LEGEND_SLOT_H   34                        // = MODE_H: one per legend row
#define LEGEND_Y      (PANEL_H - LEGEND_SLOT_H * LEGEND_ROWS)  // 218
#define CELL_H        ((LEGEND_Y - MAIN_Y) / 2)   // 75
// The legend rows are a fixed button grid (see the BTN_ constants): the same
// four columns every row starts from at the left, so the strip lines up with
// the cell captions above it instead of re-flowing with the text.
#define PAD_X           6                       // cell left inset
#define TEXT_W       (CELL_W - PAD_X * 2)       // 108: 13 characters at 16 px
#define TEXT_H          16                      // one line of FONT_SMALL
// A cell reads caption over value.  The caption sits near the top so the value
// has the room for a second line under it: 10..25 for the caption, 32..47 for
// the value, 48..63 for its wrapped continuation - all inside the 75 px cell,
// with a hand's width of air left at the bottom.
#define LABEL_Y        10                       // caption top inside a cell
#define VALUE_Y        (LABEL_Y + TEXT_H + 6)   // value top: 6 px under it

// One button for the mode row and the legend alike: the same width, the same
// 16 px face, the same 26 px height, the same radius - and the same columns.
// The width is the widest legend there is (PROBABILITY: 11 characters at the
// 8 px advance of FONT_SMALL = 88, plus this padding), so every row is a fixed
// grid that starts at the cell captions' left edge and never re-flows when a
// label changes or a slot goes empty.
#define BTN_H          26                       // 4 px above and below in a 34
#define BTN_R           6
#define BTN_PAD_X       8                       // room each side of the longest
                                                 // label; it is what sets BTN_W
#define BTN_W         (11 * 8 + BTN_PAD_X * 2)  // 104: the widest legend
#define BTN_GAP        16                       // between two buttons
#define BTN_X0         PAD_X                    // 6: same edge as a cell caption

// Dial geometry inside a cell (ring outer radius 26, caption in the notch).
// The cell is 75 tall: centred at 33 the 32 px repaint box starts one pixel
// below the cell's top edge (so it never eats the grid line) and its bottom
// edge at 56 clears the caption's 16 px box at 57..72, which in turn ends two
// pixels short of the cell's own bottom edge at 74.
#define DIAL_CY         33
#define DIAL_R          26
#define DIAL_CAPTION_Y  65
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
  char value[24];   // value text (unused in dial mode); 23 characters so a
                    // spelled-out name like "Neapolitan Major" still fits
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
static bool legendDirty;            // the whole strip: one button's fill moves with it

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
// 32 px and had to be an abbreviation.  A cell here is 120x75, so the panel
// spells the tokens out instead of sharing the short spelling - Bindings.cpp
// and Display.cpp keep the tables the OLED needs, and this table is applied
// where the caption, the value or a legend label is drawn.  Only real words
// are expanded: MIDI's own terms (CC, BPM), the physical key names (M1..M4,
// F1..F4, PREV, NEXT), the chord names (sus4, m7, 9) and tokens whose meaning
// is not certain (MOD, SD) are left exactly as they are.
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
  // Scale names (Chords.cpp's kScaleName): the OLED's value column is seven
  // characters, so a scale arrives here abbreviated.  The panel has two lines
  // for it and a player reads a scale name, not an acronym - "Harmonic Minor",
  // not "HarmMin".  Only what is unambiguous is expanded; names that are
  // already words and ones whose short form is the project's own spelling
  // (DbHarm, PelogS, PelogT) are left alone.
  { "Chr",    "Chromatic" },
  { "HarmMin", "Harmonic Minor" },
  { "MelMin", "Melodic Minor" },
  { "Phryg",  "Phrygian" },
  { "Mixoly", "Mixolydian" },
  { "MajPent", "Major Pentatonic" },
  { "MinPent", "Minor Pentatonic" },
  { "WhoTone", "Whole Tone" },
  { "DimHW",  "Diminished Half-Whole" },
  { "DimWH",  "Diminished Whole-Half" },
  { "PhrDom", "Phrygian Dominant" },
  { "HunMin", "Hungarian Minor" },
  { "Mishbrk", "Misheberakh" },
  { "LydDom", "Lydian Dominant" },
  { "BebopD", "Bebop Dominant" },
  { "BebopM", "Bebop Major" },
  { "LydAug", "Lydian Augmented" },
  { "Mixb6",  "Mixolydian b6" },
  { "LocN2",  "Locrian Natural 2" },
  { "Dor#4",  "Dorian #4" },
  { "Lyd#2",  "Lydian #2" },
  { "Ultra",  "Ultralocrian" },
  { "NeaMin", "Neapolitan Minor" },
  { "NeaMaj", "Neapolitan Major" },
  { "Augmnt", "Augmented" },
  { "Promth", "Promethean" },
  { "Enigm",  "Enigmatic" },
  { "HarmMaj", "Harmonic Major" },
  { "Mesia3", "Messiaen 3" },
  { "Mesia4", "Messiaen 4" },
  { "Mesia5", "Messiaen 5" },
  { "Mesia6", "Messiaen 6" },
  { "Mesia7", "Messiaen 7" },
  { "Span8",  "Spanish 8" },
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
// here either - a run wider than the cell is cut at its last space and carried
// onto the next 16 px line, up to maxLines of them, which is what lets a value
// be read as "Harmonic Minor" over two lines instead of "HarmMin" on one.  A
// word longer than the cell is broken where it runs out, and anything past the
// last line is dropped rather than drawn over the cell's edge.
static void drawText(const char* s, int x, int y, uint8_t maxLines,
                     uint16_t color, uint16_t bg) {
  if (!s || !s[0]) return;
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  useGrid();
  tft.setTextColor(color, bg);

  char line[32];        // one drawn line: TEXT_W holds 13 characters at 8 px
  char one[2] = {0, 0};
  int n = 0;            // characters in the line being built
  int w = 0;            // ...and its width in pixels
  int brk = -1;         // index of its last space: where the line can be cut
  uint8_t row = 0;      // which line is being built (0 = the first)
  const char* p = s;

  for (;;) {
    const char c = *p;
    if (c == '\0') {                       // the tail fits: draw it and stop
      if (n) { line[n] = '\0'; tft.drawString(line, x, y + row * TEXT_H); }
      return;
    }
    if (c == ' ' && n == 0) { p++; continue; }  // never open a line on a space

    one[0] = c;
    const int cw = (int)tft.textWidth(one);
    if (w + cw <= TEXT_W) {                // it fits: keep growing the line
      line[n++] = c;
      w += cw;
      if (c == ' ') brk = n - 1;
      p++;
      continue;
    }

    // This character does not fit: draw the line, cut at its last space if it
    // has one (a word longer than the cell is cut where it ran out), then go
    // on with what is left of it, and of the text, one line below.
    const int cut = (brk >= 0) ? brk : n;
    line[cut] = '\0';
    tft.drawString(line, x, y + row * TEXT_H);
    if (row + 1 >= maxLines) return;       // no line under this one: stop here
    row++;
    if (brk >= 0) {                        // keep the part after the space
      const int keep = n - cut - 1;
      memmove(line, line + cut + 1, keep);
      n = keep;
      w = 0;
      for (int i = 0; i < n; i++) { one[0] = line[i]; w += (int)tft.textWidth(one); }
    } else {
      n = 0;                               // the word is spent: start a new one
      w = 0;
    }
    brk = -1;
  }
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
  useGrid();  // the grid's one size: the caption's own face
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
  useHeader();
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
  useHeader();
  int noteY = 4;
  if (tft.textWidth(right) > avail) {
    useGrid();
    noteY = 9;  // the caption size sits a little lower in the band
  }
  tft.setTextColor(headerTransient ? COL_ACCENT : COL_DIM, COL_BAND);
  tft.setTextDatum(lgfx::textdatum_t::top_right);
  tft.drawString(right, PANEL_W - 12, noteY);
}

// Mode row: three always-visible mode buttons, the active one filled.  The
// chip is the legend's - same width, same 16 px face, same 26 px height, same
// columns - so the row reads as buttons and not as a band of its own, and the
// three sit on the same left-aligned grid as the rows under the knob band.
static void drawModes() {
  tft.fillRect(0, MODE_Y, PANEL_W, MODE_H, COL_BG);
  tft.drawFastHLine(0, MODE_Y + MODE_H - 1, PANEL_W, COL_GRID);

  const int by = MODE_Y + (MODE_H - BTN_H) / 2;  // 4 px above and below
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  useGrid();
  for (uint8_t i = 0; i < 3; i++) {
    const int bx = BTN_X0 + i * (BTN_W + BTN_GAP);  // 6 / 126 / 246
    const bool on = (i == activeMode);
    tft.fillRoundRect(bx, by, BTN_W, BTN_H, BTN_R, on ? COL_ACCENT : COL_BAND);
    tft.setTextColor(on ? COL_BG : COL_DIM, on ? COL_ACCENT : COL_BAND);
    tft.drawString(modeLabel[i], bx + BTN_W / 2, by + BTN_H / 2);
  }
  tft.setTextColor(COL_DIM, COL_BG);  // back to the default for later draws
  useGrid();        // the panel's default size
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
    useGrid();
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(cells[i].label, x + CELL_W / 2, y + DIAL_CAPTION_Y);
  } else {
    drawText(cells[i].label, x + PAD_X, y + LABEL_Y, 1, COL_DIM, COL_BG);
    drawText(cells[i].value, x + PAD_X, y + VALUE_Y, 2, COL_TEXT, COL_BG);
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
// Control legend: three rows of buttons, left-aligned on one grid.
//
// The OLED's 5 + 4 slots are split 2 + 3 + 4 here so PREV / NEXT have a row to
// themselves above the two control rows.  Every button is BTN_W wide - the
// widest label there is, PROBABILITY at 11 characters - starts at BTN_X0 and
// steps by BTN_W + BTN_GAP, so all three rows share four columns with the mode
// row and sit on the same left edge as the cell captions above.  A label is
// centred in its button, so TIE and PROBABILITY read the same way, and an
// empty slot leaves its column standing instead of pulling the row left.
// Positions no longer come from the text, so a change repaints the whole strip
// (3 rows, ~2 ms) rather than one slot - it only happens on a mode or page
// change, never on a knob turn.
//
// One button can be ARMED: the chosen F radio, or the M button whose menu is
// open.  It is filled with the accent and printed dark - the same treatment the
// mode row gives the active mode - so the band says what the pads already say,
// since a chosen F pad burns white (Modes.cpp).  The fill does not move any
// label: the layout is the fixed grid either way.
// ---------------------------------------------------------------------------
static void drawLegendRow(uint8_t first, uint8_t count, int yCenter) {
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  useGrid();
  for (uint8_t k = 0; k < count; k++) {
    if (!legendText[first + k][0]) continue;
    const int bx = BTN_X0 + k * (BTN_W + BTN_GAP);  // 6 / 126 / 246 / 366
    const bool on = legendSel[first + k];
    tft.fillRoundRect(bx, yCenter - BTN_H / 2, BTN_W, BTN_H, BTN_R,
                      on ? COL_ACCENT : COL_BAND);
    tft.setTextColor(on ? COL_BG : COL_DIM, on ? COL_ACCENT : COL_BAND);
    tft.drawString(legendText[first + k], bx + BTN_W / 2, yCenter);
  }
  tft.setTextColor(COL_DIM, COL_BG);  // back to the default for later draws
}

// The strip is background-coloured so the chips stand out of it the way the
// mode buttons stand out of the mode row; the hairline keeps the grid above it
// separate.  Rows are centred at +17 / +51 / +85 in their 34 px band.
static void drawLegendBand() {
  tft.fillRect(0, LEGEND_Y, PANEL_W, PANEL_H - LEGEND_Y, COL_BG);
  tft.drawFastHLine(0, LEGEND_Y, PANEL_W, COL_GRID);
  drawLegendRow(0, 2, LEGEND_Y + LEGEND_SLOT_H / 2);
  drawLegendRow(2, 3, LEGEND_Y + LEGEND_SLOT_H + LEGEND_SLOT_H / 2);
  drawLegendRow(5, 4, LEGEND_Y + LEGEND_SLOT_H * 2 + LEGEND_SLOT_H / 2);
}

// ---------------------------------------------------------------------------
// Screen interface
// ---------------------------------------------------------------------------
void scrBegin() {
  tft.init();
  scrLoadFonts();      // Nasalization before the first string is measured
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
  copyText(buf, sizeof(buf), fullName(text));   // the panel spells values out too
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

// Armed label: filled in the band.  Only the flag changes here - the fill
// spreads over the whole fixed button, so the band has to be repainted for one
// label to gain a background (it happens on a page change, not on a knob turn).
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
    // No band-wide fill: the eight cell repaints cover MAIN_Y..217 completely
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
