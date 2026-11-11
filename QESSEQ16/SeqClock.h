#pragma once

#include <Arduino.h>

// USB-MIDI slave clock (uClock EXTERNAL_CLOCK, 24 PPQN input).
// Advances a playhead 0..15 on every 16th note (setOnStep).

void initSeqClock();        // uClock config + usbMIDI handlers; call once in setup
void seqPumpMidi();         // drain USB MIDI (clock/start/stop); bounded, call often
void pollSeqClock();        // pump + redraw playhead; call every loop
void seqStepBreath();       // dispatch a step the ISR queued, right now (also
                            // called between painted regions: a long panel
                            // repaint must never hold the beat back)
uint8_t seqCurrentStep();   // playhead 0..15 (safe to read from loop)
bool seqIsRunning();
void seqSendStart();        // emit USB-MIDI System Real-Time Start (0xFA)
                            // (+ starts internal transport when SYNC=INT)
void seqTogglePlay();       // PLAY: start, or stop when INT transport runs

// SEQMODE3 sync settings: INT = Teensy is clock master (streams Clock+Start
// out at tempoBpm); EXT = follow incoming USB-MIDI clock (original behavior)
void seqSetSyncInternal(bool on);
bool seqIsSyncInternal();
void seqSetTempo(uint16_t bpm);     // clamped 40..240
uint16_t seqTempo();

// SEQMODE3 TRACK (ENC6): track type of the edit sequence — DRUM / MONO /
// POLY, clamped, never wraps.  Per channel: channel 1 defaults to DRUM,
// channels 2..16 to POLY.  4 encoder counts = 1 step (residual kept).
enum SeqTrackType : uint8_t { SEQ_TRACK_DRUM = 0, SEQ_TRACK_MONO = 1, SEQ_TRACK_POLY = 2 };
void seqTrackAdjust(int16_t counts);
uint8_t seqTrack();                 // track type of the edit sequence
const char* seqTrackName();         // "DRUM" / "MONO" / "POLY"
uint8_t seqTrackOf(uint8_t idx);    // track type of sequence idx 0..15
void seqSetTrackType(uint8_t idx, uint8_t t);  // raw set (EEPROM load), clamped

// SEQMODE3 TYPE (ENC7): mono transform of a MONO sequence, applied at
// playback (steps keep storing the full incoming chord).
// ROOT = chord-root detection (template based, fallback lowest note),
// BASS = lowest note, ARP = chord notes ascending, 1 note per playhead
// step, wraps; counter resets on transport stop.  STRUM = the whole chord
// raked inside each step (ascending, each note cutting the last, last note
// held to the gate).  LEAD = the generated MELODY line: its own steps gate
// the rhythm, the notes are re-resolved onto the feeding track's chord via
// FOLLOW + MAP (ct = nearest chord tone, gd = nearest guide tone = 3rd/7th)
// - see the LEAD generator in Gen.cpp.  Default ROOT everywhere.
// NOTE: POLY shares this byte (0..2 is always a valid POLY value; a stale
// 3/4 from a MONO edit reads as STEP).
enum SeqMonoType : uint8_t { SEQ_MONO_ROOT = 0, SEQ_MONO_BASS = 1,
                             SEQ_MONO_ARP = 2, SEQ_MONO_STRUM = 3,
                             SEQ_MONO_LEAD = 4 };
void seqMonoTypeAdjust(int16_t counts);  // edit channel; 4 counts = 1 step, no wrap
uint8_t seqMonoType();                   // edit channel
const char* seqMonoTypeName();           // "ROOT" / "BASS" / "ARP" / "STRUM" / "LEAD"
uint8_t seqMonoTypeOf(uint8_t idx);      // sequence idx 0..15
void seqSetMonoType(uint8_t idx, uint8_t m);   // raw set (EEPROM load), clamped

// SEQMODE3 TYPE (ENC7) on a POLY track: the track default for every step
// that carries no per-step override (F2 TYPE).
// STEP = chords play as blocks (the workflow as before), STRUM = each step
// is raked ascending with the fixed tempo-based spread (8-40 ms), HAND =
// the keyboard performs: chord building is silent (incoming chords still
// update it), the 7 white keys strum the chord, REC captures the strum
// gesture (note order + real timing) for playback.  HARM = the track is a
// harmonizer: it plays its stored chords like STEP, but the M2 HARM page
// (M3) bakes a chord progression inferred from the FOLLOW feeder's bassline
// into the steps (inverse FOLLOW - bass -> HARM -> chords).  Stored in the
// same byte as the MONO transform; stale MONO values > 3 read as STEP (the
// track kind stored alongside disambiguates on load).  SKANK = the track
// derives each step's voicing live from the FOLLOW feeder (or its own
// stored chord) - the M2 SKANK page shapes the walk, see Skank.h.  The
// byte is shared with the MONO types (LEAD = 4), so 4 stays unassigned:
// a stale MONO/LEAD value keeps reading as STEP and the dial skips it.
enum SeqPolyType : uint8_t { SEQ_POLY_STEP = 0, SEQ_POLY_STRUM = 1,
                             SEQ_POLY_HAND = 2, SEQ_POLY_HARM = 3,
                             SEQ_POLY_SKANK = 5 };
void seqPolyTypeAdjust(int16_t counts);  // edit channel; 4 counts = 1 step, no wrap
uint8_t seqPolyType();                   // edit channel, clamped (4 = stale reads STEP)
const char* seqPolyTypeName();           // "STEP" / "STRUM" / "HAND" / "HARM" / "SKANK"
uint8_t seqPolyTypeOf(uint8_t idx);      // sequence idx 0..15, clamped (stale 4 -> STEP)

// ARP options (SEQMODE M2 page, edit track must be MONO + TYPE=ARP):
// span = octaves the arp is stretched over (Arpie: up to 3 more octaves on
// top of the chord), mode = the order they play in, inst = Arpie's note
// insert patterns, skip = an empty/failed step still advances the arp
// position instead of pausing it.  Everything is per track.
enum SeqArpMode : uint8_t {
  SEQ_ARP_UP = 0, SEQ_ARP_DOWN, SEQ_ARP_UPDOWN,
  SEQ_ARP_RANDOM, SEQ_ARP_RANDOM2,
  SEQ_ARP_OUTIN, SEQ_ARP_INOUT, SEQ_ARP_CONV, SEQ_ARP_DIV
};
enum SeqArpInst : uint8_t {
  SEQ_INST_OFF = 0, SEQ_INST_HI, SEQ_INST_LOW,
  SEQ_INST_3F1B, SEQ_INST_4F2B
};
void        seqArpSpanAdjust(int16_t counts);  // edit channel; 4 counts = 1
uint8_t     seqArpSpanOf(uint8_t idx);         // 1..4 octaves
void        seqArpSpanSet(uint8_t idx, uint8_t v);
void        seqArpModeAdjust(int16_t counts);  // edit channel; 4 counts = 1
uint8_t     seqArpModeOf(uint8_t idx);
void        seqArpModeSet(uint8_t idx, uint8_t v);
const char *seqArpModeName(uint8_t v);         // "UP" ... "DIV" (OLED)
void        seqArpInstAdjust(int16_t counts);  // edit channel; 4 counts = 1
uint8_t     seqArpInstOf(uint8_t idx);
void        seqArpInstSet(uint8_t idx, uint8_t v);
const char *seqArpInstName(uint8_t v);         // "OFF" ... "4F2B" (OLED)
void        seqArpSkipToggle(int16_t counts);  // CW = ON, CCW = OFF
bool        seqArpSkipOf(uint8_t idx);
void        seqArpSkipSet(uint8_t idx, bool on);

// --- ARP F sub-pages -------------------------------------------------------
// F1..F4 on the ARP page each own the whole encoder row.  Every knob they
// serve reads the same per-track block through these ids, so the dispatch,
// the label row and the value column can never drift apart.  ORDER 0 means
// "follow MODE" and INSERT 0 "follow INST", so the plain page keeps
// working exactly as before while the sub-pages add the new shapes.
enum SeqArpParam : uint8_t {
  ARP_P_ORDER = 0,   // 0 = MODE, else the extended walk order
  ARP_P_STEP,        // skip pattern 1/1 .. 4/2 over that walk
  ARP_P_PIVOT,       // converge/diverge/weight target
  ARP_P_WEIGHT,      // 0..100% pull toward PIVOT
  ARP_P_MUTATE,      // 0..100% chance a step becomes another chord tone
  ARP_P_INSERT,      // 0 = INST, else OFF/HI/LOW/3F1B/4F2B/RAND
  ARP_P_CYCLE,       // 1/2/4/8/16 bars between RANDOM2 reshuffles
  ARP_P_MORPH,       // 0..100% of steps played in the plain walk
  ARP_P_ENGINE,      // OFF / MARKOV / WALK
  ARP_P_RATE,        // how often the engine re-rolls (0..100)
  ARP_P_DEPTH,       // how far the engine may wander (0..100)
  ARP_P_MEM,         // repeat / history bias (0..100)
  ARP_P_SEED,        // 0..63, re-roll by turning
  ARP_P_BIAS,        // NONE/HIGH/LOW/ROOT/PIVOT/SPARSE/DENSE
  ARP_P_TARGET,      // ORDER / PITCH / BOTH
  ARP_P_CLOCK,       // STEP / PASS / BAR / PHRASE
  ARP_P_OPT1, ARP_P_OPT2, ARP_P_OPT3, ARP_P_OPT4,   // engine-specific
  ARP_P_COUNT
};
enum SeqArpEngine : uint8_t {
  SEQ_ARP_ENG_OFF = 0, SEQ_ARP_ENG_MARKOV, SEQ_ARP_ENG_WALK
};
enum SeqArpTarget : uint8_t {
  SEQ_ARP_TGT_ORDER = 0, SEQ_ARP_TGT_PITCH, SEQ_ARP_TGT_BOTH
};
enum SeqArpClock : uint8_t {
  SEQ_ARP_CLK_STEP = 0, SEQ_ARP_CLK_PASS, SEQ_ARP_CLK_BAR, SEQ_ARP_CLK_PHRASE
};
enum SeqArpBias : uint8_t {
  SEQ_ARP_BIAS_NONE = 0, SEQ_ARP_BIAS_HIGH, SEQ_ARP_BIAS_LOW,
  SEQ_ARP_BIAS_ROOT, SEQ_ARP_BIAS_PIVOT, SEQ_ARP_BIAS_SPARSE,
  SEQ_ARP_BIAS_DENSE
};

void        seqArpPAdjust(uint8_t p, int16_t counts);  // edit channel, 4 = 1
uint8_t     seqArpPOf(uint8_t idx, uint8_t p);
void        seqArpPSet(uint8_t idx, uint8_t p, uint8_t v);
uint8_t     seqArpPMax(uint8_t p);
const char *seqArpPName(uint8_t p);            // "ORDER", "PIVOT", ... (OLED)
const char *seqArpPValue(uint8_t p, uint8_t v); // short value text (OLED)

// SEQMODE3 pattern setup: ENC2 = pattern length of the edit sequence
// (0..64 steps, boot default 16, independent per track); the C D E F
// keyboard bars pick the 16-step edit window (shown/edited by the step
// pads in the other variants) and pressing the SELECTED bar again loops
// playback on that bar only.
void seqPatLenAdjust(int16_t counts);    // edit channel; 4 counts = 1 step
int  seqPatLen(void);                    // length of the edit sequence
uint8_t seqPatLenOf(uint8_t ch);         // length of sequence ch 0..15
void    seqPatLenSetOf(uint8_t ch, uint8_t len);  // restore (seqCfgLoad)
void    seqPatLenApplyOf(uint8_t ch, uint8_t v);  // clamped + window

// SEQMODE3 C# / D# (the black keys of the bar octave): halve / double the
// edit track's pattern length in one press - the same 0..64 range ENC2
// dials, and the 16-step edit window follows the length down so the pads
// never sit on a bar that no longer exists.  A silent track (length 0)
// doubles to 16 - there is nothing to halve - and a halve stops at 1 step,
// so a press can never silence a track by accident.  Both return the
// track's new length
uint8_t seqPatLenHalf(void);
uint8_t seqPatLenDouble(void);
// SEQMODE3 ENC4: per-track pattern SPEED - that sequence's step rate
// relative to the master clock.  Ladder of 9, index 6 = normal (one step
// per 16th note, the boot default): 1/64, 1/32, 1/16, 1/8, 1/4, 1/2, 1, 2, 4
void        seqSpeedAdjust(int16_t counts);  // edit channel; 4 counts = 1 step
uint8_t     seqSpeedIdxOf(uint8_t ch);       // index 0..8 of sequence ch
void        seqSpeedSetOf(uint8_t ch, uint8_t idx);   // restore (seqCfgLoad)
const char *seqSpeedName(uint8_t idx);       // "1/64" ... "4" for the OLED
uint8_t     seqSpeedNumOf(uint8_t ch);       // sub-steps per master tick
uint8_t     seqSpeedDenOf(uint8_t ch);       // master ticks per sub-step

// --- per-track swing (SEQMODE 4: ENC1 = style, ENC2 = amount) --------------
// The groove of the WHOLE track shifts: the late half of every swing pair
// (counted on the track's own step grid, so it follows its pattern length,
// bar loop and speed) plays LATE by a style-shaped fraction of the pair -
// the classic drum-machine feel, independent per track.  Each style is an
// emulation with two properties: the GRID it swings (pair = how many of the
// track's own steps one swing pair spans - 2 = 16th swing, every odd step
// is late; 4 = 8th shuffle, only the "&" is late) and the deepest ratio
// rMax (percent of the pair) the AMT knob can reach.  AMT 0 = straight,
// AMT 100 = the machine's full shove; the effective ratio is
// 50% + amt/100 x (rMax - 50), e.g. MPC60 at amt 50 = 62%, TRIPLET at 100
// = 67% (hard triplets).  Boot: style OFF, amt 50; persisted per track in
// the SEQ EEPROM block.  The scheduler (seqSwingPoll in Keyboard.cpp)
// defers the off-step's whole pass - release and note-on slide together
// and the playhead LED follows the sound
enum SeqSwingStyle : uint8_t {
  SEQ_SWING_OFF = 0,     // straight (no deferral)
  SEQ_SWING_MPC60,       // 16th swing, the reference Linn groove, deep (75)
  SEQ_SWING_MPC3K,       // 16th swing, smoother MPC3000 feel (70)
  SEQ_SWING_SP1200,      // 16th swing, E-mu SP-1200 shove (72)
  SEQ_SWING_LINN,        // 16th swing, tight LinnDrum (62)
  SEQ_SWING_TR808,       // 16th swing, 808 shuffle (66)
  SEQ_SWING_TR909,       // 16th swing, 909 house swing (64)
  SEQ_SWING_SHUFFLE,     // 8th blues shuffle, triplet at amt ~66 (75)
  SEQ_SWING_TRIPLET,     // 8th hard triplet feel (67)
  SEQ_SWING_COUNT
};
void        seqSwingStyleAdjust(int16_t counts);  // edit track; 4 counts = 1, no wrap
uint8_t     seqSwingStyle();                      // edit track (OLED)
uint8_t     seqSwingStyleOf(uint8_t ch);          // per track (0 = straight)
void        seqSwingStyleSet(uint8_t ch, uint8_t v); // EEPROM load (bad -> OFF)
const char *seqSwingStyleName(uint8_t v);         // "MPC60" ... (<= 7 chars)
void        seqSwingAmtAdjust(int16_t counts);    // edit track; encDrain, 0..100
uint8_t     seqSwingAmt();                        // edit track (OLED)
uint8_t     seqSwingAmtOf(uint8_t ch);            // per track
void        seqSwingAmtSet(uint8_t ch, uint8_t v); // EEPROM load (bad -> 50)
uint8_t     seqSwingRatioOf(uint8_t ch);          // effective ratio 50..75 (%)
uint8_t     seqSwingPairOf(uint8_t ch);           // pair span in steps, 0 = off
uint16_t    seqSwingDepthOf(uint8_t ch);          // delay permille of the pair

void seqPlayRange(uint8_t ch, uint8_t *lo, uint8_t *hi);  // playback window
bool seqLoopOn(uint8_t ch);           // that track's bar loop is armed
uint8_t seqBarPress(uint8_t bar);        // 0 = page set, 1 = loop on, 2 = off
uint8_t seqStepViewBase(void);           // first step of the edit window
void    seqStepViewFollow(uint8_t step); // move the edit window onto that step
                                         // (KEYMODE4 focus; no loop side effect)
uint8_t seqStepPage(void);               // edit window 0..3
// SEQMODE3 G#: the 16-step edit window CHASES the playhead - the bar (the
// window, not the playback) moves with the green dot, so the pads always
// show the bar that is sounding.  Per track like the window itself, RAM
// only; seqStepViewChaseSync is called from the playhead advance
bool    seqStepViewChaseGet(void);
bool    seqStepViewChaseToggle(void);    // returns the new state
void    seqStepViewChaseSync(uint8_t ch, uint8_t step);  // chase if enabled
bool seqBarLooped(uint8_t page);         // bar of the edit sequence looping
bool seqFollow();                        // ENC3: edit track follows its feeder
                                         //  (true for ON or FREEZE - both are
                                         //  a form of following)
bool seqFollowOf(uint8_t ch);            // follow active (ON or FREEZE) of ch
uint8_t seqFollowMode();                 // edit track's mode 0=OFF 1=ON 2=FREEZE
uint8_t seqFollowModeOf(uint8_t ch);     // per-track mode 0..2
bool seqFollowFreezeOf(uint8_t ch);      // true when that track is FREEZE
void seqFollowModeSet(uint8_t m);        // edit track (ENC3 cycles 0..2)
void seqFollowModeSetOf(uint8_t ch, uint8_t m); // restore (seqCfgLoad), clamped
// GNR8 BASS mapping (F2 ENC3) + LEAD melody resolution: how FOLLOW
// re-resolves a generated step note onto the incoming chord - NEAR =
// nearest chord tone inside the LOW..RANGE window, ROOT = the chord's
// lowest tone (octave-shaped), DEGREE = scale degree -> Nth chord tone,
// IVL = lowest tone + semitone offset from KEY root (can leave the chord),
// CT = chord TONE whose OCTAVE follows the generated contour note (melody
// shape preserved across the chord), GD = nearest GUIDE tone = the chord's
// 3rd and 7th only (falls back to NEAR when the chord has none).  Global,
// boot NEAR, persisted in the reserved SEQ config byte b[2] (0..5)
enum { SEQ_MAP_NEAR = 0, SEQ_MAP_ROOT, SEQ_MAP_DEG, SEQ_MAP_IVL,
       SEQ_MAP_CT, SEQ_MAP_GD };
uint8_t     seqMap();                   // current mode 0..5
void        seqMapSet(uint8_t m);       // clamped (bad -> NEAR); restore path
void        seqMapAdjust(int16_t counts);  // edit map (4 counts = 1 mode)
const char *seqMapName(uint8_t m);      // "near" "root" "deg" "ivl" "ct" "gd"
void seqDiagNote(uint8_t section, uint32_t us);  // track worst loop stalls; 1Hz report
