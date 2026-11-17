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

// Dial geometry inside a cell (ring outer radius 26, caption in the notch).// The cell is 75 tall: centred at 33 the 32 px repaint box starts one pixel
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

// Bar cell (FLAVOR = BARS): a 108 px track across the cell with the level in
// it, the CC number above and the number itself under the track.
#define BAR_Y          36
#define BAR_H          18
#define BAR_NUM_Y      62
// Envelope row (FLAVOR = ENVELOPE): the curve takes the top of the cell, then a
// 6 px bar and the CC caption + number under it, all inside the 75 px row.
//
// The row is split in two: the FILTER under ENC1 / ENC2 on the left, the ADSR
// of ENC5..8 on the right.  The curve used to span the whole 480 px, which made
// the four values unreadable off the shape; at half the width the envelope
// reads at a glance and the left half gains the filter the cutoff / resonance
// knobs are driving.
#define ENV_TOP         8
#define ENV_H          36
#define ENV_BAR_Y     (ENV_TOP + ENV_H + 4)     // 48
#define ENV_BAR_H       6
#define ENV_CAP_Y     (ENV_BAR_Y + ENV_BAR_H + 2)  // 56
#define ENV_HALF_W    (PANEL_W / 2)             // 240: filter | envelope
#define ENV_SLOT_W    (ENV_HALF_W / 4)          // 60: one ADSR value's column
// A blank band between the two curves.  The filter may end anywhere, but the
// envelope has to START on the cell edge: its four 60 px readouts must each
// stay inside the cell that paints them, or the neighbour's fill erases them.
#define ENV_GAP       50                        // blank band, 190..239
#define ENV_FLT_END   (ENV_HALF_W - ENV_GAP)    // 190: the filter curve's width
#define ENV_ENV_X     ENV_HALF_W                // 240: the ADSR starts here
// The LFO row uses the same skeleton: the filter on the left, the blank band,
// then the waveform on the right.  Its four readouts sit on ONE line, caption
// left and value right, exactly like the ADSR's - so they have to be wider than
// the ADSR's 52 px ("SHAPE RANDOM" is 11 characters of the 10 px face).  The
// line therefore starts at 120, the ENC2 column, and runs to the row's right
// edge in four 90 px slots: it uses the band under the filter's tail, which the
// curve itself never reaches (the curve ends at y+43, this line sits at y+56).
#define LFO_READ_X    (PANEL_W / 4)                     // 120: under ENC2
#define LFO_SLOT_W    ((PANEL_W - LFO_READ_X) / 4)      // 90: one readout
#define LFO_READ_Y    ENV_CAP_Y                         // 56: the ADSR's line

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
// How a cell draws itself.  The CC row can carry any of these: a dial, a
// level bar, its slice of the shared ADSR curve or of the LFO's waveform (the
// FLAVORS of ENCMODE4); everything else in the firmware is text.
enum CellMode : uint8_t { CELL_TEXT = 0, CELL_DIAL, CELL_BAR, CELL_ENV, CELL_LFO };

struct Cell {
  char label[18];   // caption / CC number / device parameter name
  char value[24];   // value text (unused in dial / bar / env mode); 23
                    // characters so a spelled-out name like "Neapolitan
                    // Major" still fits
  uint8_t mode;     // CELL_TEXT / CELL_DIAL / CELL_BAR / CELL_ENV / CELL_LFO
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

// The envelope row's state, shared by the four cells that draw it: each cell
// paints its own slice of a curve that spans two cells, so it needs the whole
// half's numbers - the four ADSR values on the right, the filter's cutoff and
// resonance (plus all six CC captions) on the left.
static uint8_t envVal[4] = { 0, 0, 0, 0 };
static char envCap[4][8] = { "", "", "", "" };
static uint8_t filterVal[2] = { 0, 0 };   // cutoff, resonance

// The LFO row's state, likewise shared by its four cells: the raw values (depth
// / rate / shape / sync) drive one waveform that spans all of them, and each
// cell carries its own caption and value text under the curve.
static uint8_t lfoVal[4] = { 0, 0, 0, 0 };
static char lfoCap[4][18] = { "", "", "", "" };
static char lfoTxt[4][18] = { "", "", "", "" };

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
  // ARP / BASS / LEAD / SKANK pages
  { "RNG",    "RANGE" },
  { "ALGO",   "ALGORITHM" },
  { "BRW",    "BORROW" },
  { "ACC",    "ACCENT" },
  { "GEN",    "GENERATE" },
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

// The two CC-row flavors (see drawBarCell / drawEnvSlice below): drawCell
// dispatches to them, so they need to be declared before it.
static void drawBarCell(uint8_t i);
static void drawEnvSlice(uint8_t i);
static void drawLfoSlice(uint8_t i);

// Repaint one knob cell and nothing else. The fill covers the separators that
// run along this cell's left and top edges, so those two get drawn back.
static void drawCell(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);
  const uint8_t col = i & 3;
  const uint8_t row = i >> 2;

  tft.fillRect(x, y, CELL_W, CELL_H, COL_BG);
  if (cells[i].mode == CELL_DIAL) {
    drawDial(x, y, cells[i].dialValue);
    tft.setTextDatum(lgfx::textdatum_t::middle_center);
    useGrid();
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(cells[i].label, x + CELL_W / 2, y + DIAL_CAPTION_Y);
  } else if (cells[i].mode == CELL_BAR) {
    drawBarCell(i);
  } else if (cells[i].mode == CELL_ENV) {
    drawEnvSlice(i);
  } else if (cells[i].mode == CELL_LFO) {
    drawLfoSlice(i);
  } else {
    drawText(cells[i].label, x + PAD_X, y + LABEL_Y, 1, COL_DIM, COL_BG);
    drawText(cells[i].value, x + PAD_X, y + VALUE_Y, 2, COL_TEXT, COL_BG);
  }
  // The envelope and the LFO are one line across the four cells of their row,
  // so those rows keep their vertical dividers off (they would cut the curve).
  // Everything else draws its own left and top edges back over the fill.
  if (col > 0 && cells[i].mode != CELL_ENV && cells[i].mode != CELL_LFO)
    tft.drawFastVLine(x, y + 8, CELL_H - 16, COL_GRID);
  if (row > 0) tft.drawFastHLine(x, y, CELL_W, COL_GRID);
}

// One bar cell: the CC number above, a 108 px track filled to the value, and
// the number itself under the track (FLAVOR = BARS).
static void drawBarCell(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);

  tft.setTextDatum(lgfx::textdatum_t::top_left);
  useGrid();
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(cells[i].label, x + PAD_X, y + LABEL_Y);

  tft.fillRect(x + PAD_X, y + BAR_Y, TEXT_W, BAR_H, COL_GRID);
  const int w = (int)((uint32_t)cells[i].dialValue * (TEXT_W - 2) / 127);
  if (w > 0) tft.fillRect(x + PAD_X + 1, y + BAR_Y + 1, w, BAR_H - 2, COL_ACCENT);

  char v[4];
  snprintf(v, sizeof(v), "%u", (unsigned)cells[i].dialValue);
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(v, x + CELL_W / 2, y + BAR_NUM_Y);
}

// Exponential fall from 1 at u = 0 to 0 at u = 1, renormalised so each segment
// lands exactly on the next level instead of leaving a step at the join.  This
// is what makes decay and release bend the way a real envelope does (a straight
// line there reads as a triangle, not as an envelope).
static float expFall(float u) {
  const float k = 0.0183156f;   // e^-4: the tail the segment is cut at
  if (u <= 0.0f) return 1.0f;
  if (u >= 1.0f) return 0.0f;
  return (expf(-4.0f * u) - k) / (1.0f - k);
}

// Where the envelope's segments sit on the row.  The envelope is laid out left
// to right from the attack, so the RELEASE END MOVES WITH THE RELEASE KNOB -
// a long release reaches further right, a short one stops earlier - and only
// the row's edge is off limits (ENV_EDGE keeps a strip of air there instead of
// letting the curve run into the side of the tile).
#define ENV_EDGE   0.88f        // the envelope never reaches past this
static void envSegments(const uint8_t v[4], float* a, float* decayEnd,
                        float* relStart, float* relEnd) {
  *a = 0.05f + 0.30f * ((float)v[0] / 127.0f);   // attack end
  const float d = 0.04f + 0.16f * ((float)v[1] / 127.0f);   // decay length
  *decayEnd = *a + d;

  *relStart = *decayEnd + 0.12f;                 // the sustain's flat run
  *relEnd = *relStart + 0.05f + 0.16f * ((float)v[3] / 127.0f);
  if (*relEnd > ENV_EDGE) {                      // full ADSR would overrun
    *relEnd = ENV_EDGE;
    if (*relStart > *relEnd - 0.02f) *relStart = *relEnd - 0.02f;
  }
}

// The ADSR level at x (0..1 across the whole row): attack ramps to full over
// its own slice, decay bends down to the sustain level, sustain holds, release
// bends down to zero.  The four values stretch their own segments, so the shape
// reads as the envelope the knobs describe (FLAVOR = ENVELOPE).  Negative =
// past the end of the envelope: there is nothing to plot out there.
static float envLevel(float x, const uint8_t v[4]) {
  float a, decayEnd, relStart, relEnd;
  envSegments(v, &a, &decayEnd, &relStart, &relEnd);
  const float s = (float)v[2] / 127.0f;                     // sustain level

  if (x <= 0.0f) return 0.0f;
  if (x < a) return a > 0.0f ? x / a : 1.0f;      // straight up to the peak
  if (x < decayEnd) return s + (1.0f - s) * expFall((x - a) / (decayEnd - a));
  if (x < relStart) return s;                     // the hold
  if (x < relEnd) return s * expFall((x - relStart) / (relEnd - relStart));
  return -1.0f;
}

// LPF24 response for the row's left half - the shape a filter display should
// have: a shelf that sits at 55% of the band with headroom above it, a resonant
// bump rising out of that shelf at the corner, and the 24 dB/oct slope dropping
// the curve to the floor just past it.  (Drawn at full scale the shelf would sit
// on the ceiling and the bump would be invisible - hence the fixed headroom.)
//
// The x axis is LOG frequency: three decades (20 Hz .. 20 kHz) across the half,
// the way a filter is actually displayed.  That is what keeps the high end in
// its place - the 24 dB/oct slope covers a fixed half decade of the axis, so the
// curve always reaches zero a set distance past the corner instead of trailing a
// zero line off towards whatever is drawn to its right.  f runs 0..1 across the
// half (axis position, not Hz), cutoff and res are the knobs' 0..127 values.
static float filterLevel(float f, uint8_t cutoff, uint8_t res) {
  const float dec = f * 3.0f;                                 // 0..3 decades
  const float f0 = 0.25f + 2.10f * ((float)cutoff / 127.0f);  // corner, decades
  const float r = (float)res / 127.0f;
  const float dd = dec - f0;                                  // decades from corner
  // Capped for the power: past 0.3 decades the response is already six orders
  // down, and x^48 would overflow a float long before that
  const float x = powf(10.0f, dd < 0.3f ? dd : 0.3f);         // f / f0
  const float x8 = x * x * x * x * x * x * x * x;             // (f / f0)^8
  const float x16 = x8 * x8;
  // 1 / sqrt(1 + x^48): a statement, not a slope.  A 24 dB/oct curve across a
  // three-decade axis still reads as a long diagonal, so the drawn response is
  // deliberately steeper than the knob's - the eye wants a wall at the end of a
  // low-pass display
  const float mag = 1.0f / sqrtf(1.0f + x16 * x16 * x16);
  // The bump is asymmetric: a shoulder rising into the corner, a much narrower
  // one leaving it, so its tail cannot soften the wall that follows it.  With
  // the clamp only as a safety net (the sum peaks around 0.94 by itself) the
  // top stays a peak instead of flattening into a plateau
  const float width = (dd < 0.0f) ? (0.10f + 0.08f * r) : (0.03f + 0.02f * r);
  const float d = dd / width;
  float lvl = 0.55f * mag + 0.42f * r * expf(-d * d);
  if (lvl > 0.97f) lvl = 0.97f;
  return lvl;
}

// One tight readout under a curve: a w px bar filled to the value, its CC
// caption on the left and the number on the right.
static void envReadout(int x, int y, int w, const char* cap, uint8_t v) {
  if (w < 8) return;
  tft.fillRect(x, y + ENV_BAR_Y, w, ENV_BAR_H, COL_GRID);
  const int fill = (int)((uint32_t)v * (uint32_t)(w - 2) / 127);
  if (fill > 0)
    tft.fillRect(x + 1, y + ENV_BAR_Y + 1, fill, ENV_BAR_H - 2, COL_ACCENT);

  char n[4];
  snprintf(n, sizeof(n), "%u", (unsigned)v);
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  useGrid();
  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(cap, x, y + ENV_CAP_Y);
  tft.setTextDatum(lgfx::textdatum_t::top_right);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(n, x + w, y + ENV_CAP_Y);
  tft.setTextDatum(lgfx::textdatum_t::top_left);
}

// One cell's slice of the row: the filter curve on the left, the ADSR curve on
// the right, and the blank band between them.  Each cell paints only the pixels
// inside its own 120 px.
static void drawEnvSlice(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);

  int prevX = x;
  int prevY = y + ENV_TOP + ENV_H - 1;
  bool joined = false;              // is prevX/prevY a point of the same curve?
  tft.startWrite();
  for (int px = x; px < x + CELL_W; px++) {
    int py;
    if (px < ENV_FLT_END) {
      const float f = (float)px / (float)(ENV_FLT_END - 1);
      const float lvl = filterLevel(f, filterVal[0], filterVal[1]);
      // past the corner there is no response left to draw: the curve ends on
      // the floor instead of running a flat line on to the half's fixed edge
      if (lvl < 0.015f) {
        joined = false;
        continue;
      }
      py = y + ENV_TOP + (int)lroundf((1.0f - lvl) * (float)(ENV_H - 1));
    } else if (px >= ENV_ENV_X) {
      const float t = (float)(px - ENV_ENV_X) /
                      (float)(PANEL_W - ENV_ENV_X - 1);
      const float lvl = envLevel(t, envVal);
      // past the release there is nothing left: the curve ends there rather
      // than lying on the floor out to the edge of the row
      if (lvl < 0.0f) {
        joined = false;
        continue;
      }
      py = y + ENV_TOP + (int)lroundf((1.0f - lvl) * (float)(ENV_H - 1));
    } else {
      joined = false;               // the gap: draw nothing, join nothing
      continue;
    }
    if (joined) tft.drawLine(prevX, prevY, px, py, COL_ACCENT);
    prevX = px;
    prevY = py;
    joined = true;
  }
  tft.endWrite();

  // Only the envelope half carries readouts - the filter's two values are the
  // ENC1 / ENC2 dials directly above it, so repeating them here is noise.
  if (i >= 6) {
    for (uint8_t j = 0; j < 2; j++) {
      const uint8_t v = (uint8_t)((i - 6) * 2 + j);
      envReadout(ENV_ENV_X + (int)v * ENV_SLOT_W + 4, y, ENV_SLOT_W - 8,
                 envCap[v], envVal[v]);
    }
  }
}

// The filter half of the row (cells 4/5): the LPF24 under ENC1 / ENC2, drawn
// exactly as the envelope row draws it - shelf, bump, wall, and no readout of
// its own because those two numbers are the dials right above it.  Shared by
// the ENVELOPE and LFO flavors.
static void drawFilterSlice(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);

  int prevX = x;
  int prevY = y + ENV_TOP + ENV_H - 1;
  bool joined = false;              // is prevX/prevY a point of the same curve?
  tft.startWrite();
  for (int px = x; px < x + CELL_W && px < ENV_FLT_END; px++) {
    const float f = (float)px / (float)(ENV_FLT_END - 1);
    const float lvl = filterLevel(f, filterVal[0], filterVal[1]);
    // past the corner there is no response left to draw: the curve ends on
    // the floor instead of running a flat line on to the blank band
    if (lvl < 0.015f) {
      joined = false;
      continue;
    }
    const int py = y + ENV_TOP + (int)lroundf((1.0f - lvl) * (float)(ENV_H - 1));
    if (joined) tft.drawLine(prevX, prevY, px, py, COL_ACCENT);
    prevX = px;
    prevY = py;
    joined = true;
  }
  tft.endWrite();
}

// The LFO row, laid out like the envelope row: the FILTER under ENC1 / ENC2 on
// the left, the blank band, then the still waveform on the right with its four
// values under it (see lfoSyncedWave in Screen.h).  Nothing here animates -
// depth sets the height around the centre line (0 = flat), rate the number of
// cycles across the half, shape the waveform and sync how the cycles line up
// (TRIG's one shot is as wide as one cycle of the rate, so the rate moves it).
static void drawLfoSlice(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);

  if (i < 6) {                      // cells 4/5: the filter half, no readout
    drawFilterSlice(i);
    return;
  }

  // cells 6/7: the LFO half.  The waveform spans 240..479, so each cell paints
  // its own 120 px of it, the same slice scheme the ADSR uses.
  const int mid = y + ENV_TOP + (ENV_H - 1) / 2;
  const int w = PANEL_W - ENV_ENV_X;          // 240: the LFO's width

  const uint8_t sync = lfoSync(lfoVal[3]);
  const float amp = ((float)lfoVal[0] / 127.0f) * (float)((ENV_H - 1) / 2);

  // the zero line: the depth reads as an amplitude around it, and a depth of 0
  // leaves the flat line the knobs describe
  tft.drawFastHLine(x, mid, CELL_W, COL_GRID);

  // beat-locked: one dim rule per cycle boundary, under the curve.  TRIG gets a
  // single rule where its one shot ends, so the flat line after it reads as the
  // end of the burst and not as a missing curve.
  if (sync == 1) {
    float cycles = roundf(lfoCycles(lfoVal[1]));
    if (cycles < 1.0f) cycles = 1.0f;
    for (uint8_t c = 1; (float)c < cycles; c++) {
      const int tx = ENV_ENV_X +
                     (int)lroundf((float)c / cycles * (float)(w - 1));
      if (tx >= x && tx < x + CELL_W)
        tft.drawFastVLine(tx, y + ENV_TOP, ENV_H, COL_GRID);
    }
  } else if (sync == 2) {
    const int tx = ENV_ENV_X +
                   (int)lroundf(lfoTrigSpan(lfoVal[1]) * (float)(w - 1));
    if (tx >= x && tx < x + CELL_W)
      tft.drawFastVLine(tx, y + ENV_TOP, ENV_H, COL_GRID);
  }

  int prevX = x;
  int prevY = mid;
  bool joined = false;
  tft.startWrite();
  for (int px = x; px < x + CELL_W; px++) {
    const float u = (float)(px - ENV_ENV_X) / (float)(w - 1);
    const float v = lfoSyncedWave(u, lfoVal[1], lfoVal[2], lfoVal[3]);
    const int py = mid - (int)lroundf(v * amp);
    if (joined) tft.drawLine(prevX, prevY, px, py, COL_ACCENT);
    prevX = px;
    prevY = py;
    joined = true;
  }
  tft.endWrite();

  // The four readouts, one line under the row: caption left, value right, in
  // 90 px slots from the ENC2 column to the row's edge.  Only the LAST cell of
  // the row draws them, because the line crosses all four cells and a cell's
  // fill would erase text an earlier cell had already written (scrUpdate paints
  // 4, 5, 6, 7 in that order, so 7 is always last).
  if (i >= 7) {
    for (uint8_t k = 0; k < 4; k++) {
      const int cx = LFO_READ_X + (int)k * LFO_SLOT_W + 4;
      const int cw = LFO_SLOT_W - 8;
      useGrid();
      // "SHAPE SQUARE" is a little wider than the slot's 82 px, so the caption
      // gives way leftwards into the air on its left instead of running into
      // its own value: the pair stays one readable line, at worst touching the
      // empty padding of the slot before it.
      const int pairW =
          (int)ceilf(tft.textWidth(lfoCap[k]) + tft.textWidth(lfoTxt[k])) + 2;
      int capX = cx;
      if (pairW > cw) capX = cx - (pairW - cw);
      tft.setTextDatum(lgfx::textdatum_t::top_left);
      tft.setTextColor(COL_DIM, COL_BG);
      tft.drawString(lfoCap[k], capX, y + LFO_READ_Y);
      tft.setTextDatum(lgfx::textdatum_t::top_right);
      tft.setTextColor(COL_TEXT, COL_BG);
      tft.drawString(lfoTxt[k], cx + cw, y + LFO_READ_Y);
      tft.setTextDatum(lgfx::textdatum_t::top_left);
    }
  }
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
    cells[i].mode = CELL_TEXT;
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
  // a text label replaces a dial / bar / env cell (same rule as the value
  // setter): the whole cell has to be repainted, not just its caption
  if (cells[i].mode == CELL_TEXT && strcmp(buf, cells[i].label) == 0) return;
  memcpy(cells[i].label, buf, sizeof(buf));
  cells[i].mode = CELL_TEXT;
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
  if (cells[i].mode == CELL_TEXT && strcmp(buf, cells[i].value) == 0) return;
  memcpy(cells[i].value, buf, sizeof(buf));
  cells[i].mode = CELL_TEXT;
  cellDirty[i] = true;
}

void scrDial(uint8_t i, const char* caption, uint8_t value) {
  if (i >= SCR_CELLS) return;
  if (value > 127) value = 127;
  char buf[sizeof(cells[i].label)];
  copyText(buf, sizeof(buf), fullName(caption));
  const bool dialOnly = (cells[i].mode == CELL_DIAL) &&
                        strcmp(buf, cells[i].label) == 0;
  if (dialOnly && value == cells[i].dialValue) return;
  memcpy(cells[i].label, buf, sizeof(buf));
  cells[i].value[0] = '\0';
  cells[i].mode = CELL_DIAL;
  cells[i].dialValue = value;
  // Same caption, already a dial: only the pointer moved, so mark just the
  // dial. A caption change (or a text/bar/env cell becoming a dial) needs the
  // whole cell.
  if (dialOnly) dialDirty[i] = true;
  else cellDirty[i] = true;
}

void scrCellBar(uint8_t i, const char* caption, uint8_t value) {
  if (i >= SCR_CELLS) return;
  if (value > 127) value = 127;
  char buf[sizeof(cells[i].label)];
  copyText(buf, sizeof(buf), fullName(caption));
  if (cells[i].mode == CELL_BAR && strcmp(buf, cells[i].label) == 0 &&
      value == cells[i].dialValue)
    return;
  memcpy(cells[i].label, buf, sizeof(buf));
  cells[i].value[0] = '\0';
  cells[i].mode = CELL_BAR;
  cells[i].dialValue = value;
  cellDirty[i] = true;   // the bar is the cell: nothing smaller to repaint
}

// The whole envelope row at once.  Each cell paints its own slice of a curve
// that spans two cells, so a change in ANY of the six numbers has to repaint all
// four cells - that is why there is no per-cell fast path here.
void scrEnvRow(const char* const envCaps[4], const uint8_t envVals[4],
               uint8_t cutoff, uint8_t resonance) {
  bool changed = false;
  const uint8_t fVal[2] = { (uint8_t)(cutoff > 127 ? 127 : cutoff),
                            (uint8_t)(resonance > 127 ? 127 : resonance) };

  // cells 4/5 hold the filter (cutoff, resonance), 6/7 the ADSR
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t i = (uint8_t)(4 + k);
    if (cells[i].mode != CELL_ENV || filterVal[k] != fVal[k]) changed = true;
    filterVal[k] = fVal[k];
    cells[i].mode = CELL_ENV;
    cells[i].label[0] = '\0';
    cells[i].value[0] = '\0';
  }
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t i = (uint8_t)(6 + k);
    if (cells[i].mode != CELL_ENV) changed = true;
    cells[i].mode = CELL_ENV;
    cells[i].label[0] = '\0';
    cells[i].value[0] = '\0';
  }
  for (uint8_t k = 0; k < 4; k++) {
    char buf[sizeof(envCap[k])];
    copyText(buf, sizeof(buf), fullName(envCaps[k]));
    const uint8_t v = envVals[k] > 127 ? 127 : envVals[k];
    if (envVal[k] != v || strcmp(buf, envCap[k]) != 0) changed = true;
    memcpy(envCap[k], buf, sizeof(buf));
    envVal[k] = v;
  }

  if (!changed) return;
  for (uint8_t k = 0; k < 4; k++) cellDirty[4 + k] = true;
}

// The whole LFO row at once, for the same reason as the envelope: a curve
// spans two cells and every readout can change width, so any change repaints
// all four.  `cutoff` / `resonance` are ENC1 / ENC2's CC values - the filter
// half's two numbers, exactly as the envelope row takes them.
void scrLfoRow(const char* const lfoCaps[4], const char* const lfoVals[4],
               uint8_t depth, uint8_t rate, uint8_t shape, uint8_t sync,
               uint8_t cutoff, uint8_t resonance) {
  bool changed = false;

  // cells 4/5: the filter (cutoff, resonance), no readout of its own
  const uint8_t fVal[2] = { lfoClamp7(cutoff), lfoClamp7(resonance) };
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t i = (uint8_t)(4 + k);
    if (cells[i].mode != CELL_LFO || filterVal[k] != fVal[k]) changed = true;
    filterVal[k] = fVal[k];
    cells[i].mode = CELL_LFO;
    cells[i].label[0] = '\0';
    cells[i].value[0] = '\0';
  }

  // cells 6/7 carry the waveform and its four readouts
  for (uint8_t k = 0; k < 2; k++) {
    const uint8_t i = (uint8_t)(6 + k);
    if (cells[i].mode != CELL_LFO) changed = true;
    cells[i].mode = CELL_LFO;
    cells[i].label[0] = '\0';
    cells[i].value[0] = '\0';
  }
  const uint8_t raw[4] = { lfoClamp7(depth), lfoClamp7(rate),
                           lfoClamp7(shape), lfoClamp7(sync) };
  for (uint8_t k = 0; k < 4; k++) {
    char cb[sizeof(lfoCap[k])];
    char vb[sizeof(lfoTxt[k])];
    copyText(cb, sizeof(cb), fullName(lfoCaps[k]));
    copyText(vb, sizeof(vb), fullName(lfoVals[k]));
    if (lfoVal[k] != raw[k] || strcmp(cb, lfoCap[k]) != 0 ||
        strcmp(vb, lfoTxt[k]) != 0)
      changed = true;
    lfoVal[k] = raw[k];
    memcpy(lfoCap[k], cb, sizeof(cb));
    memcpy(lfoTxt[k], vb, sizeof(vb));
  }

  if (!changed) return;
  for (uint8_t k = 0; k < 4; k++) cellDirty[4 + k] = true;
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
