#pragma once

#include <stdint.h>
#include <math.h>     // the LFO row's mapping (powf / sinf / floorf)

/*****************************************************************************
 Drawing layer under Display.cpp.

 Display.cpp owns what a screen says: the label and value of each of the eight
 knob cells, the nine control-legend slots, the header label and the header
 note.  It never draws.  Everything below is the one place the active panel is
 chosen:

   ScreenOled.cpp   Teensy 4.1, SSD1306 128x64 - the layout the firmware was
                    written on (header, 4x2 label/value cells, 5+4 legend)
   ScreenPanel.cpp  SC01 Plus, ST7796 480x320 - the panel layout: a header
                    with the name and a 12 px status slot, the step lane, the
                    4x2 card of 117x65 cells, the help row and the legend in
                    3 + 3 + 4 columns, with CC dials

 The vocabulary is the OLED's - eight cells (four per row), nine legend slots
 - because that is what Display.cpp's layout logic produces on either panel.
 The 480x320 panel simply gives each cell 117x65 px to say it in, splits the
 nine slots over three rows instead of two, and adds the two things the strip
 has no room for: the step lane (scrStepLane) and the header's status slot
 (scrStatus).  Both are panel-only; the strip implements them as no-ops.
*****************************************************************************/

#define SCR_CELLS    8
#define SCR_LEGENDS  9
#define SCR_MODE_NONE 0xFF

void scrBegin();                 // panel init + clear (first draw)
void scrClear();                 // blank every cell and legend slot
// A full frame is about to be described.  The framebuffer backend starts from
// a blank buffer; the panel has no framebuffer, so it keeps what is already on
// screen as the reference and each scr*() call then repaints only the element
// that actually differs.  A band-wide blank there repaints everything on every
// knob turn - a visible flash, and a block in the loop long enough to be heard.
void scrFrameBegin();
// Header band: left label, right note.  `transient` = a fresh announcement
// (accent colour, takes the whole strip if long); otherwise the note is the
// persistent one and the left label shares the strip with it.
void scrHeader(const char* left, const char* right, bool transient);
// Active family: 0 ENC, 1 SEQ, 2 KEY.  Neither panel draws a mode row any more
// - the family is named in the header - but the panel keeps it, because the
// help row falls back on it when the header carries a page name instead.
void scrModeRow(uint8_t active);
// SC01 only: the header's 12 px status slot, used when no temporary message is
// up and the persistent note is empty - `TRACK 02 - STEP 08/16`.  No-op on the
// 128x64 strip, whose header has no room beside the label.
void scrStatus(const char* text);
// SC01 only: the step lane under the header, one numbered square per pad of the
// edit window.  `chan` is the channel the current mode is working on (1..16; 0
// lights none) and fills that pad's square, `used` a bit per slot with notes,
// `play` the pad the playhead is on (0xFF for none).  No-op on the 128x64 strip:
// there the pads themselves are the picture.
void scrStepLane(uint8_t chan, uint16_t used, uint8_t play);

void scrCellLabel(uint8_t i, const char* text);       // caption over the value
void scrCellLabelSmall(uint8_t i, const char* text);  // ENCABLETONMODE names
void scrCellValue(uint8_t i, const char* text);
// Dial cell: 270-degree ring, pointer across the rim, value inside and the
// caption (the CC number) in the ring's bottom notch.
void scrDial(uint8_t i, const char* caption, uint8_t value);
// Level-bar cell: the value as a horizontal bar, caption above it and the
// number under the bar - a volume / expression CC reads better as a level than
// as a ring (ENCMODE4 FLAVOR = BARS).
void scrCellBar(uint8_t i, const char* caption, uint8_t value);
// The envelope row (ENCMODE4 FLAVOR = ENVELOPE), split in two halves with a
// blank gap between them:
//   LEFT  the filter - a low-pass curve whose corner follows the cutoff value
//         and whose peak follows the resonance one (ENC1 / ENC2's
//         CCs).  It carries NO readout of its own: those two numbers are the
//         ENC1 / ENC2 dials directly above it
//   RIGHT the ADSR curve of values[0..3] = attack, decay, sustain, release
//         (ENC5..ENC8) at half the row's width, each value keeping a tight bar
//         and its CC number under it
void scrEnvRow(const char* const envCaps[4], const uint8_t envVals[4],
               uint8_t cutoff, uint8_t resonance);
// The LFO row (ENCMODE4 FLAVOR = LFO), laid out like the envelope row: the
// FILTER under ENC1 / ENC2 on the left (the same curve), a blank band, then on
// the right ONE still waveform drawn from the four values - ENC5 depth, ENC6
// rate, ENC7 shape, ENC8 sync - with their readouts under it (DEPTH 75% /
// RATE 0.9HZ / SHAPE RANDOM / SYNC BEAT).  A still picture, not a running one:
// the parameters change its shape instead of moving it.  Depth sets the height
// around the centre line (flat at 0), rate how many cycles fit across the half
// (a high rate squeezes them), shape picks the waveform and sync how the cycles
// line up (free-running / whole beats with boundary ticks / one shot, whose
// width is that one cycle's, so the rate still moves it).
// Every readout is ONE line - "DEPTH 75%", caption then value - and may run
// left past the waveform onto the empty band under ENC2 to stay that way.
// cutoff / resonance are ENC1 / ENC2's CC values, the filter half's numbers.
void scrLfoRow(const char* const lfoCaps[4], const char* const lfoVals[4],
               uint8_t depth, uint8_t rate, uint8_t shape, uint8_t sync,
               uint8_t cutoff, uint8_t resonance);

// ---------------------------------------------------------------------------
// LFO parameter mapping.  Raw 0..127 knob values -> what the row draws and what
// Display.cpp prints.  It lives here rather than in one screen backend because
// the panel, the 128x64 strip and the readout text have to agree.
// ---------------------------------------------------------------------------
#define LFO_SHAPES 6        // SINE / TRI / RISE / FALL / SQUARE / RANDOM
#define LFO_SYNCS  3        // HZ / BEAT / TRIG

static inline uint8_t lfoClamp7(uint8_t v) { return v > 127 ? 127 : v; }
// 0 SINE / 1 TRI / 2 RISE / 3 FALL / 4 SQUARE / 5 RANDOM
static inline uint8_t lfoShape(uint8_t v) {
  return (uint8_t)(((uint16_t)lfoClamp7(v) * LFO_SHAPES) >> 7);
}
// 0 HZ (free running) / 1 BEAT (whole cycles) / 2 TRIG (one shot)
static inline uint8_t lfoSync(uint8_t v) {
  return (uint8_t)(((uint16_t)lfoClamp7(v) * LFO_SYNCS) >> 7);
}
// 0.01 .. 10 Hz, exponential: the slow end would otherwise be one detent wide
static inline float lfoRateHz(uint8_t v) {
  return 0.01f * powf(1000.0f, (float)lfoClamp7(v) / 127.0f);
}
// Cycles drawn across the row: 0.5 .. 8, log - a high rate squeezes the wave
static inline float lfoCycles(uint8_t v) {
  return 0.5f * powf(16.0f, (float)lfoClamp7(v) / 127.0f);
}
// The waveform at u (0..1 across the row) for `cycles` cycles in that row,
// -1..1.  RANDOM is a sample-and-hold of one value per cycle - but never fewer
// than four steps, because a slow random LFO would otherwise hold a single
// value across the whole row and read as a flat line, i.e. as depth 0.
static inline float lfoWave(float u, float cycles, uint8_t shape) {
  if (cycles <= 0.0f) cycles = 1.0f;
  const float pos = u * cycles;
  const uint32_t step = (uint32_t)floorf(pos);
  const float p = pos - (float)step;
  switch (shape) {
    case 1:   // triangle, starting at 0 and rising
      if (p < 0.25f) return 4.0f * p;
      if (p < 0.75f) return 2.0f - 4.0f * p;
      return 4.0f * p - 4.0f;
    case 2: return 2.0f * p - 1.0f;                     // rising saw
    case 3: return 1.0f - 2.0f * p;                     // falling saw
    case 4: return (p < 0.5f) ? 1.0f : -1.0f;           // square
    case 5: {                                           // random, stepped
      uint32_t steps = (uint32_t)ceilf(cycles);
      if (steps < 4) steps = 4;
      uint32_t h = (uint32_t)floorf(u * (float)steps) * 2654435761u + 0x9E3779B9u;
      h ^= h >> 15;
      h *= 2246822519u;
      h ^= h >> 13;
      return ((float)(h & 0xFFFFu) / 32767.5f) - 1.0f;
    }
    default: return sinf(6.2831853f * p);               // sine
  }
}
// TRIG (0..1): how much of the row ONE cycle of this rate takes.  A one shot
// fires once and is then over, so the rest of the row is the flat line it
// leaves behind - and because that width follows the rate, ENC6 still changes
// the picture with TRIG selected (a fast rate packs the burst into the start of
// the row, a slow one spreads it across the half) instead of every rate
// drawing the same full-width cycle.
static inline float lfoTrigSpan(uint8_t v) {
  const float c = lfoCycles(v);
  return 1.0f / (c < 1.0f ? 1.0f : c);
}
// The row's value at u (0..1) with the sync mode applied: HZ free-runs, BEAT
// snaps to whole cycles, TRIG is a single cycle inside lfoTrigSpan().
static inline float lfoSyncedWave(float u, uint8_t rate, uint8_t shape,
                                  uint8_t sync) {
  const uint8_t sy = lfoSync(sync);
  const uint8_t sh = lfoShape(shape);
  if (sy == 1) {                        // BEAT: whole cycles across the row
    float c = roundf(lfoCycles(rate));
    if (c < 1.0f) c = 1.0f;
    return lfoWave(u, c, sh);
  }
  if (sy == 2) {                        // TRIG: one cycle, then nothing
    const float span = lfoTrigSpan(rate);
    if (u > span) return 0.0f;
    return lfoWave(u / span, 1.0f, sh);
  }
  return lfoWave(u, lfoCycles(rate), sh);
}

void scrLegend(uint8_t i, const char* text);  // "" blanks the slot
// Which legend slot is ARMED right now: the F1..F4 radio of the open page, or
// the M button whose menu is open.  The panel fills that label the way the mode
// row fills the active mode, so the strip says what the pads already show - a
// chosen F pad burns white there (Modes.cpp), and the legend names which one.
// No-op on the 128x64 panel: its 4x6 strip has no room to invert a label.
void scrLegendSel(uint8_t i, bool on);

void scrUpdate();   // push whatever changed (one OLED page per call)

// Panel backlight, 0..100 %.  No-op on the 128x64 OLED header, which has no
// dimmer (and no touchscreen, so the settings page it belongs to is panel-only)
void scrSetBrightness(uint8_t percent);
// A fresh tap inside the header band (edge-detected, throttled).  False on the
// Teensy: there is nothing to tap.
bool scrTapHeader();
