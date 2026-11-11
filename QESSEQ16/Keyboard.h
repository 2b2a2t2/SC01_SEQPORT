#pragma once

#include <Arduino.h>

// KEYMODE keyboard pads -> USB-MIDI notes with octave control (ENC1).
// Octave range -2..8, clamped (never wraps).
// note = 12*octave + 24 + semitone  (octave 3 -> C3 = 60 = middle C)
// Keys are indexed 0..11 in groupKeyboard order (C, C#, D ... A#, B).

int  keyPadPress(uint8_t idx);       // returns note sent, or -1
int  keyPadRelease(uint8_t idx);     // returns note released, or -1
void keyOctaveAdjust(int16_t counts); // 4 encoder counts = 1 octave step
int  keyOctave();                    // current octave (-2..8)

// called by the chord engine (Chords.cpp) whenever the sounding chord's
// notes change: re-snap the pending chord of every sequence the keyboard
// channel reaches (chords feed the sequencer like single notes)
void keyPendingRefresh();

// keyboard MIDI output channel (1..16); changing it first cuts every note
// sounding on the old channel (no stuck notes), held keys re-strike on the
// new channel.  The keyboard channel also SELECTS which sequence the
// keyboard feeds: plays on channel N update sequence N's pending chord.
// When that channel's TRACK is MONO the live keyboard is processed by it:
// ROOT = root note, BASS = lowest, ARP = next chord note per playhead step
// (transport-gated), raw notes suppressed - recording still sees the chord
void keySetChannel(uint8_t ch);
uint8_t keyChannel();
void keyMonoPoll();               // reconcile the live voice (loop context)

// sequencer edit channel (1..16): the playhead plays EVERY channel's
// sequence at once (each on its own MIDI channel); this selects which
// channel the step pads display and edit.  Focus switch only - running
// sequences are never cut.
void seqSetEditChannel(uint8_t ch);
uint8_t seqEditChannel();

// KEYMODE3 ENC3/ENC4 channel link: ENCOD / KBOARD, states free/locked.
// The SEQ edit channel is the hub - a LOCKED family moves with it (and a
// change in a locked family drives the hub, so the other locked family
// follows too); free stays independent (each mode's own channel, as
// always).  Both links boot LOCKED; the choice persists in the SEQ EEPROM
// block (a save from before that default carries no choice and boots
// locked too) and at boot the locked families take the hub channel
bool chanEncLocked();
bool chanKeyLocked();
void chanEncLinkSet(uint8_t on);
void chanKeyLinkSet(uint8_t on);
void keyChannelForce(uint8_t ch);  // boot reconcile only (raw, no cleanup)

// input (listen) channel of the edit sequence (1..16): notes arriving
// there feed THAT sequence's pending chord - every sequence listening to
// the channel is fed at once (no edit-focus gating).  Default sequence N
// listens to channel N.  Changing the input channel BACKFILLS the pending
// chord from that channel's track (record on ch2, point ch3 at ch2, ch3
// can activate immediately).  ENC5 in SEQMODE3 adjusts it (4 counts = 1
// step, clamped, no wrap)
void seqInputChannelAdjust(int16_t counts);
uint8_t seqInputChannel();

// octave shift of the SELECTED sequence's playback (ENC1 in SEQMODE, all
// variants): +1 = one octave up, applied by the playhead only - stored
// notes stay raw so the shift is live-changeable.  Clamped so shifted
// notes fit the -2..8 octave convention; the range is recalculated from
// the stored notes (notes at C3 -> -5..+5, empty -> keyboard-octave
// reference), 4 counts = 1 octave, no wrap
void seqOctaveShiftAdjust(int16_t counts);
int  seqOctaveShift();                // edit sequence's shift (for the OLED)

// --- external MIDI input (per track, SEQMODE 4 ENC3/ENC4) ------------------
// Every sequence has always listened to external notes whose channel
// matched its input channel; these two knobs steer that per track:
//   EXT IN (ENC3): OFF = ignore external MIDI, ON = listen (feed the
//             pending chord / record / assign), THRU = listen AND forward
//             the notes it receives (still input-channel filtered) out on
//             the track's output channel over USB - a per-track MIDI thru
//             over USB, LIVE = listen AND play the pending chord through
//             the track's own engine on EMPTY steps while the transport
//             runs (a DAW chord arpeggiates without recording it; stored
//             steps keep their chords; release = rest again)
//   SRC    (ENC4): USB / DIN / BOTH - which physical input the track
//             listens to (DIN = the 5-pin input on Serial2, pin 7; notes
//             only - clock/transport stays USB-only)
// Both clamped, no wrap; persisted per track in the SEQ EEPROM block.
// Boot: ON + USB = the behavior every build had before this knob
enum SeqExtInMode : uint8_t {
  SEQ_EXTIN_OFF = 0,   // ignore external MIDI
  SEQ_EXTIN_ON,        // listen (pending chord / record / assign)
  SEQ_EXTIN_THRU,      // listen + forward to the track's output channel
  SEQ_EXTIN_LIVE,      // listen + play the pending chord live (MONO/POLY)
  SEQ_EXTIN_COUNT
};
enum SeqExtInSrc : uint8_t {
  SEQ_EXTIN_USB = 0,   // only notes arriving over USB
  SEQ_EXTIN_DIN,       // only notes arriving on the DIN input (Serial2)
  SEQ_EXTIN_BOTH,      // either input
  SEQ_EXTINSRC_COUNT
};
// which physical input a note arrived on (port arg of extNoteOn/Off)
enum ExtInPort : uint8_t { EXT_PORT_USB = 0, EXT_PORT_DIN };

void        seqExtInModeAdjust(int16_t counts); // edit track; 4 counts = 1, no wrap
uint8_t     seqExtInMode();                     // edit track (OLED)
uint8_t     seqExtInModeOf(uint8_t ch);         // per track
void        seqExtInModeSet(uint8_t ch, uint8_t v); // EEPROM load (bad -> ON)
const char *seqExtInModeName(uint8_t v);        // "OFF" / "ON" / "THRU" / "LIVE"
void        seqExtInSrcAdjust(int16_t counts);  // edit track; 4 counts = 1, no wrap
uint8_t     seqExtInSrc();                      // edit track (OLED)
uint8_t     seqExtInSrcOf(uint8_t ch);          // per track
void        seqExtInSrcSet(uint8_t ch, uint8_t v); // EEPROM load (bad -> USB)
const char *seqExtInSrcName(uint8_t v);         // "USB" / "DIN" / "BOTH"

// --- live-record options (SEQMODE 1/2, M1 step-edit row: ENC2/3/4) ---------
// What the live REC pass writes and how the pattern replays it, per track:
//   LISTEN (ENC2):  CHORD = the keyboard gesture records as always (a
//             fresh gesture replaces the step); NOTE = the ARP's output
//             note records onto the step it sounds on - one note per step,
//             a pass PRINTS the arp line (capture hooks: the live keyboard
//             arp and the EXT IN = LIVE injected pass; the track's own
//             stored-step playback never feeds the recorder).  While NOTE
//             is armed keyboard gesture edges do not record
//   PLAYBACK (ENC3): GATE = stored steps play through the track's engine
//             as always (ARP walks the chord, ROOT/BASS/LEAD pick); RECORDED
//             = the stored notes play VERBATIM, all at once - no arp walk /
//             root pick / window fold, no step FX / strum timing / FScale
//             snap on POLY (octave + per-step OCT/TRANS still apply, the
//             record path is pre-compensated for the octave)
//   OVERDUB (ENC4): OFF = a fresh gesture replaces the step (as always);
//             ON = new notes STACK onto the step's content, and a MONO
//             track's multi-note steps then play ALL AT ONCE (the track
//             is no longer mono on playback)
// All clamped, no wrap (4 counts = 1 state); persisted per track in the
// SEQ EEPROM block.  Boot CHORD/GATE/OFF = the behavior every build had.
// DRUM already overdubs and ignores these knobs
enum SeqRecListen : uint8_t { REC_LISTEN_CHORD = 0, REC_LISTEN_NOTE };
enum SeqRecPlayback : uint8_t { REC_PLAY_GATE = 0, REC_PLAY_RECORDED };
enum SeqRecOverdub : uint8_t { REC_OVER_OFF = 0, REC_OVER_ON };

void        seqRecListenAdjust(int16_t counts); // edit track; 4 counts = 1, no wrap
uint8_t     seqRecListen();                     // edit track (OLED)
uint8_t     seqRecListenOf(uint8_t ch);         // per track
void        seqRecListenSet(uint8_t ch, uint8_t v); // EEPROM load (bad -> CHORD)
const char *seqRecListenName(uint8_t v);        // "CHORD" / "NOTE"
void        seqRecPlaybackAdjust(int16_t counts); // edit track; no wrap
uint8_t     seqRecPlayback();                   // edit track (OLED)
uint8_t     seqRecPlaybackOf(uint8_t ch);       // per track
void        seqRecPlaybackSet(uint8_t ch, uint8_t v); // EEPROM load (bad -> GATE)
const char *seqRecPlaybackName(uint8_t v);      // "GATE" / "RECORDED"
void        seqRecOverdubAdjust(int16_t counts); // edit track; no wrap
uint8_t     seqRecOverdub();                    // edit track (OLED)
uint8_t     seqRecOverdubOf(uint8_t ch);        // per track
void        seqRecOverdubSet(uint8_t ch, uint8_t v); // EEPROM load (bad -> OFF)
const char *seqRecOverdubName(uint8_t v);       // "OFF" / "ON"

// incoming MIDI notes (port = the physical input they arrived on) feed
// EVERY sequence whose input channel matches AND whose SRC allows that
// port, independently of the edit focus, plus the step held in assign
// mode; a THRU track also forwards them to its output channel
void extNoteOn(uint8_t port, uint8_t ch, uint8_t note, uint8_t vel);
void extNoteOff(uint8_t port, uint8_t ch, uint8_t note);

// DRUM track: per-voice sequencing.  Parts = 16 drum notes C1(36)..D#2(51);
// part-select overlay pad N = note 36+N (pad0 = C1, the GM kick).  Press M1
// in SEQMODE (TRACK=DRUM) for the overlay (dim orange, selected part bright,
// shows on press = instant); pressing a step there selects the part.  In the
// normal step view a press toggles the selected part on/off at that step
// (the playhead plays all of them).
void drumPartSelectHold(bool on);     // M1 held / released
bool drumPartSelectActive();
void drumPartSet(uint8_t idx);        // overlay pad 0..15
uint8_t drumPartNote();
const char* drumPartName();           // "C0" ... "D#1"
int stepDrumToggle(uint8_t step);     // -2 = part removed, -1 = no room/invalid,
                                      // else = part note added
// DRUM generator write path (Drum.cpp drives it): replace a step's parts
// with these drum notes and stamp the per-voice accent mask (bit n = note
// 36+n, the mask the playhead reads for accent velocity).  Clears the
// step's attributes first - stepAttClear wipes the drum masks too
int stepDrumWrite(uint8_t step, const uint8_t *notes, uint8_t n,
                  uint16_t accentMask);   // notes written, -1 = bad step

// ENCMODE + hold M1 = drum-voice mute: step pad N toggles the mute of
// drum note 36+N (C1..D#2) across EVERY drum-track sequence (state
// persists; playback skips the muted voice's stored note, MONO/POLY
// sequences are never affected)
void drumMuteHold(bool on);           // M1 held / released
bool drumVoiceMuteActive();           // overlay up (flag + ENCMODE)
bool drumVoiceMuted(uint8_t note);    // drum note 36..51
void drumVoiceMuteToggle(uint8_t note);
const char* drumVoiceName(uint8_t note);  // "C1" .. "D#2"

// ENC mode output mute: hold M2 = overlay where step pad N toggles the mute
// of output channel N+1 (pad N plays sequence N: dim orange-red = playing,
// bright orange-red = muted).  Mute gates that sequence's PLAYBACK only -
// recording, pending chords and the live keyboard stay untouched; muting
// cuts its sounding notes at once, unmuting resumes at the next step.
// The flags persist until toggled back (and clear if the mode changes)
void encMuteHold(bool on);            // M2 held / released
bool encMuteActive();                 // overlay up (flag + ENCMODE)
bool seqIsMuted(uint8_t seq);         // sequence index 0..15
void seqMuteToggle(uint8_t seq);      // step pad press

// --- overdub layers (hold M2 = keyboard overlay, SEQMODE 1/2/4) -------------
// Every stored note carries the layer of the record PASS that wrote it
// (4 layers max; pre-existing content reads as layer 1, a pass picks the
// first layer that holds no notes yet and a full set stacks into layer 4).
// Holding M2 turns the keyboard row into the layer panel: white C D E F
// pick layers 1..4 (MUTE is the resting selection: the panel opens on it and
// it never disarms, so layers mute one after another; with ERASE selected
// they confirm an erase and the row falls back to MUTE), black C# / D#
// select MUTE / ERASE, F# / G# / A# fire undo / redo / reset.  Layer mutes gate
// playback per track (DRUM steps never carry
// tags), erase drops the layer's notes, reset flattens all layers of the
// edit track into one - commands ride an 8-deep undo/redo stack.  Patterns
// are RAM-only, so layers are too (no EEPROM change)
#define LAY_CMD_NONE  0xFF           // no confirm command armed
#define LAY_CMD_MUTE  0              // white key = mute/unmute that layer
#define LAY_CMD_ERASE 1              // white key = erase that layer
// layKeyHandle() results: 0 = untouched (Events runs the normal key path),
// LAY_ACT_EAT = swallowed silently, the rest = the action whose header
// line Events.cpp builds from it
enum LayAct : uint8_t {
  LAY_ACT_NONE = 0, LAY_ACT_EAT, LAY_ACT_ARM_MUTE, LAY_ACT_ARM_ERASE,
  LAY_ACT_MUTE_ON, LAY_ACT_MUTE_OFF, LAY_ACT_ERASED,
  LAY_ACT_EMPTY, LAY_ACT_REPORT, LAY_ACT_UNDO_OK, LAY_ACT_UNDO_NO,
  LAY_ACT_REDO_OK, LAY_ACT_REDO_NO, LAY_ACT_FLATTEN
};
bool    layOvEligible(void);         // SEQMODE 1/2/4 + non-DRUM edit track
void    layOvOpen(void);             // M2 hold started
bool    layOvClose(void);            // M2 let go: true = the overlay was up
bool    layOvActive(void);           // up right now (flag + eligibility)
uint8_t layKeyHandle(uint8_t kidx, uint8_t ev);   // LAY_ACT_* (0 = not ours)
uint8_t layArmCmd(void);             // LAY_CMD_* armed, LAY_CMD_NONE = none
uint8_t layInfo(uint8_t layer);      // LED state: bit0 = notes, bit1 = muted
uint8_t layNotes(uint8_t layer);     // notes of that layer on the edit track

// SEQMODE3 erase: clear one sequence (all steps + its history + sounding
// notes) or the COMPLETE pattern (all 16, also while playing - sounding
// notes are cut at once).  Listener sequences keep their own copy
void seqErase(uint8_t seq);           // sequence index 0..15
void seqEraseAll();

// --- whole-pattern clipboard (SEQMODE3 F# copy / A# paste) -----------------
// One RAM slot for the WHOLE step table of one track: notes and chord
// lengths, the attribute byte (incl. the POLY step-TYPE override), the FX
// byte, the KEYMODE4 embellishment params, the per-step modifiers
// (OCT / BRW / PROB / TRANS), the HAND strum onsets, the DRUM per-voice
// accent and probability masks and the SKANK locks.  A paste writes all 64
// steps and leaves the destination's TRACK settings alone (length,
// FILL/MOT, genre, kit map, TRANS, swing, channel) - only step data moves.
// seqPatCopy returns the active steps copied (0 = empty track), seqPatPaste
// the steps written (-1 = nothing copied yet); both act on the edit track
int  seqPatCopy(void);
int  seqPatPaste(void);
// SEQMODE3 D#: the steps a doubling opens up are filled with a copy of the
// pattern as it was - nothing to copy from a track at length 0
void        seqPatRepeatInto(uint8_t ch, uint8_t oldLen);
bool seqPatClipHas(void);

// --- SEQMODE step-edit for MONO tracks (radio M1/F1-F4) ---------------------
// per-step attributes: TIE sustains the note through the next step (no
// retrigger, chains, works over empty steps), GLIDE overlaps the note-on
// with the old note-off (legato - the synth's own portamento sounds),
// ACCENT plays the step at velocity 127 (other steps use the track's
// default), PROB gates the step by its stored 0..100% chance.  Track note
// shaping via ENC5/ENC6 in SEQMODE: gate = % of one step (100 = hold to
// the next step), velocity = default note velocity (boot 100).  Storage
// clears with the step (record / tap-disable / erase).
// Bits 6-7 are the POLY step TYPE override (F2 target): 00 = inherit the
// track's TYPE, 01/10/11 = STEP/STRUM/HAND - never stored as the classic
// flags, so the LED pulse masks (bits 0..5) stay clean.  Bit 4 is not an
// edit attribute either: the record pass stamps it on a step it stacked
// with OVERDUB = ON (the OVERDUB claim), and stepAttrs() masks bits 4-5
// back out before it synthesises the FX / TYPE pulses - a claimed step
// must not light the armed-attribute LED
#define SEQ_ATT_TIE    0x01
#define SEQ_ATT_GLIDE  0x02
#define SEQ_ATT_ACCENT 0x04
#define SEQ_ATT_PROB   0x08
#define SEQ_ATT_OVDM   0x10   // record pass stacked this step (OVERDUB ON)
bool stepAttrToggle(uint8_t step, uint8_t att);
                                      // true = attribute now on.  MONO: the
                                      // step's own flag; DRUM: the SELECTED
                                      // PART's accent / probability (PROB
                                      // only marks the step - the % comes
                                      // from the track's own probability)
uint8_t stepAttrs(uint8_t step);      // edit channel's attribute mask for the
                                      // LED pulse - DRUM follows the part select
bool stepTypeToggle(uint8_t step, uint8_t type);
                                      // stamp / clear the bits6-7 override
                                      // (type 0..2 = STEP/STRUM/HAND): true =
                                      // the step now carries an override

// POLY step FX (embellishments, F1 = FX target in SEQMODE): one byte per
// step beside stepAtt - SUS2/SUS4, COLOR (quiet half-step-below-root ghost)
// in the low nibble, a CHORD_EXT-style extension mask (6/m7/M7/9) in the
// top nibble.  Playback transforms the chord as it leaves (buildNotes
// keeps writing plain triads/extensions).  Strumming moved to the TYPE
// system (track default on ENC7 / per-step override on F2).
// Same lifecycle as the attributes: RAM only, dies with the step
enum StepFxType : uint8_t {
  SEQ_FX_SUS2 = 0, SEQ_FX_SUS4, SEQ_FX_EXT6, SEQ_FX_EXTm7,
  SEQ_FX_EXTM7, SEQ_FX_EXT9, SEQ_FX_COLOR
};
uint8_t stepFxBit(uint8_t type);      // type index -> bit in the FX byte
bool stepFxToggle(uint8_t step, uint8_t type);
                                      // true = FX now on; the SUS pair is
                                      // mutually exclusive (sus2 vs sus4)

// HAND performs RIGHT NOW: the keyboard channel reaches a POLY sequence
// whose TRACK type is HAND (live gate is track-level - per-step overrides
// only shape the sequence's playback).  The track's chords resolve into a
// silent string BUFFER (the keyboard strums instead of hearing them), the
// 7 white keys are the strings, the black keys toggle the strum options,
// the chord engine stays muted (chordMute) so keys build silently
bool handLive();

// the keyboard channel points at a DRUM sequence: the 12 pads are a drum
// pad grid (C..B = drum notes), so no KEY / SCALE may shape them - the
// press skips the scale filter and the chord engines, and the LEDs drop
// the scale/chord colors for one dim orange-red row.  A pad press also
// selects that drum voice for the step edit (the direct form of hold M1 +
// step N), and a DRUM row defaults to octave 1 (C1 = 36 = the kit) while
// every track keeps its own octave
bool keyDrumTrack();
void keyOctTrackPoll();            // per-track KEYMODE octave swap (loop)

// strum options - the black keys while handLive() (tap toggles, RAM only,
// booted as SUS+SUSCOMMON): Le Strum's set ported from StrumController.c
#define STRUM_OPT_GUITAR  0x01  // guitar chord voicing (6-string shapes)
#define STRUM_OPT_GUITAR2 0x02  // upper-octave layer on the high strings
#define STRUM_OPT_ADDS    0x04  // master: the F-row ADDS colors apply
#define STRUM_OPT_SUS     0x08  // strum rings till the buffered chord changes
#define STRUM_OPT_SUSCOM  0x10  // common tones survive that chord change
uint8_t strumOptsGet();             // current option bitfield
// selected ADDS colors (temporary F-row, gated by STRUM_OPT_ADDS):
// bit0 = sus4 (replaces the 3rd), bit1 = add6, bit2 = add9 - combinable
uint8_t strumExtGet();
void    strumExtSet(uint8_t v);
// note of white-key string s (0..6): 0 = no chord yet, 0xFF = this string
// has no note (guitar shape gap) - the LED dims those strings
uint8_t handStrNote(uint8_t s);
// cut a ringing HAND track (POLY TYPE change / keyboard channel change)
void    handRingCut(uint8_t c);

// SEQMODE4 + REC: persist all SEQ settings to EEPROM - track type, mono
// type, octave, input channel, velocity, accent velocity, gate, mutes,
// probability, pattern length, FOLLOW, edit channel, per-track swing
// (style + amount) and per-track EXT IN (mode + source).  NOTE PATTERNS ARE
// NOT STORED: the steps, their contents and the tie/glide/accent/prob
// attributes stay RAM only.
// seqCfgLoad() restores them at boot (before the first draw); a blank or
// corrupt block keeps the defaults
void seqCfgSave();
void seqCfgLoad();
void seqGateAdjust(int16_t counts);   // ENC5: 4 counts = 5%, clamped 10..100
int  seqGatePct();                    // edit track's gate (% of one step)
void seqVelocityAdjust(int16_t counts); // ENC6: 4 counts = 1, clamped 1..127
int  seqVelocity();                   // edit track's default velocity
void seqProbAdjust(int16_t counts);   // ENC8: 4 counts = 1%, clamped 0..100
int  seqProbPct();                    // edit track's probability (%)
void seqAccentVelocityAdjust(int16_t counts); // ENC6 + F3 held: accent vel
int  seqAccentVelocity();             // edit track's accented-step velocity
// per-track readers for the ENCABLETONMODE bridge (Link.cpp dumps any
// channel's stored pattern, not just the edit one)
int  seqOctaveShiftOf(uint8_t ch);    // 0..15, octave shift of track ch
int  seqGatePctOf(uint8_t ch);
int  seqVelocityOf(uint8_t ch);
int  seqAccentVelocityOf(uint8_t ch);
int  seqProbOf(uint8_t ch);
uint8_t stepAttrsOf(uint8_t ch, uint8_t step);   // raw per-track attribute byte
void seqSetAccentVelocity(uint8_t seq, uint8_t v); // EEPROM load, clamped
void seqGatePoll();                   // fire gate note-offs (loop context)
void seqSwingPoll();                  // drain deferred (swung) off-step
                                      // passes, due time reached (loop
                                      // context, beside seqGatePoll)
void seqStrumPoll();                  // drain staggered strum note-ons
                                      // (loop context, beside seqGatePoll)
void compPoll();                      // drain COMP stab queue (loop context,
                                      // beside seqStrumPoll)
void seqCompHitsAdjust(int16_t counts); // COMP page ENC1: 4 counts = 1 stab
int  seqCompHits();                   // edit track's comp stabs (0..8)
uint8_t seqCompHitsOf(uint8_t i);     // raw per-track (EEPROM save)
void seqCompHitsSet(uint8_t i, uint8_t v); // EEPROM load (bad -> 0)
// SKANK page params of the edit track (M2, POLY TYPE=SKANK):
// 0 = TES (tessitura anchor), 1 = MOT (motion, boot 50 = centered),
// 2 = SHP (shape LIN/PP/RND), 3 = THN (thinning), 4 = EXT (extension %),
// 5 = SUB (substitution %) - all 0..100, 4 counts = 1 unit, no wrap
void    skankPrmAdjust(uint8_t idx, int16_t counts);
uint8_t skankPrm(uint8_t idx);            // edit track (page display)
uint8_t skankPrmOf(uint8_t ch, uint8_t idx);   // raw per-track (EEPROM save)
void    skankPrmSet(uint8_t ch, uint8_t idx, uint8_t v); // load (bad -> default)
void seqTransAdjust(int16_t counts);  // PREV-shift row ENC2: 1 count = 1 st,
                                      // -12..+12, DRUM tracks refuse
int  seqTrans();                      // edit track's semitone TRANS
int8_t seqTransOf(uint8_t i);         // raw per-track (playback/EEPROM), 0 on DRUM
void seqTransSet(uint8_t i, int8_t v); // EEPROM load (clamped -12..+12)
bool seqKlockSetDir(int16_t dir);        // PREV-shift row ENC3: per-track
                                        // scale-lock for TRANS - two-state,
                                        // NO wrap (CW = ON, CCW = OFF, like
                                        // ENCOD/KBOARD; DRUM refuses); true
                                        // when the flag changed
bool seqKlock();                        // edit track's Klock flag
bool seqKlockOf(uint8_t i);             // raw per-track (playback/EEPROM),
                                        // false on DRUM
void seqKlockSet(uint8_t i, bool v);    // EEPROM load
uint8_t seqFMask();                   // attributes the playhead reads this step
                                      // (F-row blink mask, edit channel)

// Step pads (0..15):
//  press   = unassigned: SILENTLY record a chord into it; sounds only when
//            the playhead reads it during playback; active: silent
//            candidate -> disabled on click.  Source: POLY recording from
//            ANOTHER channel (input != own) takes the held gesture first
//            (keys / latched chord / held external notes), then the INPUT
//            track's timeline (last active step at or before this position,
//            like MONO), history, pending; every other case records the
//            pending chord (fed by its input channel and by the keyboard
//            when the keyboard channel selects it)
//  hold    = assign mode: keyboard keys played while held REPLACE that
//            step's chord (first key starts the new chord)
int  stepChordPress(uint8_t step);   // notes recorded; -1 = silent (active/bad)
// why the last press recorded nothing (0 notes): a short full-word reason for
// the header - "Feeder track empty" when the input track on ENC5 has nothing
// to copy, else "No notes to record".  Empty string = the press landed
const char* stepRecordWarn();
int  stepChordClick(uint8_t step);   // EV_CLICKED: 0 = step disabled
void stepAssignSet(int8_t step);     // step = 0..63 to arm, -1 to disarm

// KEYMODE4 (Key4.cpp drives it): direct write of a generated chord into the
// edit sequence (pre-compensated for the sequence octave shift, like the
// live gesture) and a one-level undo of the last write batch
int            stepChordWrite(uint8_t step, const uint8_t *notes, uint8_t n);
                                                           // stored n, -1 = bad
const uint8_t* stepChordAt(uint8_t step, uint8_t *len);    // stored chord (edit seq)
// HARM (Key4.cpp reads the feeder bassline): the input channel the edit
// channel follows (1-based, 0 = none/self) and a channel-targeted step
// reader so the harmonizer can read another track's stored notes
uint8_t        seqFeederChannel();
const uint8_t* stepChordAtCh(uint8_t ch, uint8_t step, uint8_t *len);
// FOLLOW FREEZE: latch the chord/note the feeder is sounding at this instant
// (SeqClock calls these from seqFollowModeSet - entering FREEZE captures, any
// other mode drops the latch so the next entry recaptures fresh).  The
// capture returns false when the feeder has nothing right now, and the
// playback path then captures on the follower's first frozen step
bool           seqFollowFreezeCapture(uint8_t ch);
void           seqFollowFreezeClear(uint8_t ch);
void           stepSnapshotTake();     // snapshot the edit sequence for undo
int            stepSnapshotRestore();  // 0 = restored (switches back the ch), -1 = none
// true when the step already holds exactly these notes as stepChordWrite
// would store them (same count, same octave pre-compensation) - lets the
// automatic HARM re-bake skip a step whose chord did not move
bool           stepChordSame(uint8_t step, const uint8_t *notes, uint8_t n);
// HARM auto re-bake: harmFeederChanged marks every POLY+HARM track that
// feeds from ch0 (FOLLOW on); harmRebakePoll runs the marked bakes - called
// once per loop pass from QESSEQ16.ino right before the display draw, so
// a burst of feeder writes costs one Viterbi pass instead of one per step
void           harmFeederChanged(uint8_t ch0);
void           harmRebakePoll();

// GNR8 generator write path (Gen.cpp drives it): one note per step,
// stamped like stepChordWrite (octave pre-compensated, followers and the
// shift range re-derived) plus the accent/glide mask the generator rolled;
// stepGenClear empties a step the roll turned into a rest.  The rotate
// moves every stored element of the edit sequence's steps within its own
// pattern length (dir > 0 = content delays one step, wraps at the end;
// elements past the length stay put), stepAttrChain stamps one attribute
// onto steps from..to-1 (wrapping inside the pattern length, end
// exclusive - TIE on X..Y-1 rings X's note through step Y) and returns
// how many steps were touched
int      stepGenWrite(uint8_t step, uint8_t note, uint8_t att);
int      stepGenClear(uint8_t step);
int      stepSkeletonWrite(uint8_t step, uint8_t stored);  // LEAD flip restore
void     stepShiftPattern(uint8_t n, int8_t dir);
uint8_t  stepAttrChain(uint8_t from, uint8_t to, uint8_t att);

// GNR8 per-step pitch modifiers (F2/F3/F4): edit-sequence accessors,
// clamped on set (octave -2..+2, borrowed chord tone 0..8 = off..n-th,
// modifier probability 0..100)
int8_t   stepOctAt(uint8_t step);
void     stepOctSet(uint8_t step, int8_t v);
uint8_t  stepBrwAt(uint8_t step);
void     stepBrwSet(uint8_t step, uint8_t v);
uint8_t  stepModPAt(uint8_t step);
void     stepModPSet(uint8_t step, uint8_t v);
int8_t   stepTrAt(uint8_t step);     // per-step TRANS: playback-only semitone
void     stepTrSet(uint8_t step, int8_t v);  // offset, clamped -12..+12
// per-BAR octave / TRANS (the SEQ3 keyboard row: hold a C D E F bar pad and
// the encoder row edits THAT bar's playback shift - 16 steps per bar, 4
// bars).  Octave -2..+2 like the step OCT, TRANS -12..+12 like the track's;
// playback-only, folded into the track's own shift so Klock still applies.
// The Begin/End pair carries the held pad (one bar at a time); the adjust
// calls return the new value and refuse DRUM tracks
uint8_t  seqBarEditSel();
void     seqBarEditBegin(uint8_t bar);
void     seqBarEditEnd();
int8_t   seqBarOctAt(uint8_t bar);
int8_t   seqBarTrAt(uint8_t bar);
int8_t   seqBarOctAdjust(int16_t counts);
int8_t   seqBarTrAdjust(int16_t counts);
// per-step SKANK locks (hold a step on a POLY+SKANK track = the lock row,
// encoders 1..6): value 0..100 or -1 = inherit the track's page knob.
// Stored with the step content (stepAttClear wipes them), RAM only like
// every step field
int16_t  stepSkankLockAt(uint8_t step, uint8_t idx);
void     stepSkankLockSet(uint8_t step, uint8_t idx, int16_t v);

// KEYMODE4 step-hold row (ENC2/3/4): rewrite the held step's STORED chord
// in place, each returning true when a note actually moved.  DEG shifts
// every tone by one scale degree in the key (intervals keep their shape;
// force-to-scale shapes them into the scale at playback), INV cycles the
// bass (d > 0 = lowest up an octave, d < 0 = highest down), VOICE spreads
// or closes the upper voices around the bass (+1 = all-but-lowest +12,
// -1 = all-but-lowest -12 while they stay above it).  OCT uses the
// existing stepOctSet/stepOctM pair
bool stepDegAdjust(uint8_t step, int8_t d);
bool stepInvAdjust(uint8_t step, int8_t d);
bool stepVoiAdjust(uint8_t step, int8_t d);

// sequencing (called from loop context, never from the clock ISR):
// press on an unassigned step records a chord into it (see above);
// seqStepAdvance runs each time the playhead reaches a step - every
// track carries its OWN position (independent pattern length / bar loop)
bool stepIsActive(uint8_t step);     // step has a recorded chord
bool seqStepUsed(uint8_t step);      // edit track reads it: inside its
                                     // pattern or inside its armed bar loop
                                     // (past it the pad is dark + inert)
void seqStepAdvance(bool start);   // transport step; start snaps to range heads
void seqStepStop();                  // transport stopped: release notes
uint8_t seqStepPlayPad();            // playhead pad 0..15 of the edit window,
                                     // 0xFF when another bar plays / silent
uint8_t seqStepPlayGhost();          // head's bar-relative pad 0..15 (dark-blue
                                     // ghost while another bar is shown)
// live record (SEQMODE REC button): 0 = off, 1 = armed, 2 = recording
uint8_t seqRecState();
uint8_t seqRecToggle();     // 0 = refused (kbd ch mismatch), 1 = armed, 2 = off
bool   seqRecTakeDone();    // true once when a pass finished by itself
uint8_t seqPlayBar();                // bar 0..3 the playhead reads (0xFF =
                                     // silent) - SEQ3 bar LED pulse

// notes the playhead currently has sounding on MIDI channel (1..16):
// the keyboard LEDs blink these pitch classes while they play on the
// keyboard channel (count 0 = silent)
uint8_t seqSoundingCount(uint8_t ch);
uint8_t seqSoundingNote(uint8_t ch, uint8_t i);

// KEYMODE4 per-step embellishment overrides (PREV hold encoders EN5-8):
// [0] = sub prob 0..100 (255 = use global), [1] = inv bias 0..3 (255 = global)
// [2] = pass amt 0..100 (255 = global), [3] = color amt 0..100 (255 = global)
uint8_t stepEmbGet(uint8_t step, uint8_t param);  // param 0..3
void    stepEmbSet(uint8_t step, uint8_t param, uint8_t val);
// SEQMODE EMB target (POLY: F1 cycles FX -> EMB): a step tap cycles its
// SUB/INV/PAS/COL override - true = the step carries a non-global param
bool    stepEmbToggle(uint8_t step);
