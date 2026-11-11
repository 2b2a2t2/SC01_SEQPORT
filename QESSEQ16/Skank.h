#pragma once

#include <stdint.h>

// SKANK: send-time generative voicing (SEQMODE, POLY track with TYPE =
// SKANK).  On every active step tick the track derives the voicing it is
// about to play from the source chord - the FOLLOW feeder's chord when
// FOLLOW points at another track, its own stored step otherwise - and the
// result runs through the normal playback pipeline (step FX, FScale,
// octave/TRANS, comp, mute).  Stored steps are never rewritten.
//
// The M2 SKANK page shapes the derivation live:
//   ENC1 TES  tessitura: the anchor inversion 0..100 (0/25/50/75 =
//        1st/2nd/3rd/root positions of the walk, 100 wraps home)
//   ENC2 MOT  motion: -50..+50 around center 50 - how far each chord slot
//        walks away from the anchor (50 = park at TES)
//   ENC3 SHP  shape of that walk: LIN (rises through the bar), PP (ping-
//        pong 0,1,2,3,2,1 across chord slots), RND (a fresh roll per hit)
//   ENC4 THN  thinning: drop the lowest tones progressively (0 = full,
//        50 = drop the lowest, 100 = keep the top two only)
//   ENC5 EXT  extension %: roll an in-scale color tone (6th / maj7 / 9th)
//        above the voicing, decided per hit (FScale key rule)
//   ENC6 SUB  substitution %: at the last chord of every bar roll a
//        passing chord sampled from the current genre's transition matrix
//        (the 18 KEYMODE4 genres) - the voicing becomes that degree's
//        root-position diatonic triad at the sounding bass
//   ENC7 TES% tessitura randomizer %: per hit, roll the inversion fully
//        at random (0..3) instead of the anchored TES/MOT/SHP walk
//   ENC8 NOT% note randomizer %: per hit, one tone of the voicing mutates
//        to a random in-scale tone at the same register (stab size
//        unchanged; applied after invert/thin, before finalize)
//
// Holding a step on the track arms the per-step LOCK row (encoders 1-7
// override that step's parameter, 0 = inherit; encoder 8 = the step's
// TRANS like the plain held-step row - NOT% stays page-level only).
// Locks live with the step content (cleared together) and stay RAM only,
// like every step field.
struct Chord {
  uint8_t notes[8];
  uint8_t count;
};

// Derive one skank voicing.  feeder = the source chord (not modified),
// skankOut receives the result (empty when the source was empty).
// stepIndex = the follower's step position (0..63 - bar = stepIndex & 15,
// chord slot inside the bar = (stepIndex & 15) >> 2).
// encTessiture/encMotion/encShape/encThinning/encExtension/encSubstitution/
// encTesRand/encNotRand are the page knobs 0..100 (see the block comment
// above).
void deriveSkankChord(const Chord &feeder, Chord &skankOut, uint8_t stepIndex,
                      uint8_t encTessiture, uint8_t encMotion, uint8_t encShape,
                      uint8_t encThinning, uint8_t encExtension,
                      uint8_t encSubstitution, uint8_t encTesRand,
                      uint8_t encNotRand);

// ENC3 value label for the header / OLED: "LIN" / "PP" / "RND"
const char *skankShpName(uint8_t encShape);
