#include "Events.h"
#include <EEPROM.h>
#include "UsbMidiShim.h"    // usbMIDI (Teensy core on the Teensy, facade on SC01)
#include "Buttons.h"
#include "Display.h"
#include "Modes.h"
#include "Encoders.h"
#include "Bindings.h"
#include "SeqClock.h"
#include "Keyboard.h"
#include "Chords.h"
#include "Key4.h"       // KEYMODE4 controls (focus/apply/undo/locks)
#include "Gen.h"        // GNR8 generator params / produce / rotate
#include "Skank.h"      // SKANK page: shape name for the header
#include "Settings.h"    // device settings page: the header tap owns the row
#include "Drum.h"       // DRUM page: genre/var/lane params + produce
#include "Link.h"       // ENCABLETONMODE bridge (clips, tracks, Live params)

// skip a log line if the USB TX buffer is congested (never block the loop —
// a blocked Serial.write stalls pollSeqClock and freezes the playhead)
static bool logRoom(int bytes) {
  return Serial.availableForWrite() >= bytes;
}

static void logSets() {
  if (!logRoom(128)) return;
  ModeBinding b = currentBinding();
  Serial.print("  [enc=");
  Serial.print(encSetName(b.enc));
  Serial.print(" ctrl=");
  Serial.print(ctrlSetName(b.ctrl));
  Serial.print(" step=");
  Serial.print(stepSetName(b.step));
  Serial.print(" key=");
  Serial.print(keySetName(b.key));
  Serial.println("]");
}

// PREV overlay (KEYMODE2) is held down - clear its slot on release
static bool prevOvActive = false;

// --- SEQMODE step-edit radio (all track types) -----------------------------
// M1 toggles the edit on/off, F1-F4 pick the target (0=STEPS, 1=TIE,
// 2=GLIDE, 3=ACCENT, 4=PROB; POLY adds 5=FX).  The state persists across
// mode switches; while ON, the step pads edit attributes instead of
// recording (STEPS target = normal recording) and ENC5/6/8 shape
// gate/velocity/probability.  DRUM/POLY take ACCENT/PROB from F3/F4 only,
// POLY takes FX from F1 (M1 keeps its part-select hold on DRUM).
// ENCMODE keeps its CCs, KEYMODE its keys - never there
static bool    stepEditOn = true;     // boot: pads record (STEPS)
static uint8_t stepEditTarget = 0;    // 0 = STEPS (normal step recording)
// which FX type the pads/ENC7 edit while target 5 is engaged (StepFxType)
static uint8_t stepFxType = SEQ_FX_SUS4;

static const char* stepFxName(uint8_t t) {
  static const char* n[7] = { "sus2", "sus4", "6", "m7",
                              "M7", "9", "color" };
  return n[t % 7];
}

// active in SEQMODE variants 1/2/4 while the edit track is MONO
static bool stepEditCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_MONO;
}

// DRUM/POLY variant: accent (F3) and probability (F4) only - TIE/GLIDE
// make no sense for one-shot drums or per-chord playback
static bool stepEditAccCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         (seqTrack() == SEQ_TRACK_DRUM || seqTrack() == SEQ_TRACK_POLY);
}

// true while an ATTRIBUTE target is engaged for the current track: MONO =
// TIE/GLIDE/ACCENT/PROB (1..4), POLY = ACCENT/PROB plus the FX target (5)
// and the TYPE target (6), EMB target (7), DRUM = ACCENT/PROB (3,4) plus the
// RATCHET target (8).  A target never
// matches a track that cannot edit it (a POLY FX/TYPE/EMB selection does not
// arm MONO/DRUM pads).  Drives the header label, the step pulse and the
// attribute pad mode
bool stepEditActive() {
  if (!stepEditOn || modeBase != MODE_SEQ || modeVariant == 3) return false;
  uint8_t trk = seqTrack();
  if (trk == SEQ_TRACK_MONO) return stepEditTarget >= 1 && stepEditTarget <= 4;
  if (trk == SEQ_TRACK_POLY)
    return stepEditTarget == 5 || stepEditTarget == 6 || stepEditTarget == 7 ||
           (stepEditTarget >= 3 && stepEditTarget <= 4);
  return (stepEditTarget >= 3 && stepEditTarget <= 4) ||
         stepEditTarget == 8;
}

static void stepEditSet(bool on, uint8_t target) {
  stepEditOn = on;
  stepEditTarget = target;
  stepChainDisarm();        // a target switch kills a TIE span arm
}

bool    stepEditIsOn() { return stepEditOn; }
uint8_t stepEditSel()  { return stepEditTarget; }
uint8_t stepFxSel()    { return stepFxType; }
const char* stepFxSelName() { return stepFxName(stepFxType); }

// which per-step TYPE the F2/arm flow stamps on a POLY track while the
// TYPE target is engaged (0..2 = SEQ_POLY_STEP / STRUM / HAND).  Stays
// across takes like the FX selection (both live in RAM only)
static uint8_t stepTypeArmV = SEQ_POLY_STEP;

void stepTypeArmAdjust(int16_t counts) {
  int t = (int)stepTypeArmV;
  int steps = counts > 0 ? (int)counts : -(int)counts;
  if (steps > 0) {
    if (counts > 0) t = (t + steps) % 3;
    else t = ((t - steps) % 3 + 3) % 3;
    stepTypeArmV = (uint8_t)t;
  }
}
uint8_t stepTypeArm() { return stepTypeArmV; }
const char* stepTypeArmName() {
  static const char *const n[3] = { "STEP", "STRUM", "HAND" };
  return n[stepTypeArmV % 3];
}

// which per-VOICE RATCHET count the F2/arm flow stamps on a DRUM track while
// the RATCHET target (8) is engaged: 2, 3 or 4 repeats inside the step,
// stamped on the step's SELECTED PART (a stamp clears back to a single hit).
// RAM only, like the FX / TYPE arms - it survives takes, not power
static uint8_t stepRatArmV = 2;

uint8_t stepRatArmSel() { return stepRatArmV; }

// 4 counts = 1 count step, clamped 2..4 (no wrap - the ends are the values,
// not a cycle point)
void stepRatArmAdjust(int16_t counts) {
  static int16_t ratRes = 0;
  ratRes += counts;
  int16_t st = 0;
  while (ratRes >= 4) { ratRes -= 4; st++; }
  while (ratRes <= -4) { ratRes += 4; st--; }
  if (!st) return;
  int v = (int)stepRatArmV + st;
  if (v < 2) v = 2;
  if (v > 4) v = 4;
  stepRatArmV = (uint8_t)v;
}

// --- SEQMODE M2 "ARP" page (variants 1/2/4) -------------------------------
// Clicking M2 rebinds the encoder row while the edit track is a MONO
// arp.  Like the other overlays the active check is a composite, so a
// mid-hold channel/TYPE/mode switch can never leave a stale page behind.
static bool arpPageOn = false;
// armed F sub-page (0..3, 0xFF = the plain 8-knob row).  Like the GNR8
// page's genOptSel this is UI state and dies with the page (toggle and
// dismiss both clear it), so a reopen always starts on the plain ARP row
static uint8_t arpOptSel = 0xFF;

static bool arpTrackCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_MONO && seqMonoType() == SEQ_MONO_ARP;
}

void arpPageToggle() { arpPageOn = !arpPageOn; arpOptSel = 0xFF; }
void arpPageDismiss() { arpPageOn = false; arpOptSel = 0xFF; }
bool arpPageActive()  { return arpPageOn && arpTrackCtx(); }
// the composite check already gates the armed page: a mid-hold channel /
// TRACK / TYPE / mode switch reads 0xFF here without needing a dismiss
uint8_t arpOptionSel() { return arpPageActive() ? arpOptSel : 0xFF; }
void arpOptionSet(uint8_t sel) { arpOptSel = (sel < 4) ? sel : 0xFF; }

// the page's sub-page names in full: F1 order & shape, F2 motion,
// F3 generative, F4 the armed engine's own options.  These are OLED
// labels, so no three-letter codes: "ARP ORDER", never "ARP ORD"
const char *arpOptionName(uint8_t sel) {
  static const char *const n[4] = { "ARP ORDER", "ARP MOTION",
                                    "ARP GENERATIVE", "ARP OPTIONS" };
  return (sel < 4) ? n[sel] : "ARP";
}

// Which ARP_P_* each sub-page puts on which encoder.  -1 keeps the plain
// ARP binding, so the base row's knobs stay reachable from inside every
// sub-page.  Encoder dispatch, the label row and the value column all
// read this one table, so a knob can never show one thing and move another
static const int8_t arpSlotT[4][8] = {
  // F1 ORDER & SHAPE
  { ARP_P_ORDER, ARP_P_STEP, ARP_P_PIVOT, ARP_P_WEIGHT, -1, -1, -1, -1 },
  // F2 MOTION
  { ARP_P_MUTATE, ARP_P_INSERT, ARP_P_CYCLE, ARP_P_MORPH, -1, -1, -1, -1 },
  // F3 GENERATIVE (ENC1 octave / ENC8 prob stay the base knobs)
  { -1, ARP_P_ENGINE, ARP_P_RATE, ARP_P_DEPTH, ARP_P_MEM, ARP_P_BIAS,
    ARP_P_SEED, -1 },
  // F4 ENGINE OPTS (the armed engine's own four, then target + clock)
  { ARP_P_OPT1, ARP_P_OPT2, ARP_P_OPT3, ARP_P_OPT4, ARP_P_TARGET,
    ARP_P_CLOCK, -1, -1 },
};

int8_t arpSlotAt(uint8_t page, uint8_t enc) {
  if (page > 3 || enc > 7) return -1;
  return arpSlotT[page][enc];
}

// --- SEQMODE M2 strum ADDS row (variants 1/2/4) ----------------------------
// Clicking M2 on a POLY STRUM/HAND edit track swaps the F row to the Le
// Strum ADDS colors: F1 = sus4, F2 = add6, F3 = add9 (independent RAM
// toggles), F4 clears all.  Composite active check like the arp page - a
// mid-hold track/TYPE/mode switch drops the row by itself
static bool strumFRowOn = false;

static bool strumFRowCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_POLY &&
         (seqPolyType() == SEQ_POLY_STRUM || seqPolyType() == SEQ_POLY_HAND);
}

void strumFRowToggle() { strumFRowOn = !strumFRowOn; }
void strumFRowDismiss() { strumFRowOn = false; }
bool strumFRowActive() { return strumFRowOn && strumFRowCtx(); }

// --- SEQMODE M2 "GNR8" page (variants 1/2/4) ------------------------------
// Clicking M2 on a MONO+BASS edit track rebinds ENC2/3/4/7 to the acid
// generator's knobs (fill/accent/glide/tie).  Like the arp page the
// active check is a composite, so a mid-hold channel/TYPE/mode switch can
// never leave a stale page behind.  ROOT/ARP/STRUM never take it
static bool genPageOn = false;
// armed F radio of that page (0..3, 0xFF = none): F1 (sel 0) owns the
// FULL four-knob options page (ENC1 TONES, ENC2 RANGE, ENC3 LOW, ENC4
// ALGO), F2 (sel 1) = STEP OCT (ENC2 of the tapped step + MAP mode on
// ENC3), F3 (sel 2) = BORROW (ENC3), F4 (sel 3) = MOD PROB (ENC4);
// it dies with the page - toggle and dismiss both clear it, so a reopen
// always starts with the plain GNR8 row
static uint8_t genOptSel = 0xFF;
// step armed as the dial target on the F2/F3/F4 mod pages (tap = select);
// dies with the page like genOptSel, validated on read
static int8_t genStepSel = -1;
// 4 encoder counts = 1 value on the mod dials (clamped, no wrap): one
// residual each for OCT / BRW / PROB (MAP keeps its own in seqMapAdjust)
static int16_t genModRes[3];

static bool genTrackCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_MONO && seqMonoType() == SEQ_MONO_BASS;
}

// SEQMODE non-DRUM: a held step pad arms the per-step TRANS overlay (ENC1)
// for that step; release closes it.  Kept separate from the GNR8-page
// genStepSel (that one lives only while the GNR8 F-radio sub-page is up)
static int8_t seqTrStepSel = -1;

void genPageToggle() {
  genPageOn = !genPageOn;
  genOptSel = 0xFF;
  genStepSel = -1;
}
void genPageDismiss() {
  genPageOn = false;
  genOptSel = 0xFF;
  genStepSel = -1;
}
bool genPageActive()  { return genPageOn && genTrackCtx(); }
uint8_t genOptionSel() { return genPageActive() ? genOptSel : 0xFF; }

// --- SEQMODE M2 "LEAD" page (variants 1/2/4) -------------------------------
// Clicking M2 on a MONO+LEAD edit track rebinds ENC2/3/4/7 to the melody
// generator's knobs (contour/low/range/density).  Like the ARP/GNR8 pages
// the active check is composite, so a mid-hold channel/TYPE/mode switch
// can never leave a stale page behind.  ROOT/BASS/ARP/STRUM never take it.
static bool leadPageOn = false;

static bool leadTrackCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_MONO && seqMonoType() == SEQ_MONO_LEAD;
}

void leadPageToggle() { leadPageOn = !leadPageOn; }
void leadPageDismiss() { leadPageOn = false; }
bool leadPageActive()  { return leadPageOn && leadTrackCtx(); }

// --- SEQMODE M2 "HARM" page (variants 1/2/4) --------------------------------
// Clicking M2 on a POLY+HARM edit track rebinds the encoder row to the
// harmonizer: ENC1 = GENRE (the harmonic grammar), ENC2 = FILL (density
// gate of the baked triads), ENC3 = SRC (bass/arp source), ENC4 = HOLD
// (force hold on a repeated bass).  M3 bakes the progression inferred from
// the FOLLOW feeder's bassline into the track's steps, M3 hold undoes it.
// Composite active check like the other pages.
static bool harmPageOn = false;

static bool harmTrackCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_POLY && seqPolyType() == SEQ_POLY_HARM;
}

void harmPageToggle() { harmPageOn = !harmPageOn; }
void harmPageDismiss() { harmPageOn = false; }
bool harmPageActive()  { return harmPageOn && harmTrackCtx(); }

// --- SEQMODE M2 "COMP" page (variants 1/2/4) --------------------------------
// Clicking M2 on a POLY+STEP edit track rebinds the encoder row to the
// comping: ENC1 = HITS (chord stabs per step on the Euclid on-positions of
// 8 slots; 0 or 1 = today's single block chord).  ENC2..4 stay free
// (rotation / per-stab gate are reserved).  Composite active check like
// the other pages - a mid-turn channel / TYPE / mode switch drops it
static bool compPageOn = false;

static bool compTrackCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_POLY && seqPolyType() == SEQ_POLY_STEP;
}

void compPageToggle() { compPageOn = !compPageOn; }
void compPageDismiss() { compPageOn = false; }
bool compPageActive()  { return compPageOn && compTrackCtx(); }

// --- SEQMODE M2 "SKANK" page (variants 1/2/4) ------------------------------
// Clicking M2 on a POLY+SKANK edit track rebinds the encoder row to the
// derivation: ENC1 = TES (tessitura anchor), ENC2 = MOT (motion), ENC3 =
// SHP (shape), ENC4 = THN (thinning), ENC5 = EXT (extension %), ENC6 =
// SUB (substitution %), ENC7 = LEN, ENC8 = SPEED - the last two mirror
// SEQMODE3's pattern knobs so the walk fits the track without a mode hop.
// Composite active check like the other pages - a mid-turn channel /
// TYPE / mode switch drops it
static bool skankPageOn = false;

static bool skankTrackCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_POLY && seqPolyType() == SEQ_POLY_SKANK;
}

void skankPageToggle() { skankPageOn = !skankPageOn; }
void skankPageDismiss() { skankPageOn = false; }
bool skankPageActive()  { return skankPageOn && skankTrackCtx(); }

// --- SEQMODE M2 "DRUM" page (variants 1/2/4) --------------------------------
// Clicking M2 on a DRUM edit track opens the pattern page (indigo pad): the
// base row is eight knobs - ENC1 GENRE, ENC2 VAR, ENC3 FILL %, ENC4 MOT %,
// ENC5..8 the KICK/SNARE/HAT/PERC line selectors - and the F row picks one
// of four knob SETS that zoom the same four encoders: GROOVE (genre/var/
// fill/mot), LINES, FILL% and MOT% per lane.  Clicking the armed set again
// returns to the base row.  Every turn rebuilds the pattern live through
// Drum.cpp; M3 re-rolls, M3 hold undoes.  Composite active check like the
// other pages - a mid-turn channel / TRACK / mode switch drops it
static bool drumPageOn = false;
static uint8_t drumSetSel = 0xFF;      // 0..3 = armed F set, 0xFF = base row

static bool drumTrackCtx() {
  return modeBase == MODE_SEQ && modeVariant != 3 &&
         seqTrack() == SEQ_TRACK_DRUM;
}

void drumPageToggle() { drumPageOn = !drumPageOn; drumSetSel = 0xFF; }
void drumPageDismiss() { drumPageOn = false; drumSetSel = 0xFF; }
bool drumPageActive()  { return drumPageOn && drumTrackCtx(); }
uint8_t drumSetSelGet() { return drumPageActive() ? drumSetSel : 0xFF; }
void drumSetSelSet(uint8_t s) { drumSetSel = (s <= 3) ? s : 0xFF; }

// selected step for the mod dials (F2/F3/F4), or -1: nothing picked yet,
// or the pattern shrank / the step lost its content under the selection
int8_t genStepSelStep() {
  if (!genPageActive() || genStepSel < 0) return -1;
  uint8_t st = (uint8_t)genStepSel;
  if (st >= seqPatLenOf((uint8_t)(seqEditChannel() - 1)) || !stepIsActive(st)) {
    genStepSel = -1;
    return -1;
  }
  return genStepSel;
}

// the held step for the SEQMODE per-step TRANS overlay; a mid-hold mode
// switch drops it by itself (composite check like the page accessors)
int8_t seqTrStepSelGet() {
  if (seqTrStepSel < 0 || modeBase != MODE_SEQ) return -1;
  return seqTrStepSel;
}

// 4 counts = 1 value step of dial i (0 = OCT, 1 = BRW, 2 = PROB)
static int16_t genModStep(uint8_t i, int16_t counts) {
  genModRes[i] += counts;
  int16_t s = 0;
  while (genModRes[i] >= 4) { genModRes[i] -= 4; s++; }
  while (genModRes[i] <= -4) { genModRes[i] += 4; s--; }
  return s;
}

// header line for an armed option (F1..F4 press) or a sub-page dial
// change.  On the F1 page sel names the knob that moved (0 TONES /
// 1 LOW / 2 RANGE / 3 ALGO): "TONES all" / "LOW C1" / "RANGE 2oct" /
// "ALGO CLASSIC".  On the mod pages sel is the armed radio itself:
// "step 5 oct +1" / "step 5 brw 3" / "step 5 prob 60%" (nothing picked:
// "step oct" / "step brw" / "step prob" - the dial itself flashes
// "pick step" instead)
static void genOptionHeader(uint8_t sel) {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  char hb[32];
  if (genOptSel == 0) {
    if (sel == 0)
      snprintf(hb, sizeof(hb), "Tones %s", seqGenTonesName(seqGenTonesOf(ch)));
    else if (sel == 1)
      snprintf(hb, sizeof(hb), "Low %s", seqGenLowName(seqGenLowOf(ch)));
    else if (sel == 2)
      snprintf(hb, sizeof(hb), "Range %u oct", seqGenRangeOf(ch));
    else
      snprintf(hb, sizeof(hb), "Algo %s", seqGenAlgoName(seqGenAlgoOf(ch)));
  } else if (sel == 1) {
    int8_t ss = genStepSelStep();
    if (ss < 0)
      snprintf(hb, sizeof(hb), "Pick a Step");
    else if (stepOctAt((uint8_t)ss))
      snprintf(hb, sizeof(hb), "Step %u Octave %+d", ss + 1,
               (int)stepOctAt((uint8_t)ss));
    else
      snprintf(hb, sizeof(hb), "Step %u Octave 0", ss + 1);
  } else if (sel == 2) {
    int8_t ss = genStepSelStep();
    if (ss < 0)
      snprintf(hb, sizeof(hb), "Pick a Step");
    else if (stepBrwAt((uint8_t)ss) == 0)
      snprintf(hb, sizeof(hb), "Step %u Borrow off", ss + 1);
    else
      snprintf(hb, sizeof(hb), "Step %u Borrow %u", ss + 1,
               stepBrwAt((uint8_t)ss));
  } else {
    int8_t ss = genStepSelStep();
    if (ss < 0)
      snprintf(hb, sizeof(hb), "Pick a Step");
    else
      snprintf(hb, sizeof(hb), "Step %u Mod Prob %u%%", ss + 1,
               stepModPAt((uint8_t)ss));
  }
  headerShow(hb);
  if (logRoom(48)) {
    Serial.print("  ");
    Serial.println(hb);
  }
}

// --- GNR8 TIE span fill (hold-arm gesture) --------------------------------
// With the step-edit radio on TIE on the GNR8 track (MONO+BASS), HOLD a
// step to arm the span start (header "chain from N" - the touch-down tap
// of that same press is reverted first, so holding never changes the
// flag); the next tap on ANOTHER step stamps TIE across start..end-1
// (wrapping inside the pattern length - the span rings over rests).  Tap
// the armed start itself = cancel; 5 s without execution disarms.
// Plain taps NEVER arm: TIE and the other attributes (GLIDE/ACCENT/PROB
// never span at all) stay ordinary per-step toggles, like in every other
// track type.
static bool     chainArmed = false;
static uint8_t  chainStart = 0;
static uint32_t chainArmAt = 0;
// the step this press toggled (0 = none, else step+1): a HOLD arriving on
// the same press reverts it, so hold-to-arm leaves the flag untouched
static uint8_t  pressToggled = 0;

void stepChainDisarm() { chainArmed = false; }
bool stepChainArmed()  { return chainArmed && genTrackCtx() &&
                                 stepEditTarget == 1; }

// ACCENT velocity of the edit track in SEQMODE: ENC7 in variants 1/2/4, for
// EVERY track type (the F3 hold that used to borrow ENC6 is gone - one knob,
// one job).  Composite, like every binding here: the POLY step-edit arms
// (FX / TYPE / EMB) own ENC7 while those targets are engaged, and the M2
// pages that bind ENC7 (GNR8 TIE, ARP INSERT, LEAD DENSITY, SKANK TES%) sit
// earlier in the encoder chain, so they win the slot whenever they are open
bool seqAccentEncCtx() {
  if (modeBase != MODE_SEQ || modeVariant == 3) return false;
  if (stepEditOn && seqTrack() == SEQ_TRACK_POLY &&
      stepEditTarget >= 5 && stepEditTarget <= 7) return false;
  return true;
}

// a SEQ channel switch always lands back in step edit on the new channel:
// M1 (STEPS target) selected, radio on, and every page overlay of the OLD
// track dismissed (the new track may be any type)
static void seqEditChannelLand(uint8_t ch) {
  seqSetEditChannel(ch);
  stepEditSet(true, 0);
  arpPageDismiss();       // the new track may not be an arp
  strumFRowDismiss();     // ...and it may not strum either
  genPageDismiss();       // nor a BASS track for the GNR8 page
  leadPageDismiss();      // nor a LEAD track for the LEAD page
  harmPageDismiss();      // nor a HARM track for the HARM page
  compPageDismiss();      // nor a POLY/STEP track for the COMP page
  skankPageDismiss();     // nor a POLY/SKANK track for the SKANK page
  drumPageDismiss();      // nor a DRUM track for the drum pattern page
  stepChainDisarm();      // ...nor an armed chain span (old ch)
  refreshAllLeds();       // M1 + F-row follow the radio again
  refreshMainScreen();
  refreshHeader();
}

// KEYMODE3 ENC3/ENC4 channel link (ENCOD / KBOARD, free or locked): the
// SEQ edit channel is the hub.  A LOCKED family follows every hub change;
// a change in a locked family drives the hub, and the other locked family
// follows transitively.  Free families stay independent - exactly the
// pre-link behavior.  Only the variant-3 channel overlays call this
// (EEPROM loads / internal focus swaps must never propagate)
static void channelLinkPropagate(uint8_t from) {  // 0 = KEY, 1 = ENC, 2 = SEQ
  static bool busy;
  if (busy) return;
  bool kL = chanKeyLocked(), eL = chanEncLocked();
  if (!kL && !eL) return;                 // both free: nothing to do
  if (from == 0 && !kL) return;           // a free family moved: local only
  if (from == 1 && !eL) return;
  busy = true;
  uint8_t ch = (from == 0) ? keyChannel()
             : (from == 1) ? encChannel()
                           : seqEditChannel();   // already 1-based
  if (from != 2 && ch != seqEditChannel())
    seqEditChannelLand(ch);               // the hub takes the new channel
  if (from != 1 && eL) encSetChannel(ch);
  if (from != 0 && kL) keySetChannel(ch);
  if (logRoom(48)) {
    Serial.print("  channel link -> ");
    Serial.println(ch);
  }
  busy = false;
}

void handleButtonEvents() {
  uint8_t pad, ev;
  for (uint8_t n = 0; n < MAX_EVENTS_PER_LOOP && popEvent(pad, ev); n++) {
    if (logRoom(48)) {
      Serial.print(buttonName(pad));
      Serial.print(" ");
      Serial.println(eventName(ev));
    }

    if (ev == EV_PRESSED) headerShow(buttonName(pad));
    if (ev == EV_CLICKED || ev == EV_HOLD || ev == EV_DOUBLE || ev == EV_RELEASED)
      modeOnEvent(pad, ev);

    // pad classification (LED numbers): keyboard chromatic group, step group
    int8_t kl = ledForPad[pad];
    uint8_t kidx = 255, sidx = 255;
    for (uint8_t i = 0; i < KEYBOARD_NUM_G; i++)
      if (groupKeyboard[i] == kl) kidx = i;
    for (uint8_t i = 0; i < STEPS_NUM_G; i++)
      if (groupSteps[i] == kl) sidx = i;

    // step pads (all modes, behavior owned by SEQMODE):
    //  press   -> trigger the step with its chord (or the last keyboard chord)
    //  hold    -> assign mode: play keys to replace this step's chord
    //  release -> note-off
    if (sidx != 255) {
      // channel overlays: steps select a MIDI channel
      // (KEY = keyboard output, SEQ = sequencer edit, ENC = encoder CCs).
      // SEQ3's selector is gone once an encoder moved - the pads are back
      // on the sequencer until SEQ is held into SEQMODE3 again.  Plain
      // ENCMODE is the locked form: the picker is simply what that variant
      // is, and ENCMODE4 hands the pads back so the sequence stays visible
      bool chOverlay = chSelOverlay();
      // DRUM track: step press = toggle the selected part (voice)
      bool drum = (modeBase == MODE_SEQ && modeVariant != 3 &&
                   seqTrack() == SEQ_TRACK_DRUM);
      // hold M1 = part-select overlay: steps choose the drum voice
      bool partSel = drum && drumPartSelectActive();
      // hold M2 in ENC mode = channel-mute overlay: steps toggle channel mutes
      bool encMute = encMuteActive();
      // hold M1 in ENC mode = drum-voice mute overlay: steps toggle the
      // muted drum voices (note 36+N) across all drum-track sequences
      bool drumVoice = drumVoiceMuteActive();
      // attribute mode: an attribute target is engaged - presses toggle it
      // on ACTIVE steps (empty steps stay inert)
      bool attrMode = stepEditActive();
      // edit off: the step pads are inert on a MONO track (no accidental
      // recording; release still runs for defensive cleanup)
      bool editLocked = stepEditCtx() && !stepEditOn;
      // pads address the edit WINDOW (one bar of 16): map pad -> absolute
      // step of the sequence (overlays above keep using the raw pad index)
      uint8_t stp = (uint8_t)(sidx + seqStepViewBase());
      // ENCABLETONMODE: the pads are Live's 16 clip slots of the selected
      // track (PREV held: the 16 tracks).  Empty slot = create + fill from
      // that channel's pattern, filled = launch, playing = stop, double =
      // re-fill.  The script answers with CLIP_STATE either way
      bool abltPads = (modeBase == MODE_ENC && abltOn() && modeVariant != 3);
      if (abltPads) {
        if (abltPrevPick()) {
          if (ev == EV_PRESSED) {
            encSetChannel((uint8_t)(sidx + 1));
            linkTrackSelect(sidx);
            char tb[20];
            snprintf(tb, sizeof(tb), "Track %u", sidx + 1);
            headerShow(tb);
            refreshMainScreen();
            if (logRoom(48)) {
              Serial.print("  live track ");
              Serial.println(sidx + 1);
            }
          }
        } else {
          uint8_t lbits = linkClipBits(sidx);
          if (ev == EV_PRESSED) {
            if (!linkAlive()) {
              headerShow("Link Down Check MIDI");
            } else if (lbits & 0x02) {
              linkClipAct(sidx, 1);
              headerShow("Clip Stopped");
            } else if (lbits & 0x01) {
              linkClipAct(sidx, 0);
              headerShow("Clip Launched");
            } else {
              linkClipAct(sidx, 2);
              linkDumpTrack((uint8_t)(encChannel() - 1));
              headerShow("Clip Filled");
            }
          } else if (ev == EV_DOUBLE && (lbits & 0x01)) {
            linkClipAct(sidx, 2);
            linkDumpTrack((uint8_t)(encChannel() - 1));
            headerShow("Clip Refilled");
          }
          if (logRoom(48)) {
            Serial.print("  live clip row ");
            Serial.print(sidx);
            Serial.print(" state ");
            Serial.println(lbits);
          }
        }
      } else if (encMute) {
        if (ev == EV_PRESSED) {
          seqMuteToggle(sidx);
          char mb[24];
          snprintf(mb, sizeof(mb), "Channel %u %s", sidx + 1,
                   seqIsMuted(sidx) ? "muted" : "on");
          headerShow(mb);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(mb);
          }
        }
      } else if (drumVoice) {
        if (ev == EV_PRESSED) {
          drumVoiceMuteToggle(36 + sidx);
          char db[26];
          snprintf(db, sizeof(db), "%s %s", drumVoiceName(36 + sidx),
                   drumVoiceMuted(36 + sidx) ? "muted" : "on");
          headerShow(db);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(db);
          }
        }
      } else if (chOverlay) {
        if (ev == EV_PRESSED) {
          // each press moves its OWN family first, then the KEYMODE3
          // channel link (ENCOD/KBOARD locked) propagates the change to
          // the hub and the other locked families
          if (modeBase == MODE_KEY) {
            keySetChannel(sidx + 1);
            seq3KbRelease();     // the keyboard's track was picked: instrument
            channelLinkPropagate(0);
          } else if (modeBase == MODE_SEQ) {
            seqEditChannelLand(sidx + 1);
            channelLinkPropagate(2);
          } else {
            encSetChannel(sidx + 1);
            linkTrackSelect(sidx);   // ENCABLETONMODE: mirror into Live
            channelLinkPropagate(1);
          }
          if (modeBase == MODE_ENC)
            refreshMainScreen();   // CC values belong to the new channel
          // the pad row IS the channel picker while variant 3 is held, so
          // the strip names the channel - the press's own "Step 7" label
          // would be the wrong thing to leave up
          char cb[20];
          snprintf(cb, sizeof(cb), "Channel %u", sidx + 1);
          headerShow(cb);
          if (logRoom(48)) {
            Serial.print(modeBase == MODE_KEY ? "  kbd channel " :
                         modeBase == MODE_SEQ ? "  seq channel " :
                                                "  enc channel ");
            Serial.println(modeBase == MODE_KEY ? keyChannel() :
                            modeBase == MODE_SEQ ? seqEditChannel() :
                                                   encChannel());
          }
        } else if (ev == EV_RELEASED) {
          stepAssignSet(-1);   // defensive: never leave assign mode armed
        }
      } else if (partSel) {
        if (ev == EV_PRESSED) {
          drumPartSet(sidx);
          headerAnnounce("Drum Part", drumPartName());
          if (logRoom(48)) {
            Serial.print("  part ");
            Serial.println(drumPartName());
          }
        }
      } else if (!seqStepUsed(stp) && (ev == EV_PRESSED || ev == EV_CLICKED ||
                                       ev == EV_HOLD)) {
        // past the pattern length: the pad is dark AND inert - no record,
        // no toggle, no assign, no attribute edit (release still falls
        // through to the defensive cleanup below)
        if (ev == EV_PRESSED && logRoom(48)) Serial.println("  step unused");
      } else if (attrMode && (ev == EV_PRESSED || ev == EV_CLICKED ||
                              ev == EV_HOLD)) {
        // attribute edit: a tap on an ACTIVE step toggles the selected
        // attribute; empty steps and clicks do nothing here.  HOLD (GNR8
        // track, TIE target only) arms the span fill: the touch-down
        // toggle of this same press is reverted first, the header
        // announces the start, and the next tap on ANOTHER step stamps
        // TIE across start..end-1 (wrapping in the pattern length - the
        // span rings over rests).  Tap the armed start = cancel, 5 s
        // disarms; every plain tap just toggles, GLIDE/ACCENT/PROB never
        // span at all
        static const uint8_t attOf[4] = { SEQ_ATT_TIE, SEQ_ATT_GLIDE,
                                          SEQ_ATT_ACCENT, SEQ_ATT_PROB };
        static const char *const nmOf[4] = { "Tie", "Glide", "Accent",
                                             "Probability" };
        bool chainCtx = genTrackCtx() && stepEditTarget == 1;  // TIE only
        char ab[24];
        // a forgotten arm expires after 5 s (plain toggles again)
        if (chainArmed && (uint32_t)(millis() - chainArmAt) >= 5000u)
          chainArmed = false;
        if (ev == EV_PRESSED) pressToggled = 0;   // fresh for this press
        if (ev == EV_HOLD && chainCtx) {
          // hold = arm the span start; undo this press's touch-down
          // toggle first, so holding never changes the flag itself
          if (pressToggled == (uint8_t)(stp + 1))
            stepAttrToggle(stp, SEQ_ATT_TIE);
          pressToggled = 0;
          chainArmed = true;
          chainStart = stp;
          chainArmAt = millis();
          snprintf(ab, sizeof(ab), "Chain From Step %u", stp + 1);
          headerShow(ab);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(ab);
          }
        } else if (ev == EV_PRESSED && chainCtx && chainArmed &&
                   stp == chainStart) {
          // tap the armed start itself = cancel the span
          chainArmed = false;
          headerShow("Chain Cleared");
          if (logRoom(48)) Serial.println("  chain off");
        } else if (ev == EV_PRESSED && chainCtx && chainArmed &&
            stp != chainStart) {
          uint8_t att = attOf[stepEditTarget - 1];
          uint8_t n = stepAttrChain(chainStart, stp, att);
          chainArmed = false;
          if (n)
            snprintf(ab, sizeof(ab), "%s %u Steps",
                     nmOf[stepEditTarget - 1], n);
          else
            snprintf(ab, sizeof(ab), "Chain Empty");
          headerShow(ab);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(ab);
          }
          refreshAllLeds();   // the F-row blink mask follows the new flags
        } else if (ev == EV_PRESSED && stepIsActive(stp)) {
          bool on = false;
          if (stepEditTarget == 5) {
            // POLY FX target: the pad arms the SELECTED FX type on it
            on = stepFxToggle(stp, stepFxType);
            snprintf(ab, sizeof(ab), "Step %u FX %s %s", stp + 1,
                     stepFxName(stepFxType), on ? "on" : "off");
          } else if (stepEditTarget == 6) {
            // POLY TYPE target: the pad stamps the ARMED override on it
            // (a second tap clears back to inherit-the-track)
            on = stepTypeToggle(stp, stepTypeArmV);
            if (on)
              snprintf(ab, sizeof(ab), "Step %u Type %s on", stp + 1,
                       stepTypeArmName());
            else
              snprintf(ab, sizeof(ab), "Step %u Type inherit", stp + 1);
          } else if (stepEditTarget == 7) {
            // POLY EMB target: cycle through SUB/INV/PAS/COL for this step
            // ENC7 selects which param to edit, tap cycles the value
            on = stepEmbToggle(stp);
            snprintf(ab, sizeof(ab), "Step %u Embed", stp + 1);
          } else if (stepEditTarget == 8) {
            // DRUM RATCHET target: stamp the ENC6-armed count (2/3/4) on the
            // SELECTED PART of the step - every voice keeps its own count, so
            // a stamped hat rolls while the kick / snare beside it play
            // straight.  Tapping a part that already carries that count
            // clears it back to a single hit
            on = stepRatchetStamp(stp, stepRatArmSel());
            char rn[8];
            if (on) snprintf(rn, sizeof(rn), "%u", stepRatArmSel());
            else    snprintf(rn, sizeof(rn), "off");
            snprintf(ab, sizeof(ab), "%s Ratchet %s", drumPartName(), rn);
          } else {
            uint8_t att = attOf[stepEditTarget - 1];
            on = stepAttrToggle(stp, att);
            if (seqTrack() == SEQ_TRACK_DRUM) {
              // the pressed pad already names the STEP, so the drum form
              // leads with the PART: "Snare Accent on"
              snprintf(ab, sizeof(ab), "%s %s %s", drumPartName(),
                       att == SEQ_ATT_ACCENT ? "Accent" : "Probability",
                       on ? "on" : "off");
            } else {
              snprintf(ab, sizeof(ab), "Step %u %s %s", stp + 1,
                       nmOf[stepEditTarget - 1], on ? "on" : "off");
            }
          }
          headerShow(ab);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(ab);
          }
          if (chainCtx) pressToggled = (uint8_t)(stp + 1);
          // (a HOLD arriving on this same press reverts that toggle and
          // arms the span from here instead)
        }
      } else if (genPageActive() && genOptSel >= 1 && genOptSel <= 3 &&
                 ev == EV_PRESSED && stepIsActive(stp)) {
        // GNR8 mod pages (F2 OCT / F3 BRW / F4 PROB): a tap on an ACTIVE
        // step selects it as the dial target (header shows its value,
        // the pad lights up); empty steps fall through to editLocked and
        // stay inert.  The selection dies with the page - F1 / none-armed
        // taps never record here (editLocked catches those below)
        genStepSel = (int8_t)stp;
        genOptionHeader(genOptSel);
        if (logRoom(48)) {
          Serial.print("  gen step ");
          Serial.println(stp + 1);
        }
        refreshAllLeds();
      } else if (editLocked && (ev == EV_PRESSED || ev == EV_CLICKED ||
                                ev == EV_HOLD)) {
        // edit off: pads inert (no record, no assign mode, no disable)
        if (ev == EV_PRESSED && logRoom(48))
          Serial.println("  step edit off");
      } else if (ev == EV_PRESSED) {
        // KEYMODE4: a step pad press moves the write target there first,
        // then the usual trigger/assign behavior runs on it
        if (modeBase == MODE_KEY && modeVariant == 4) k4FocusSet(stp);
        if (drum) {
          int r = stepDrumToggle(stp);
          if (r == -2 && logRoom(48)) {
            Serial.println("  part off");
          } else if (r >= 0 && logRoom(48)) {
            Serial.print("  part on ");
            Serial.println(drumPartName());
          }
        } else {
          int n = stepChordPress(stp);
          if (n > 0 && logRoom(48)) {
            Serial.print("  step record notes=");
            Serial.println(n);
          } else if (n == 0) {
            // refused: the header names WHY (a follower's feeder not fed yet,
            // or nothing held to record) - a dark pad plus a silent header
            // reads like a dead step
            const char *w = stepRecordWarn();
            if (w && w[0]) {
              headerShow(w);
              if (logRoom(48)) {
                Serial.print("  ");
                Serial.println(w);
              }
            }
          }
        }
      } else if (ev == EV_CLICKED) {
        // KEYMODE4 AUDITION: a tap never disables a step - disable only
        // lands in WRITE (AUDITION promises nothing is written here)
        bool k4 = (modeBase == MODE_KEY && modeVariant == 4);
        if (k4 && !k4Write()) {
          if (logRoom(48)) Serial.println("  k4 audition: step kept");
        } else if (!drum && stepChordClick(stp) == 0 && logRoom(48))
          Serial.println("  step disabled");
      } else if (ev == EV_HOLD) {
        if (modeBase == MODE_KEY && modeVariant == 4) {
          // KEYMODE4: holding a step swaps ENC1..ENC4 onto OCT/DEG/INV/
          // VOICE for THAT step (release restores the mode row, or the EMB
          // menu if it was open); WRITE keeps the assign gesture on top
          k4StepSelSet((int8_t)stp);
          setOverlay(G_ENC, ENC_KEY4_STEP);
          if (k4Write()) stepAssignSet((int8_t)stp);
          refreshMainScreen();    // the encoder labels are the overlay's now
          char sb[24];
          snprintf(sb, sizeof(sb), "Step %u", stp + 1);
          headerShow(sb);
          if (logRoom(48)) {
            Serial.print("  step hold ");
            Serial.println(stp + 1);
          }
        } else if (!drum) {       // DRUM steps are toggles, no assign mode
          stepAssignSet((int8_t)stp);
          // SEQMODE: the hold ALSO swaps ENC1 onto this step's TRANS
          // offset (the other encoders keep their idle row; DRUM tracks
          // never take it - their steps are voice toggles)
          if (modeBase == MODE_SEQ) {
            seqTrStepSel = (int8_t)stp;
            // a POLY+SKANK track swaps the row onto its LOCK knobs instead
            // (encoders 1..6 = per-step derivation overrides, 8 = TRANS)
            bool skankRow = modeVariant != 3 &&
                            seqTrack() == SEQ_TRACK_POLY &&
                            seqPolyType() == SEQ_POLY_SKANK;
            setOverlay(G_ENC, skankRow ? ENC_SEQ_SKANK_STEP
                                       : ENC_SEQ_STEPOV);
            refreshMainScreen();  // the encoder labels are the overlay's
            char tb[24];
            if (skankRow) {
              snprintf(tb, sizeof(tb), "Step %u Locks", stp + 1);
            } else {
              int8_t tv = stepTrAt(stp);
              if (tv >= 0)
                snprintf(tb, sizeof(tb), "Step %u Transpose +%d", stp + 1, tv);
              else
                snprintf(tb, sizeof(tb), "Step %u Transpose %d", stp + 1, tv);
            }
            headerShow(tb);
            if (logRoom(48)) {
              Serial.print("  ");
              Serial.println(tb);
            }
          } else if (logRoom(48)) {
            Serial.print("  step assign ");
            Serial.println(stp + 1);
          }
        }
      } else if (ev == EV_RELEASED) {
        if (k4StepSelGet() >= 0) {
          // end the per-step row (never clobber an overlay someone else
          // took over in the meantime, e.g. PREV pressed mid-hold)
          k4StepSelSet(-1);
          if (currentBinding().enc == ENC_KEY4_STEP) {
            clearOverlay(G_ENC);
            if (k4EmbMenu()) setOverlay(G_ENC, ENC_KEY4_EMB);
            else if (k4Tonality())
              setOverlay(G_ENC, (uint8_t)(ENC_KEY4_MENU + k4TonaBank()));
          }
          refreshMainScreen();    // labels back to the mode's own row
        }
        if (seqTrStepSel >= 0) {
          // SEQMODE step-trans row lets go (same never-clobber guard);
          // both held-step rows (plain TRANS, SKANK locks) share the sel
          seqTrStepSel = -1;
          if (currentBinding().enc == ENC_SEQ_STEPOV ||
              currentBinding().enc == ENC_SEQ_SKANK_STEP)
            clearOverlay(G_ENC);
          // a bar pad may still be down underneath (SEQ 3): the step row
          // step aside, so the bar's OCT/TRANS comes back instead of
          // leaving the encoder row dead until the next bar press
          if (seqBarEditSel() < 4) setOverlay(G_ENC, ENC_SEQ_BAR_OV);
          refreshMainScreen();
        }
        stepAssignSet(-1);
      }
      refreshAllLeds();   // recording may have toggled the step's dim-green LED
    }

    // PART SELECT (DRUM track): press M1 -> orange overlay on the steps to
    // pick which drum voice the steps sequence (release closes it; the
    // overlay comes up on press so it appears immediately, no 300ms wait).
    // ENCMODE: press M1 instead opens the drum-VOICE mute overlay
    if (ledForPad[pad] == groupControls[2]) {
      if (ev == EV_PRESSED && modeBase == MODE_SEQ && modeVariant != 3 &&
          seqTrack() == SEQ_TRACK_DRUM) {
        drumPartSelectHold(true);
        headerShow("Drum Part Select");
        if (logRoom(48)) {
          Serial.print("  part select (");
          Serial.print(drumPartName());
          Serial.println(")");
        }
        refreshAllLeds();
      } else if (ev == EV_RELEASED && drumPartSelectActive()) {
        drumPartSelectHold(false);
        refreshAllLeds();
      } else if (ev == EV_PRESSED && modeBase == MODE_ENC) {
        drumMuteHold(true);
        headerShow("Drum Voice Mute");
        if (logRoom(48)) Serial.println("  drum voice mute");
        refreshAllLeds();
      } else if (ev == EV_RELEASED && drumVoiceMuteActive()) {
        drumMuteHold(false);
        refreshAllLeds();
      } else if (ev == EV_CLICKED && stepEditCtx()) {
        // SEQMODE + MONO: CLICK on M1 cycles the edit radio - OFF -> ON
        // (STEPS target), ON+STEPS -> OFF, ON+attribute -> back to STEPS.
        // Turning it ON closes the M2 GNR8/ARP page first - the keys are
        // one radio (each page's own toggle does the reverse)
        bool droppedPage = false;
        if (!stepEditOn) {
          if (genPageActive()) {      // M1 wins: the GNR8 page leaves
            genPageDismiss();
            droppedPage = true;
            if (logRoom(48)) Serial.println("  gen page off (edit on)");
          }
          if (arpPageActive()) {      // ...and so does the ARP page
            arpPageDismiss();
            droppedPage = true;
            if (logRoom(48)) Serial.println("  arp page off (edit on)");
          }
          if (leadPageActive()) {     // ...and the LEAD page
            leadPageDismiss();
            droppedPage = true;
            if (logRoom(48)) Serial.println("  lead page off (edit on)");
          }
          if (harmPageActive()) {     // ...and the HARM page
            harmPageDismiss();
            droppedPage = true;
            if (logRoom(48)) Serial.println("  harm page off (edit on)");
          }
          if (compPageActive()) {     // ...and the COMP page
            compPageDismiss();
            droppedPage = true;
            if (logRoom(48)) Serial.println("  comp page off (edit on)");
          }
          if (skankPageActive()) {    // ...and the SKANK page
            skankPageDismiss();
            droppedPage = true;
            if (logRoom(48)) Serial.println("  skank page off (edit on)");
          }
          if (drumPageActive()) {     // ...and the DRUM pattern page
            drumPageDismiss();
            droppedPage = true;
            if (logRoom(48)) Serial.println("  drum page off (edit on)");
          }
          stepEditSet(true, 0);
          headerShow("Step Edit on");
          if (logRoom(48)) Serial.println("  step edit on (steps)");
        } else if (stepEditTarget != 0) {
          stepEditSet(true, 0);
          headerShow("Step Recording");
          if (logRoom(48)) Serial.println("  edit target steps");
        } else {
          stepEditSet(false, 0);
          headerShow("Step Edit off");
          if (logRoom(48)) Serial.println("  step edit off");
        }
        refreshAllLeds();
        // The band marks the armed radio, so it has to follow the click itself
        // and not only a dismissed page - a page also rewrites the encoder row,
        // but the radio moving STEPS -> OFF -> STEPS is a legend change alone
        (void)droppedPage;
        refreshMainScreen();
      } else if (ev == EV_CLICKED && stepEditAccCtx()) {
        // SEQMODE + DRUM/POLY: a QUICK tap of M1 toggles the edit master only
        // (a HOLD still opens the part select - hold suppresses the click)
        if (stepEditOn) {
          stepEditSet(false, 0);
          headerShow("Step Edit off");
          if (logRoom(48)) Serial.println("  step edit off");
        } else {
          // master on: reuse the last attribute, default to ACCENT
          uint8_t t = (stepEditTarget >= 3) ? stepEditTarget : 3;
          if (t == 5 && seqTrack() != SEQ_TRACK_POLY) t = 3; // FX is POLY-only
          if (t == 6 && seqTrack() != SEQ_TRACK_POLY) t = 3; // TYPE is POLY-only
          if (t == 8 && seqTrack() != SEQ_TRACK_DRUM) t = 3; // RATCHET is DRUM-only
          stepEditSet(true, t);
          if (t == 3) headerShow("Accent");
          else if (t == 4) headerShow("Probability");
          else if (t == 8) {
            char hb[24];
            snprintf(hb, sizeof(hb), "Ratchet %u", stepRatArmSel());
            headerShow(hb);
          } else {
            char hb[24];
            snprintf(hb, sizeof(hb), "%s %s", t == 6 ? "Step Type" : "Step FX",
                     t == 6 ? stepTypeArmName() : stepFxName(stepFxType));
            headerShow(hb);
          }
          if (logRoom(80)) {
            Serial.print("  step edit on (");
            Serial.println(t == 3 ? "accent)" : t == 4 ? "prob)"
                           : t == 6 ? "type)" : t == 8 ? "ratchet)" : "fx)");
          }
        }
        refreshAllLeds();
      }
    }

    // STRUM ADDS row (open via M2): F1..F3 toggle sus4 / add6 / add9,
    // F4 clears the selection - independent bits, combinable, RAM only
    if (ev == EV_CLICKED && strumFRowActive() &&
        kl >= groupControls[5] && kl <= groupControls[8]) {
      uint8_t idx = (uint8_t)(kl - groupControls[5]);      // 0..3
      static const char *const xn[3] = { "sus4", "add6", "add9" };
      if (idx == 3) {
        strumExtSet(0);
        headerShow("Strum Adds cleared");
        if (logRoom(48)) Serial.println("  adds cleared");
      } else {
        strumExtSet((uint8_t)(strumExtGet() ^ (1u << idx)));
        char hb[16];
        snprintf(hb, sizeof(hb), "%s %s", xn[idx],
                 (strumExtGet() >> idx) & 1 ? "on" : "off");
        headerShow(hb);
        if (logRoom(48)) {
          Serial.print("  adds ");
          Serial.println(hb);
        }
      }
      refreshAllLeds();
    } else
    // ARP page (open via M2 on a MONO/ARP track): F1..F4 arm the encoder
    // SUB-PAGES (order & shape / motion / generative / engine options).
    // Click the armed one again to fall back to the plain 8-knob row, click
    // another to switch.  The claim sits BEFORE the step-edit target radio
    // below - that radio would otherwise dismiss the page - so the open
    // page owns the F row, exactly like the GNR8 page does
    if (ev == EV_CLICKED && arpPageActive() &&
        kl >= groupControls[5] && kl <= groupControls[8]) {
      uint8_t idx = (uint8_t)(kl - groupControls[5]);      // 0..3
      if (arpOptionSel() == idx) arpOptionSet(0xFF);       // tap again = plain row
      else                       arpOptionSet(idx);
      headerShow(arpOptionName(arpOptionSel()));
      refreshAllLeds();
      refreshMainScreen();
      if (logRoom(48)) {
        Serial.print("  arp sub-page ");
        Serial.println(arpOptionSel() == 0xFF ? 0 : idx + 1);
      }
    } else
    // GNR8 page (open via M2): F1..F4 are RADIO switches for the option
    // sub-pages (values live in the SEQ EEPROM block / the step table) -
    // F1 arms the FULL page (ENC1 TONES, ENC2 RANGE, ENC3 LOW, ENC4
    // ALGO), F2 arms STEP OCT (+ MAP on ENC3), F3 BORROW, F4 MOD PROB
    // (tap a step first, the armed page dials it).  Click the armed
    // one again releases, click another switches.  The F-pad LEDs follow
    // via refreshAllLeds, the row via refreshMain
    if (ev == EV_CLICKED && genPageActive() &&
        kl >= groupControls[5] && kl <= groupControls[8]) {
      uint8_t idx = (uint8_t)(kl - groupControls[5]);      // 0..3
      if (genOptSel == idx) {
        genOptSel = 0xFF;
        headerShow("Generator");
        if (logRoom(48)) Serial.println("  gen option off");
      } else {
        genOptSel = idx;
        if (idx == 0) {                // F1 = the four-knob options page
          headerShow("Generator Options");
          if (logRoom(48)) Serial.println("  gen opts page");
        } else {
          genOptionHeader(idx);
          if (logRoom(48)) {
            Serial.print("  gen option arm ");
            Serial.println(idx);
          }
        }
      }
      refreshAllLeds();
      refreshMainScreen();
    } else
    // DRUM page (open via M2): F1..F4 pick the knob SET - GROOVE (genre /
    // VAR / FILL / MOT on ENC1..4), LINES (the four lane selectors on
    // ENC1..4), FILL% and MOT% (the per-lane values on ENC1..4).  Clicking
    // the armed set again returns to the base eight-knob row
    if (ev == EV_CLICKED && drumPageActive() &&
        kl >= groupControls[5] && kl <= groupControls[8]) {
      uint8_t idx = (uint8_t)(kl - groupControls[5]);      // 0..3
      uint8_t want = (drumSetSelGet() == idx) ? 0xFF : idx;
      drumSetSelSet(want);
      static const char *const sn[4] = { "Groove", "Lines", "Fill %", "Motion %" };
      headerShow(want == 0xFF ? "Drum Pattern" : sn[idx]);
      if (logRoom(48)) {
        Serial.print("  drum set ");
        Serial.println(want == 0xFF ? "all" : sn[idx]);
      }
      refreshAllLeds();
      refreshMainScreen();
    } else
    // F1-F4 in SEQMODE (MONO): pick the edit target and switch the radio
    // on (TIE/GLIDE/ACCENT/PROB).  DRUM/POLY tracks only take ACCENT/PROB
    // from F3/F4 - clicking the selected one again closes the radio there;
    // POLY adds F1 = FX and F2 = TYPE (STEP/STRUM/HAND override per step).
    // SEQMODE only: ENCMODE/KEYMODE keep their F-pad jobs.  The strum
    // ADDS row above takes the F row first while it is open, then the
    // GNR8 page's option radio row (OPTS/OCT/BRW/PROB)
    if (ev == EV_CLICKED && !strumFRowActive() && !genPageActive() &&
        (stepEditCtx() || stepEditAccCtx()) &&
        kl >= groupControls[5] && kl <= groupControls[8]) {
      uint8_t idx = (uint8_t)(kl - groupControls[5]);      // 0..3
      static const char *const tn[4] = { "Tie", "Glide", "Accent", "Probability" };
      bool droppedArp = false;
      if (stepEditCtx()) {
        // arming a target wins over the M2 ARP page (the radio is one
        // group - same exclusivity the GNR8 page has with its F row)
        if (arpPageActive()) {
          arpPageDismiss();
          droppedArp = true;
          if (logRoom(48)) Serial.println("  arp page off (edit target)");
        }
        if (leadPageActive()) {     // ...same exclusivity for the LEAD page
          leadPageDismiss();
          droppedArp = true;
          if (logRoom(48)) Serial.println("  lead page off (edit target)");
        }
        if (harmPageActive()) {     // ...and the HARM page
          harmPageDismiss();
          droppedArp = true;
          if (logRoom(48)) Serial.println("  harm page off (edit target)");
        }
        if (compPageActive()) {     // ...and the COMP page
          compPageDismiss();
          droppedArp = true;
          if (logRoom(48)) Serial.println("  comp page off (edit target)");
        }
        if (skankPageActive()) {    // ...and the SKANK page
          skankPageDismiss();
          droppedArp = true;
          if (logRoom(48)) Serial.println("  skank page off (edit target)");
        }
        if (drumPageActive()) {     // ...and the DRUM pattern page
          drumPageDismiss();
          droppedArp = true;
          if (logRoom(48)) Serial.println("  drum page off (edit target)");
        }
        // A second click on the armed target hands the pads back to the steps
        // themselves - the radio STAYS on, on the STEPS target.  It used to go
        // to "Step Edit off", which left the pads dead until EDIT was pressed
        // again, so leaving TIE cost two presses (MONO had no way out at all
        // before that: TIE/GLIDE/ACCENT/PROB only left through M1)
        if (stepEditOn && stepEditTarget == (uint8_t)(idx + 1)) {
          stepEditSet(true, 0);
          headerShow("Step Edit on");
          if (logRoom(48)) Serial.println("  edit target steps");
        } else {
          stepEditSet(true, (uint8_t)(idx + 1));
          headerShow(tn[idx]);
          if (logRoom(48)) {
            Serial.print("  edit target ");
            Serial.println(tn[idx]);
          }
        }
      } else if (idx >= 2) {               // DRUM/POLY: ACCENT (F3) / PROB (F4)
        if (stepEditOn && stepEditTarget == idx + 1) {
          stepEditSet(true, 0);            // back to editing the steps
          headerShow("Step Edit on");
          if (logRoom(48)) Serial.println("  edit target steps");
        } else {
          stepEditSet(true, (uint8_t)(idx + 1));
          headerShow(tn[idx]);
          if (logRoom(48)) {
            Serial.print("  edit target ");
            Serial.println(tn[idx]);
          }
        }
      } else if (idx == 0 && seqTrack() == SEQ_TRACK_POLY) {
        // POLY: F1 cycles FX (5) -> EMB (7) -> back to STEPS (0)
        if (stepEditOn && stepEditTarget == 5) {
          stepEditSet(true, 7);
          char hb[24];
          snprintf(hb, sizeof(hb), "Embed Sub %u Inv %u", k4EmbSubProb(), k4EmbInvBias());
          headerShow(hb);
          if (logRoom(48)) { Serial.print("  edit target emb "); Serial.println(hb); }
        } else if (stepEditOn && stepEditTarget == 7) {
          stepEditSet(true, 0);            // third tap: back to the steps
          headerShow("Step Edit on");
          if (logRoom(48)) Serial.println("  edit target steps");
        } else {
          stepEditSet(true, 5);
          char hb[24];
          snprintf(hb, sizeof(hb), "Step FX %s", stepFxName(stepFxType));
          headerShow(hb);
          if (logRoom(48)) {
            Serial.print("  edit target fx ");
            Serial.println(stepFxName(stepFxType));
          }
        }
      } else if (idx == 1 && seqTrack() == SEQ_TRACK_POLY) {
        // POLY: F2 = the TYPE target (STEP/STRUM/HAND per step); ENC7 picks
        // the armed value, a tap on a step stamps it (tap again = inherit)
        if (stepEditOn && stepEditTarget == 6) {
          stepEditSet(true, 0);            // back to editing the steps
          headerShow("Step Edit on");
          if (logRoom(48)) Serial.println("  edit target steps");
        } else {
          stepEditSet(true, 6);
          char hb[24];
          snprintf(hb, sizeof(hb), "Step Type %s", stepTypeArmName());
          headerShow(hb);
          if (logRoom(48)) {
            Serial.print("  edit target type ");
            Serial.println(stepTypeArmName());
          }
        }
      } else if (idx == 1 && seqTrack() == SEQ_TRACK_DRUM) {
        // DRUM: F2 = the RATCHET target (2/3/4 repeats inside one step, per
        // voice).  ENC6 picks the armed count, a tap stamps it on the step's
        // SELECTED PART; tapping a part that already carries that count
        // clears it.  M2 keeps opening the DRUM pattern page - F2 is the
        // only new owner of this key here
        if (stepEditOn && stepEditTarget == 8) {
          stepEditSet(true, 0);            // back to editing the steps
          headerShow("Step Edit on");
          if (logRoom(48)) Serial.println("  edit target steps");
        } else {
          stepEditSet(true, 8);
          char hb[24];
          snprintf(hb, sizeof(hb), "Ratchet %u", stepRatArmSel());
          headerShow(hb);
          if (logRoom(80)) {
            Serial.print("  edit target ratchet ");
            Serial.println(stepRatArmSel());
          }
        }
      }
      refreshAllLeds();
      // likewise every target click: the band's armed label IS the target, and
      // before this the redraw waited for the next page change or step tap,
      // which made the highlight lag one press behind the knob
      (void)droppedArp;
      refreshMainScreen();
    }

    // NEXT in ENCMODE: the ENCABLETONMODE overlay (latched).  NEXT has no
    // other job in this family, so in ENCMODE the toggle is its whole meaning
    if (ev == EV_CLICKED && modeBase == MODE_ENC && kl == groupControls[1]) {
      abltToggle();
      headerShow(abltOn() ? "Live Mode On" : "Live Mode Off");
      refreshAllLeds();
      refreshMainScreen();
      refreshHeader();
      if (logRoom(48)) {
        Serial.print("  ableton overlay ");
        Serial.println(abltOn() ? "on" : "off");
      }
    }

    // M3 in ENCABLETONMODE: swaps the F row to the clip-action set
    // (fill track / fill scene / stop track / stop all)
    if (ev == EV_CLICKED && modeBase == MODE_ENC && abltOn() &&
        kl == groupControls[4]) {
      abltActionToggle();
      headerShow(abltActionSet() ? "Clip Actions" : "Bank Actions");
      refreshAllLeds();
      refreshMainScreen();
      if (logRoom(48)) {
        Serial.print("  ableton F row ");
        Serial.println(abltActionSet() ? "clip" : "bank");
      }
    }

    // M3 in ENCMODE: toggles the page group of the ENC5..8 banks (off =
    // F1..F4 = CC1..16, on = CC21..36; ENC1..4 always send CC17..20).
    // The pad LED follows (dim green = lower group, purple = upper), the
    // encoder labels/values repaint with the group
    if (ev == EV_CLICKED && modeBase == MODE_ENC && !abltOn() &&
        kl == groupControls[4]) {
      bool on = !encM3Page();
      encM3PageSet(on);
      headerShow(on ? "CC Bank 21-36" : "CC Bank 1-16");
      if (logRoom(48)) {
        Serial.print("  enc page group ");
        Serial.println(on ? "2 (CC21-36)" : "1 (CC1-16)");
      }
      refreshAllLeds();
      refreshMainScreen();
    }

    // F1-F4 in ENCABLETONMODE: bank left/right + device left/right, or -
    // on the M3 action set - fill track / fill scene / stop track / stop all
    if (ev == EV_CLICKED && modeBase == MODE_ENC && abltOn() &&
        kl >= groupControls[5] && kl <= groupControls[8]) {
      uint8_t fidx = (uint8_t)(kl - groupControls[5]);
      uint8_t hrow = linkHighlightRow();
      if (hrow > 15) hrow = 0;
      if (!abltActionSet()) {
        if (fidx == 0) { linkBankStep(-1); headerShow("Bank Previous"); }
        else if (fidx == 1) { linkBankStep(1); headerShow("Bank Next"); }
        else if (fidx == 2) { linkDeviceStep(-1); headerShow("Device Previous"); }
        else { linkDeviceStep(1); headerShow("Device Next"); }
      } else if (fidx == 0) {
        linkClipAct(hrow, 2);
        linkDumpTrack((uint8_t)(encChannel() - 1));
        headerShow("Track To Clip");
      } else if (fidx == 1) {
        for (uint8_t t = 0; t < 16; t++) linkClipAct(hrow, 2);
        linkDumpAll();
        headerShow("Scene To Clips");
      } else if (fidx == 2) {
        linkStopTrack();
        headerShow("Track Stopped");
      } else {
        linkStopAll();
        headerShow("All Clips Stopped");
      }
      if (logRoom(48)) {
        Serial.print("  ableton F");
        Serial.println(fidx + 1);
      }
    }

    // F1-F4 in ENCMODE: radio switches for the ENC5..8 CC bank (F1 =
    // CC1..4 ... F4 = CC13..16 in the lower M3 page group, F1 = CC21..24
    // ... F4 = CC33..36 in the upper one; ENC1..4 always send CC17..20).
    // RAM only and always one bank armed - re-clicking the selected one
    // keeps it.  The pad LEDs follow via refreshAllLeds, the encoder
    // labels/values via refreshMainScreen
    if (ev == EV_CLICKED && modeBase == MODE_ENC && !abltOn() &&
        kl >= groupControls[5] && kl <= groupControls[8]) {
      uint8_t idx = (uint8_t)(kl - groupControls[5]);      // 0..3
      encBankSet(idx);
      uint8_t c1 = encCcNum(4);          // first CC of the armed block
      char hb[24];
      snprintf(hb, sizeof(hb), "CC Bank %u-%u", c1, c1 + 3);
      headerShow(hb);
      if (logRoom(48)) {
        Serial.print("  enc bank ");
        Serial.println(hb);
      }
      refreshAllLeds();
      refreshMainScreen();
    }

    // MUTE overlay (ENC mode): hold M2 -> the steps show the channel mute
    // state (dim orange-red = playing, bright orange-red = muted), tapping
    // one toggles that channel's playback; release closes the overlay.
    // SEQMODE3: a CLICK on M2 instead erases the selected track
    if (ledForPad[pad] == groupControls[3]) {
      if (ev == EV_PRESSED && modeBase == MODE_ENC) {
        encMuteHold(true);
        headerShow("Channel Mute");
        if (logRoom(48)) Serial.println("  mute overlay");
        refreshAllLeds();
      } else if (ev == EV_HOLD && layOvEligible()) {
        // hold M2 (SEQMODE 1/2/4, non-DRUM edit track) = the overdub-layer
        // overlay: the keyboard row becomes the layer panel until release.
        // A hold never emits EV_CLICKED, so the tap actions below (pages,
        // erase, strum row) stay taps
        layOvOpen();
        headerShow("Overdub Layers");
        if (logRoom(48)) Serial.println("  layer overlay");
        refreshAllLeds();
      } else if (ev == EV_RELEASED && (layOvClose() || encMuteActive())) {
        // release closes whichever hold overlay rode in; layOvClose() runs
        // unconditionally so a mode switch mid-hold cannot leave it stuck
        encMuteHold(false);
        refreshAllLeds();
      } else if (ev == EV_CLICKED && modeBase == MODE_SEQ && modeVariant == 3) {
        seqErase((uint8_t)(seqEditChannel() - 1));
        char eb[24];
        snprintf(eb, sizeof(eb), "Channel %u erased", seqEditChannel());
        headerShow(eb);
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(eb);
        }
        refreshAllLeds();   // step LEDs dim: that pattern is gone
      } else if (ev == EV_CLICKED && strumFRowCtx()) {
        // SEQMODE 1/2/4 + POLY STRUM/HAND: M2 clicks the strum ADDS row
        // (F1..F4 = sus4/add6/add9/clear) on/off - POLY had no M2 job yet,
        // so this never collides with the ARP page (MONO) or erase (mode 3)
        strumFRowToggle();
        headerShow(strumFRowActive() ? "Strum Adds" : "Strum Adds off");
        if (logRoom(48)) {
          Serial.print("  strum adds row ");
          Serial.println(strumFRowActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      } else if (ev == EV_CLICKED && arpTrackCtx()) {
        // SEQMODE 1/2/4 + MONO/ARP: M2 clicks the ARP encoder page on/off.
        // Opening it drops the M1 edit radio - the two keys are mutually
        // exclusive (M1's toggle closes this page instead)
        arpPageToggle();
        if (arpPageActive()) {
          if (stepEditIsOn()) {
            stepEditSet(false, 0);
            if (logRoom(48)) Serial.println("  edit off (arp open)");
          }
        } else {
          stepChainDisarm();           // the F row leaves too
        }
        headerShow(arpPageActive() ? "Arpeggiator" : "Arpeggiator off");
        if (logRoom(48)) {
          Serial.print("  arp page ");
          Serial.println(arpPageActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      } else if (ev == EV_CLICKED && genTrackCtx()) {
        // SEQMODE 1/2/4 + MONO/BASS: M2 clicks the GNR8 encoder page on/off.
        // Opening it drops the M1 edit radio - the two keys are mutually
        // exclusive (M1's toggle closes this page instead)
        genPageToggle();
        if (genPageActive()) {
          if (stepEditIsOn()) {
            stepEditSet(false, 0);
            if (logRoom(48)) Serial.println("  edit off (gnr8 open)");
          }
        } else {
          stepChainDisarm();           // the F row leaves too
        }
        headerShow(genPageActive() ? "Generator" : "Generator off");
        if (logRoom(48)) {
          Serial.print("  gen page ");
          Serial.println(genPageActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      } else if (ev == EV_CLICKED && leadTrackCtx()) {
        // SEQMODE 1/2/4 + MONO/LEAD: M2 clicks the LEAD encoder page on/off.
        // Opening it drops the M1 edit radio - the two keys are mutually
        // exclusive (M1's toggle closes this page instead)
        leadPageToggle();
        if (leadPageActive()) {
          if (stepEditIsOn()) {
            stepEditSet(false, 0);
            if (logRoom(48)) Serial.println("  edit off (lead open)");
          }
        } else {
          stepChainDisarm();           // the F row leaves too
        }
        headerShow(leadPageActive() ? "Lead" : "Lead off");
        if (logRoom(48)) {
          Serial.print("  lead page ");
          Serial.println(leadPageActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      } else if (ev == EV_CLICKED && harmTrackCtx()) {
        // SEQMODE 1/2/4 + POLY/HARM: M2 clicks the HARM encoder page on/off.
        // Opening it drops the M1 edit radio - the two keys are mutually
        // exclusive (M1's toggle closes this page instead)
        harmPageToggle();
        if (harmPageActive()) {
          if (stepEditIsOn()) {
            stepEditSet(false, 0);
            if (logRoom(48)) Serial.println("  edit off (harm open)");
          }
        } else {
          stepChainDisarm();           // the F row leaves too
        }
        headerShow(harmPageActive() ? "Harmonizer" : "Harmonizer off");
        if (logRoom(48)) {
          Serial.print("  harm page ");
          Serial.println(harmPageActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      } else if (ev == EV_CLICKED && compTrackCtx()) {
        // SEQMODE 1/2/4 + POLY/STEP: M2 clicks the COMP encoder page on/off
        compPageToggle();
        if (compPageActive()) {
          if (stepEditIsOn()) {
            stepEditSet(false, 0);
            if (logRoom(48)) Serial.println("  edit off (comp open)");
          }
        } else {
          stepChainDisarm();           // the F row leaves too
        }
        headerShow(compPageActive() ? "Comping" : "Comping off");
        if (logRoom(48)) {
          Serial.print("  comp page ");
          Serial.println(compPageActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      } else if (ev == EV_CLICKED && skankTrackCtx()) {
        // SEQMODE 1/2/4 + POLY/SKANK: M2 clicks the SKANK encoder page on/
        // off.  Opening it drops the M1 edit radio - the two keys are
        // mutually exclusive (M1's toggle closes this page instead)
        skankPageToggle();
        if (skankPageActive()) {
          if (stepEditIsOn()) {
            stepEditSet(false, 0);
            if (logRoom(48)) Serial.println("  edit off (skank open)");
          }
        } else {
          stepChainDisarm();           // the F row leaves too
        }
        headerShow(skankPageActive() ? "Skank" : "Skank off");
        if (logRoom(48)) {
          Serial.print("  skank page ");
          Serial.println(skankPageActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      } else if (ev == EV_CLICKED && drumTrackCtx()) {
        // SEQMODE 1/2/4 + DRUM: M2 clicks the DRUM pattern page on/off
        // (indigo pad).  Opening it drops the M1 edit radio - the two keys
        // are mutually exclusive (M1's toggle closes this page instead)
        drumPageToggle();
        if (drumPageActive()) {
          if (stepEditIsOn()) {
            stepEditSet(false, 0);
            if (logRoom(48)) Serial.println("  edit off (drum open)");
          }
        } else {
          stepChainDisarm();           // the F row leaves too
        }
        headerShow(drumPageActive() ? "Drum Pattern" : "Drum Pattern off");
        if (logRoom(48)) {
          Serial.print("  drum page ");
          Serial.println(drumPageActive() ? "on" : "off");
        }
        refreshAllLeds();
        refreshMainScreen();
      }
    }

    // SEQMODE3 + REC: erase the COMPLETE pattern (all 16 tracks) - allowed
    // while the transport runs; every sounding note is cut at once
    if (ev == EV_PRESSED && ledForPad[pad] == groupTransport[1] &&
        modeBase == MODE_SEQ && modeVariant == 3) {
      seqEraseAll();
      headerShow("All Erased");
      if (logRoom(48)) Serial.println("  erase all tracks");
      refreshAllLeds();
    }

    // ENCMODE4 + REC: save the knob values + channel to EEPROM (restored
    // on the next boot; DAW feedback still wins live) - save only, no push
    if (ev == EV_PRESSED && ledForPad[pad] == groupTransport[1] &&
        modeBase == MODE_ENC && modeVariant == 4) {
      encCcSave();
      headerShow("CC Saved");
      if (logRoom(48)) Serial.println("  cc values saved to eeprom");
    }

    // SEQMODE4 + REC: save every SEQ setting (track type, mono type, octave,
    // input channel, velocity, accent, gate, mutes, probability, pattern
    // length, FOLLOW, arp options, edit channel).  Notes, step contents and
    // the tie/glide/accent/prob attributes are NOT stored, they stay RAM
    if (ev == EV_PRESSED && ledForPad[pad] == groupTransport[1] &&
        modeBase == MODE_SEQ && modeVariant == 4) {
      seqCfgSave();
      headerShow("Sequence Saved");
      if (logRoom(48)) Serial.println("  seq config saved to eeprom");
    }

    // SEQMODE1/2 and KEYMODE1/2/4 + REC: live record - arm one pass over
    // the edit track's pattern from its next 1st bar (auto-off after one
    // pass; the keyboard channel must match the edit track or arming is
    // refused).  KEYMODE2 records the whole chord the engine builds
    if (ev == EV_PRESSED && ledForPad[pad] == groupTransport[1] &&
        ((modeBase == MODE_SEQ && (modeVariant == 1 || modeVariant == 2)) ||
         (modeBase == MODE_KEY &&
          (modeVariant == 1 || modeVariant == 2 || modeVariant == 4)))) {
      uint8_t r = seqRecToggle();
      if (r == 0) headerShow("Keyboard Ch Mismatch");
      else if (r == 1) headerShow("Record armed");
      else headerShow("Record off");
      refreshAllLeds();
      if (logRoom(48)) {
        Serial.print("  rec ");
        Serial.println(r == 0 ? "refused (kbd ch)" : (r == 1 ? "armed" : "off"));
      }
    }

    // KEYMODE2 chord buttons (momentary): M1/M2/M3/NEXT select the quality
    // Maj/Min/Dim/Sus, F1..F4 the extensions 6/m7/M7/9 - press sets,
    // release clears, a sounding chord re-triggers.  M1 keeps its DRUM
    // part-select / ENCMODE voice-mute priority, M2 its ENCMODE mute /
    // SEQMODE3 erase above the quality keys
    if ((ev == EV_PRESSED || ev == EV_RELEASED) && keyBehaviorVariant() == 2) {
      int8_t ci = -1;
      for (uint8_t i = 0; i < CONTROLS_NUM_G; i++)
        if (groupControls[i] == kl) ci = (int8_t)i;
      bool m1SeqRadio = (ci == 2 && modeBase == MODE_SEQ &&
                         modeVariant != 3);               // edit radio M1
      bool m1Enc = (ci == 2 && modeBase == MODE_ENC);
      bool m2Enc = (ci == 3 && modeBase == MODE_ENC);
      bool m2Seq3 = (ci == 3 && modeBase == MODE_SEQ && modeVariant == 3);
      bool fSeq = (ci >= 5 && modeBase == MODE_SEQ && modeVariant != 3);
      bool fEnc = (ci >= 5 && modeBase == MODE_ENC);
      bool m3Enc = (ci == 4 && modeBase == MODE_ENC);
      // NEXT belongs to the ENCABLETONMODE toggle in this family, not to
      // the KEYMODE2 quality momentaries
      bool nextEnc = (ci == 1 && modeBase == MODE_ENC);
      // F1-F4 belong to the SEQMODE edit radio (target / accent / prob)
      // plus the F3 accent-velocity hold - they never touch the chords
      // there, on any track type; in ENCMODE they are the CC-bank radio
      // and M3 the page-group toggle
      if (ci > 0 && !nextEnc && !m1SeqRadio && !m1Enc && !m2Enc && !m2Seq3 &&
          !m3Enc && !fSeq && !fEnc) {                     // ci 0 = PREV
        bool on = (ev == EV_PRESSED);
        static const uint8_t extOf[4] = { CHORD_EXT_6, CHORD_EXT_m7,
                                          CHORD_EXT_M7, CHORD_EXT_9 };
        if (ci >= 5) {
          chordSetExt(extOf[ci - 5], on);
        } else {
          static const uint8_t qOf[5] = { CHORD_Q_NONE, CHORD_Q_SUS,
                                          CHORD_Q_MAJ, CHORD_Q_MIN, CHORD_Q_DIM };
          // order on the panel: NEXT=1 Sus, M1=2 Maj, M2=3 Min, M3=4 Dim
          chordSetQuality(on ? qOf[ci] : CHORD_Q_NONE);
        }
        refreshAllLeds();               // sounding pitch classes may change
        if (chordName()[0]) headerShow(chordName());
        if (logRoom(48)) {
          Serial.print(on ? "  chord set " : "  chord clear ");
          Serial.println(chordName());
        }
      }
    }

    // KEYMODE4 control row: NEXT tap = the EMB menu (sticky: the encoder
    // row becomes EMB/PLACE, tap again to close), NEXT hold = focus+1,
    // M1 = write latch (WRITE lands
    // the proposal on the focused step / AUDITION plays it only), M2 =
    // auto-advance after every write, M3 tap = apply the generator to every
    // unlocked step / M3 hold = undo, F1 = variation (reseed + generate),
    // F2 = lock the focused step (apply skips it), F3 = VOICE-LEAD
    // inversions, F4 held = audition only (no write, no advance)
    int8_t k4ci = -1;
    for (uint8_t i = 0; i < CONTROLS_NUM_G; i++)
      if (groupControls[i] == kl) k4ci = (int8_t)i;
    if (modeBase == MODE_KEY && modeVariant == 4) {
      // TONALITY page open: F1..F4 pick its bank.  They shadow the usual F
      // row for as long as the page is up (VARIATION / LOCK / LEAD / AUTO
      // come back with the page closed) - the pad of the open bank burns
      // white, the other three drift through a slow dim hue cycle
      if (k4Tonality() && k4ci >= 5 && ev == EV_CLICKED) {
        uint8_t bk = (uint8_t)(k4ci - 5);
        k4TonaBankSet(bk);
        setOverlay(G_ENC, (uint8_t)(ENC_KEY4_MENU + bk));
        headerShow(k4TonaBankLong());
        refreshMainScreen();
        refreshAllLeds();
        if (logRoom(48)) {
          Serial.print("  bank ");
          Serial.println(k4TonaBankLong());
        }
      } else if (k4ci == 1 && ev == EV_CLICKED) {
        bool on = !k4EmbMenu();
        k4EmbMenuSet(on);
        if (on) setOverlay(G_ENC, ENC_KEY4_EMB);
        else clearOverlay(G_ENC);
        headerShow(on ? "Embed Menu on" : "Embed Menu off");
        refreshMainScreen();   // the row swaps to EMB/PLACE labels
        if (logRoom(48))
          Serial.println(on ? "  emb menu on" : "  emb menu off");
      } else if (k4ci == 1 && ev == EV_HOLD) {
        k4FocusStep(1);            // announces focus + progression itself
        refreshAllLeds();
        if (logRoom(48)) Serial.println("  focus next");
      } else if (k4ci == 2 && ev == EV_CLICKED) {
        bool on = k4WriteToggle();
        headerShow(on ? "Key Write" : "Key Audition");
        if (logRoom(48))
          Serial.println(on ? "  key write" : "  key audition");
      } else if (k4ci == 3 && ev == EV_CLICKED) {
        // M2 = the TONALITY menu (sticky, like the EMB row): the four
        // tonality knobs on the encoder row, no PREV hold needed.  Auto
        // advance moved to F4 when this page took the M2 slot
        bool on = k4TonalityToggle();
        if (on) setOverlay(G_ENC, (uint8_t)(ENC_KEY4_MENU + k4TonaBank()));
        else if (k4EmbMenu()) setOverlay(G_ENC, ENC_KEY4_EMB);
        else clearOverlay(G_ENC);
        headerShow(on ? "Tonality On" : "Tonality Off");
        refreshMainScreen();   // the row swaps to OCT/SCALE/KEY/FSCALE
        refreshAllLeds();      // the M2 pad brightens while the page is up
        if (logRoom(48))
          Serial.println(on ? "  tonality on" : "  tonality off");
      } else if (k4ci == 4 && ev == EV_CLICKED) {
        int w = k4ApplyAll();
        if (w > 0) k4AnnouncePattern();   // the progression that was written
        else headerShow("Nothing Written");
        refreshAllLeds();
        refreshMainScreen();   // focus/locks may have been re-read
        if (logRoom(48)) Serial.println(w > 0 ? "  applied" : "  applied 0");
      } else if (k4ci == 4 && ev == EV_HOLD) {
        int u = k4Undo();
        if (u == 0) k4AnnouncePattern();   // the restored progression
        else headerShow("Nothing To Undo");
        refreshAllLeds();
        refreshMainScreen();
        if (logRoom(48)) Serial.println("  undo");
      } else if (k4ci == 5 && ev == EV_CLICKED) {
        int w = k4Variation();
        if (w > 0) k4AnnouncePattern();   // the reseeded progression
        else headerShow("No Variation");
        refreshAllLeds();
        if (logRoom(48)) Serial.println(w > 0 ? "  variation" : "  var 0");
      } else if (k4ci == 6 && ev == EV_CLICKED) {
        bool on = k4LockToggle(k4Focus());
        char lb[24];
        snprintf(lb, sizeof(lb), "Step %u %s", (unsigned)(k4Focus() + 1),
                 on ? "Locked" : "Unlocked");
        headerShow(lb);
        refreshAllLeds();       // amber focus/lock pads follow
        if (logRoom(48)) { Serial.print("  "); Serial.println(lb); }
      } else if (k4ci == 7 && ev == EV_CLICKED) {
        bool on = k4LeadToggle();
        headerShow(on ? "Lead on" : "Lead off");
        if (logRoom(48)) Serial.println(on ? "  lead on" : "  lead off");
      } else if (k4ci == 8 && ev == EV_CLICKED) {
        // F4 tap = auto advance (moved off M2); a long press is still the
        // audition-only hold below (a hold never reports a click)
        bool on = k4AutoToggle();
        headerShow(on ? "Auto Advance on" : "Auto Advance off");
        if (logRoom(48)) Serial.println(on ? "  auto on" : "  auto off");
      } else if (k4ci == 8 && ev == EV_PRESSED) {
        k4HoldSet(true);
        if (logRoom(48)) Serial.println("  hold audition");
      }
    }
    // F4 release, ungated by mode: the momentary hold flag clears even if
    // the mode changed while the key was down (never a stuck audition)
    if (k4ci == 8 && ev == EV_RELEASED && k4Hold()) {
      k4HoldSet(false);
      if (logRoom(48)) Serial.println("  hold off");
    }

    // GNR8 (SEQMODE 1/2/4 + MONO/BASS): M3 tap = generate the pattern
    // from the page's parameters (fill/acc/glide/tie + the F-row options,
    // notes inside the KEY + SCALE), M3 hold = undo it again - the shared
    // one-level step snapshot, mirroring KEYMODE4's apply/undo pair
    if (modeBase == MODE_SEQ && modeVariant != 3 && genTrackCtx() &&
        k4ci == 4) {
      if (ev == EV_CLICKED) {
        int w = seqGenProduce();
        char gb[32];
        snprintf(gb, sizeof(gb), "Generator %d Steps", w > 0 ? w : 0);
        headerShow(gb);
        refreshAllLeds();       // the step LEDs follow the new pattern
        refreshMainScreen();
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(gb);
        }
      } else if (ev == EV_HOLD) {
        int u = stepSnapshotRestore();
        headerShow(u == 0 ? "Undo" : "Nothing To Undo");
        refreshAllLeds();
        refreshMainScreen();
        if (logRoom(48))
          Serial.println(u == 0 ? "  undo" : "  nothing to undo");
      }
    }

    // LEAD (SEQMODE 1/2/4 + MONO/LEAD): M3 tap = generate the melody from
    // the page's contour + LOW/RANGE/density (notes inside the KEY + SCALE,
    // stored as a skeleton LEAD + FOLLOW + MAP re-resolves onto the feeder
    // chord), M3 hold = undo - the shared one-level step snapshot, exactly
    // like the GNR8 generate/undo pair
    if (modeBase == MODE_SEQ && modeVariant != 3 && leadTrackCtx() &&
        k4ci == 4) {
      if (ev == EV_CLICKED) {
        int w = seqLeadProduce();
        char lb[32];
        snprintf(lb, sizeof(lb), "Lead %d Steps", w > 0 ? w : 0);
        headerShow(lb);
        refreshAllLeds();       // the step LEDs follow the new melody
        refreshMainScreen();
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(lb);
        }
      } else if (ev == EV_HOLD) {
        int u = stepSnapshotRestore();
        headerShow(u == 0 ? "Undo" : "Nothing To Undo");
        refreshAllLeds();
        refreshMainScreen();
        if (logRoom(48))
          Serial.println(u == 0 ? "  undo" : "  nothing to undo");
      }
    }

    // HARM (SEQMODE 1/2/4 + POLY/HARM): M3 tap = bake the chord progression
    // inferred from the FOLLOW feeder's bassline/arp into this track's steps
    // (k4Harm: Viterbi over source-fit + the KEYMODE4 genre grammar), M3
    // hold = undo - the shared one-level step snapshot, like GNR8/LEAD
    if (modeBase == MODE_SEQ && modeVariant != 3 && harmTrackCtx() &&
        k4ci == 4) {
      if (ev == EV_CLICKED) {
        int w = k4Harm();
        char hb[32];
        snprintf(hb, sizeof(hb), "Harmonizer %d Steps", w > 0 ? w : 0);
        headerShow(hb);
        refreshAllLeds();       // the step LEDs follow the new chords
        refreshMainScreen();
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(hb);
        }
      } else if (ev == EV_HOLD) {
        int u = stepSnapshotRestore();
        headerShow(u == 0 ? "Undo" : "Nothing To Undo");
        refreshAllLeds();
        refreshMainScreen();
        if (logRoom(48))
          Serial.println(u == 0 ? "  undo" : "  nothing to undo");
      }
    }

    // DRUM (SEQMODE 1/2/4 + DRUM track): M3 tap = re-roll the pattern from
    // the page's genre / variation / lanes / FILL / MOT settings, M3 hold =
    // undo - the shared one-level step snapshot, like GNR8/LEAD/HARM.  The
    // knob turns already rebuild live; this is the "new roll" gesture (the
    // FILL ghosts, the MOT accents and the bar-end fill are re-rolled)
    if (modeBase == MODE_SEQ && modeVariant != 3 && drumTrackCtx() &&
        k4ci == 4) {
      if (ev == EV_CLICKED) {
        int w = drumProduce();
        char rb[32];
        snprintf(rb, sizeof(rb), "Drum Pattern %d Steps", w > 0 ? w : 0);
        headerShow(rb);
        refreshAllLeds();       // the step LEDs follow the new pattern
        refreshMainScreen();
        if (logRoom(48)) { Serial.print("  "); Serial.println(rb); }
      } else if (ev == EV_HOLD) {
        int u = stepSnapshotRestore();
        headerShow(u == 0 ? "Undo" : "Nothing To Undo");
        refreshAllLeds();
        refreshMainScreen();
        if (logRoom(48))
          Serial.println(u == 0 ? "  undo" : "  nothing to undo");
      }
    }

    // PREV = momentary encoder overlay: KEYMODE2 remaps the row to
    // Oct/Scale/Root/Latch/KLock/Perf, KEYMODE4 to Oct/Scale/Key; release
    // restores it.  A quick KEYMODE4 tap (no hold) steps the focus back
    if (kl == groupControls[0]) {
      if (modeBase == MODE_ENC && abltOn()) {
        // ENCABLETONMODE: PREV (hold) turns the pads into the 16 Live tracks;
        // release hands them straight back to the clip grid
        if (ev == EV_PRESSED) {
          abltPrevHold(true);
          headerShow("Live Tracks");
          refreshAllLeds();
          if (logRoom(48)) Serial.println("  ableton track picker");
        } else if (ev == EV_RELEASED) {
          abltPrevHold(false);
          refreshAllLeds();
        }
      } else if (ev == EV_PRESSED && modeBase == MODE_SEQ) {
        // SEQMODE: PREV held = the SHIFT/TRANS row (ENC1 = pattern rotate,
        // ENC2 = semitone transpose of the sequence's playback).  The GNR8
        // page's old rotate row lives on this same overlay (ENC1 still =
        // rotate).  Checked FIRST - the KEYMODE2 quality row claims PREV
        // via keyBehaviorVariant() no matter which mode is up, so it must
        // not steal the hold in SEQMODE
        setOverlay(G_ENC, ENC_SEQ_ROT_OV);
        prevOvActive = true;
        refreshMainScreen();
        if (logRoom(48)) Serial.println("  overlay SEQ SHIFT/TRANS");
      } else if (ev == EV_PRESSED && keyBehaviorVariant() == 2) {
        setOverlay(G_ENC, ENC_KEY2_PREV);
        prevOvActive = true;
        refreshMainScreen();
        if (logRoom(48)) Serial.println("  overlay KEY2 PREV");
      } else if (ev == EV_PRESSED && modeBase == MODE_KEY &&
                 modeVariant == 4) {
        setOverlay(G_ENC, ENC_KEY4_PREV);
        prevOvActive = true;
        refreshMainScreen();
        if (logRoom(48)) Serial.println("  overlay KEY4 PREV");
      } else if (ev == EV_RELEASED && prevOvActive) {
        // PREV lets go: back to the EMB menu row when it is still open
        // (the menu is sticky, the PREV row above was only borrowed -
        // and only a live KEYMODE4 owns that menu, never a stale flag)
        if (modeBase == MODE_KEY && modeVariant == 4 && k4EmbMenu())
          setOverlay(G_ENC, ENC_KEY4_EMB);
        else if (modeBase == MODE_KEY && modeVariant == 4 && k4Tonality())
          setOverlay(G_ENC, (uint8_t)(ENC_KEY4_MENU + k4TonaBank()));
        else clearOverlay(G_ENC);
        prevOvActive = false;
        refreshMainScreen();
        if (logRoom(48)) Serial.println("  overlay off");
      } else if (ev == EV_CLICKED && modeBase == MODE_KEY &&
                 modeVariant == 4) {
        k4FocusStep(-1);           // announces focus + progression itself
        refreshAllLeds();
        if (logRoom(48)) Serial.println("  focus prev");
      }
    }

    // PLAY in SEQMODE: Start (or Stop while the INT transport runs).
    // ENCMODE3: PLAY = toggle the clock source instead (frees ENC4 for
    // the pattern speed) - it never starts the transport there
    if (ev == EV_PRESSED && ledForPad[pad] == groupTransport[0]) {
      if (modeBase == MODE_ENC && modeVariant == 3) {
        seqSetSyncInternal(!seqIsSyncInternal());
        headerShow(seqIsSyncInternal() ? "Clock Internal" : "Clock External");
        if (logRoom(48)) {
          Serial.print("  clock ");
          Serial.println(seqIsSyncInternal() ? "INT" : "EXT");
        }
      } else if (modeBase == MODE_SEQ ||
                 (modeBase == MODE_KEY && modeVariant == 4)) {
        seqTogglePlay();
        if (logRoom(48)) Serial.println("  -> MIDI Play toggle");
      }
    }

    // keyboard pads -> MIDI notes in every mode (behavior defined by KEYMODE,
    // inherited by SEQMODE/ENCMODE); release always clears the key state
    // (even if the mode changed while it was held: no stuck notes).
    // SEQMODE3 intercepts C D E F first: they pick the edit window (bar)
    // and press the selected bar again to loop it - swallowed presses are
    // remembered so the matching release never sends a stray note-off
    if (kidx != 255) {
      // hold-M2 overdub-layer overlay: its dispatch runs FIRST, so the
      // layer panel owns the row even over a latched bar/verb row; swallowed
      // presses remember themselves (layKeyHandle) so the matching release
      // / click never reaches the instrument.  The header line is built
      // here - the action codes carry no text
      uint8_t lact = layKeyHandle(kidx, ev);
      if (lact) {
        if (lact != LAY_ACT_EAT) {
          uint8_t ln = (kidx == 0) ? 1 : (kidx == 2) ? 2 :
                       (kidx == 4) ? 3 : (kidx == 5) ? 4 : 0;
          char lb[24];
          lb[0] = 0;
          switch (lact) {
            case LAY_ACT_ARM_MUTE:
              snprintf(lb, sizeof(lb), "Mute layer?");
              break;
            case LAY_ACT_ARM_ERASE:
              snprintf(lb, sizeof(lb), "Erase layer?");
              break;
            case LAY_ACT_MUTE_ON:
              snprintf(lb, sizeof(lb), "Layer %u Muted", ln);
              break;
            case LAY_ACT_MUTE_OFF:
              snprintf(lb, sizeof(lb), "Layer %u On", ln);
              break;
            case LAY_ACT_ERASED:
              snprintf(lb, sizeof(lb), "Layer %u Erased", ln);
              break;
            case LAY_ACT_EMPTY:
              snprintf(lb, sizeof(lb), "Layer %u Empty", ln);
              break;
            case LAY_ACT_REPORT: {
              uint8_t nn = layNotes((uint8_t)(ln - 1));
              if (nn) snprintf(lb, sizeof(lb), "Layer %u: %u notes", ln, nn);
              else snprintf(lb, sizeof(lb), "Layer %u empty", ln);
              break;
            }
            case LAY_ACT_UNDO_OK:
              snprintf(lb, sizeof(lb), "Undone");
              break;
            case LAY_ACT_UNDO_NO:
              snprintf(lb, sizeof(lb), "Nothing to undo");
              break;
            case LAY_ACT_REDO_OK:
              snprintf(lb, sizeof(lb), "Redone");
              break;
            case LAY_ACT_REDO_NO:
              snprintf(lb, sizeof(lb), "Nothing to redo");
              break;
            case LAY_ACT_FLATTEN:
              snprintf(lb, sizeof(lb), "Layers flattened");
              break;
            default:
              break;
          }
          if (lb[0]) {
            headerShow(lb);
            if (logRoom(48)) {
              Serial.print("  ");
              Serial.println(lb);
            }
          }
        }
        refreshAllLeds();   // arm / mute / erase / undo all move the LEDs
        continue;           // the overlay owned this event
      }
      static uint16_t barHeld = 0;         // kidx bits eaten as pattern bars /
                                           // track verbs - uint16 because
                                           // G# and A# sit at kidx 8 and 10
      static uint8_t barSel = 0;           // which C D E F bars are down now
                                           // (bit 0..3) - the bar OCT/TRANS
                                           // row stays up until the last one
                                           // lets go
      bool isBar = (ev == EV_PRESSED && seq3KbRole() &&
                    kl >= 25 && kl <= 28);
      // ...and the five BLACK keys of the same octave carry the track
      // verbs: C# = pattern length / 2, D# = pattern length x 2, F# = copy
      // the pattern, G# = the edit window follows the playhead (the bar
      // moves with the green dot), A# = paste the clipboard.  Swallowed
      // exactly like the bar keys, so the release sends no stray note
      bool isSeq3Fn = (ev == EV_PRESSED && seq3KbRole() &&
                       kl >= 37 && kl <= 41);
      if (isBar) {
        uint8_t bar = (uint8_t)(kl - 25);  // 25,26,27,28 -> bar 0..3
        seq3KbLatch(seqEditChannel());     // the keys stay on this track's row
        barHeld |= (uint16_t)(1u << kidx);
        // holding the pad also arms the bar's own row: ENC1 = that bar's
        // OCT, ENC2 = its TRANS (playback-only, like the held-step TRANS).
        // The press above still selects / loops the bar exactly as before
        barSel |= (uint8_t)(1u << bar);
        seqBarEditBegin(bar);
        setOverlay(G_ENC, ENC_SEQ_BAR_OV);
        // the overlay's labels are the encoder row's: repaint it now, or
        // the screen keeps the base row until something else redraws
        // (headerShow only touches the header band)
        refreshMainScreen();
        uint8_t r = seqBarPress(bar);
        char bb[20];
        if (r == 0)
          snprintf(bb, sizeof(bb), "Steps %u-%u",
                   bar * 16 + 1, bar * 16 + 16);
        else if (r == 1)
          snprintf(bb, sizeof(bb), "Loop %u-%u", bar * 16 + 1, bar * 16 + 16);
        else
          snprintf(bb, sizeof(bb), "Loop Off");
        headerShow(bb);
        if (logRoom(48)) {
          Serial.print("  bar ");
          Serial.println(bb);
        }
      } else if (isSeq3Fn) {
        seq3KbLatch(seqEditChannel());     // a verb commits the row as well
        barHeld |= (uint16_t)(1u << kidx);      // swallow the matching release
        char fb[24];
        if (kl == 37) {                       // C#: pattern length / 2
          snprintf(fb, sizeof(fb), "Pattern %u Steps",
                   (unsigned)seqPatLenHalf());
          refreshAllLeds();
          refreshMainScreen();
        } else if (kl == 38) {                // D#: pattern length x 2
          snprintf(fb, sizeof(fb), "Pattern %u Steps",
                   (unsigned)seqPatLenDouble());
          refreshAllLeds();
          refreshMainScreen();
        } else if (kl == 39) {                // F#: copy the pattern
          int n = seqPatCopy();
          if (n <= 0) snprintf(fb, sizeof(fb), "Nothing to Copy");
          else snprintf(fb, sizeof(fb), "Copied %d Steps", n);
        } else if (kl == 41) {                // A#: paste into the edit track
          int n = seqPatPaste();
          if (n < 0) {
            snprintf(fb, sizeof(fb), "Clipboard Empty");
          } else {
            snprintf(fb, sizeof(fb), "Pasted %d Steps", n);
            refreshAllLeds();                 // the pads show the new pattern
            refreshMainScreen();
          }
        } else {                              // G#: bar follow on / off
          bool on = seqStepViewChaseToggle();
          if (on) {
            uint8_t pb = seqPlayBar();        // jump onto the sounding bar now
            if (pb <= 3) seqStepViewFollow((uint8_t)(pb * 16));
          }
          snprintf(fb, sizeof(fb), "Bar Follow %s", on ? "On" : "Off");
          refreshMainScreen();
        }
        headerShow(fb);
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(fb);
        }
      } else if (ev == EV_PRESSED) {
        int n = keyPadPress(kidx);
        if (n >= 0 && logRoom(48)) {
          Serial.print("  note on ");
          Serial.println(n);
        }
        // chord engine variants: the header shows the chord that started
        if (n >= 0 && keyChordMode() && chordName()[0])
          headerShow(chordName());
      } else if (ev == EV_RELEASED) {
        if (barHeld & (1u << kidx)) {
          barHeld &= (uint16_t)~(1u << kidx);
          // a C D E F pad letting go drops the bar OCT/TRANS row with it
          // (the five verbs ride the same mask but never arm it); holding
          // two bars leaves the row up until the second pad goes
          if (kl >= 25 && kl <= 28) {
            barSel &= (uint8_t)~(1u << (kl - 25));
            if (!barSel) {
              seqBarEditEnd();
              if (currentBinding().enc == ENC_SEQ_BAR_OV)
                clearOverlay(G_ENC);
              refreshMainScreen();
            }
          }
        } else keyPadRelease(kidx);
      }
      refreshAllLeds();   // first assign key can activate the held step's LED
    }

    if (ev == EV_PRESSED || ev == EV_CLICKED || ev == EV_HOLD || ev == EV_DOUBLE)
      logSets();
  }
}

// ENCMODE knob positions: ENC1..ENC4 always send CC17..CC20; ENC5..ENC8
// send the F1..F4 bank's block inside the active M3 page group (off =
// F1 = CC1..4 ... F4 = CC13..16; on = F1 = CC21..4 ... F4 = CC33..36)
// on the encoder channel (ENCMODE3).  Values are stored PER CC NUMBER
// (36 CCs x 16 channels): switching channels loads that channel's own
// value instead of inheriting the previous channel's (no jump); DAW
// feedback updates the slots via encCcIncoming()
static int16_t ccVal[36][16] = {{0}};

// F1..F4 bank radio for ENC5..8 - RAM only, remembered PER ENCODER
// CHANNEL (the page last used on each channel is recalled when the
// channel changes or ENCMODE is entered - encPageRecall)
static uint8_t encBankSel = 0;
static uint8_t encBankMem[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 0};

// M3 page group: off = the banks live in CC1..16, on = CC21..36 (the
// upper group; CC17..20 stay reserved for ENC1..4).  RAM only, likewise
// remembered per encoder channel
static bool encM3Sel = false;
static bool encM3Mem[16] = {false, false, false, false, false, false,
                            false, false, false, false, false, false,
                            false, false, false, false};

uint8_t encBank() { return (encBankSel < 4) ? encBankSel : 0; }
void encBankSet(uint8_t b) {
  if (b >= 4) return;
  encBankSel = b;
  encBankMem[encChannel() - 1] = b;   // every change lands in this channel
}

bool encM3Page() { return encM3Sel; }
void encM3PageSet(bool on) {
  encM3Sel = on;
  encM3Mem[encChannel() - 1] = on;    // ...so the switch can recall it
}

// encoder channel changed (or ENCMODE re-entered): take the page the new
// channel last used (setters store, so this just replays them)
void encPageRecall() {
  encBankSet(encBankMem[encChannel() - 1]);
  encM3PageSet(encM3Mem[encChannel() - 1]);
}

// EEPROM accessors (the SEQ block stores the per-channel page memory)
uint8_t encBankMemOf(uint8_t ch) {
  if (ch < 1 || ch > 16) return 0;
  return (encBankMem[ch - 1] < 4) ? encBankMem[ch - 1] : 0;
}

void encBankMemSet(uint8_t ch, uint8_t v) {
  if (ch < 1 || ch > 16) return;
  encBankMem[ch - 1] = (v < 4) ? v : 0;
}

bool encM3MemOf(uint8_t ch) {
  return ch >= 1 && ch <= 16 && encM3Mem[ch - 1];
}

void encM3MemSet(uint8_t ch, bool on) {
  if (ch >= 1 && ch <= 16) encM3Mem[ch - 1] = on;
}

// CC number encoder ei (0..7) sends: ENC1..4 = CC17..20 (fixed), the
// rest = the current bank's block of four in the active page group
uint8_t encCcNum(uint8_t ei) {
  if (ei < 4) return (uint8_t)(17 + ei);
  uint8_t base = encM3Sel ? 20 : 0;
  return (uint8_t)(base + encBank() * 4 + (ei - 4) + 1);
}

uint8_t encCcValue(uint8_t idx) {
  if (idx >= 8) return 0;
  return (uint8_t)ccVal[encCcNum(idx) - 1][encChannel() - 1];
}

// incoming CC from the DAW: sync the knob positions of CC1..CC36 on any
// channel (store only - never re-sent, so no feedback loop; deduped so an
// echo of our own move doesn't repaint the screen)
void encCcIncoming(uint8_t ch, uint8_t cc, uint8_t val) {
  if (ch < 1 || ch > 16 || cc < 1 || cc > 36) return;
  if (ccVal[cc - 1][ch - 1] == (int16_t)(val & 0x7F)) return;
  ccVal[cc - 1][ch - 1] = val & 0x7F;
  if (modeBase == MODE_ENC) refreshMainScreen();   // knobs are on screen
}

// EEPROM persistence (ENCMODE4 + REC saves, boot loads).  This block is
// magic | encCh | 576 values (cc*16+ch) | checksum = 579 bytes; a blank or
// corrupt block (including a save from an older 8- or 20-CC layout) just
// keeps the defaults (zeros).
//
// It used to start at address 0, which is where the block grew INTO the SEQ
// block at 160 (Keyboard.cpp) when the CC set went from 8/20 CCs to 36 - the
// two overlapped by 419 bytes, so every save of one invalidated the other
// and nothing survived no matter how often you pressed REC.  The CC block
// now sits after the SEQ block: 160 + 1398 = 1558, so 1600..2178 is free.
static const uint8_t CC_EE_MAGIC = 0xCE;
static const uint16_t CC_EE_ADDR = 1600;
static const uint16_t CC_EE_LEN = 579;

// The store itself.  On the SC01 the EEPROM is NVS - emulated - so put() and
// get() do nothing at all until begin() has opened the region, and a put()
// only reaches flash on commit().  The Teensy 4.1 writes its emulated EEPROM
// on every put() and has neither call, hence the guards.  Both are called for
// the SEQ block too (Keyboard.cpp), so the whole layout is opened once here.
#define CFG_EE_BYTES 2200   // 160 pad + 1398 SEQ + 579 CC = 2137, rounded

void cfgStoreBegin() {
#ifdef use_SC01Plus
  EEPROM.begin(CFG_EE_BYTES);
#endif
}

void cfgStoreCommit() {
#ifdef use_SC01Plus
  EEPROM.commit();
#endif
}

void encCcSave() {
  uint8_t b[CC_EE_LEN];
  b[0] = CC_EE_MAGIC;
  b[1] = encChannel();
  for (uint8_t e = 0; e < 36; e++)
    for (uint8_t c = 0; c < 16; c++)
      b[2 + e * 16 + c] = (uint8_t)ccVal[e][c] & 0x7F;
  uint8_t sum = 0;
  for (uint16_t i = 0; i < CC_EE_LEN - 1; i++) sum += b[i];
  b[CC_EE_LEN - 1] = sum;
  EEPROM.put(CC_EE_ADDR, b);   // put() writes only the bytes that changed
  cfgStoreCommit();            // ...and commit() is what reaches NVS
}

void encCcLoad() {
  uint8_t b[CC_EE_LEN];
  EEPROM.get(CC_EE_ADDR, b);
  uint8_t sum = 0;
  for (uint16_t i = 0; i < CC_EE_LEN - 1; i++) sum += b[i];
  if (b[0] != CC_EE_MAGIC || sum != b[CC_EE_LEN - 1]) return;
  for (uint8_t e = 0; e < 36; e++)
    for (uint8_t c = 0; c < 16; c++)
      ccVal[e][c] = b[2 + e * 16 + c] & 0x7F;
  encSetChannel(b[1]);
}

// KEYMODE4: the M2 TONALITY menu borrows the four tonality knobs that
// otherwise only live on the PREV overlay (OCT / SCALE / KEY / FSCALE), s
// they can be dialled without holding PREV.  One predicate for the four
// dispatch guards below
// the M2 TONALITY page owns the encoder row while it is open - and it has
// FOUR banks, so the test is a range rather than one set id
static bool k4TonalityRow() {
  EncSet e = currentBinding().enc;
  return e >= ENC_KEY4_MENU && e <= ENC_KEY4_MENU4;
}

void handleEncoderEvents() {
  uint8_t ei;
  int16_t ed;
  bool any = false;
  for (uint8_t n = 0; n < MAX_EVENTS_PER_LOOP && popEncEvent(ei, ed); n++) {
    // DEVICE SETTINGS (header tap): ENC1 = LED brightness, ENC2 = screen
    // brightness, ENC5 = clock source.  Every other knob is idle, so nothing
    // behind the page can be dialled by accident
    if (settingsOpen()) {
      if (ed != 0) settingsAdjust(ei, ed);
      continue;
    }

    // SEQMODE3: only the two knobs that change what the step row itself
    // shows - pattern LENGTH (ENC2) and SPEED (ENC4) - take the pads back
    // from the channel selector for the rest of the hold (so the playhead,
    // recorded steps and the pattern length read again).  FOLLOW (ENC3),
    // INPUT CHANNEL (ENC5), TRACK (ENC6), TYPE (ENC7), TEMPO (ENC8) and OCT
    // (ENC1) leave the picker up: they are set-up knobs dialled while the
    // channel is being chosen, and dropping the picker there meant holding
    // SEQ again just to pick the next channel.  The overlay rows (PREV, bar
    // pad, held step) keep it too - their ENC2/ENC4 mean something else
    if (ed != 0 && modeBase == MODE_SEQ && modeVariant == 3 &&
        currentBinding().enc <= ENC_SEQ4 && (ei == 1 || ei == 3) &&
        seq3ChSelDismiss()) {
      refreshAllLeds();
      // the pads are back on the sequencer: the left slot stops claiming
      // to be the channel picker (the turn's own message follows below)
      headerSetLeft(modeHeaderLabel());
    }

    // KEYMODE4 TONALITY page (M2): the open bank's slot for this encoder.
    // Six slots mirror a control that lives elsewhere (KEY / SCALE / FSCALE /
    // SURP / VOICE-LEAD / PHRASE) and forward the turn, so the page and the
    // PREV overlay can never disagree; the rest store their own value
    if (modeBase == MODE_KEY && modeVariant == 4 && ed != 0 &&
        k4TonalityRow()) {
      uint8_t slot = (uint8_t)(k4TonaBank() * 8 + ei);
      uint8_t mir = k4TonaMirror(slot);
      if (mir == 1) {                       // KEY
        keyRootAdjust(ed);
        refreshAllLeds();
        k4Refresh();
        k4Announce();
      } else if (mir == 2) {                // SCALE
        keyScaleAdjust(ed);
        refreshAllLeds();
        k4Refresh();
        k4Announce();
      } else if (mir == 3) {                // FSCALE
        chordFScaleAdjust(ed);
      } else if (mir == 4) {                // SURP
        k4KnobAdjust(6, ed);
        k4Regenerate();
      } else if (mir == 5) {                // VOICE-LEAD
        if ((ed > 0) != k4Lead()) k4LeadToggle();
      } else if (mir == 6) {                // PHRASE
        k4PhraseAdjust(ed);
      } else {
        k4TonaAdjust(slot, ed);
      }
      char hb[22];
      snprintf(hb, sizeof(hb), "%s %s", k4TonaName(slot), k4TonaLabel(slot));
      headerShow(hb);
      refreshMainScreen();
      if (logRoom(64)) {
        Serial.print("  ");
        Serial.println(hb);
      }
      any = true;
    }

    // KEYMODE: ENC1 = octave -2..8 (clamped, no wrap; 4 counts = 1 octave).
    // KEYMODE4: the main row owns ENC1 for GENRE - the octave lives on the
    // PREV overlay row and on the M2 tonality menu (same composite guard
    // the scale row uses)
    if (modeBase == MODE_KEY && ei == 0 && ed != 0 &&
        (modeVariant != 4 || currentBinding().enc == ENC_KEY4_PREV)) {
      keyOctaveAdjust(ed);
      headerAnnounceInt("Octave", keyOctave());
      if (logRoom(48)) {
        Serial.print("  octave ");
        Serial.println(keyOctave());
      }
      any = true;
    }

    // KEYMODE: ENC2 = scale (every variant, and the PREV overlay row);
    // 4 counts = 1 scale, clamped 0..12, no wrap.  Selecting a scale
    // flashes its mood ("Happy", "Sad", ...) in the temporary header -
    // in KEYMODE4 the persistent progression behind it re-ranks live
    if (modeBase == MODE_KEY && ei == 1 && ed != 0 &&
        (modeVariant != 4 || currentBinding().enc == ENC_KEY4_PREV)) {
      keyScaleAdjust(ed);
      refreshAllLeds();             // in-scale / out-of-scale keys repaint
      if (modeVariant == 4) {
        k4Refresh();                // the framework follows the scale
        k4Announce();               // the sticky header shows the ranking
      }
      // the MOOD only ("Scale Happy", "Scale Sad"): the name is already on
      // the knob's own value line, so repeating it here just squeezed the
      // interesting part off the strip
      headerAnnounce("Scale", keyScaleMood());
      if (logRoom(48)) {
        Serial.print("  scale ");
        Serial.println(keyScaleName());
      }
      any = true;
    }

    // KEYMODE1/2: ENC3 = KEY (the key/root) - shared by both variants: it
    // roots the scale mask in KEYMODE1 and the chord engine in KEYMODE2.
    // KEYMODE4: same, but only while its PREV overlay row is up (the main
    // row uses ENC3 for TENS)
    if (modeBase == MODE_KEY &&
        (modeVariant == 1 || modeVariant == 2 ||
         currentBinding().enc == ENC_KEY4_PREV) &&
        ei == 2 && ed != 0) {
      keyRootAdjust(ed);
      refreshAllLeds();           // the scale mask is rooted here too
      if (modeVariant == 4) {     // the framework follows the root
        k4Refresh();
        k4Announce();
      }
      headerAnnounce("Key", keyRootName());
      if (logRoom(48)) {
        Serial.print("  root ");
        Serial.println(keyRootName());
      }
      any = true;
    }

    // KEYMODE4 + step held: the overlay row edits THAT step - ENC1 OCT
    // (playback transpose, GNR8's -2..+2 stored separately), ENC2 DEG
    // (shift every tone one scale degree in the key), ENC3 INV (cycle the
    // bass up / top down an octave), ENC4 VOICE (spread/close the upper
    // voices around the bass).  Every turn echoes into the header
    if (modeBase == MODE_KEY && modeVariant == 4 && ed != 0 &&
        currentBinding().enc == ENC_KEY4_STEP && ei <= 3 &&
        k4StepSelGet() >= 0) {
      uint8_t stp = (uint8_t)k4StepSelGet();
      char hb[32];
      char dgs[8], ivs[8];
      if (ei == 0) {
        stepOctSet(stp, (int8_t)(stepOctAt(stp) + ((ed > 0) ? 1 : -1)));
        int8_t nv = stepOctAt(stp);
        if (nv == 0) snprintf(hb, sizeof(hb), "Step %u Octave 0", stp + 1);
        else snprintf(hb, sizeof(hb), "Step %u Octave %+d", stp + 1, nv);
        headerShow(hb);
        if (logRoom(48)) { Serial.print("  "); Serial.println(hb); }
      } else if (ei == 1) {
        int8_t dd = (ed > 12) ? 12 : (ed < -12) ? -12 : (int8_t)ed;
        stepDegAdjust(stp, dd);
        if (k4StepLabels(stp, dgs, sizeof(dgs), NULL, 0)) {
          snprintf(hb, sizeof(hb), "Step %u Degree %s", stp + 1, dgs);
          headerShow(hb);
          if (logRoom(48)) { Serial.print("  "); Serial.println(hb); }
        }
      } else if (ei == 2) {
        stepInvAdjust(stp, (int8_t)((ed > 0) ? 1 : -1));
        if (k4StepLabels(stp, NULL, 0, ivs, sizeof(ivs))) {
          snprintf(hb, sizeof(hb), "Step %u Inversion %s", stp + 1, ivs);
          headerShow(hb);
          if (logRoom(48)) { Serial.print("  "); Serial.println(hb); }
        }
      } else {
        stepVoiAdjust(stp, (int8_t)((ed > 0) ? 1 : -1));
        uint8_t n = 0;
        const uint8_t *nt = stepChordAt(stp, &n);
        if (n) {
          uint8_t lo = nt[0], hi = nt[0];
          for (uint8_t i = 1; i < n; i++) {
            if (nt[i] < lo) lo = nt[i];
            if (nt[i] > hi) hi = nt[i];
          }
          snprintf(hb, sizeof(hb), "Step %u Voicing %u oct", stp + 1,
                   (uint8_t)((hi - lo) / 12));
          headerShow(hb);
          if (logRoom(48)) { Serial.print("  "); Serial.println(hb); }
        }
      }
      any = true;
    }

    // KEYMODE4: the main context row = ENC1 GENRE (1 count = 1 genre,
    // wraps), ENC2..8 = MOOD/TENS/MOTION/CAD/CPX/PHRASE/SURP (1 count =
    // 1 unit, clamped 0..100).  Every change re-ranks the proposals AND
    // re-generates every unlocked step live, so the progression playing
    // in the sequencer follows the knobs in real time (locked steps are
    // never touched; the first count of a tweak burst claims the undo
    // slot, so M3-hold restores the pre-tweak progression)
    if (modeBase == MODE_KEY && modeVariant == 4 && ed != 0 &&
        currentBinding().enc == ENC_KEY4) {
      if (ei == 0) {
        k4GenreAdjust(ed);
        headerAnnounce("Genre", k4GenreName());
        if (logRoom(48)) {
          Serial.print("  genre ");
          Serial.println(k4GenreName());
        }
      } else if (ei == 6) {
        // PHRASE took the ERA slot: the template is a word, not a 0..100
        // bias, and it is the one knob that decides whether the pattern has
        // an arc at all - it should not be two gestures deep
        k4PhraseAdjust(ed);
        headerAnnounce("Phrase", k4PhraseName());
        if (logRoom(48)) {
          Serial.print("  phrase ");
          Serial.println(k4PhraseName());
        }
      } else {
        k4KnobAdjust((uint8_t)(ei - 1), ed);
        static const char* const kn[7] = { "Mood", "Tension", "Motion",
                                           "Cadence", "Complexity", "Era",
                                           "Surprise" };
        headerAnnounceInt(kn[ei - 1], (int)k4Knob((uint8_t)(ei - 1)));
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(kn[ei - 1]);
        }
      }
      k4Regenerate();       // the written progression follows the knobs
      refreshAllLeds();   // the keyboard repaints the re-ranked proposals
      k4Announce();       // ...and the header names them (I V vi IV)
      // the word-valued knobs say the word LAST, so the proposals line
      // cannot scroll it away: "Mood MIXED", "Cadence DECEPTIVE", "Phrase
      // AABA" (the row keeps the short form: MIXED / V-vi / AABA)
      if (ei == 1) headerAnnounce("Mood", k4MoodName());
      else if (ei == 4) headerAnnounce("Cadence", k4CadenceName());
      else if (ei == 6) headerAnnounce("Phrase", k4PhraseName());
      any = true;
    }

    // KEYMODE4: the PREV overlay row = ENC1 Octave / ENC2 Scale / ENC3 Key
    // / ENC4 = Force-to-scale (same global ON/OFF as KEYMODE2's ENC4: with
    // it on, sequencer playback snaps stored notes to the current scale,
    // so turning the scale recolors recorded progressions live)
    // EN5-EN8 = embellishment encoders: SUB prob / INV bias / PAS amt / COL amt
    if (modeBase == MODE_KEY && modeVariant == 4 && ei >= 3 && ei <= 7 && ed != 0 &&
        currentBinding().enc == ENC_KEY4_PREV) {
      if (ei == 3) {
        chordFScaleAdjust(ed);
        headerAnnounce("Force To Scale", chordFScale() ? "on" : "off");
        if (logRoom(48)) {
          Serial.print("  fscale ");
          Serial.println(chordFScale() ? "ON" : "OFF");
        }
      } else if (ei == 4) {       // EN5 = SUB prob
        k4EmbSubProbAdjust(ed);
        headerAnnounceInt("Substitution", k4EmbSubProb(), "%");
        if (logRoom(48)) {
          Serial.print("  sub% ");
          Serial.println(k4EmbSubProb());
        }
      } else if (ei == 5) {       // EN6 = INV bias
        k4EmbInvBiasAdjust(ed);
        headerAnnounceInt("Inversion Bias", k4EmbInvBias(), "%");
        if (logRoom(48)) {
          Serial.print("  inv ");
          Serial.println(k4EmbInvBias());
        }
      } else if (ei == 6) {       // EN7 = PAS amt
        k4EmbPassAmtAdjust(ed);
        headerAnnounceInt("Passing Amount", k4EmbPassAmt(), "%");
        if (logRoom(48)) {
          Serial.print("  pas% ");
          Serial.println(k4EmbPassAmt());
        }
      } else if (ei == 7) {       // EN8 = COL amt
        k4EmbColorAmtAdjust(ed);
        headerAnnounceInt("Colour Amount", k4EmbColorAmt(), "%");
        if (logRoom(48)) {
          Serial.print("  col% ");
          Serial.println(k4EmbColorAmt());
        }
      }
      any = true;
    }

    // KEYMODE4: the sticky EMB menu row = ENC1 = embellish mode (off ->
    // pass -> pass+sub, wraps), ENC2 = passing placement (odd / distance,
    // two-state: CW one way, CCW the other), ENC3 = ERA.  The other five
    // are "--"
    if (modeBase == MODE_KEY && modeVariant == 4 && k4EmbMenu() && ed != 0 &&
        currentBinding().enc == ENC_KEY4_EMB) {
      if (ei == 0) {
        k4EmbModeAdjust(ed);
        headerAnnounce("Embed Mode", k4EmbModeName());
        if (logRoom(48)) { Serial.print("  embed "); Serial.println(k4EmbModeName()); }
      } else if (ei == 1) {
        k4EmbPlaceAdjust(ed);
        headerAnnounce("Embed Place", k4EmbPlaceName());
        if (logRoom(48)) { Serial.print("  place "); Serial.println(k4EmbPlaceName()); }
      } else if (ei == 2) {
        // ERA moved off the encoder row (PHRASE took the slot).  Unlike the
        // two entries above it is generative, so it regenerates the way the
        // row knobs do - otherwise the turn would look dead until the next
        k4KnobAdjust(5, ed);
        headerAnnounceInt("Era", (int)k4Knob(5));
        k4Regenerate();
        refreshAllLeds();
        k4Announce();
        headerAnnounceInt("Era", (int)k4Knob(5));
        if (logRoom(48)) { Serial.print("  era "); Serial.println(k4Knob(5)); }
      }
      any = true;
    }

    // KEYMODE2: ENC4 = Force-to-scale (FScale), ENC5..ENC7 = bass/spread/
    // density, ENC8 = latch.  With PREV held the row remaps: ENC4 = latch,
    // ENC5 = KeyLock, ENC6 = Perf (state only)
    if (modeBase == MODE_KEY && modeVariant == 2 && ei != 2 && ed != 0) {
      ModeBinding b = currentBinding();
      if (b.enc == ENC_KEY2_PREV) {
        if (ei == 3) chordLatchAdjust(ed);
        else if (ei == 4) chordKeyLockAdjust(ed);
        else if (ei == 5) chordPerfAdjust(ed);
        if (ei >= 3 && ei <= 5) {
          refreshAllLeds();         // latch off silences the chord's tint
          headerAnnounce(ei == 3 ? "Chord Latch" :
                         ei == 4 ? "Chord Key Lock" : "Chord Performance",
                         ei == 3 ? (chordLatch() ? "on" : "off") :
                         ei == 4 ? (chordKeyLock() ? "on" : "off") :
                                   (chordPerf() ? "on" : "off"));
          if (logRoom(48)) {
            Serial.print(ei == 3 ? "  latch " : ei == 4 ? "  klock " : "  perf ");
            Serial.println(ei == 3 ? (chordLatch() ? "ON" : "OFF") :
                           ei == 4 ? (chordKeyLock() ? "ON" : "OFF") :
                                     (chordPerf() ? "ON" : "OFF"));
          }
          any = true;
        }
      } else if (ei == 7) {
        chordLatchAdjust(ed);       // main row: ENC8 = latch
        refreshAllLeds();
        headerAnnounce("Chord Latch", chordLatch() ? "on" : "off");
        if (logRoom(48)) {
          Serial.print("  latch ");
          Serial.println(chordLatch() ? "ON" : "OFF");
        }
        any = true;
      } else if (ei == 3) {
        chordFScaleAdjust(ed);      // main row: ENC4 = Force-to-scale
        refreshAllLeds();
        headerAnnounce("Force To Scale", chordFScale() ? "on" : "off");
        if (logRoom(48)) {
          Serial.print("  fscale ");
          Serial.println(chordFScale() ? "ON" : "OFF");
        }
        any = true;
      } else if (ei >= 4 && ei <= 6) {
        chordParamAdjust(ei - 3, ed);   // BASS / SPRD / DENS
        static const char* const pn[4] = { "", "Chord Bass", "Chord Spread",
                                           "Chord Density" };
        headerAnnounceInt(pn[ei - 3], (int)chordParam(ei - 3));
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(pn[ei - 3]);
        }
        any = true;
      }
    }

    // KEYMODE3: ENC3 = ENCOD, ENC4 = KBOARD - channel-link switches,
    // states free/locked (two-state, NO wrap: CW saturates at locked,
    // CCW at free - a turn never flips past the end).  LOCKED = this
    // family's channel moves with the SEQ edit channel (the hub) and with
    // the other locked family (see channelLinkPropagate); FREE = each mode
    // keeps its own independent channel, as always
    if (modeBase == MODE_KEY && modeVariant == 3 && ed != 0 &&
        (ei == 2 || ei == 3)) {
      bool want = (ed > 0);          // CW = locked, CCW = free
      if (ei == 2) {
        if (want != chanEncLocked()) {
          chanEncLinkSet(want);
          headerShow(want ? "Encoder Link locked" : "Encoder Link free");
          if (logRoom(48)) {
            Serial.print("  encod link ");
            Serial.println(want ? "locked" : "free");
          }
          any = true;
        }
      } else {
        if (want != chanKeyLocked()) {
          chanKeyLinkSet(want);
          headerShow(want ? "Keyboard Link locked" : "Keyboard Link free");
          if (logRoom(48)) {
            Serial.print("  kboard link ");
            Serial.println(want ? "locked" : "free");
          }
          any = true;
        }
      }
    }

    // ENCMODE: ENC1..ENC4 send CC17..CC20, ENC5..ENC8 send the F bank's
    // block (F1 = CC1..4 ... F4 = CC13..16) on the encoder channel
    // (ENCMODE3); 1 count = 1 value step, clamped 0..127, no wrap;
    // positions persist across mode changes like a real knob
    // ENCABLETONMODE: the eight knobs send CC64..71 on the encoder channel
    // (a NEW automap block - the ENCMODE CC map below is untouched, so
    // every MIDI-learn map made there keeps working) and the same turn is
    // announced over SysEx, because a remote script cannot rely on hearing a
    // CC on all 16 channels
    if (modeBase == MODE_ENC && abltOn() && ei < 8 && ed != 0) {
      uint8_t acch = encChannel();
      int16_t av = (int16_t)linkParamValue(ei) + ed;
      if (av < 0) av = 0;
      if (av > 127) av = 127;
      if (av != (int16_t)linkParamValue(ei)) {
        linkParamTouch(ei, (uint8_t)av);
        usbMIDI.sendControlChange((uint8_t)(64 + ei), (uint8_t)av, acch);
        linkKnob(ei, (uint8_t)av);
        const char* pn = linkParamName(ei);
        char ab[26];
        snprintf(ab, sizeof(ab), "%s %d", pn[0] ? pn : "Param", (int)av);
        headerShow(ab);
        if (logRoom(48)) {
          Serial.print("  live CC ");
          Serial.print(64 + ei);
          Serial.print("=");
          Serial.println(av);
        }
      }
      any = true;
    }

    if (modeBase == MODE_ENC && !abltOn() && ei < 8 && ed != 0) {
      uint8_t cch = encChannel();
      uint8_t cc = encCcNum(ei);
      int16_t v = ccVal[cc - 1][cch - 1] + ed;
      if (v < 0) v = 0;
      if (v > 127) v = 127;
      if (v != ccVal[cc - 1][cch - 1]) {
        ccVal[cc - 1][cch - 1] = v;
        usbMIDI.sendControlChange(cc, (uint8_t)v, cch);
        char ccb[24];
        snprintf(ccb, sizeof(ccb), "CC %u = %d", (unsigned)cc, (int)v);
        headerShow(ccb);
        if (logRoom(48)) {
          Serial.print("  CC ");
          Serial.print(cc);
          Serial.print("=");
          Serial.println(v);
        }
      }
      any = true;
    }

    // SEQMODE: ENC1 = octave shift of the selected sequence (all variants)
    // SEQMODE3: ENC4 = pattern SPEED of the edit sequence (9 rungs, 6 =
    //           normal; sync moved to ENCMODE3+PLAY)
    //           ENC5 = input channel of the edit sequence, 1..16
    //           ENC6 = TRACK DRUM/MONO/POLY (three-state, no wrap)
    //           ENC7 = TYPE ROOT/BASS/ARP (MONO sequences only, no wrap)
    //           ENC8 = tempo 40..240 (moved from ENC1)
    if (modeBase == MODE_SEQ) {
      if (ed != 0 && currentBinding().enc == ENC_SEQ_ROT_OV) {
        // SHIFT/TRANS overlay (PREV held): ENC1 = rotate the pattern,
        // 4 counts = 1 step (GNR8 page's old ROT row, same knob); ENC2 =
        // semitone TRANS of the sequence's playback, 1 count = 1 st,
        // -12..+12 (DRUM tracks refuse the turn).  The other six idle
        if (ei == 0) {
          seqGenShiftAdjust(ed);
          char rb[24];
          snprintf(rb, sizeof(rb), "Pattern Start %u", seqGenShiftPos() + 1);
          headerShow(rb);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(rb);
          }
        } else if (ei == 1) {
          seqTransAdjust(ed);
          char tb[24];
          int8_t tv = (int8_t)seqTrans();
          if (tv >= 0) snprintf(tb, sizeof(tb), "Transpose +%d", (int)tv);
          else         snprintf(tb, sizeof(tb), "Transpose %d", (int)tv);
          headerShow(tb);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(tb);
          }
        } else if (ei == 2) {
          // ENC3 = per-track scale-lock (Klock): snap the TRANS result back
          // into the current scale so the semitone knob stays diatonic.
          // Two-state, NO wrap (like ENCOD/KBOARD): CW = ON, CCW = OFF - a
          // continuous turn saturates at the end instead of toggling on
          // every count; only a change repaints/echoes
          if (seqKlockSetDir(ed)) {
            headerShow(seqKlock() ? "Scale Lock on" : "Scale Lock off");
            if (logRoom(48)) {
              Serial.print("  klock ");
              Serial.println(seqKlock() ? "ON" : "OFF");
            }
          }
        }
        any = true;
      } else if (ed != 0 && currentBinding().enc == ENC_SEQ_BAR_OV &&
                 seqBarEditSel() < 4) {
        // SEQMODE3 held-bar row: a C D E F pad is down, so ENC1/ENC2 edit
        // THAT bar's playback shift - OCT (-2..+2, the step OCT's range)
        // and TRANS (-12..+12 st).  The stored chords never change; the
        // shift folds into the track's own octave/TRANS at playback, so a
        // Klock track snaps the transposed bar back into scale.  DRUM
        // tracks have no pitch to move and say so instead of doing nothing
        uint8_t bar = seqBarEditSel();
        char bb[24];
        if (seqTrack() == SEQ_TRACK_DRUM)
          snprintf(bb, sizeof(bb), "Bar %u Drum Track", bar + 1);
        else if (ei == 0)
          snprintf(bb, sizeof(bb), "Bar %u Octave %+d", bar + 1,
                   (int)seqBarOctAdjust(ed));
        else if (ei == 1)
          snprintf(bb, sizeof(bb), "Bar %u Transpose %+d", bar + 1,
                   (int)seqBarTrAdjust(ed));
        else
          snprintf(bb, sizeof(bb), "Bar %u", bar + 1);
        headerShow(bb);
        refreshMainScreen();       // the value column follows the turn
        if (logRoom(48)) {
          Serial.print("  ");
          Serial.println(bb);
        }
        any = true;
      } else if (ed != 0 && currentBinding().enc == ENC_SEQ_STEPOV &&
                 seqTrStepSel >= 0) {
        // held-step overlay in SEQMODE (non-DRUM): ENC1 = the held step's
        // TRANS offset, 1 count = 1 st, -12..+12 - a playback-only
        // modifier like the step's octave, the stored chord never changes
        if (ei == 0) {
          int8_t cur = stepTrAt((uint8_t)seqTrStepSel);
          int16_t nv = (int16_t)cur + ed;
          if (nv < -12) nv = -12;
          if (nv > 12) nv = 12;
          stepTrSet((uint8_t)seqTrStepSel, (int8_t)nv);
          char tb[28];
          if (nv >= 0)
            snprintf(tb, sizeof(tb), "Step %d Transpose +%d", seqTrStepSel + 1, nv);
          else
            snprintf(tb, sizeof(tb), "Step %d Transpose %d", seqTrStepSel + 1, nv);
          headerShow(tb);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(tb);
          }
        }
        any = true;
      } else if (ed != 0 && currentBinding().enc == ENC_SEQ_SKANK_STEP &&
                 seqTrStepSel >= 0) {
        // the SKANK lock row (a step held on a POLY+SKANK track): ENC1..7
        // override THAT step's derivation knob - turning while "inherit"
        // grabs the track's page value and nudges it, turning under 0
        // clears the lock back to inherit (0 is a legal value, the lock
        // storage keeps it distinct); ENC8 = the step's TRANS like the
        // plain held-step row (NOT% stays page-level, no lock slot)
        int8_t stp = seqTrStepSel;
        if (ei < 7) {
          static const char *nm[7] = { "Tessitura", "Motion", "Shape",
                                       "Thinning", "Extension", "Substitution",
                                       "Tessitura" };
          int16_t cur = stepSkankLockAt((uint8_t)stp, (uint8_t)ei);
          if (cur < 0) cur = skankPrm((uint8_t)ei);   // inherit: grab the knob
          int16_t nv = cur + ed;
          if (nv > 100) nv = 100;
          if (nv < -1) nv = -1;
          stepSkankLockSet((uint8_t)stp, (uint8_t)ei, nv);
          char tb[32];
          if (nv < 0)
            // the long knob names (Substitution / Random Tessitura) do not
            // fit behind a two-digit step number, and the held pad already
            // is the step: name the parameter and its "auto" value only
            snprintf(tb, sizeof(tb), "%s auto", nm[ei]);
          else if (ei == 2)
            snprintf(tb, sizeof(tb), "Step %d Shape %s", stp + 1,
                     skankShpName((uint8_t)nv));
          else if (ei == 6)
            snprintf(tb, sizeof(tb), "Step %d Tessitura %d%%", stp + 1, nv);
          else
            snprintf(tb, sizeof(tb), "Step %d %s %d", stp + 1, nm[ei], nv);
          headerShow(tb);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(tb);
          }
        } else if (ei == 7) {      // ENC8 = the step's TRANS (shared knob)
          int8_t cur = stepTrAt((uint8_t)stp);
          int16_t nv = (int16_t)cur + ed;
          if (nv < -12) nv = -12;
          if (nv > 12) nv = 12;
          stepTrSet((uint8_t)stp, (int8_t)nv);
          char tb[28];
          if (nv >= 0)
            snprintf(tb, sizeof(tb), "Step %d Transpose +%d", stp + 1, nv);
          else
            snprintf(tb, sizeof(tb), "Step %d Transpose %d", stp + 1, nv);
          headerShow(tb);
          if (logRoom(48)) {
            Serial.print("  ");
            Serial.println(tb);
          }
        }
        any = true;
      } else if (genPageActive() && genOptSel != 0xFF) {
        // armed F radio: the sub-page swallows the whole row.  F1 = the
        // FULL options page - ENC1 TONES, ENC2 RANGE, ENC3 LOW, ENC4
        // ALGO (4 counts = 1 detent = 1 value, clamped, no wrap).  F2 =
        // STEP OCT on ENC2 (the tapped step) + MAP mode on ENC3, F3 =
        // BORROW on ENC3, F4 = MOD PROB on ENC4; ENC5-8 idle
        if (genOptSel == 0) {
          if (ei == 0) {
            seqGenTonesAdjust(ed);
            genOptionHeader(0);
          } else if (ei == 1) {
            seqGenRangeAdjust(ed);
            genOptionHeader(2);
          } else if (ei == 2) {
            seqGenLowAdjust(ed);
            genOptionHeader(1);
          } else if (ei == 3) {
            seqGenAlgoAdjust(ed);
            genOptionHeader(3);
          }
        } else if (genOptSel == 1 && (ei == 1 || ei == 2)) {
          if (ei == 1) {                    // ENC2: the step's octave
            int8_t ss = genStepSelStep();
            if (ss < 0) headerShow("Pick a Step");
            else {
              int v = (int)stepOctAt((uint8_t)ss) + genModStep(0, ed);
              if (v < -2) v = -2;
              if (v > 2) v = 2;
              stepOctSet((uint8_t)ss, (int8_t)v);
              genOptionHeader(1);
            }
          } else {                          // ENC3: mapping mode
            seqMapAdjust(ed);
            char mb[24];
            snprintf(mb, sizeof(mb), "Kit Map %s", seqMapName(seqMap()));
            headerShow(mb);
            if (logRoom(48)) {
              Serial.print("  ");
              Serial.println(mb);
            }
          }
        } else if (genOptSel == 2 && ei == 2) {
          int8_t ss = genStepSelStep();     // ENC3: borrowed chord tone
          if (ss < 0) headerShow("Pick a Step");
          else {
            int v = (int)stepBrwAt((uint8_t)ss) + genModStep(1, ed);
            if (v < 0) v = 0;
            if (v > 8) v = 8;
            stepBrwSet((uint8_t)ss, (uint8_t)v);
            genOptionHeader(2);
          }
        } else if (genOptSel == 3 && ei == 3) {
          int8_t ss = genStepSelStep();     // ENC4: modifier probability
          if (ss < 0) headerShow("Pick a Step");
          else {
            int v = (int)stepModPAt((uint8_t)ss) + genModStep(2, ed);
            if (v < 0) v = 0;
            if (v > 100) v = 100;
            stepModPSet((uint8_t)ss, (uint8_t)v);
            genOptionHeader(3);
          }
        }
        any = true;
      } else if (modeVariant == 4 && ed != 0 && !harmPageActive() &&
                 !compPageActive() && !skankPageActive() && !drumPageActive() &&
                 !(arpPageActive() && arpSlotAt(arpOptionSel(), 0) >= 0) &&
                 (ei == 0 ||
                  (ei == 1 && !genPageActive() && !leadPageActive() &&
                   !(arpPageActive() && arpSlotAt(arpOptionSel(), 1) >= 0)))) {
        // SEQMODE 4 is the timing screen: ENC1 = the per-track swing STYLE
        // (a machine emulation - which grid it swings and how deep it can
        // shove the off-steps), ENC2 = the AMT (0 = straight .. 100 = the
        // machine's full swing, 1 count = 1%; the header names the
        // EFFECTIVE ratio).  Variants 1/2 keep ENC1 = OCTAVE / ENC2 idle;
        // a page that owns the slot keeps it here too (the guard list above
        // mirrors the plain-row ownership)
        if (ei == 0) {
          seqSwingStyleAdjust(ed);
          headerAnnounce("Swing", seqSwingStyleName(seqSwingStyle()));
          if (logRoom(48)) {
            Serial.print("  swing ");
            Serial.println(seqSwingStyleName(seqSwingStyle()));
          }
        } else {
          seqSwingAmtAdjust(ed);
          headerAnnounceInt("Swing", seqSwingRatioOf(seqEditChannel() - 1), "%");
          if (logRoom(48)) {
            Serial.print("  swing amt ");
            Serial.println(seqSwingRatioOf(seqEditChannel() - 1));
          }
        }
        any = true;
      } else if (modeVariant == 4 && ed != 0 && (ei == 2 || ei == 3) &&
                 !genPageActive() && !leadPageActive() && !harmPageActive() &&
                 !compPageActive() && !skankPageActive() &&
                 !drumPageActive() && !arpPageActive()) {
        // SEQMODE 4 input row: ENC3 = EXT IN mode (OFF/ON/THRU, clamped,
        // no wrap) - whether the edit track listens to external incoming
        // MIDI; ENC4 = SRC (USB/DIN/BOTH) - which physical input it listens
        // to (DIN = the Serial2 5-pin input).  THRU also forwards the notes
        // it receives (input-channel filtered) out on the track's output
        // channel over USB.  Every page owns ENC3/4 when open, so the guard
        // list above mirrors the plain-row ownership
        if (ei == 2) {
          seqExtInModeAdjust(ed);
          headerAnnounce("Ext In", seqExtInModeName(seqExtInMode()));
          if (logRoom(48)) {
            Serial.print("  extin ");
            Serial.println(seqExtInModeName(seqExtInMode()));
          }
        } else {
          seqExtInSrcAdjust(ed);
          headerAnnounce("Ext Source", seqExtInSrcName(seqExtInSrc()));
          if (logRoom(48)) {
            Serial.print("  extsrc ");
            Serial.println(seqExtInSrcName(seqExtInSrc()));
          }
        }
        any = true;
      } else if (ei == 0 && ed != 0 && modeVariant != 4 && !harmPageActive() &&
                 !compPageActive() && !skankPageActive() && !drumPageActive() &&
                 arpSlotAt(arpOptionSel(), 0) < 0) {
        seqOctaveShiftAdjust(ed);           // octave shift of the edit seq
        // (SEQMODE 4 moves ENC1 to the swing style - its octave stays on
        //  variants 1/2/3; the open HARM page takes ENC1 for its GENRE, the
        //  COMP page for its HITS, the SKANK page for its TES and an armed
        //  ARP sub-page that owns ENC1 for its own knob - handled below; the
        //  table row is 1=OCT, 2=--, 4=SWING)
        headerAnnounceInt("Octave Shift", seqOctaveShift());
        if (logRoom(48)) {
          Serial.print("  octshift ");
          Serial.println(seqOctaveShift());
        }
        any = true;
      } else if (modeVariant != 3) {
        // GNR8 page: ENC2 = fill %, ENC3 = accent %, ENC4 = glide %,
        // ENC7 = tie % - the page's four own knobs; ENC1 (octave), ENC5/6
        // (gate/vel) and ENC8 (prob) keep the bindings they had, and a
        // channel/TYPE/mode change drops the page (composite check)
        if (genPageActive() && (ei == 1 || ei == 2 || ei == 3 || ei == 6)) {
          uint8_t gs = (uint8_t)(seqEditChannel() - 1);
          if (ei == 1) {
            seqGenFillAdjust(ed);
            headerAnnounceInt("Fill", seqGenFillOf(gs), "%");
            if (logRoom(48)) {
              Serial.print("  fill ");
              Serial.print(seqGenFillOf(gs));
              Serial.println("%");
            }
          } else if (ei == 2) {
            seqGenAccAdjust(ed);
            headerAnnounceInt("Accent", seqGenAccOf(gs), "%");
            if (logRoom(48)) {
              Serial.print("  acc ");
              Serial.print(seqGenAccOf(gs));
              Serial.println("%");
            }
          } else if (ei == 3) {
            seqGenGlideAdjust(ed);
            headerAnnounceInt("Glide", seqGenGlideOf(gs), "%");
            if (logRoom(48)) {
              Serial.print("  glide ");
              Serial.print(seqGenGlideOf(gs));
              Serial.println("%");
            }
          } else {
            seqGenTieAdjust(ed);
            headerAnnounceInt("Tie", seqGenTieOf(gs), "%");
            if (logRoom(48)) {
              Serial.print("  tie ");
              Serial.print(seqGenTieOf(gs));
              Serial.println("%");
            }
          }
        // M2 ARP page with an F sub-page armed: that page owns its slots
        // (see arpSlotT).  Slots it does not own fall through to the plain
        // page branch below, so SPAN/SKIP/MODE/INST/GATE/VEL/OCT/PROB stay
        // reachable from inside every sub-page
        } else if (ed != 0 && arpPageActive() &&
                   arpSlotAt(arpOptionSel(), ei) >= 0) {
          uint8_t slot = (uint8_t)arpSlotAt(arpOptionSel(), ei);
          uint8_t seq = (uint8_t)(seqEditChannel() - 1);
          seqArpPAdjust(slot, ed);
          {
            char ab[32];
            snprintf(ab, sizeof(ab), "Arp %s %s", seqArpPName(slot),
                     seqArpPValue(slot, seqArpPOf(seq, slot)));
            headerShow(ab);
          }
          if (logRoom(56)) {
            Serial.print("  ");
            Serial.print(seqArpPName(slot));
            Serial.print(' ');
            Serial.println(seqArpPValue(slot, seqArpPOf(seq, slot)));
          }
        // M2 ARP page: GATE / VEL / PROB are the plain row's knobs and the
        // page always turns the step-edit radio off, which is what those
        // three encoders are normally gated on - so they get their own
        // path here, or the slots read a value a turn can never move.  A
        // sub-page that owns the slot (F3's MEM / BIAS) is handled above
        } else if (ed != 0 && arpPageActive() &&
                   (ei == 4 || ei == 5 || ei == 7) &&
                   arpSlotAt(arpOptionSel(), ei) < 0) {
          if (ei == 4) {
            seqGateAdjust(ed);
            headerAnnounceInt("Gate", seqGatePct(), "%");
            if (logRoom(48)) {
              Serial.print("  gate ");
              Serial.print(seqGatePct());
              Serial.println("%");
            }
          } else if (ei == 5) {
            seqVelocityAdjust(ed);
            headerAnnounceInt("Velocity", seqVelocity());
            if (logRoom(48)) {
              Serial.print("  vel ");
              Serial.println(seqVelocity());
            }
          } else {
            seqProbAdjust(ed);
            headerAnnounceInt("Probability", seqProbPct(), "%");
            if (logRoom(48)) {
              Serial.print("  prob ");
              Serial.print(seqProbPct());
              Serial.println("%");
            }
          }
        // M2 ARP page: ENC2 = span (1..4 octaves), ENC3 = skip rests,
        // ENC4 = mode, ENC7 = insert mode.  These four are the page's own
        // knobs; ENC1 (octave), ENC5/6 (gate/vel) and ENC8 (prob) keep the
        // bindings they had, and a TYPE/channel/mode change drops the page
        } else if (ed != 0 && arpPageActive() &&
            (ei == 1 || ei == 2 || ei == 3 || ei == 6)) {
          uint8_t seq = (uint8_t)(seqEditChannel() - 1);
          if (ei == 1) {
            seqArpSpanAdjust(ed);
            headerAnnounceInt("Arp Span", seqArpSpanOf(seq), " oct");
            if (logRoom(48)) {
              Serial.print("  span ");
              Serial.print(seqArpSpanOf(seq));
              Serial.println(" oct");
            }
          } else if (ei == 2) {
            seqArpSkipToggle(ed);
            headerAnnounce("Arp Skip", seqArpSkipOf(seq) ? "on" : "off");
            if (logRoom(48)) {
              Serial.print("  skip ");
              Serial.println(seqArpSkipOf(seq) ? "ON" : "OFF");
            }
          } else if (ei == 3) {
            seqArpModeAdjust(ed);
            headerAnnounce("Arp Mode", seqArpModeName(seqArpModeOf(seq)));
            if (logRoom(48)) {
              Serial.print("  arpmode ");
              Serial.println(seqArpModeName(seqArpModeOf(seq)));
            }
          } else {
            seqArpInstAdjust(ed);
            headerAnnounce("Arp Insert", seqArpInstName(seqArpInstOf(seq)));
            if (logRoom(48)) {
              Serial.print("  inst ");
              Serial.println(seqArpInstName(seqArpInstOf(seq)));
            }
          }
        // M2 LEAD page: ENC2 = contour (ARCH/DESC/WALK, live), ENC3 = LOW,
        // ENC4 = RANGE (1..5 oct), ENC5 = CT (chord-tone vs scale blend),
        // ENC6 = GD (guide-tone set depth), ENC7 = density % (the shared
        // FILL slot).  ENC1 (octave) and ENC8 (prob) keep their bindings;
        // GATE/VEL live in the M1 edit menu, and a TYPE/channel/mode change
        // drops the page
        } else if (ed != 0 && leadPageActive() &&
            (ei == 1 || ei == 2 || ei == 3 || ei == 4 || ei == 5 ||
             ei == 6)) {
          uint8_t seq = (uint8_t)(seqEditChannel() - 1);
          if (ei == 1) {
            // CONTOUR is live: turning it reshapes the stored sequence in
            // place (the old contour's version is cached, the target's
            // last version restored - or reshaped fresh the first time),
            // no GEN press needed
            bool changed = seqLeadContourTurn(ed);
            if (changed) {
              refreshAllLeds();       // the step LEDs follow the melody
              refreshMainScreen();
            }
            headerAnnounce("Contour", seqLeadContourName(seqLeadContourOf(seq)));
            if (logRoom(48)) {
              Serial.print("  contour ");
              Serial.println(seqLeadContourName(seqLeadContourOf(seq)));
            }
          } else if (ei == 2) {
            seqGenLowAdjust(ed);
            headerAnnounce("Low", seqGenLowName(seqGenLowOf(seq)));
            if (logRoom(48)) {
              Serial.print("  low ");
              Serial.println(seqGenLowName(seqGenLowOf(seq)));
            }
          } else if (ei == 3) {
            seqGenRangeAdjust(ed);
            headerAnnounceInt("Range", seqGenRangeOf(seq), " oct");
            if (logRoom(48)) {
              Serial.print("  range ");
              Serial.print(seqGenRangeOf(seq));
              Serial.println(" oct");
            }
          } else if (ei == 4) {
            seqLeadCtAdjust(ed);          // LEAD CT blend %
            headerAnnounceInt("Chord Tone", seqLeadCtOf(seq), "%");
            if (logRoom(48)) {
              Serial.print("  ct blend ");
              Serial.print(seqLeadCtOf(seq));
              Serial.println("%");
            }
          } else if (ei == 5) {
            seqLeadGdAdjust(ed);          // LEAD GD set depth
            headerAnnounce("Guide Tones", seqLeadGdName(seqLeadGdOf(seq)));
            if (logRoom(48)) {
              Serial.print("  gd set ");
              Serial.println(seqLeadGdName(seqLeadGdOf(seq)));
            }
          } else {
            seqGenFillAdjust(ed);          // LEAD density % (shared FILL)
            headerAnnounceInt("Density", seqGenFillOf(seq), "%");
            if (logRoom(48)) {
              Serial.print("  dens ");
              Serial.print(seqGenFillOf(seq));
              Serial.println("%");
            }
          }
        // M2 HARM page: ENC1 = GENRE (the harmonic grammar), ENC2 = FILL
        // (the HARM track's FILL% - the same per-track roll gate the
        // generators use; M3 lands chords on the rolled steps and clears
        // the rest), ENC3 = SRC (bass/arp source for the feeder read),
        // ENC4 = HOLD (force hold on a repeated bass).  M3 bakes; ENC5..8
        // keep their bindings, and a TYPE/channel/mode change drops the page
        } else if (ed != 0 && harmPageActive() &&
            (ei == 0 || ei == 1 || ei == 2 || ei == 3)) {
          if (ei == 0) {
            k4GenreAdjust(ed);
            headerAnnounce("Genre", k4GenreName());  // the picked genre
            if (logRoom(48)) {
              Serial.print("  genre ");
              Serial.println(k4GenreName());
            }
          } else if (ei == 1) {
            seqGenFillAdjust(ed);         // FILL% of the HARM track itself
            headerAnnounceInt("Fill",
                              seqGenFillOf((uint8_t)(seqEditChannel() - 1)), "%");
          } else if (ei == 2) {
            k4HarmInputAdjust(ed);
            headerAnnounce("Source", k4HarmInputName());
          } else {
            k4HarmHoldAdjust(ed);
            headerAnnounce("Hold", k4HarmHoldName());
          }
        } else if (ed != 0 && drumPageActive()) {
          // DRUM page (M2 on a DRUM track): the armed F set picks which
          // parameter each encoder drives - 0xFF = the base row (GENRE /
          // VAR / FILL / MOT on ENC1..4, the four lane line selectors on
          // ENC5..8), GROOVE = the first four, LINES / FILL% / MOT% = the
          // same four encoders for the per-lane values.  Every turn
          // rebuilds the pattern in place (Drum.cpp takes the undo snapshot
          // once per burst), so a continuous turn undoes as one gesture
          uint8_t set = drumSetSelGet();
          int8_t prm = -1;
          if (set == 0xFF) prm = (ei <= 3) ? (int8_t)ei : (int8_t)(10 + ei - 4);
          else if (set == 0 && ei <= 3) prm = (int8_t)ei;
          else if (set == 1 && ei <= 3) prm = (int8_t)(10 + ei);
          else if (set == 2 && ei <= 3) prm = (int8_t)(20 + ei);
          else if (set == 3 && ei <= 3) prm = (int8_t)(30 + ei);
          static const char *const lane[4] = { "Kick", "Snare", "Hat", "Perc" };
          char db[32];
          db[0] = 0;
          if (prm == 0) {
            drumGenreAdjust(ed);
            snprintf(db, sizeof(db), "Genre %s", drumGenreName(drumGenre()));
          } else if (prm == 1) {
            drumVarAdjust(ed);
            snprintf(db, sizeof(db), "Variation %s", drumVarName(drumVar()));
          } else if (prm == 2) {
            drumFillAdjust(ed);
            snprintf(db, sizeof(db), "Fill %u%%", (unsigned)drumFill());
          } else if (prm == 3) {
            drumMotAdjust(ed);
            snprintf(db, sizeof(db), "Motion %u%%", (unsigned)drumMot());
          } else if (prm >= 10 && prm <= 13) {
            uint8_t ln = (uint8_t)(prm - 10);
            drumLineAdjust(ln, ed);
            snprintf(db, sizeof(db), "%s Line %s", lane[ln],
                     drumLineName(drumLine(ln)));
          } else if (prm >= 20 && prm <= 23) {
            uint8_t ln = (uint8_t)(prm - 20);
            drumLaneFillAdjust(ln, ed);
            snprintf(db, sizeof(db), "%s Fill %u%%", lane[ln],
                     (unsigned)drumLaneFill(ln));
          } else if (prm >= 30 && prm <= 33) {
            uint8_t ln = (uint8_t)(prm - 30);
            drumLaneMotAdjust(ln, ed);
            snprintf(db, sizeof(db), "%s Motion %u%%", lane[ln],
                     (unsigned)drumLaneMot(ln));
          }
          if (prm >= 0) {
            drumProduce();            // live rebuild of the track's steps
            headerShow(db);
            refreshAllLeds();         // the step LEDs follow the pattern
            refreshMainScreen();
            if (logRoom(48)) { Serial.print("  "); Serial.println(db); }
          }
          any = true;
        } else if (ed != 0 && compPageActive() && ei == 0) {
          seqCompHitsAdjust(ed);            // ENC1 = HITS (stabs, 0..8,
                                            // 4 counts = 1)
          char cb[24];
          if (seqCompHits() <= 1) {
            headerShow("Comping off");
          } else {
            snprintf(cb, sizeof(cb), "Comping Hits %d", (int)seqCompHits());
            headerShow(cb);
          }
          if (logRoom(48)) {
            Serial.print("  comp hits ");
            Serial.println(seqCompHits());
          }
        } else if (ed != 0 && skankPageActive()) {
          // the SKANK row: ENC1 TES, ENC2 MOT, ENC3 SHP, ENC4 THN, ENC5
          // EXT, ENC6 SUB, ENC7 TES%, ENC8 NOT% (0..100, 4 counts = 1
          // unit each) - the last two are the per-hit randomizers: TES%
          // rolls the inversion fully at random instead of the walk,
          // NOT% mutates one voicing tone to a random in-scale note
          if (ei < 8) {
            skankPrmAdjust((uint8_t)ei, ed);
            char sb[32];
            if (ei == 2) {
              snprintf(sb, sizeof(sb), "Shape %s", skankShpName(skankPrm(2)));
            } else if (ei == 4 || ei == 5 || ei == 6 || ei == 7) {
              static const char *nm[4] = { "Extension", "Substitution",
                                           "Random Tessitura", "Random Note" };
              snprintf(sb, sizeof(sb), "%s %u%%", nm[ei - 4],
                       skankPrm((uint8_t)ei));
            } else {
              static const char *nm[4] = { "Tessitura", "Motion", "Shape",
                                           "Thinning" };
              snprintf(sb, sizeof(sb), "%s %u", nm[ei], skankPrm((uint8_t)ei));
            }
            headerShow(sb);
            if (logRoom(48)) { Serial.print("  "); Serial.println(sb); }
          }
        // ACCENT velocity of the edit track (ENC7, every track type).  The
        // M2 pages that own ENC7 were dispatched above, and the POLY
        // step-edit FX / TYPE arms take it through seqAccentEncCtx() below.
        // Otherwise the step-edit value encoders: ENC5 = gate %,
        // ENC6 = default velocity (MONO only) or the armed ratchet count
        // (DRUM, target 8), ENC7 = the FX type or the armed step TYPE while
        // those targets are engaged (POLY),
        // ENC8 = the probability of the edit track itself - live, so every
        // PROB step of that track re-rolls at the new rate
        } else if (ei == 6 && ed != 0 && seqAccentEncCtx()) {
          seqAccentVelocityAdjust(ed);
          headerAnnounceInt("Accent Velocity", seqAccentVelocity());
          if (logRoom(48)) {
            Serial.print("  accvel ");
            Serial.println(seqAccentVelocity());
          }
        } else if (ed != 0 && stepEditOn) {
          // M1 step-edit row, variants 1/2: ENC2 = LISTEN (CHORD/NOTE) -
          // what live record writes (NOTE stores the arp output onto the
          // step it sounds on), ENC3 = PLAYBACK (GATE/RECORDED) - engine-
          // shaped vs as-recorded, ENC4 = OVERDUB (OFF/ON) - a fresh
          // gesture replaces the step vs notes stacking onto it.  Per
          // track, clamped (4 counts = 1 state, no wrap).  A page that
          // owns ENC2..4 keeps them while open
          if ((ei == 1 || ei == 2 || ei == 3) &&
              (modeVariant == 1 || modeVariant == 2) &&
              seqTrack() != SEQ_TRACK_DRUM &&
              !genPageActive() && !leadPageActive() && !harmPageActive() &&
              !compPageActive() && !skankPageActive() && !drumPageActive() &&
              !arpPageActive()) {
            if (ei == 1) {
              seqRecListenAdjust(ed);
              headerAnnounce("Listen", seqRecListenName(seqRecListen()));
              if (logRoom(48)) {
                Serial.print("  listen ");
                Serial.println(seqRecListenName(seqRecListen()));
              }
            } else if (ei == 2) {
              seqRecPlaybackAdjust(ed);
              headerAnnounce("Playback",
                             seqRecPlaybackName(seqRecPlayback()));
              if (logRoom(48)) {
                Serial.print("  playback ");
                Serial.println(seqRecPlaybackName(seqRecPlayback()));
              }
            } else {
              seqRecOverdubAdjust(ed);
              headerAnnounce("Overdub", seqRecOverdubName(seqRecOverdub()));
              if (logRoom(48)) {
                Serial.print("  overdub ");
                Serial.println(seqRecOverdubName(seqRecOverdub()));
              }
            }
          } else if (ei == 4 && stepEditCtx()) {
            seqGateAdjust(ed);
            headerAnnounceInt("Gate", seqGatePct(), "%");
            if (logRoom(48)) {
              Serial.print("  gate ");
              Serial.print(seqGatePct());
              Serial.println("%");
            }
          } else if (ei == 5 && stepEditCtx()) {
            seqVelocityAdjust(ed);
            headerAnnounceInt("Velocity", seqVelocity());
            if (logRoom(48)) {
              Serial.print("  vel ");
              Serial.println(seqVelocity());
            }
          } else if (ei == 6 && stepEditTarget == 5 &&
                     seqTrack() == SEQ_TRACK_POLY) {
            // FX type of the pads: sus2 sus4 6 m7 M7 9 color (wrap, 7)
            uint8_t t = (uint8_t)((stepFxType + (ed > 0 ? 1 : 6)) % 7);
            if (t != stepFxType) {
              stepFxType = t;
              char hb[24];
              snprintf(hb, sizeof(hb), "Step FX %s", stepFxName(stepFxType));
              headerShow(hb);
              if (logRoom(48)) {
                Serial.print("  fx type ");
                Serial.println(stepFxName(stepFxType));
              }
            }
          } else if (ei == 6 && stepEditTarget == 6 &&
                     seqTrack() == SEQ_TRACK_POLY) {
            // armed step TYPE of the pads: STEP STRUM HAND (wrap)
            stepTypeArmAdjust(ed);
            char hb[24];
            snprintf(hb, sizeof(hb), "Step Type %s", stepTypeArmName());
            headerShow(hb);
            if (logRoom(48)) {
              Serial.print("  type arm ");
              Serial.println(stepTypeArmName());
            }
          } else if (ei == 5 && stepEditTarget == 8 &&
                     seqTrack() == SEQ_TRACK_DRUM && stepEditAccCtx()) {
            // armed ratchet count of the DRUM pads: 2 / 3 / 4 repeats inside
            // one step (4 counts = 1 step, clamped, no wrap)
            stepRatArmAdjust(ed);
            char hb[24];
            snprintf(hb, sizeof(hb), "Ratchet %u", stepRatArmSel());
            headerShow(hb);
            if (logRoom(48)) {
              Serial.print("  ratchet ");
              Serial.println(stepRatArmSel());
            }
          } else if (ei == 7 &&
                     (stepEditCtx() || stepEditAccCtx())) {
            seqProbAdjust(ed);
            headerAnnounceInt("Probability", seqProbPct(), "%");
            if (logRoom(48)) {
              Serial.print("  prob ");
              Serial.print(seqProbPct());
              Serial.println("%");
            }
          }
        }
      } else if (ei == 1 && ed != 0) {
        // SEQMODE3: ENC2 = pattern length of the edit sequence (0..64
        // steps, boot default 16; 0 = silent track; per-track RAM only)
        seqPatLenAdjust(ed);
        refreshAllLeds();       // playhead / step view follows the length
        headerAnnounceInt("Pattern Length", seqPatLen());
        if (logRoom(48)) {
          Serial.print("  len ");
          Serial.println(seqPatLen());
        }
        any = true;
      } else if (ei == 2 && ed != 0) {
        if (modeVariant == 3 && seqTrack() == SEQ_TRACK_DRUM) {
          // SEQMODE3 PAD on a DRUM track (the FOLLOW knob has no follower
          // job there): LIVE = a keyboard press picks the voice AND sounds
          // it, SELECT = picks it silently.  Two-state, 4 counts = 1 state,
          // clamped like every other SEQMODE3 dial (no wrap)
          seqPadModeAdjust(ed);
          headerAnnounce("Pad", seqPadModeName(seqPadMode()));
          if (logRoom(48)) {
            Serial.print("  pad ");
            Serial.println(seqPadModeName(seqPadMode()));
          }
        } else {
          // SEQMODE3: ENC3 = FOLLOW, three-state (OFF / ON / FREEZE): OFF =
          // own steps play the stored line, ON = notes re-resolve live from
          // the feeder, FREEZE = hold the chord the feeder is playing at the
          // moment you switch.  4 counts = 1 state, clamped at both ends like
          // every other SEQMODE dial (no wrap - a turn past FREEZE used to
          // drop straight back to OFF)
          static int16_t folRes = 0;
          folRes += ed;
          int8_t st = 0;
          while (folRes >= 4) { folRes -= 4; st++; }
          while (folRes <= -4) { folRes += 4; st--; }
          if (st) {
            static const char *folN[3] = { "Off", "On", "Freeze" };
            int m = (int)seqFollowMode() + st;
            if (m < 0) m = 0;
            if (m > 2) m = 2;
            if ((uint8_t)m != seqFollowMode()) seqFollowModeSet((uint8_t)m);
            headerAnnounce("Follow", folN[m]);
            if (logRoom(48)) {
              Serial.print("  follow ");
              Serial.println(folN[m]);
            }
          }
        }
        any = true;
      } else if (ei == 7 && ed != 0) {
        long bpm = (long)seqTempo() + ed;
        if (bpm < 40) bpm = 40;
        if (bpm > 240) bpm = 240;
        seqSetTempo((uint16_t)bpm);
        headerAnnounceInt("Tempo", seqTempo());
        if (logRoom(48)) {
          Serial.print("  tempo ");
          Serial.println(seqTempo());
        }
        any = true;
      } else if (ei == 3 && ed != 0) {
        seqSpeedAdjust(ed);               // SPEED: 1/64 .. 1 .. 4 (no wrap)
        headerAnnounce("Pattern Speed", seqSpeedName(seqSpeedIdxOf(
            (uint8_t)(seqEditChannel() - 1))));
        if (logRoom(48)) {
          Serial.print("  speed ");
          Serial.println(seqSpeedName(seqSpeedIdxOf(
              (uint8_t)(seqEditChannel() - 1))));
        }
        any = true;
      } else if (ei == 4 && ed != 0) {
        seqInputChannelAdjust(ed);         // input channel of the edit seq
        headerAnnounceInt("Input Channel", seqInputChannel());
        if (logRoom(48)) {
          Serial.print("  in ch ");
          Serial.println(seqInputChannel());
        }
        any = true;
      } else if (ei == 5 && ed != 0) {
        seqTrackAdjust(ed);                // TRACK: DRUM/MONO/POLY (no wrap)
        if (seqTrack() != SEQ_TRACK_MONO) arpPageDismiss();  // no arp left
        if (!strumFRowCtx()) strumFRowDismiss();   // nor a strum ADDS row
        genPageDismiss();                  // the GNR8 page needs MONO+BASS
        leadPageDismiss();                 // ...the LEAD page needs MONO+LEAD
        harmPageDismiss();                 // ...the HARM page needs POLY+HARM
        compPageDismiss();                 // ...and COMP needs POLY+STEP
        skankPageDismiss();                // ...and SKANK needs POLY+SKANK
        drumPageDismiss();                 // ...and the DRUM page needs a DRUM track
        stepChainDisarm();                 // ...and a chain arm outlives nothing
        refreshAllLeds();                  // the step-edit radio follows the track type
        headerAnnounce("Track Type", seqTrackName());
        if (logRoom(48)) {
          Serial.print("  track ");
          Serial.println(seqTrackName());
        }
        any = true;
      } else if (ei == 6 && ed != 0 && seqTrack() == SEQ_TRACK_DRUM) {
        // SEQMODE3 TYPE knob (ENC7) on a DRUM track: a drum has no mono/poly
        // subtype, so the slot was dead there - it now carries the kit
        // MAPPING instead (CUSTOM = this kit, boot default; GM = General
        // MIDI percussion).  Two-state, NO wrap (CW = GM, CCW = CUSTOM, like
        // KLOCK); a change re-bakes the pattern onto the new map (one undo
        // burst).  This is the ONLY place the map is set - the pattern row
        // keeps ENC5 = gate on a drum track, as it was before
        if (drumMapSetDir(ed)) {
          drumProduce();
          char mb[24];
          snprintf(mb, sizeof(mb), "Kit Map %s", drumMapName(drumMap()));
          headerShow(mb);
          refreshAllLeds();
          refreshMainScreen();
          if (logRoom(48)) { Serial.print("  "); Serial.println(mb); }
        }
        any = true;
      } else if (ei == 6 && ed != 0 &&
                 (seqTrack() == SEQ_TRACK_MONO ||
                  seqTrack() == SEQ_TRACK_POLY)) {
        if (seqTrack() == SEQ_TRACK_MONO) {
          seqMonoTypeAdjust(ed);           // TYPE: ROOT/BASS/ARP/STRUM/LEAD
          if (seqMonoType() != SEQ_MONO_ARP) arpPageDismiss();
          strumFRowDismiss();     // a MONO track never takes the ADDS row
          genPageDismiss();       // only TYPE = BASS takes the GNR8 page
          leadPageDismiss();      // ...and only TYPE = LEAD takes the LEAD page
          compPageDismiss();      // ...and COMP needs a POLY track
          skankPageDismiss();     // ...and so does the SKANK page
          stepChainDisarm();      // ...and the chain arm belongs to BASS
          headerAnnounce("Mono Type", seqMonoTypeName());
          if (logRoom(48)) {
            Serial.print("  mono ");
            Serial.println(seqMonoTypeName());
          }
        } else {
          seqPolyTypeAdjust(ed);     // TYPE: STEP/STRUM/HAND/HARM (no wrap)
          if (!strumFRowCtx()) strumFRowDismiss();   // left STRUM/HAND
          if (seqPolyType() != SEQ_POLY_HARM) harmPageDismiss();  // left HARM
          if (seqPolyType() != SEQ_POLY_STEP) compPageDismiss();  // left STEP
          if (seqPolyType() != SEQ_POLY_SKANK)
            skankPageDismiss();   // left SKANK
          headerAnnounce("Poly Type", seqPolyTypeName());
          if (logRoom(48)) {
            Serial.print("  poly ");
            Serial.println(seqPolyTypeName());
          }
        }
        any = true;
      }
    }

    if (logRoom(80)) {
      Serial.print("ENC ");
      Serial.print(ei + 1);
      Serial.print(" ");
      Serial.print(ed);
      ModeBinding b = currentBinding();
      Serial.print("  [");
      Serial.print(encSetName(b.enc));
      Serial.print(" ");
      Serial.print(encLabel(b.enc, ei));
      Serial.println("]");
    }
    any = true;
  }
  if (any) refreshMainScreen();
}
