#pragma once

#include <Arduino.h>

// --- GNR8: generative pattern page (SEQMODE variants 1/2/4) ----------------
// Active only while the edit track is MONO with TYPE = BASS ("GNR8" -
// the acid generator; ROOT / ARP / STRUM and every other track type keep
// their existing controls).  M2 clicks the encoder page on/off:
//   ENC2 = FILL%  ENC3 = ACC%  ENC4 = GLIDE%  ENC7 = TIE%
//   ENC1/5/6/8 keep OCT / GATE / VEL / PROB
// The F row = RADIO switches for the four option sub-pages while the page
// is open (F1 OPTS / F2 STEP OCT+MAP / F3 BORROW / F4 MOD PROB): click
// arms, click the armed one again releases, click another switches.
// While one is armed the whole encoder row becomes that sub-page - F1
// puts TONES/RNG/LOW/ALGO on ENC1-4, F2/F3/F4 dial the tapped step's
// octave / borrowed chord tone / modifier probability (4 counts = 1
// detent = 1 value, clamped, no wrap); every other encoder reads ---.
// M3 tap = generate the pattern from those parameters (Aciduino V2's
// acidRandomize: random or Bjorklund/Euclidean placement, notes limited
// to the current KEY + SCALE), M3 hold = undo (shared one-level step
// snapshot), PREV held = ENC1 rotates the pattern (4 counts = 1 step).
// TIE% = chance a note holds over the FOLLOWING REST (Aciduino's tie on
// a rest step), not a flag on the note itself.  Params are per sequence
// (16 slots) and persist in the SEQ EEPROM block.
//
// LOW picks the bottom of the generated range, naming follows the
// device convention (MIDI 24 = C0, 36 = C1, 48 = C2 - drumVoiceName
// uses the same base); RANGE = octaves above it (1..5).

// storage capacity mirrored from the engine (steps stored per track)
#define SEQ_GEN_STEPS 64

enum SeqGenTones : uint8_t {   // how many scale degrees the notes may use
  SEQ_GEN_TONES_ALL = 0, SEQ_GEN_TONES_5, SEQ_GEN_TONES_4, SEQ_GEN_TONES_3
};
enum SeqGenLow : uint8_t {     // bottom of the note range
  SEQ_GEN_LOW_C0 = 0, SEQ_GEN_LOW_C1, SEQ_GEN_LOW_C2      // 24 / 36 / 48
};
enum SeqGenAlgo : uint8_t {    // step placement
  SEQ_GEN_CLASSIC = 0, SEQ_GEN_EUCLID
};
// LEAD contour (SEQMODE M2 LEAD page, edit track must be MONO + TYPE=LEAD):
// the melodic shape the generator walks - ARCH rises to a peak mid-pattern
// then falls back, DESC steps down across the pattern (wraps), WALK is a
// random ±1/±2 scale-step walk (reflects at the window edges).  The line
// snaps to the KEY + SCALE inside the LOW..RANGE window (shared with GNR8).
enum SeqLeadContour : uint8_t {
  SEQ_LEAD_ARCH = 0, SEQ_LEAD_DESC, SEQ_LEAD_WALK
};

// encoder knobs (4 counts = 1 unit, clamped; residuals stay per parameter)
void    seqGenFillAdjust(int16_t counts);    // FILL%   0..100
void    seqGenAccAdjust(int16_t counts);     // ACC%    0..100
void    seqGenGlideAdjust(int16_t counts);   // GLIDE%  0..100
void    seqGenTieAdjust(int16_t counts);     // TIE%    0..100
void    seqGenFillSet(uint8_t idx, uint8_t v);    // Set = clamped (EEPROM)
void    seqGenAccSet(uint8_t idx, uint8_t v);
void    seqGenGlideSet(uint8_t idx, uint8_t v);
void    seqGenTieSet(uint8_t idx, uint8_t v);
uint8_t seqGenFillOf(uint8_t idx);
uint8_t seqGenAccOf(uint8_t idx);
uint8_t seqGenGlideOf(uint8_t idx);
uint8_t seqGenTieOf(uint8_t idx);

// F-row options + EEPROM persistence (Set clamps, bad input -> default)
void    seqGenTonesSet(uint8_t idx, uint8_t v);
void    seqGenLowSet(uint8_t idx, uint8_t v);
void    seqGenRangeSet(uint8_t idx, uint8_t v);
void    seqGenAlgoSet(uint8_t idx, uint8_t v);
uint8_t seqGenTonesOf(uint8_t idx);
uint8_t seqGenLowOf(uint8_t idx);
uint8_t seqGenRangeOf(uint8_t idx);
uint8_t seqGenAlgoOf(uint8_t idx);

// option dials of the F radio sub-pages (ENC1): 4 counts = 1 detent =
// 1 value, clamped at both ends (no wrap), residuals per parameter
void    seqGenTonesAdjust(int16_t counts);   // index 0..3 (all/5/4/3)
void    seqGenLowAdjust(int16_t counts);     // index 0..2 (C0/C1/C2)
void    seqGenRangeAdjust(int16_t counts);   // 1..5 octaves
void    seqGenAlgoAdjust(int16_t counts);    // 0..1 (CLASSIC/EUCLID)

const char* seqGenTonesName(uint8_t v);      // "all" "5" "4" "3"
const char* seqGenLowName(uint8_t v);        // "C0" "C1" "C2"
const char* seqGenAlgoName(uint8_t v);       // "CLASSIC" "EUCLID"

// generate: roll every step of the edit sequence's own pattern length
// (takes the undo snapshot first; empty steps are cleared, filled ones
// get one note + accent/glide rolled per step).  Returns the number of
// notes written, -1 = refused (bad channel / empty pattern)
int  seqGenProduce();

// one lazy randomSeed() shared by every roll (GNR8/LEAD produce, the HARM
// bake's FILL gate): the first roll after boot picks the seed
void genSeedOnce();

// LEAD contour knob + the melody generator (M2 LEAD page, M3 tap = gen /
// hold = undo, mirroring GNR8).  seqLeadProduce() walks the contour over
// the in-scale ladder of the LOW..RANGE window: ARCH rises to a mid-pattern
// peak then falls, DESC steps down (wraps), WALK random-walks ±1/±2 scale
// steps.  Rhythm = the shared FILL% density gate (classic or euclid via
// ALGO).  Notes are STORED; at playback LEAD + FOLLOW + MAP (ct/gd) re-
// resolves them onto the feeder's chord.  Returns notes written, -1 refused
void    seqLeadContourAdjust(int16_t counts);   // ARCH/DESC/WALK, 4 = 1 step
void    seqLeadContourSet(uint8_t idx, uint8_t v);
uint8_t seqLeadContourOf(uint8_t idx);
const char* seqLeadContourName(uint8_t v);      // "ARCH" "DESC" "WALK"
// LEAD page CT knob: chord-tone vs scale blend (per-note chance to keep
// the skeleton's in-scale note instead of snapping to the feeder chord);
// bites only when the global MAP mode is ct
void    seqLeadCtAdjust(int16_t counts);        // 0..100%, 4 counts = 1
void    seqLeadCtSet(uint8_t idx, uint8_t v);
uint8_t seqLeadCtOf(uint8_t idx);
// LEAD page GD knob: guide-tone set depth (which extensions gd may use);
// bites only when the global MAP mode is gd
void    seqLeadGdAdjust(int16_t counts);        // 0..3, 4 counts = 1 step
void    seqLeadGdSet(uint8_t idx, uint8_t v);
uint8_t seqLeadGdOf(uint8_t idx);
const char* seqLeadGdName(uint8_t v);           // "3/7" "+9" "+13" "EXT"
// live CONTOUR flip (M2 LEAD page ENC2): reshapes the stored sequence in
// place without a GEN press - the pattern under the old contour is kept
// in a per-track cache and the target contour's last version is restored
// (or the existing notes deterministically reshaped on first visit), so
// each contour stays revisitable.  Returns true when the steps changed
bool    seqLeadContourTurn(int16_t counts);
int  seqLeadProduce();

// rotate: move the stored steps of the edit pattern one slot per 4 counts
// (CW = content delays one step, wrap at the pattern length; snapshots at
// the start of a rotation burst so M3-hold undoes the whole gesture).
// seqGenShiftPos() = cumulative rotation inside the pattern (display 1..N)
void    seqGenShiftAdjust(int16_t counts);
uint8_t seqGenShiftPos();
