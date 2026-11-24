#pragma once

#include <stdint.h>

// DRIFT: a second progression derived from a feeder progression (SEQMODE,
// POLY track with TYPE = DRIFT).
//
// The FOLLOW feeder's STORED chords are read as a progression and cut into
// blocks - one chord plus how many steps it lasts.  The blocks are fitted
// onto the DRIFT track's own pattern length and answered by one of three
// policies, then WRITTEN into the track's own steps (M3).  Playback is the
// normal POLY path over those stored steps, so the pads show the derived
// progression, it can be edited by hand, and a third track can read it:
// A -> DRIFT B -> DRIFT C chains.
//
// M3 tap = derive + write (one undo snapshot; RHYTHM decides which steps
// carry a chord - BLOCK = block heads only, FILL = the track's FILL/ALGO
// roll like the HARM bake, STAB = every step), M3 hold = undo.  F4 APPLY is
// the same chord fitting on demand over the steps the track already uses,
// leaving its activation and rhythm untouched.  Editing the feeder's notes
// or length re-derives those steps (unchanged ones left alone); the feeder's
// SPEED never matters, because the map counts steps, not time.
//
// The M2 DRIFT page (Events.cpp) arms three F banks:
//   F1 BASE  GENRE / FILL / HOW / ANCHOR
//   F2 VOICE SHAPE / SIZE / VOICE / INV  (this engine's own voicing values)
//   F3 FIT   SPAN / READ / RHYTHM / --
//   F4       APPLY (a gesture, not a bank)
// Everything is RAM only, like the HARM page settings.

enum DriftHow : uint8_t {
  DRIFT_HOW_SAME = 0,   // every step of a block plays the feeder chord
  DRIFT_HOW_WALK,       // land on the chord, then lead into the next one
  DRIFT_HOW_REPLY       // answer with a related chord (ANCHOR keeps some)
};

enum DriftRead : uint8_t {
  DRIFT_READ_CHRD = 0,  // decode the feeder step's whole chord
  DRIFT_READ_ROOT      // lowest note is the root, quality from the scale
};

// which steps of a block carry a chord.  POLY holds a chord only for its own
// gate and cannot sustain across steps, so a block that re-strikes every step
// reads as a pile of stabs - BLOCK exists to leave the block room to ring
enum DriftRhythm : uint8_t {
  DRIFT_RHY_BLOCK = 0,  // the block's first step only; every other step is a rest
  DRIFT_RHY_FILL,       // the track's FILL% (and ALGO = EUCLID) rolls the steps
  DRIFT_RHY_STAB        // every step of the pattern gets a chord
};

// --- F1 BASE ---------------------------------------------------------------
void        driftHowAdjust(int16_t counts);   // SAME / WALK / REPLY, no wrap
uint8_t     driftHow();
const char* driftHowName();                   // "SAME" / "WALK" / "REPLY"

void        driftAnchorAdjust(int16_t counts);  // 0..100%, 1 count = 1
uint8_t     driftAnchor();                      // chance to keep the feeder chord

// --- F3 FIT ----------------------------------------------------------------
void        driftSpanAdjust(int16_t counts);  // AUTO then 1..8 steps per block
uint8_t     driftSpan();                      // 0 = AUTO
const char* driftSpanName();                  // "AUTO" / "1".."8"

void        driftReadAdjust(int16_t counts);  // CHRD / ROOT, no wrap
uint8_t     driftRead();
const char* driftReadName();                  // "CHRD" / "ROOT"

void        driftRhythmAdjust(int16_t counts);  // BLOCK / FILL / STAB, no wrap
uint8_t     driftRhythm();
const char* driftRhythmName();                  // "BLOCK" / "FILL" / "STAB"

// --- F2 VOICE (DRIFT's own values; HARM's bank is untouched) ---------------
void        driftShapeAdjust(int16_t counts);   // 0 close / 1 open / 2 drop
uint8_t     driftShape();                       // edit track (voicing)
const char* driftShapeName();                   // "Close" / "Open" / "Drop"
void        driftSizeAdjust(int16_t counts);    // 0 triad / 1 7th / 2 9th
uint8_t     driftSize();
const char* driftSizeName();                    // "Triad" / "7th" / "9th"
void        driftSpreadAdjust(int16_t counts);  // 0..3 octaves above the bass
uint8_t     driftSpread();
void        driftInvAdjust(int16_t counts);     // 0 nearest / 1..3 forced
uint8_t     driftInv();
const char* driftInvName();                     // "Nat" / "1st" / "2nd" / "3rd"

// --- the engine ------------------------------------------------------------
// M3 tap: read the feeder, fit, answer, write (undo snapshot; the steps it
// fills come from RHYTHM).
// Returns steps written, -1 = refused (no feeder chords / bad pattern).
int      driftBake();
// automatic re-derive after a feeder change: active steps only, unchanged
// steps skipped (no snapshot, no FILL re-roll); under BLOCK it also empties
// the steps inside a block, because there the block is one chord plus rests
int      driftRebake();
// RHYTHM knob: re-lay the pattern with the new rhythm right away - an
// explicit write (undo snapshot) that keeps the current REPLY answer, so
// BLOCK / FILL / STAB can be compared on the same progression
int      driftRelay();
// F4 APPLY: fit the answer onto the steps the track ALREADY uses - the pass a
// feeder edit runs, but on demand.  Every active step gets the chord the
// engine places there; nothing is activated, emptied or re-rolled (a BLOCK
// track keeps its hand-written interiors), and the current REPLY answer is
// kept.  Takes the undo snapshot, so M3 hold restores the pre-APPLY chords
int      driftApply();
// the chord DRIFT would play at one step (step recording on a DRIFT track):
// returns the note count, 0 = nothing to record
uint8_t  driftStepChord(uint8_t step, uint8_t *notes);
