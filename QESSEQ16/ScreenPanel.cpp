#include "Config.h"

#ifdef use_SC01Plus

#include "Screen.h"
#include "PanelSC01.h"
#include "SeqClock.h"   // seqStepBreath(): the paint loop hands the beat through

#include "Keyboard.h"  // keyChannel(), seqEditChannel()
#include "Encoders.h"  // encChannel()

#include <math.h>
#include <stdio.h>
#include <string.h>

// The single panel instance, owned here (PanelSC01.h declares it).
PanelSC01 tft;

/*****************************************************************************
 SC01 Plus (480x320 ST7796) - the panel layout: the content the OLED carries,
 given the room a 480 px screen has.  The one band with no OLED counterpart is
 the step lane; everything else is the strip's own furniture, larger:

   OLED 128x64                  SC01 Plus 480x320
   ------------------------------------------------------------------
   y  0..15  header strip   ->  y   0..35   header (36 px): the page's name
                                            at 16 px, a 12 px status slot on
                                            the right, accent rule under it
   (no OLED counterpart)    ->  y  37..64   step lane: sixteen squares numbered
                                            1..16, the encoder channel outlined,
                                            the sequencer's lit, the keyboard's
                                            underlined, the playhead marked
   y 16..47  two knob rows  ->  y  66..195  one card: 4 x 2 cells of 117x65
   y 48..63  control legend ->  y 197..234  help row: a pictogram, the page's
                                            name and one line about it
                             ->  y 236..313  legend: 3 rows, 3 + 3 + 4 chips

 PAD (1 px) separates every band; the bottom 6 px stay dark.

 Fonts: the panel draws in Nasalization, embedded as a compact bitmap font.
 Three roles and no shrinking to fit - 16 px for the header's name and every
 cell value, 12 px for the legend chips and the help row's name, 10 px for the
 captions, the status slot, the lane numbers, the readouts and the help line
 (see the type block further down).  The OLED's own fonts are still the
 fallback if that load fails: font 4 (26 px) for the header, font 2 (16 px) for
 the rest, font 1 (8 px) for the small print.

 A cell reads caption over value: the caption at 10 px on y+7, the value at
 16 px on y+21, and a value too wide for the 101 px column is wrapped onto a
 second line at y+38 instead of being abbreviated or allowed to run into the
 next cell - so a scale name is read as "Harmonic Minor" over two lines, not as
 "HarmMin".

 Everything is immediate mode, as on the Teensy: the scr*() calls record what
 a cell should say and scrUpdate() draws only what changed, so a knob turn
 repaints one dial and not the whole band.
*****************************************************************************/

// ---------------------------------------------------------------------------
// Palette (RGB565) - from SC01_SEQPORT/Display.cpp
// ---------------------------------------------------------------------------
// The pre-redesign theme, kept under the new layout: a black page, dark grey
// cards and chips, grey hairlines, WHITE for every value and name, a light grey
// for the captions read second, and cyan only where something is live - the
// accented chip, the lane's three channel marks, the playhead, the curves and
// the transient message.  The lane sits apart from the other chips: a square's
// fill is the page's black, so the row reads as sixteen pads on the page and
// not as a grey band, and the three marked squares are the only thing that
// draws the eye.
// White-on-black is what the panel had before the mockup
// restyle; it is the more comfortable of the two to read at 16 px.  Hex in the
// comments is the 8-bit colour these 565 words were rounded from.
static const uint16_t COL_BG = 0x0000;      // page:      #000000
static const uint16_t COL_CARD = 0x18E3;    // card:      #1C1C1C
static const uint16_t COL_GRIDBG = 0x0000;  // grid:      black
static const uint16_t COL_GRID = 0x2965;    // hairline:  #292929
static const uint16_t COL_EDGE = 0x4A49;    // chip edge: #4A4A4A
static const uint16_t COL_BTN = 0x18E3;     // level-bar track: #1C1C1C
static const uint16_t COL_DIM = 0x8C71;     // dim text:  #8C8C8C
static const uint16_t COL_CAPTION = 0xC618; // caption:   #C6C6C6
static const uint16_t COL_TEXT = 0xFFFF;    // value:     white
static const uint16_t COL_RING = 0x630C;    // dial ring, unfilled
static const uint16_t COL_ACCENT = 0x07FF;  // live:      cyan

// ---------------------------------------------------------------------------
// Type: one family, two fixed sizes, and nothing shrinks to fit.
//
// The panel used to mix FreeSans with the bitmap fonts and let every over-wide
// string drop a size, which is why ACCENT and PROBABILITY - the same legend row
// - came out in different fonts and sizes.  Nothing scales any more; the size
// is part of the role, and the boxes were sized for the face: at 10 px the
// widest caption (INPUT CHANNEL) is 87 px inside the 101 px text box, at 12 px
// the widest legend (PROBABILITY) is 83 px inside a 111 px chip.
// ---------------------------------------------------------------------------
#define FONT_NOTE  1   // 8 px: the 10 px face's stand-in
#define FONT_BIG   4   // 26 px: the 16 px face's stand-in
#define FONT_SMALL 2   // 16 px: the 12 px face's stand-in

// ---------------------------------------------------------------------------
// Type face: Nasalization, the brand face, instead of the built-in bitmap
// fonts above - which stay as the fallback if the font ever fails to load.
//
// The OTF cannot be drawn by this panel, so it was converted to LovyanGFX's
// compact bitmap font (head/cmap/loca/glyf, 1 bpp) and embedded in flash by
// fonts/nasa_fonts.h; PointerWrapper feeds the bytes to lgfx::BFFfont, so the
// board needs no filesystem and no SD card to have its type.  Three sizes, the
// ones the layout was built around:
//
//   16 px  the header's name and every cell value (useTitle)
//   12 px  every legend chip and the help row's name (useGrid)
//   10 px  cell captions, the header's status slot, the lane numbers, the
//          ADSR / LFO readouts and the help line (useCaption)
//
// The caption face is the smaller one because the caption is read second: the
// value is the number the hand is on.  A 12 px caption would also put INPUT
// CHANNEL at 104 px, which still leaves only 8 px of air in the 117 px cell;
// at 10 px it is 87 px and the cell breathes.
//
// loadFont() parses the tables into RAM once, from scrBegin().  A failed load
// leaves the built-in faces selected, so a bad font can never blank the panel.
// ---------------------------------------------------------------------------
#include "fonts/nasa_fonts.h"

static lgfx::BFFfont nasaHeader;
static lgfx::BFFfont nasaGrid;
static lgfx::BFFfont nasaNote;
static lgfx::PointerWrapper nasaHeaderSrc(kFontNasa16, sizeof(kFontNasa16));
static lgfx::PointerWrapper nasaGridSrc(kFontNasa12, sizeof(kFontNasa12));
static lgfx::PointerWrapper nasaNoteSrc(kFontNasa10, sizeof(kFontNasa10));
static bool nasaUp = false;
static bool noteUp = false;

static void scrLoadFonts() {
  if (nasaUp) return;
  const bool h = nasaHeader.loadFont(&nasaHeaderSrc);
  const bool g = nasaGrid.loadFont(&nasaGridSrc);
  const bool n = nasaNote.loadFont(&nasaNoteSrc);
  nasaUp = h && g;
  noteUp = n;      // the message line falls back on its own if this one fails
}

// Three faces, three jobs, each with its own fallback if the load failed:
//   useTitle()   16 px  the header's name and every cell value
//   useGrid()    12 px  legend chips and the help row's name
//   useCaption() 10 px  cell captions, the header's status slot, the lane's
//                       numbers, the ADSR / LFO readouts and the help line
static void useTitle() {
  if (nasaUp) tft.setFont(&nasaHeader); else tft.setTextFont(FONT_BIG);
}
static void useGrid() {
  if (nasaUp) tft.setFont(&nasaGrid); else tft.setTextFont(FONT_SMALL);
}
static void useCaption() {
  if (noteUp) tft.setFont(&nasaNote); else tft.setTextFont(FONT_NOTE);
}

// ---------------------------------------------------------------------------
// Layout: header 0..35, step lane 37..64, cells 66..195, help row 197..234,
// legend 236..313 - one pixel of page background between the bands.
// ---------------------------------------------------------------------------
#define PANEL_W       480
#define PANEL_H       320
#define PAD             1                       // air between the vertical bands
// The header is ONE line: the family / page name at 16 px on the left and the
// 10 px slot on the right.  The slot carries the temporary message while one
// is up and the panel status (`TRACK 02 - STEP 08/16`) when none is, so the
// announcement never disturbs the name and never needs a line of its own.
#define HEADER_H       36
#define HEADER_TEXT_Y   8                       // the 16 px name's ink
#define STATUS_TEXT_Y  12                       // the 12 px slot's ink
// The step lane is the pads' own picture: sixteen 25 x 22 squares, each with
// its own pad number (1..16) drawn inside, and the three channel marks every
// mode shows at once - the encoder channel's square edged, the sequence
// channel's filled in the accent, the keyboard channel's number underlined -
// with the playhead marked inside its own square.  With the number inside
// instead of under a 13 px bar, the band is the squares plus 3 px of air above
// and below.  The squares are 22 px tall: a chip, not a tile - the row is the
// strip along the top of the page, and the knob card under it is the page
// itself.
#define LANE_Y        (HEADER_H + PAD)          // 37
#define LANE_H         28
#define LANE_BAR_Y    (LANE_Y + 3)              // 40
#define LANE_BAR_H     22                      // a square: the number fits inside
#define LANE_SLOTS     16
#define LANE_W         25                      // 16 x (25 + 4) = 460, 10..469
#define LANE_GAP        4
#define LANE_X0        10
// The knob grid is ONE rounded card with hairline dividers inside it: four
// columns of 117 px and two rows of 65, inset 6 px from the panel's edges.
#define GRID_X          6
#define GRID_Y        (LANE_Y + LANE_H + PAD)   // 66
#define GRID_W        468
#define GRID_R          6
#define CELL_W        (GRID_W / 4)              // 117
#define CELL_H         65
#define MAIN_Y        GRID_Y
#define PAD_X           8                       // cell left inset
#define TEXT_W        (CELL_W - PAD_X * 2)      // 101
// A cell reads caption over value: the 10 px caption at +7, the 16 px value at
// +21 and its wrapped second line at +38 - all inside the 65 px cell.
#define CAP_H          11                       // one line of the 10 px face
#define LABEL_Y         7
#define VALUE_Y        21
#define VAL_H          17                       // one line of the 16 px face

// The help row: a card with a pictogram, the page's name and one line saying
// what that page is for.
#define INFO_Y        (GRID_Y + CELL_H * 2 + PAD)     // 197
#define INFO_H         38

#define LEGEND_Y      (INFO_Y + INFO_H + PAD)         // 236
#define LEGEND_ROWS     3
#define LEGEND_SLOT_H  26

// The legend is 3 + 3 + 4 columns, the mockup's split: the first two rows are
// wider (150 px, 9 apart = 468) because PREV / NEXT and the three menus read
// as one group, the last row is four (111 px, 8 apart = 468) because the
// attributes are short words.  Every chip is 24 px tall in its 26 px row, 5 px
// radius, and an empty slot keeps its column standing as an outline.
#define BTN_H          24
#define BTN_R           5
#define LEG_WIDE_W    150
#define LEG_WIDE_GAP    9
#define LEG_NARROW_W  111        // row 2
#define LEG_NARROW_GAP  8
#define LEG_X0          6

// Dial geometry inside a cell (ring outer radius 24, caption in the notch).
// The cell is 65 tall: centred at 30 the 29 px repaint box starts one pixel
// below the cell's top edge (so it never eats the grid line); its lower edge
// runs to 52 because the pointer's lowest reach is 50, which lands in the top
// of the 10 px caption's ink - drawDialOnly() redraws the caption afterwards
// rather than leave the label cut.
#define DIAL_CY         30
#define DIAL_R          24
#define DIAL_CAPTION_Y  52
// Region repainted when only a dial's value changed: ring, pointer at its
// farthest (R+4 plus the 2 px stroke) and the value text. From the cell centre
// that is 29 px sideways/up and 22 px down, so it stops short of the cell's
// left edge, its top edge and the caption: the grid lines and the label are
// never erased by an encoder step.
#define DIAL_CLR_DOWN   22              // the pointer's lowest end is 20
#define DIAL_CLR        (DIAL_R + 5)    // 29: the ring and the pointer's reach

// Bar cell (FLAVOR = BARS): a 101 px track across the cell with the level in
// it, the CC number above and the number itself under the track.
#define BAR_Y          24
#define BAR_H          16
#define BAR_NUM_Y      50
// Envelope row (FLAVOR = ENVELOPE): the curve takes the top of the cell, then a
// 6 px bar and the CC caption + number under it, all inside the 67 px row.
//
// The row is split in two: the FILTER in ENC1's cell on the left, the ADSR of
// ENC5..8 on the right.  The curve used to span the whole 480 px, which made
// the four values unreadable off the shape; at half the width the envelope
// reads at a glance and ENC1's cell gains the filter the cutoff / resonance
// knobs are driving.
#define ENV_TOP         6
#define ENV_H          30
#define ENV_BAR_Y     (ENV_TOP + ENV_H + 3)     // 39
#define ENV_BAR_H       5
#define ENV_CAP_Y     (ENV_BAR_Y + ENV_BAR_H + 1)  // 45
#define ENV_HALF_W    (PANEL_W / 2)             // 240: filter | envelope
#define ENV_SLOT_W    (ENV_HALF_W / 4)          // 60: one ADSR value's column
// The filter graph fills ENC1's cell: a rectangle the size of the container
// (117 x 65, inset two pixels) with the curve drawn inside it, so the graph
// reads as its own instrument panel instead of a curve floating in the row.
// It is the shape its two dials are about, so it gets the whole cell and the
// taller plot that comes with it - nearly twice the ADSR's 30 px band.
#define ENV_FLT_BOX_X   2                       // inset from the cell's edges
#define ENV_FLT_BOX_Y   2
#define ENV_FLT_BOX_W  (CELL_W - 4)             // 113
#define ENV_FLT_BOX_H  (CELL_H - 4)             // 61
#define ENV_FLT_PAD     4          // air on the left only: the passband starts clear
#define ENV_FLT_PLOT_X (ENV_FLT_BOX_X + ENV_FLT_PAD)      // 6
// Vertically the curve uses the frame's whole interior, because a maxed
// resonance has to touch the frame's top side and the fall to reach the floor
// with no gutter under it.  The passband still lands on the frame's middle row
// (32): 3..61 is symmetric about it, like the frame's own 2..62.
#define ENV_FLT_PLOT_Y (ENV_FLT_BOX_Y + 1)                // 3
#define ENV_FLT_PLOT_H (ENV_FLT_BOX_H - 2)                // 59
// The curve's own x span: it starts in the frame's left air but runs all the
// way to the frame's right side.  A maxed cutoff has to END on that right edge,
// so the axis cannot stop at the inset plot's edge and leave a 4 px gap under
// the fall - the wall would hang in the middle of the rectangle.
#define ENV_FLT_CURVE_W (ENV_FLT_BOX_W - ENV_FLT_PAD - 1)  // 108: x 6..113
// The filter curve, in dB (see filterLevel).  The passband is 0 dB, the peak
// sits FLT_DB_PEAK above it, and the window ends FLT_DB_BOT below it.
//
// The window is SYMMETRIC, +32 / -32 dB, and that is what puts the passband -
// the flat run the curve starts with on the left - on the frame's middle row.
//
// FLT_DB_PEAK is the resonance's height in dB, an absolute amplitude so the bell
// does not change size with the window: 33.1 dB over a response that is already
// a decibel or two down there is the 31.5 dB that puts a maxed resonance's tip
// on the plot's top row, one pixel under the frame's top line.
#define FLT_DB_TOP      32.0f
#define FLT_DB_PEAK     34.0f                  // the resonance at 127, in dB
#define FLT_DB_BOT    (-32.0f)
// The response is a 4-pole low-pass - 24 dB/oct, the slope the module's name is
// about - and the order is fixed, because the slope is a property of the filter
// and not of the knob.  Only where the knee sits changes with the cutoff.  At
// this width the fall is 0.40 decades, fifteen of the curve's 108 columns: that
// is what gives the knee its tight bend and the descent its wall-like look
// without the fall turning into a vertical line.  Two poles spread it over
// twenty-nine columns and read as a hill; sixteen drew it in five and read as
// a cut.
#define FLT_DB_ORDER     4
// The corner's span along the 0..3 decade axis.  The top of the range puts the
// maxed corner at 2.60 decades - four fifths of the way across, the far right -
// and the fifteen columns of fall that follow land on the floor exactly at the
// frame's right side, so a wide-open filter is a flat line, a tight knee and a
// steep drop into the bottom-right corner.
#define FLT_DB_F0_MIN   0.18f
#define FLT_DB_F0_MAX   2.60f
// The envelope has to START on the cell edge: its four 60 px readouts must each
// stay inside the cell that paints them, or the neighbour's fill erases them.
#define ENV_ENV_X     ENV_HALF_W                // 240: the ADSR starts here
// The LFO row uses the same skeleton: the filter on the left, then the waveform
// on the right.  Its four readouts sit on ONE line under the row as
// "CAP: value" - the value directly behind its caption instead of parked at
// the far end of a slot, which read as four loose numbers.  The line is
// RIGHT-aligned: the last value ends 4 px from the right edge and the pairs run
// left from there, so the four readouts sit under the half they describe
// instead of starting under ENC2's column and drifting inwards whenever a value
// changed width.  The line sits at ENV_CAP_Y, below the filter's corner.
#define LFO_READ_R    (PANEL_W - 8)                     // 472: right-aligned end
#define LFO_READ_GAP  14                                // air between pairs
#define LFO_READ_Y    ENV_CAP_Y                         // 45: the ADSR's line

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
// The header's right slot when nothing is being announced: the panel status
// (scrStatus), `TRACK 02 - STEP 08/16`.  It has its own repaint box, because it
// changes on every step while the name above it does not.
static char statusText[32] = "";

// The names the help row falls back on when the header has no page name.
static char modeLabel[3][10] = { "ENCODER", "SEQUENCER", "KEYBOARD" };
static uint8_t activeMode = SCR_MODE_NONE;

// The step lane, from scrStepLane: the three channels every mode marks at once
// (the sequencer's edit channel, the encoder bank's CC channel, the keyboard's
// channel), one bit per slot for a step that carries notes, and the pad the
// playhead is on (0xFF = none).  The three channels are read fresh on every
// pass and compared here, so a channel move repaints the lane even when the
// rest of the screen has nothing to say about it.
static uint8_t laneSeqCh = 0;
static uint8_t laneEncCh = 0;
static uint8_t laneKeyCh = 0;
static uint16_t laneUsed = 0;
static uint8_t lanePlay = 0xFF;
static uint8_t lanePlayDrawn = 0xFF;

static bool headerDirty = true;
static bool statusDirty = true;
static bool laneDirty = true;
static bool helpDirty = true;
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
// 32 px and had to be an abbreviation.  A cell here is 117x65, so the panel
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

// One text run in one of the cell's two faces: `big` picks the value's 16 px
// cut over the caption's 10 px one.  Nothing shrinks to fit - a run wider than
// the cell is cut at its last space and carried onto the next line of the same
// face, up to maxLines of them, which is what lets a value read as "Harmonic
// Minor" over two lines instead of "HarmMin" on one.  A word longer than the
// cell is broken where it runs out, and anything past the last line is dropped
// rather than drawn over the cell's edge.
static void drawText(const char* s, int x, int y, uint8_t maxLines,
                     uint16_t color, uint16_t bg, bool big) {
  if (!s || !s[0]) return;
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  if (big) useTitle(); else useCaption();
  const int lh = big ? VAL_H : CAP_H;
  tft.setTextColor(color, bg);

  char line[32];        // one drawn line: TEXT_W holds a 14-char caption
  char one[2] = {0, 0};
  int n = 0;            // characters in the line being built
  int w = 0;            // ...and its width in pixels
  int brk = -1;         // index of its last space: where the line can be cut
  uint8_t row = 0;      // which line is being built (0 = the first)
  const char* p = s;

  for (;;) {
    const char c = *p;
    if (c == '\0') {                       // the tail fits: draw it and stop
      if (n) { line[n] = '\0'; tft.drawString(line, x, y + row * lh); }
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
    tft.drawString(line, x, y + row * lh);
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

  // the number inside the ring is a VALUE: the 16 px face, in the live colour
  char v[4];
  snprintf(v, sizeof(v), "%u", (unsigned)val);
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  useTitle();
  tft.setTextColor(COL_TEXT, COL_GRIDBG);
  tft.drawString(v, cx, cy);
}

// ---------------------------------------------------------------------------
// Bands
// ---------------------------------------------------------------------------
// The status slot's own box: it starts well right of the longest page name
// (`CHANNEL SELECT` is 150 px at 16 px) so a per-step redraw never touches it.
#define STATUS_X0     240

static void drawStatus() {
  tft.fillRect(STATUS_X0, STATUS_TEXT_Y - 5, PANEL_W - STATUS_X0, 20, COL_BG);
  const char* r = headerRight[0] ? headerRight : statusText;
  if (!r[0]) return;
  tft.setTextDatum(lgfx::textdatum_t::top_right);
  useGrid();          // 12 px: the 10 px face was too small to read at a glance
  tft.setTextColor((headerRight[0] && headerTransient) ? COL_ACCENT : COL_CAPTION,
                   COL_BG);
  tft.drawString(r, PANEL_W - 12, STATUS_TEXT_Y);
}

// The header: the page's name at 16 px in the accent, the accent rule under it,
// and the 10 px slot on the right.  The slot carries the temporary message
// while one is up - so the announcement never disturbs the name and never needs
// a line of its own - and the panel status when none is.
static void drawHeader() {
  tft.fillRect(0, 0, PANEL_W, HEADER_H, COL_BG);
  tft.fillRect(0, HEADER_H - 2, PANEL_W, 1, COL_ACCENT);

  if (headerLeft[0]) {
    tft.setTextDatum(lgfx::textdatum_t::top_left);
    useTitle();
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(headerLeft, 12, HEADER_TEXT_Y);
  }
  drawStatus();
  useGrid();          // back to the default face for later draws
}

// One square of the step lane, its own pad number (1..16) drawn inside it, and
// the three channel marks every mode shows at the same time:
//
//   outlined     the encoder channel's square has the accent edge
//   lit          the sequencer channel's square is filled with the accent
//   underlined   the keyboard channel's number carries a bar under it
//
// The marks are read from the three channel getters rather than from "the
// channel the mode is on", because in every mode all three apply at once: a
// keyboard page still shows which channel the sequence and the encoder bank
// are on, and vice versa.  KEYMODE3 locks the encoder and the keyboard to the
// sequence's channel, so the locked square naturally carries all three - the
// fill, the edge and the underline land on the same pad.
//
// An edge on an accent fill would be invisible, so the encoder's outline is
// drawn in the page's black where the square is already lit; elsewhere it is
// cyan on the page's black.  The number's colour still says whether the pad
// carries notes - white for a used step, grey for an empty one - which is the
// mark the accent edge used to carry before the edge became the encoder's.  The
// playhead keeps its bar at the bottom of the square: black on the accent, cyan
// everywhere else, so it reads on any fill, and clear of the underline above it.
static void drawLaneChip(uint8_t k) {
  const int x = LANE_X0 + k * (LANE_W + LANE_GAP);
  const bool lit = (laneSeqCh == (uint8_t)(k + 1));
  const bool edge = (laneEncCh == (uint8_t)(k + 1));
  const bool under = (laneKeyCh == (uint8_t)(k + 1));
  const bool used = (laneUsed & (uint16_t)(1u << k)) != 0;
  const bool play = (lanePlay == k);

  const uint16_t fill = lit ? COL_ACCENT : COL_BG;
  const uint16_t ring = edge ? (lit ? COL_GRIDBG : COL_ACCENT)
                             : (lit ? COL_ACCENT : COL_EDGE);
  tft.fillRoundRect(x, LANE_BAR_Y, LANE_W, LANE_BAR_H, 3, fill);
  tft.drawRoundRect(x, LANE_BAR_Y, LANE_W, LANE_BAR_H, 3, ring);

  // The number sits a little above the square's centre: its box is 11 px and
  // the square is 22, which leaves the underline room under the ink (53..54)
  // and the playhead its bar at the bottom (58..59).
  char n[4];
  snprintf(n, sizeof(n), "%u", (unsigned)(k + 1));
  const int cx = x + LANE_W / 2;
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  useCaption();
  tft.setTextColor(lit ? COL_GRIDBG : (used ? COL_TEXT : COL_DIM), fill);
  tft.drawString(n, cx, LANE_BAR_Y + 8);

  if (under)
    tft.fillRect(cx - 7, LANE_BAR_Y + 13, 14, 2, lit ? COL_GRIDBG : COL_ACCENT);

  if (play)
    tft.fillRect(x + 3, LANE_BAR_Y + LANE_BAR_H - 4, LANE_W - 6, 2,
                 lit ? COL_GRIDBG : COL_ACCENT);
}

static void drawStepLane() {
  tft.fillRect(0, LANE_Y, PANEL_W, LANE_H, COL_BG);
  for (uint8_t k = 0; k < LANE_SLOTS; k++) drawLaneChip(k);
  lanePlayDrawn = lanePlay;
}

// The playhead moved and nothing else did: two chips carry the change, not the
// sixteen (a step would otherwise repaint the whole lane, every step).
static void drawLanePlay() {
  if (lanePlayDrawn < LANE_SLOTS) drawLaneChip(lanePlayDrawn);
  if (lanePlay < LANE_SLOTS) drawLaneChip(lanePlay);
  lanePlayDrawn = lanePlay;
}

// ---------------------------------------------------------------------------
// Help row: a pictogram, the page's name and one line saying what it is for.
//
// The line is looked up in the table below from the header's own left label, so
// the row follows the page the rest of the screen is already on; a page the
// table does not know simply gets no line.  These are the panel's words, the
// same way the caption table above is: the 128x64 strip has no room for them.
// ---------------------------------------------------------------------------
enum HelpIcon : uint8_t { HICON_NONE = 0, HICON_PULSE, HICON_STEP, HICON_KNOB,
                          HICON_KEY, HICON_SPARK };
struct HelpLine { const char* match; const char* line; uint8_t icon; };
static const HelpLine kHelp[] = {
  // pages that own the header's left slot
  { "DRUM PATTERN", "Kit and pattern of the drum track", HICON_STEP },
  { "CHANNEL SELECT", "Pick the MIDI channel to edit", HICON_KEY },
  { "STEP FX",   "Poly effects on this step",        HICON_SPARK },
  { "STEP TYPE", "Which voice plays the step",       HICON_SPARK },
  { "EMBED",     "A chord inside the step",          HICON_STEP },
  { "RATCHET",   "Repeats the step inside itself",   HICON_PULSE },
  { "PROBABILITY", "Chance the step plays",          HICON_STEP },
  { "TIE",       "Holds the step into the next one", HICON_PULSE },
  { "GLIDE",     "Slides into the next step's pitch", HICON_PULSE },
  { "ACCENT",    "Extra velocity on this step",      HICON_SPARK },
  { "GENERATOR", "Writes a pattern into this track", HICON_SPARK },
  { "STRUM ADDS", "Extra notes on a strum",          HICON_KEY },
  { "FLAVOR",    "The second row's drawing",         HICON_KNOB },
  { "CC CONFIG", "What each knob sends",             HICON_KNOB },
  { "ARP",       "Arpeggiator order and options",    HICON_SPARK },
  { "ERASE",     "Hold to clear this track",         HICON_NONE },
  // the three families, which is what the header says the rest of the time
  { "ENCODER",   "Knobs send CCs, F picks the bank", HICON_KNOB },
  { "SEQUENCER", "Steps, page and track config",     HICON_STEP },
  { "KEYBOARD",  "Play, chord or genre page",        HICON_KEY },
};

static const HelpLine* helpFor(const char* label) {
  if (!label || !label[0]) return NULL;
  for (uint8_t i = 0; i < sizeof(kHelp) / sizeof(kHelp[0]); i++)
    if (strstr(label, kHelp[i].match)) return &kHelp[i];
  return NULL;
}

// The pictograms, drawn on an 80 x 22 box at (GRID_X + 16, INFO_Y + 8).
static void drawHelpIcon(uint8_t icon) {
  const int x0 = GRID_X + 16;          // 22
  const int y0 = INFO_Y + 8;           // 207
  const int cy = y0 + 10;
  tft.startWrite();
  switch (icon) {
    case HICON_PULSE: {                // two held notes and the air between
      const int top = y0 + 2, bot = y0 + 19;
      tft.drawLine(x0, bot, x0 + 6, bot, COL_ACCENT);
      tft.drawLine(x0 + 6, bot, x0 + 6, top, COL_ACCENT);
      tft.drawLine(x0 + 6, top, x0 + 20, top, COL_ACCENT);
      tft.drawLine(x0 + 20, top, x0 + 20, bot, COL_ACCENT);
      tft.drawLine(x0 + 20, bot, x0 + 28, bot, COL_ACCENT);
      tft.drawLine(x0 + 28, bot, x0 + 28, top, COL_ACCENT);
      tft.drawLine(x0 + 28, top, x0 + 42, top, COL_ACCENT);
      tft.drawLine(x0 + 42, top, x0 + 42, bot, COL_ACCENT);
      tft.drawLine(x0 + 42, bot, x0 + 50, bot, COL_ACCENT);
      tft.drawLine(x0 + 50, bot, x0 + 50, cy, COL_ACCENT);
      tft.drawLine(x0 + 50, cy, x0 + 72, cy, COL_ACCENT);
      break;
    }
    case HICON_STEP:                   // four chips, the third one lit
      for (uint8_t k = 0; k < 4; k++) {
        const int x = x0 + k * 18;
        if (k == 2) tft.fillRoundRect(x, y0 + 3, 14, 14, 3, COL_ACCENT);
        tft.drawRoundRect(x, y0 + 3, 14, 14, 3, k == 2 ? COL_ACCENT : COL_EDGE);
      }
      break;
    case HICON_KNOB:                   // a ring with its pointer
      tft.drawCircle(x0 + 34, cy, 10, COL_ACCENT);
      tft.drawLine(x0 + 34, cy, x0 + 41, y0 + 3, COL_ACCENT);
      break;
    case HICON_KEY:                    // three white keys, two black
      tft.drawRect(x0, y0 + 2, 42, 17, COL_ACCENT);
      tft.drawFastVLine(x0 + 14, y0 + 2, 17, COL_ACCENT);
      tft.drawFastVLine(x0 + 28, y0 + 2, 17, COL_ACCENT);
      tft.fillRect(x0 + 10, y0 + 2, 7, 9, COL_ACCENT);
      tft.fillRect(x0 + 24, y0 + 2, 7, 9, COL_ACCENT);
      break;
    case HICON_SPARK: {                // an eight-point spark
      tft.drawLine(x0 + 24, cy, x0 + 44, cy, COL_ACCENT);
      tft.drawLine(x0 + 34, cy - 10, x0 + 34, cy + 10, COL_ACCENT);
      tft.drawLine(x0 + 28, cy - 6, x0 + 40, cy + 6, COL_ACCENT);
      tft.drawLine(x0 + 28, cy + 6, x0 + 40, cy - 6, COL_ACCENT);
      break;
    }
    default: break;
  }
  tft.endWrite();
}

static void drawHelp() {
  const char* name = headerLeft[0] ? headerLeft
                                   : modeLabel[activeMode < 3 ? activeMode : 0];
  const HelpLine* h = helpFor(name);

  tft.fillRect(0, INFO_Y, PANEL_W, INFO_H, COL_BG);
  tft.fillRoundRect(GRID_X, INFO_Y, GRID_W, INFO_H, GRID_R, COL_BG);
  tft.drawRoundRect(GRID_X, INFO_Y, GRID_W, INFO_H, GRID_R, COL_GRID);

  drawHelpIcon(h ? h->icon : HICON_NONE);
  tft.drawFastVLine(GRID_X + 102, INFO_Y + 6, INFO_H - 12, COL_GRID);

  tft.setTextDatum(lgfx::textdatum_t::top_left);
  useGrid();
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(name, GRID_X + 112, INFO_Y + 4);
  if (h && h->line[0]) {
    useCaption();
    tft.setTextColor(COL_CAPTION, COL_BG);
    tft.drawString(h->line, GRID_X + 112, INFO_Y + 21);
  }
  useGrid();
}

static int cellX(uint8_t i) { return GRID_X + (i & 3) * CELL_W; }
static int cellY(uint8_t i) { return GRID_Y + (i >> 2) * CELL_H; }

// A row keeps its vertical dividers unless it is one of the curve flavours,
// whose picture spans two cells (a divider would cut the ADSR or the LFO).
static bool rowPlain(uint8_t row) {
  const uint8_t i = (uint8_t)(row * 4);
  return cells[i].mode != CELL_ENV && cells[i].mode != CELL_LFO;
}

// The card's outline and the hairlines inside it.  Every cell repaint redraws
// the whole frame rather than its own two edges: the fill covers the outline
// wherever the cell touches it, and one 1 px outline is a few hundred pixels
// of SPI - cheaper than tracking which corner a cell owns.
static void drawGridFrame() {
  for (uint8_t c = 1; c < 4; c++) {
    if (rowPlain(0))
      tft.drawFastVLine(GRID_X + c * CELL_W, GRID_Y + 1, CELL_H - 1, COL_GRID);
    if (rowPlain(1))
      tft.drawFastVLine(GRID_X + c * CELL_W, GRID_Y + CELL_H, CELL_H - 1, COL_GRID);
  }
  tft.drawFastHLine(GRID_X + 1, GRID_Y + CELL_H, GRID_W - 2, COL_GRID);
  tft.drawRoundRect(GRID_X, GRID_Y, GRID_W, CELL_H * 2, GRID_R, COL_GRID);
}

// The two CC-row flavors (see drawBarCell / drawEnvSlice below): drawCell
// dispatches to them, so they need to be declared before it.
static void drawBarCell(uint8_t i);
static void drawEnvSlice(uint8_t i);
static void drawLfoSlice(uint8_t i);

// Repaint one knob cell and nothing else.  The fill is the card's own colour
// and the frame is drawn back over it, so the cell reads as a compartment of
// the one card instead of a tile of its own.
static void drawCell(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);

  tft.fillRect(x, y, CELL_W, CELL_H, COL_GRIDBG);
  if (cells[i].mode == CELL_DIAL) {
    drawDial(x, y, cells[i].dialValue);
    tft.setTextDatum(lgfx::textdatum_t::middle_center);
    useCaption();
    tft.setTextColor(COL_CAPTION, COL_GRIDBG);
    tft.drawString(cells[i].label, x + CELL_W / 2, y + DIAL_CAPTION_Y);
  } else if (cells[i].mode == CELL_BAR) {
    drawBarCell(i);
  } else if (cells[i].mode == CELL_ENV) {
    drawEnvSlice(i);
  } else if (cells[i].mode == CELL_LFO) {
    drawLfoSlice(i);
  } else {
    drawText(cells[i].label, x + PAD_X, y + LABEL_Y, 1, COL_CAPTION, COL_GRIDBG, false);
    drawText(cells[i].value, x + PAD_X, y + VALUE_Y, 2, COL_TEXT, COL_GRIDBG, true);
  }
  drawGridFrame();
}

// One bar cell: the CC number above, a 101 px track filled to the value, and
// the number itself under the track (FLAVOR = BARS).
static void drawBarCell(uint8_t i) {
  const int x = cellX(i);
  const int y = cellY(i);

  tft.setTextDatum(lgfx::textdatum_t::top_left);
  useCaption();
  tft.setTextColor(COL_CAPTION, COL_GRIDBG);
  tft.drawString(cells[i].label, x + PAD_X, y + LABEL_Y);

  tft.fillRoundRect(x + PAD_X, y + BAR_Y, TEXT_W, BAR_H, 3, COL_BTN);
  const int w = (int)((uint32_t)cells[i].dialValue * (TEXT_W - 2) / 127);
  if (w > 0) tft.fillRect(x + PAD_X + 1, y + BAR_Y + 1, w, BAR_H - 2, COL_ACCENT);

  char v[4];
  snprintf(v, sizeof(v), "%u", (unsigned)cells[i].dialValue);
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  useTitle();
  tft.setTextColor(COL_TEXT, COL_GRIDBG);
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
  // attack 0 must be an instant jump, not a short ramp: the segment starts at
  // a single pixel and only the knob makes it an angle
  *a = 0.005f + 0.30f * ((float)v[0] / 127.0f);  // attack end
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

// The low-pass response for the row's left half, drawn the way a filter display
// reads: a flat passband, a corner at -3 dB, and a roll-off that keeps falling
// all the way to the right edge.  The curve is the filter's actual magnitude,
// not a shape tuned to look like one:
//
//     |H|^2 = 1 + u^8,   u = 10^d,   d = decades from the corner
//
// a 4-pole Butterworth low-pass.  Everything the picture needs falls out of that
// one expression: at d = 0 it is exactly -3 dB (0.707 of the passband), so the
// knee is a real bend and not a corner; before it the passband is flat to a
// hundredth of a dB; and behind it the slope settles at -80 dB/decade, the
// 24 dB/oct the module is about, which is steep enough at this width to read as
// a wall without becoming a vertical line.
//
// The y axis is dB, and that is what makes the fall read as a fall: plotted as
// amplitude, ANY filter curve is wrong on a small display - the tail plunges
// and then approaches zero asymptotically, which draws as a flat line along the
// floor.  In dB the roll-off is a straight line in the limit, so the curve has
// one slope from the peak to the bottom of the band.  The passband sits at 0 dB
// half way up the band; everything over it is the headroom the peak uses, and a
// maxed resonance spends all of it and touches the top row.
//
// The x axis is LOG frequency: three decades across the graph, the way a filter
// is actually displayed, and everything is measured in decades along it.  That
// is what keeps the peak and the knee the same shape wherever the cutoff sits.
// f runs 0..1 across the graph (axis position, not Hz), cutoff and res are the
// knobs' 0..127 values.
static float filterLevel(float f, uint8_t cutoff, uint8_t res) {
  const float dec = f * 3.0f;                                 // 0..3 decades
  const float f0 = FLT_DB_F0_MIN + (FLT_DB_F0_MAX - FLT_DB_F0_MIN) *
                                       ((float)cutoff / 127.0f);  // corner
  const float r = (float)res / 127.0f;                        // 0..1
  const float d = dec - f0;                                   // decades past corner

  // the filter itself: |H|^2 = 1 + u^8, u = 10^d - exactly -3 dB at the corner,
  // then a steady -80 dB/decade.  The knee is where the flat passband turns into
  // that slope; the turn is tight because the order is high, but it is still a
  // curve - nothing here is a corner or a vertical line.
  const float db = -10.0f * log10f(1.0f + powf(10.0f, 2.0f * FLT_DB_ORDER * d));

  // resonance: a smooth bell in dB sitting on top of that response, wide below
  // the corner and narrower above it like an analog peak.  At 0 it is not there
  // at all, so the curve is a plain low-pass.
  const float w = (d < 0.0f) ? (0.24f + 0.14f * r) : (0.20f + 0.10f * r);
  const float u = d / w;
  const float total = db + FLT_DB_PEAK * r * expf(-u * u);

  // below the window the curve rides its floor row (it is never cut off): the
  // caller draws every column, so the curve always reaches the right edge
  const float lvl = (total - FLT_DB_BOT) / (FLT_DB_TOP - FLT_DB_BOT);
  if (lvl <= 0.0f) return 0.0f;
  return lvl > 1.0f ? 1.0f : lvl;
}

// One tight readout under a curve: a w px bar filled to the value, its CC
// caption on the left and the number on the right.
static void envReadout(int x, int y, int w, const char* cap, uint8_t v) {
  if (w < 8) return;
  tft.fillRect(x, y + ENV_BAR_Y, w, ENV_BAR_H, COL_BTN);
  const int fill = (int)((uint32_t)v * (uint32_t)(w - 2) / 127);
  if (fill > 0)
    tft.fillRect(x + 1, y + ENV_BAR_Y + 1, fill, ENV_BAR_H - 2, COL_ACCENT);

  char n[4];
  snprintf(n, sizeof(n), "%u", (unsigned)v);
  tft.setTextDatum(lgfx::textdatum_t::top_left);
  useCaption();
  tft.setTextColor(COL_CAPTION, COL_GRIDBG);
  tft.drawString(cap, x, y + ENV_CAP_Y);
  tft.setTextDatum(lgfx::textdatum_t::top_right);
  tft.setTextColor(COL_TEXT, COL_GRIDBG);
  tft.drawString(n, x + w, y + ENV_CAP_Y);
  tft.setTextDatum(lgfx::textdatum_t::top_left);
}

// The filter graph: a rectangle the size of ENC1's cell, then the low-pass
// curve inside it.  The curve ends on the plot's bottom row where the response
// leaves the band and the floor is not drawn on to the edge: a line running
// along the bottom reads as part of the frame, not as a response.
// Shared by the ENVELOPE and LFO flavors (both own cell 4 only).
static void drawFilterBox() {
  const int x = cellX(4);
  const int y = cellY(4);

  tft.drawRect(x + ENV_FLT_BOX_X, y + ENV_FLT_BOX_Y,
               ENV_FLT_BOX_W, ENV_FLT_BOX_H, COL_GRID);

  const int px0 = x + ENV_FLT_PLOT_X;
  const int py0 = y + ENV_FLT_PLOT_Y;
  const int bottom = py0 + ENV_FLT_PLOT_H - 1;

  int prevX = px0;
  int prevY = bottom;
  bool joined = false;              // is prevX/prevY a point of the same curve?
  tft.startWrite();
  for (int k = 0; k < ENV_FLT_CURVE_W; k++) {
    const int px = px0 + k;
    const float f = (float)k / (float)(ENV_FLT_CURVE_W - 1);
    const float lvl = filterLevel(f, filterVal[0], filterVal[1]);
    // the response has left the bottom of its band: close the fall on the last
    // row and stop, rather than trailing a line along the floor to the frame
    if (lvl <= 0.0f) {
      if (joined) tft.drawLine(prevX, prevY, px, bottom, COL_ACCENT);
      break;
    }
    const int py = py0 + (int)lroundf((1.0f - lvl) * (float)(ENV_FLT_PLOT_H - 1));
    if (joined) tft.drawLine(prevX, prevY, px, py, COL_ACCENT);
    prevX = px;
    prevY = py;
    joined = true;
  }
  tft.endWrite();
}

// One cell's slice of the row: the framed filter in cell 4, the ADSR curve from
// cell 6 on, and nothing in between.  Each cell paints only the pixels inside
// its own 120 px.
static void drawEnvSlice(uint8_t i) {
  if (i == 4) {                     // ENC1's cell: the framed filter graph
    drawFilterBox();
    return;
  }
  if (i < 6) return;                // ENC2 / ENC3: nothing in this flavor

  const int x = cellX(i);
  const int y = cellY(i);

  int prevX = x;
  int prevY = y + ENV_TOP + ENV_H - 1;
  bool joined = false;              // is prevX/prevY a point of the same curve?
  tft.startWrite();
  for (int px = x; px < x + CELL_W; px++) {
    const float t = (float)(px - ENV_ENV_X) /
                    (float)(PANEL_W - ENV_ENV_X - 1);
    const float lvl = envLevel(t, envVal);
    // past the release there is nothing left: the curve ends there rather
    // than lying on the floor out to the edge of the row
    if (lvl < 0.0f) {
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

// The filter of the row, for the LFO flavor: the same framed graph the
// envelope row draws - passband, peak, fall inside ENC1's rectangle - and no
// readout of its own because those two numbers are the dials right above it.
// Only cell 4 has it; cell 5 stays blank so the two curves never overlap.
static void drawFilterSlice(uint8_t i) {
  if (i == 4) drawFilterBox();
}

// The LFO row, laid out like the envelope row: the FILTER in ENC1's framed
// cell on the left, the blank band, then the still waveform on the right with
// its four values under it (see lfoSyncedWave in Screen.h).  Nothing here animates -
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
  // its own 120 px of it, the same slice scheme the ADSR uses (the right half
  // keeps the taller footprint free for the readout line under it).
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

  // The four readouts, one line under the row: "DEPTH: 75%", "RATE: 1.2HZ"...
  // Caption and value are one phrase, so the value starts right behind its
  // colon instead of at the far end of a 90 px slot.  The four phrases are
  // measured, then laid out RIGHT-aligned: the last value ends at LFO_READ_R
  // and the rest run left from it, so the block is flush with the panel edge
  // whatever widths the values happen to have.
  //
  // Only the LAST cell of the row draws them: the line crosses all four cells
  // and a cell's fill would erase text an earlier cell had already written
  // (scrUpdate paints 4, 5, 6, 7 in that order, so 7 is always last).
  if (i >= 7) {
    char capColon[4][16];
    int capW[4], valW[4];
    int span = (int)(3 * LFO_READ_GAP);
    useCaption();
    for (uint8_t k = 0; k < 4; k++) {
      snprintf(capColon[k], sizeof(capColon[k]), "%s:", lfoCap[k]);
      capW[k] = (int)ceilf(tft.textWidth(capColon[k])) + 4;   // + the space
      valW[k] = (int)ceilf(tft.textWidth(lfoTxt[k]));
      span += capW[k] + valW[k];
    }
    int px0 = LFO_READ_R - span;
    if (px0 < 2) px0 = 2;              // more than the row holds: start at the edge
    tft.setTextDatum(lgfx::textdatum_t::top_left);
    for (uint8_t k = 0; k < 4; k++) {
      tft.setTextColor(COL_CAPTION, COL_GRIDBG);
      tft.drawString(capColon[k], px0, y + LFO_READ_Y);
      tft.setTextColor(COL_TEXT, COL_GRIDBG);
      tft.drawString(lfoTxt[k], px0 + capW[k], y + LFO_READ_Y);
      px0 += capW[k] + valW[k] + LFO_READ_GAP;
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
               DIAL_CLR + DIAL_CLR_DOWN + 1, COL_GRIDBG);
  drawDial(x, y, cells[i].dialValue);

  // The box has to reach the pointer's lowest point (50, in the ring's open
  // bottom notch), and that is inside the top of the CC caption's ink at 52.
  // The caption is drawn back over the cleared rows: without it a knob step
  // left every label with its top cut off, which read as the number being
  // hidden behind the dial's own background.
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  useCaption();
  tft.setTextColor(COL_CAPTION, COL_GRIDBG);
  tft.drawString(cells[i].label, cx, y + DIAL_CAPTION_Y);
}

// ---------------------------------------------------------------------------
// Control legend: three rows of buttons, 3 + 3 + 4 columns.
//
// The OLED's 5 + 4 slots are split 2 + 3 + 4 here so PREV / NEXT have a row to
// themselves above the two control rows, and the mockup's column split does the
// grouping: the first two rows are wide chips (150 px, 9 apart) because
// PREV / NEXT and the three menus read as one group, the last row is four
// narrow ones (111 px, 8 apart) because the attributes are short words.  Rows
// start at the cell captions' left edge (6) and fill the 468 px, so the strip
// lines up with the card above it and never re-flows when a label changes.
//
// An empty slot keeps its column standing as an outline instead of pulling the
// row left, and the first row's third column has no slot at all - it gets the
// same outline, which is what makes the top two rows read as one grid.
//
// One button can be ARMED: the chosen F radio, or the M button whose menu is
// open.  It is filled with the accent and printed dark, so the band says what
// the pads already say, since a chosen F pad burns white (Modes.cpp).  The fill
// does not move any label: the layout is the fixed grid either way.  Positions
// come from the row's own column grid, so a change repaints the whole strip
// (3 rows, ~2 ms) rather than one slot - it happens on a page change, never on
// a knob turn.
// ---------------------------------------------------------------------------
static int legendX(uint8_t row, uint8_t col) {
  return (row < 2) ? GRID_X + col * (LEG_WIDE_W + LEG_WIDE_GAP)
                   : GRID_X + col * (LEG_NARROW_W + LEG_NARROW_GAP);
}
static int legendW(uint8_t row) {
  return (row < 2) ? LEG_WIDE_W : LEG_NARROW_W;
}

static void drawLegendRow(uint8_t row, uint8_t first, uint8_t count) {
  const int y = LEGEND_Y + row * LEGEND_SLOT_H;
  const int w = legendW(row);
  tft.setTextDatum(lgfx::textdatum_t::middle_center);
  useGrid();
  for (uint8_t k = 0; k < count; k++) {
    const int bx = legendX(row, k);
    if (!legendText[first + k][0]) {          // empty: the column stays standing
      tft.drawRoundRect(bx, y, w, BTN_H, BTN_R, COL_EDGE);
      continue;
    }
    const bool on = legendSel[first + k];
    tft.fillRoundRect(bx, y, w, BTN_H, BTN_R, on ? COL_ACCENT : COL_BG);
    tft.drawRoundRect(bx, y, w, BTN_H, BTN_R, on ? COL_ACCENT : COL_EDGE);
    tft.setTextColor(on ? COL_BG : COL_CAPTION, on ? COL_ACCENT : COL_BG);
    tft.drawString(legendText[first + k], bx + w / 2, y + BTN_H / 2);
  }
  tft.setTextColor(COL_CAPTION, COL_BG);  // back to the default for later draws
}

// The strip is page-coloured so the chips stand out of it; there is no hairline
// above it (the help card's outline closes the band).
static void drawLegendBand() {
  tft.fillRect(0, LEGEND_Y, PANEL_W, PANEL_H - LEGEND_Y, COL_BG);
  drawLegendRow(0, 0, 2);
  tft.drawRoundRect(legendX(0, 2), LEGEND_Y, legendW(0), BTN_H, BTN_R, COL_EDGE);
  drawLegendRow(1, 2, 3);
  drawLegendRow(2, 5, 4);
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
  statusDirty = true;
  laneDirty = true;
  helpDirty = true;
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
  statusText[0] = '\0';
  statusDirty = true;
  laneUsed = 0;
  lanePlay = 0xFF;
  lanePlayDrawn = 0xFF;
  laneDirty = true;   // the lane repaints with the channel statics as they are;
                      // scrStepLane() re-reads them on its next pass, before
                      // scrUpdate() gets to draw
  helpDirty = true;
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
  const bool labelMoved = (strcmp(l, headerLeft) != 0);
  memcpy(headerLeft, l, sizeof(l));
  memcpy(headerRight, r, sizeof(r));
  headerTransient = transient;
  headerDirty = true;
  if (labelMoved) helpDirty = true;   // the help row describes the new page
}

// The family is named in the header now that the mode row is gone, so this only
// remembers which one is active: the help row falls back on it when the header
// carries a page name instead.
void scrModeRow(uint8_t active) {
  if (active >= 3) active = SCR_MODE_NONE;
  activeMode = active;
}

// SC01 only: the header's status slot, drawn when nothing is being announced
// and no persistent note owns the slot.  It has its own repaint box, because it
// changes on every step while the name beside it does not.
void scrStatus(const char* text) {
  char buf[sizeof(statusText)];
  copyText(buf, sizeof(buf), text);
  if (strcmp(buf, statusText) == 0) return;
  memcpy(statusText, buf, sizeof(buf));
  statusDirty = true;
}

// SC01 only: the step lane under the header.  `used` is a bit per slot for a
// step that carries notes, `play` the pad the playhead is on (0xFF for none).
// The three channels the lane marks are read here instead of passed in: it
// shows all of them at once, in every mode, and this runs once per loop - so an
// encoder or keyboard channel move repaints it too, not only a sequencer one.
// A playhead move on its own repaints two chips, not the sixteen.
void scrStepLane(uint16_t used, uint8_t play) {
  if (play >= LANE_SLOTS) play = 0xFF;
  const uint8_t seq = seqEditChannel();
  const uint8_t enc = encChannel();
  const uint8_t key = keyChannel();
  if (used != laneUsed || seq != laneSeqCh || enc != laneEncCh ||
      key != laneKeyCh) {
    laneUsed = used;
    laneSeqCh = seq;
    laneEncCh = enc;
    laneKeyCh = key;
    laneDirty = true;
  }
  lanePlay = play;
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
  // The lane goes first: its playhead is the one thing that moves without an
  // event of its own, and a full lane is sixteen chips.
  if (laneDirty) {
    drawStepLane();
    laneDirty = false;
    seqStepBreath();
  } else if (lanePlay != lanePlayDrawn) {
    drawLanePlay();
  }
  if (mainDirty) {
    // The card's ground goes down once.  The eight cell repaints cover its
    // middle, but not the rounded corners the frame leaves showing - and the
    // grid is meant to read as a black card, not as a lighter one behind black
    // cells.  Each cell still draws only its own rectangle.
    tft.fillRoundRect(GRID_X, GRID_Y, GRID_W, CELL_H * 2, GRID_R, COL_GRIDBG);
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
  if (helpDirty) {
    drawHelp();
    helpDirty = false;
    seqStepBreath();
  }
  if (statusDirty) {
    drawStatus();
    statusDirty = false;
  }
  if (headerDirty) {
    drawHeader();
    headerDirty = false;
    statusDirty = false;   // the header draw wrote the slot as well
  }
}

#endif // use_SC01Plus
