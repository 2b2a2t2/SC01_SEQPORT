#pragma once

#include <Arduino.h>

// KEYMODE scale state (shared by all KEY variants) and the KEYMODE2 chord
// mode - a port of TeensikDiM's chordmode: quality/extension buttons,
// diatonic auto-chords, voicing/bass/spread/density, latch, KeyLock.
// The keyboard (Keyboard.cpp) owns the pads; this module owns the sound.

#define KEY_SCALE_COUNT 54

// momentary quality buttons (M1/M2/M3/NEXT) - CHORD_Q_NONE on release
#define CHORD_Q_NONE 0
#define CHORD_Q_MAJ  1
#define CHORD_Q_MIN  2
#define CHORD_Q_DIM  3
#define CHORD_Q_SUS  4

// extension buttons F1..F4 (bitmask, held together)
#define CHORD_EXT_6   0x01
#define CHORD_EXT_m7  0x02
#define CHORD_EXT_M7  0x04
#define CHORD_EXT_9   0x08
#define CHORD_EXT_11  0x10
#define CHORD_EXT_13  0x20

// --- scale (ENC2 in every KEY variant; root in KEYMODE2 / the PREV row) --
void        keyScaleAdjust(int16_t counts);  // 4 counts = 1 scale, clamp, no wrap
int         keyScale();                      // 0 = Chrom (no filter, no LED change)
const char* keyScaleName();                  // <= 7 chars, the OLED column
const char* keyScaleMood();                  // 1-word mood - the header on a scale change
                                             // (the knob value carries the name)
void        keyRootAdjust(int16_t counts);   // 4 counts = 1 root, clamp, no wrap
int         keyRoot();                       // 0 = C
const char* keyRootName();
bool        keyIsInScale(uint8_t kidx);      // key 0..11 in scale rooted at keyRoot

// --- chord engine (KEYMODE2) ---------------------------------------------
// press/release come from Keyboard.cpp (the root note = the pad's note)
int     chordPadPress(uint8_t kidx, uint8_t note);   // returns the root played
int     chordPadRelease(uint8_t kidx);               // -1 = key was not held

void    chordSetQuality(uint8_t q);          // press sets, release -> NONE
void    chordSetExt(uint8_t ext, bool on);   // press sets, release clears
void    chordParamAdjust(uint8_t idx, int16_t counts);  // 0..3 = voic/bass/sprd/dens
uint8_t chordParam(uint8_t idx);             // 0..3
void    chordLatchAdjust(int16_t counts);    // CW = latch on, CCW = off
void    chordKeyLockAdjust(int16_t counts);  // CW = KeyLock on, CCW = off
void    chordPerfAdjust(int16_t counts);     // CW = Perf on, CCW = off (state only)
void    chordFScaleAdjust(int16_t counts);   // CW = FScale on, CCW = off
bool    chordFScale();        // ON: every chord tone is snapped to the scale
uint8_t chordForceNote(uint8_t note);  // the FScale snap (identity when off)

// --- POLY step FX (playback-time embellishments) --------------------------
// low-nibble flags of the per-step FX byte (its top nibble carries the
// CHORD_EXT_* extension mask); written by stepFxBit() in Keyboard.cpp
#define CHORD_FX_SUS2   0x01
#define CHORD_FX_SUS4   0x02
#define CHORD_FX_COLOR  0x04
uint8_t chordFxApply(const uint8_t *in, uint8_t n, uint8_t fx,
                     uint8_t *out, uint8_t *nCore);
                    // reshape a stored chord the moment it plays: SUS swaps
                    // the third for a 4th/2nd, the extension mask runs the
                    // 6/m7/M7/9 colors, COLOR appends a raw half-step-below-
                    // root approach note at the END of the list.  Returns
                    // the total; *nCore = the transformable core - FScale
                    // must snap [0, nCore) only (the tail stays chromatic)
                    // and the tail must be deduped against the emitted notes
bool    chordLatch();
bool    chordKeyLock();
bool    chordPerf();

const uint8_t* chordNotes();   // notes sounding through the engine
uint8_t        chordCount();
bool           chordSounding();
bool           chordPlaysPitch(uint8_t pc);  // pitch class currently sounding
const char*    chordName();    // "GMaj7" for the header ("" before first chord)

// --- KEYMODE4 (genre-driven proposals) -------------------------------------
// the genre engine drives the build state directly: press builds an explicit
// quality/extension/bass-inversion chord (KeyLock and latch are bypassed for
// the gesture - their KEYMODE2 state is saved and restored by chordKey4Clear)
int     chordKey4Press(uint8_t kidx, uint8_t note, uint8_t q, uint8_t ext,
                       uint8_t bass);          // returns the root played
void    chordKey4Bucket(uint8_t q, uint8_t ext, uint8_t bass);
                                                // re-voice the remaining keys
void    chordKey4Clear();                       // gesture over: restore state
uint8_t chordBuild(uint8_t root, uint8_t q, uint8_t ext, uint8_t bass,
                   uint8_t *out);               // notes only (apply path)

// lifecycle (keyboard output channel / octave / mode changes)
void chordShift(int semitones);  // octave shift of the held/sounding chord
void chordCut();                 // note-offs on the current channel
void chordRestore();             // re-send what chordCut() silenced
void chordMute(bool on);         // live MONO-track voice: state kept, sound
                                 // muted (off re-sends whatever sounds now)
void chordStopAll();             // silence + reset gesture (leave KEYMODE2)
