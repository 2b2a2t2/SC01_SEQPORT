#pragma once

#include <Arduino.h>

// drain input event queues each loop.
// capped at MAX_EVENTS_PER_LOOP per call so loop() can never be starved
// (queues are also fixed-size rings, so drains are bounded anyway).
#define MAX_EVENTS_PER_LOOP 8

void handleButtonEvents();
void handleEncoderEvents();

// ENCMODE: current CC value (0..127) of encoder 0..7 on the CURRENT
// encoder channel (values are stored per channel).  ENC1..4 always map
// CC17..20; ENC5..8 follow the F1..F4 bank radio inside the active M3
// page group (off = CC1..16, on = CC21..36) - see encBank()/encM3Page()
uint8_t encCcValue(uint8_t idx);

// CC number encoder 0..7 currently sends - the single source of truth
// for the MIDI send, the OLED labels and the DAW-feedback mapping
uint8_t encCcNum(uint8_t ei);

// ENCMODE F1..F4 radio: which CC block ENC5..ENC8 send (0..3 = F1..F4;
// RAM only, resets to F1 whenever the ENCMODE family is entered)
uint8_t encBank();
void    encBankSet(uint8_t b);
// recall the F bank + page group the current encoder channel last used
// (RAM-only per-channel memory; encBankSet/encM3PageSet store it)
void    encPageRecall();
// EEPROM accessors for the per-channel page memory (1..16)
uint8_t encBankMemOf(uint8_t ch);
void    encBankMemSet(uint8_t ch, uint8_t v);
bool    encM3MemOf(uint8_t ch);
void    encM3MemSet(uint8_t ch, bool on);

// ENCMODE M3 toggle: page group of the ENC5..8 banks (off = F1..F4 send
// CC1..16, on = CC21..36; RAM only, off again on ENCMODE entry)
bool encM3Page();
void encM3PageSet(bool on);

// incoming CC feedback from the DAW: syncs the ENCMODE knob positions
// (channel 1..16, CC 1..36); store only, never re-sent
void encCcIncoming(uint8_t ch, uint8_t cc, uint8_t val);

// ENCMODE4 + REC: persist the 576 knob values (36 CCs x 16 channels) +
// selected channel to EEPROM; encCcLoad() restores them at boot (before
// the first draw).  Save only - nothing is ever pushed to the DAW
void encCcSave();
void encCcLoad();

// Open / flush the settings store that both EEPROM blocks share (NVS on the
// SC01, where get()/put() are inert until begin() and lost until commit();
// no-ops on the Teensy, which writes on put()).  cfgStoreBegin() runs once in
// setup before the two loaders, cfgStoreCommit() at the end of each save.
void cfgStoreBegin();
void cfgStoreCommit();

// SEQMODE step-edit radio (variants 1/2/4): is the edit on, which target
// is selected (0=STEPS, 1=TIE, 2=GLIDE, 3=ACCENT, 4=PROB; POLY adds
// 5=FX on F1 and 6=TYPE on F2), and whether an attribute target is engaged
// for the current track (MONO = 1..4, POLY = 3/4/5/6, DRUM = ACCENT/PROB
// only).  The probability % itself is per track (ENC8 -> seqProbAdjust /
// seqProbPct in Keyboard.h)
bool    stepEditIsOn();
uint8_t stepEditSel();
bool    stepEditActive();
uint8_t stepFxSel();    // selected FX type for the FX target (StepFxType,
                        // 0..6 - which FX the pads/ENC7 edit)
const char* stepFxSelName();  // its display name (sus2 ... color)
// TYPE arm of the F2 target: which value a pad tap stamps onto the step
// (0..2 = STEP/STRUM/HAND; tap again clears back to inherit-the-track).
// ENC7 cycles it while target 6 is engaged, like ENC7 picks the FX type
void    stepTypeArmAdjust(int16_t counts);
uint8_t stepTypeArm();
const char* stepTypeArmName();   // "STEP" / "STRUM" / "HAND"

// HOLD F3 in SEQMODE (variants 1/2/4): ENC6 edits the track's ACCENT
// velocity instead of its default velocity (release restores ENC6)
void    f3AccentHold(bool on);
bool    f3AccentActive();

// SEQMODE M2 (variants 1/2/4) on a MONO+ARP edit track: a click toggles
// the ARP encoder page - ENC2 = span, ENC3 = skip rests, ENC4 = mode,
// ENC7 = insert mode; ENC1/5/6/8 keep their knobs.  The page only reads
// active while the panel is on SEQMODE and the track still wants an arp;
// a channel switch, a TYPE change or leaving SEQMODE dismisses it
void arpPageToggle();
void arpPageDismiss();
bool arpPageActive();

// ...and F1-F4 on that page arm one encoder SUB-PAGE each: F1 = order &
// shape, F2 = motion, F3 = generative, F4 = engine options.  A sub-page
// owns the whole 8-knob row - a slot is either one of the page's own
// parameters or an inherited plain-ARP binding (OCT/SPAN/SKIP/MODE/GATE/
// VEL/INST/PROB), so nothing becomes unreachable while it is armed.  The
// armed page is UI state only: it dies with the page (toggle/dismiss clear
// it), so a reopen always starts on the plain ARP row
uint8_t arpOptionSel();                  // 0..3, 0xFF = plain row
void    arpOptionSet(uint8_t sel);       // 0xFF = back to the plain row
const char *arpOptionName(uint8_t sel);  // header: "ARP" / "ARP ORD" ...
// The sub-page slot table: ARP_P_* for a knob that page owns, -1 for one
// that keeps its plain ARP binding.  Shared by the encoder dispatch, the
// label row and the value column (see Events.cpp)
int8_t arpSlotAt(uint8_t page, uint8_t enc);

// SEQMODE M2 (variants 1/2/4) on a POLY STRUM/HAND edit track: a click
// toggles the strum ADDS row - F1 = sus4, F2 = add6, F3 = add9 (each a
// toggle), F4 clears.  Same composite rule as the arp page: the row only
// reads active while the panel is on SEQMODE and the track still strums
void strumFRowToggle();
void strumFRowDismiss();
bool strumFRowActive();

// SEQMODE M2 (variants 1/2/4) on a MONO+BASS edit track: a click toggles
// the GNR8 encoder page - ENC2 = fill %, ENC3 = accent %, ENC4 = glide %,
// ENC7 = tie %; the F row = radio switches for the four option sub-pages
// (F1 OPTS = TONES/RNG/LOW/ALGO on ENC1-4, F2 = STEP OCT + MAP, F3 =
// BORROW, F4 = MOD PROB - tap a step first, the armed page dials it;
// M3 tap = generate / M3 hold = undo, PREV held = ENC1 rotates the
// pattern).  Composite active check like the arp page: a mid-hold
// channel / TYPE / mode switch drops the page by itself
void genPageToggle();
void genPageDismiss();
bool genPageActive();
// armed F radio of that page (0 = F1 opts, 1 = F2 oct, 2 = F3 brw,
// 3 = F4 prob, 0xFF = none); dies with the page, so Display/Modes only
// need this one accessor
uint8_t genOptionSel();
// step armed as the F2/F3/F4 dial target (tap = select), or -1 when
// nothing valid is picked - dies with the page
int8_t genStepSelStep();

// SEQMODE non-DRUM: the step currently held for the per-step TRANS overlay
// (ENC1), or -1 when none / the panel left SEQMODE.  EV_HOLD arms it,
// EV_RELEASED clears it (the composite check drops a stale hold)
int8_t seqTrStepSelGet();

// SEQMODE M2 (variants 1/2/4) on a MONO+LEAD edit track: a click toggles
// the LEAD encoder page - ENC2 = contour (ARCH/DESC/WALK), ENC3 = LOW,
// ENC4 = RANGE, ENC7 = density %; ENC1/5/6/8 keep OCT/GATE/VEL/PROB.
// M3 tap = generate the melody / M3 hold = undo, mirroring GNR8.
// Composite active check like the arp page: a mid-hold channel / TYPE /
// mode switch drops the page by itself
void leadPageToggle();
void leadPageDismiss();
bool leadPageActive();

// SEQMODE M2 (variants 1/2/4) on a POLY+HARM edit track: a click toggles
// the HARM encoder page - ENC1 = GENRE, ENC2 = FILL, ENC3 = SRC (bass/
// arp), ENC4 = HOLD.  M3 tap = bake the progression inferred from the
// FOLLOW feeder's bassline / M3 hold = undo, mirroring GNR8/LEAD.
// Composite active check like the other pages: a mid-hold channel / TYPE /
// mode switch drops it by itself
void harmPageToggle();
void harmPageDismiss();
bool harmPageActive();

// SEQMODE M2 (variants 1/2/4) on a POLY+STEP edit track: a click toggles
// the COMP encoder page - ENC1 = HITS (chord stabs per step, Euclid in 8
// slots, 0/1 = one block chord like before).  Same radio rules as HARM:
// opening drops the M1 edit radio, a channel / TYPE / mode switch drops it
void compPageToggle();
void compPageDismiss();
bool compPageActive();

// SEQMODE M2 (variants 1/2/4) on a POLY+SKANK edit track: a click toggles
// the SKANK encoder page - ENC1 = TES, ENC2 = MOT, ENC3 = SHP, ENC4 = THN,
// ENC5 = EXT, ENC6 = SUB (the derivation knobs of Skank.h), ENC7 = LEN,
// ENC8 = SPEED.  M1's radio and the other pages stay mutually exclusive
// with it; a channel / TRACK / TYPE / mode switch drops it like the rest
void skankPageToggle();
void skankPageDismiss();
bool skankPageActive();

// SEQMODE M2 (variants 1/2/4) on a DRUM edit track: a click toggles the DRUM
// pattern page (indigo pad).  The row is eight knobs - GENRE, VAR, FILL %,
// MOT %, then the KICK/SNARE/HAT/PERC line selectors - and the F row picks a
// knob SET that zooms the same four encoders (GROOVE / LINES / FILL% / MOT%);
// clicking the armed set again returns to the base row.  Every turn rebuilds
// the pattern live (Drum.cpp), M3 re-rolls and M3 hold undoes
void drumPageToggle();
void drumPageDismiss();
bool drumPageActive();
uint8_t drumSetSelGet();          // 0..3 = armed F set, 0xFF = base row
void drumSetSelSet(uint8_t s);

// GNR8 TIE span fill (MONO+BASS, TIE target only): HOLD a step to arm the
// span start, the next tap on another step stamps TIE across the span
// (called on target / mode / channel / track switches so a stale arm can
// never land on a later edit)
void stepChainDisarm();
bool stepChainArmed();
