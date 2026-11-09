#pragma once

#include <Arduino.h>

// --- DRUM pattern page (SEQMODE M2 on a DRUM track) -------------------------
// Pre-authored one-bar drum patterns, INDEPENDENT of the KEYMODE4 progression
// genre (the drum style is its own list).  Every genre ships four LANES -
// KICK / SNARE / HAT / PERC - and each lane carries two authored 16-step
// lines.  A line is a string of step characters:
//   '-'      rest
//   'a'..'g' core hit, that lane's voice map entry 0..6
//   'A'..'G' optional hit, rolled by that lane's FILL %
// A lane knob picks line A, line B or OFF, so the four lanes recombine into
// one pattern.  FILL % is the ghost-layer density, MOT % the per-hit accent
// chance (it rides the existing per-voice drum accent mask, so the accent
// velocity shapes the dynamics), VAR adds a rolled bar-end fill (none /
// last beat / last half bar).
//
// The page lives behind M2 on a DRUM track (indigo pad) and every knob turn
// rebuilds the pattern LIVE; M3 = re-roll, M3 hold = undo (the shared
// one-level step snapshot).  Everything is per drum track (16 slots) and
// persists in the SEQ EEPROM block.

#define DRUM_GENRES 18
#define DRUM_VOICES 7      // letters a..g per lane (see Drum.cpp)

enum DrumLane : uint8_t {
  DRUM_KICK = 0, DRUM_SNARE, DRUM_HAT, DRUM_PERC, DRUM_LANES
};
enum DrumVar : uint8_t {
  DRUM_VAR_OFF = 0,   // straight pattern
  DRUM_VAR_4,         // rolled fill over the last beat
  DRUM_VAR_8          // rolled fill over the last half bar
};
enum DrumLine : uint8_t {
  DRUM_LINE_A = 0, DRUM_LINE_B, DRUM_LINE_OFF
};
// Kit MAPPING of the DRUM track (SEQMODE ENC5, variants 1/2/4).  CUSTOM is
// the kit this controller drives (C1 bass drum, C#1 hand clap, D1/D#1 hats,
// E1 snare, F1/F#1/G1 congas, G#1/A1/A#1 toms, C2 cow bell, C#2 claves,
// D2 maracas, D#2 cymbal) and the default; GM is the General MIDI percussion
// map (36 kick, 37 rim, 38 snare, 39 clap, 40 e.snare, 41/43 floor toms,
// 42/44/46 hats, 45/47/48/50 toms, 49 crash, 51 ride).  Per track, RAM +
// the SEQ EEPROM block
enum DrumMap : uint8_t { DRUM_MAP_CUSTOM = 0, DRUM_MAP_GM };
// two-state, NO wrap (like KLOCK/ENCOD): CW = GM, CCW = CUSTOM, saturating;
// returns true when the flag actually changed
bool        drumMapSetDir(int16_t dir);
uint8_t     drumMap();                  // edit track
const char* drumMapName(uint8_t v);     // "CUSTOM" / "GM"
void        drumMapSet(uint8_t ch, uint8_t v);   // EEPROM load (bad -> CUSTOM)
uint8_t     drumMapOf(uint8_t ch);

// M2 page knobs (4 counts = 1 unit, clamped, no wrap; residuals kept)
void    drumGenreAdjust(int16_t counts);      // genre list
void    drumVarAdjust(int16_t counts);        // bar-end fill depth
void    drumFillAdjust(int16_t counts);       // master FILL % (fans out)
void    drumLaneFillAdjust(uint8_t lane, int16_t counts);
void    drumMotAdjust(int16_t counts);        // master MOT % (fans out)
void    drumLaneMotAdjust(uint8_t lane, int16_t counts);
void    drumLineAdjust(uint8_t lane, int16_t counts);   // A / B / OFF

uint8_t drumGenre();                          // edit track
uint8_t drumVar();
uint8_t drumFill();                           // master value (page)
uint8_t drumLaneFill(uint8_t lane);
uint8_t drumMot();
uint8_t drumLaneMot(uint8_t lane);
uint8_t drumLine(uint8_t lane);

const char* drumGenreName(uint8_t v);         // "HOUSE" ... "REGGAE"
const char* drumVarName(uint8_t v);           // "off" "fill4" "fill8"
const char* drumLineName(uint8_t v);          // "A" "B" "OFF"

// persistence (the SEQ EEPROM block; blank 0xFF falls back to the default)
void    drumGenreSet(uint8_t ch, uint8_t v);
void    drumVarSet(uint8_t ch, uint8_t v);
void    drumFillSet(uint8_t ch, uint8_t v);
void    drumLaneFillSet(uint8_t ch, uint8_t lane, uint8_t v);
void    drumMotSet(uint8_t ch, uint8_t v);
void    drumLaneMotSet(uint8_t ch, uint8_t lane, uint8_t v);
void    drumLineSet(uint8_t ch, uint8_t lane, uint8_t v);
uint8_t drumGenreOf(uint8_t ch);
uint8_t drumVarOf(uint8_t ch);
uint8_t drumFillOf(uint8_t ch);
uint8_t drumLaneFillOf(uint8_t ch, uint8_t lane);
uint8_t drumMotOf(uint8_t ch);
uint8_t drumLaneMotOf(uint8_t ch, uint8_t lane);
uint8_t drumLineOf(uint8_t ch, uint8_t lane);

// Bake the pattern into the edit track: one 16-step cell repeated over the
// track's pattern length (steps past it are left alone, like the other
// generators).  The undo snapshot is taken once per knob burst, so a
// continuous turn is a single undo.  Returns steps written, -1 = refused
// (the edit track is not DRUM / the pattern is empty)
int  drumProduce();
