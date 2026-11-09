#pragma once

#include <Arduino.h>
#include <FastLED.h>

// mode buttons = groupMode LEDs: 32=ENC, 33=SEQ, 34=KEY
enum ModeBase : uint8_t { MODE_NONE = 0, MODE_ENC, MODE_SEQ, MODE_KEY };

extern ModeBase modeBase;    // which family
extern uint8_t modeVariant;  // 0=none, 1..4

void initModes();                    // installs LED color hook
void pollModeLeds();                 // call every loop: pulse/blink animation
void modeOnEvent(uint8_t pad, uint8_t ev);  // EV_CLICKED / EV_HOLD / EV_DOUBLE
const char* modeName();              // "---", "ENC", "ENC2", "ENC3", "ENC4"...
// the OLED left-slot spelling of the same state: "ENCODER 1" / "SEQUENCER 2"
// / "CHANNEL SELECT" (variant 3) - whole words, never the internal code
const char* modeHeaderLabel();

// which KEY variant defines the keyboard behavior: the current one while
// in KEYMODE, the last latched KEY variant elsewhere (KEYMODE2 = chord
// mode is inherited by SEQMODE/ENCMODE)
uint8_t keyBehaviorVariant();

// per-channel KEYMODE memory: each keyboard channel recalls the KEY
// variant last latched on it (RAM only).  keySetChannel() calls
// keyModeRecall(); modeOnEvent() stores on every latched variant change
void keyModeRecall();
uint8_t keyModeMemOf(uint8_t ch);        // EEPROM accessors (1..16)
void    keyModeMemSet(uint8_t ch, uint8_t v);

// true while the keyboard sounds through the chord engine: KEYMODE2
// (diatonic chords) and KEYMODE4 (genre proposals) share it for the sound,
// the pending snapshot and the MONO voice - the latched variant elsewhere
// inherits the same way keyBehaviorVariant() does
bool keyChordMode();

// SEQMODE3 step-pad channel overlay: the 16 steps become the edit-channel
// selector while it is up.  Holding SEQ into SEQMODE3 brings it back; any
// encoder movement dismisses it for the rest of the hold so the pads show
// the sequencer again (playhead, recorded/empty, pattern length)
bool seq3ChSelActive();
void seq3ChSelEnter();               // SEQ held into SEQMODE3
bool seq3ChSelDismiss();         // encoder moved: true if it was up
// Is the step-pad CHANNEL PICKER up right now?  Variant 3 in all three
// families (momentary hold) AND plain ENCMODE, where the picker is the locked
// screen: entering ENCMODE puts the pads on channel select and they stay
// there until the mode changes, so the CC channel can be picked without
// holding three buttons at once (ENCMODE4 keeps the pads on the sequence).
// One predicate because four places must agree (the press handler, the LED
// row, the playhead and the header label)
bool chSelOverlay();// the keyboard's pads stay on the SEQ3 bar / verb row for the track a
// bar or verb was pressed on, in every mode, until KEYMODE3 picks the
// keyboard's own track again (seq3KbRelease)
bool seq3KbRole();
// ENCABLETONMODE: the latched NEXT overlay in the ENCMODE family.  It is not
// a fourth mode family (the three mode buttons only offer click/hold/double
// for variants 1-4), it is a screen of its own that the companion Ableton
// remote script feeds: pads = Live's clip slots, encoders = the selected
// device's parameters on CC64..71, PREV = the 16 tracks, M3 = the clip-action
// F row.  RAM only: it re-latches with a NEXT press after a power cycle
bool abltOn();
void abltSet(bool on);
void abltToggle();
bool abltPrevPick();          // PREV held: the pads are the track picker
void abltPrevHold(bool on);
bool abltActionSet();         // M3 set: F1-F4 are clip actions, not bank nav
void abltActionToggle();

void seq3KbLatch(uint8_t ch);
void seq3KbRelease();
