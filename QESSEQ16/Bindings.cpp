#include "Bindings.h"
#include "Modes.h"

// [MODE_ENC][MODE_SEQ][MODE_KEY] x variants 1..4 — from the mode matrix
static const ModeBinding table[3][4] = {
  // ENCMODE
  {
    { ENC_ENC,  CTRL_SEQ, STEP_SEQ,  KEYSET_BASE },  // 1: shared SEQ controls/steps, keyboard*
    { ENC_ENC2, CTRL_ENC2, STEP_ENC2, KEYSET_BASE }, // 2
    { ENC_ENC3, CTRL_ENC3, STEP_ENC3, KEYSET_3 },    // 3
    { ENC_ENC4, CTRL_ENC4, STEP_ENC4, KEYSET_BASE }, // 4: keyboard*
  },
  // SEQMODE
  {
    { ENC_SEQ,  CTRL_SEQ, STEP_SEQ,  KEYSET_BASE },  // 1
    { ENC_SEQ2, CTRL_SEQ2, STEP_SEQ, KEYSET_BASE },  // 2
    { ENC_SEQ3, CTRL_SEQ3, STEP_SEQ3, KEYSET_3 },    // 3
    { ENC_SEQ4, CTRL_SEQ4, STEP_SEQ, KEYSET_BASE },  // 4
  },
  // KEYMODE
  {
    { ENC_KEY,  CTRL_SEQ, STEP_SEQ,  KEYSET_BASE },  // 1
    { ENC_KEY2, CTRL_SEQ2, STEP_SEQ, KEYSET_2 },     // 2
    { ENC_KEY3, CTRL_SEQ3, STEP_SEQ3, KEYSET_3 },    // 3
    { ENC_KEY4, CTRL_SEQ4, STEP_SEQ4, KEYSET_4 },    // 4
  },
};

// encoder legend rows: the DEFAULT (idle) bindings of each set - overlays
// repaint over them when they take the row (arp/gen pages, step-edit radio,
// CC banks, held-step rows).  "--" = no idle binding (the knob only comes
// alive under its page/radio, e.g. ENC7 = the POLY FX/TYPE dial).  The
// SEQ row's ENC2-4 read LISTEN/PLAY/OVERDUB: idle labels, but the knobs
// only turn with the step-edit radio on (like GATE/VEL on row 2)
static const char* encLabels[ENC_SET_COUNT][8] = {
  /* ENC_SEQ          */ { "OCT", "LISTEN", "PLAY", "OVERDUB", "GATE", "VEL", "--", "PROB" },
  /* ENC_SEQ2         */ { "OCT", "LISTEN", "PLAY", "OVERDUB", "GATE", "VEL", "--", "PROB" },
  /* ENC_SEQ3         */ { "OCT", "LEN", "FOLLOW", "SPEED", "INCH", "TRACK", "TYPE", "BPM" },
  /* ENC_SEQ4         */ { "SWING", "AMT", "EXT IN", "SRC", "GATE", "VEL", "--", "PROB" },
  /* ENC_ENC          */ { "CC17", "CC18", "CC19", "CC20", "CC1", "CC2", "CC3", "CC4" },
  /* ENC_ENC2         */ { "CC17", "CC18", "CC19", "CC20", "CC1", "CC2", "CC3", "CC4" },
  /* ENC_ENC3         */ { "CC17", "CC18", "CC19", "CC20", "CC1", "CC2", "CC3", "CC4" },
  /* ENC_ENC4         */ { "CC17", "CC18", "CC19", "CC20", "CC1", "CC2", "CC3", "CC4" },
  /* ENC_KEY          */ { "OCT", "SCALE", "KEY", "--", "--", "--", "--", "--" },
  /* ENC_KEY2         */ { "OCT", "SCALE", "KEY", "FSCALE", "BASS", "SPRD", "DENS", "LATCH" },
  /* ENC_KEY3         */ { "OCT", "SCALE", "ENCOD", "KBOARD", "--", "--", "--", "--" },
  /* ENC_KEY4         */ { "GENRE", "MOOD", "TENS", "MOTION", "CAD", "CPX", "PHRASE", "SURP" },
  /* ENC_SEQ_STEP_HELD*/ { "--", "--", "--", "--", "--", "--", "--", "--" },  // set not wired yet
  /* ENC_SEQ_ROT_OV   */ { "SHIFT", "TRANS", "KLOCK", "--", "--", "--", "--", "--" },
  /* ENC_SEQ_STEPOV   */ { "TRANS", "--", "--", "--", "--", "--", "--", "--" },
  /* ENC_SEQ_SKANK_STEP*/ { "TES", "MOT", "SHP", "THN", "EXT", "SUB", "TES%", "TRANS" },
  /* ENC_SEQ_BAR_OV   */ { "OCT", "TRANS", "--", "--", "--", "--", "--", "--" },
  /* ENC_KEY2_PREV    */ { "OCT", "SCALE", "KEY", "LATCH", "KLOCK", "PERF", "--", "--" },
  /* ENC_KEY4_PREV    */ { "OCT", "SCALE", "KEY", "FSCALE", "SUB", "INV", "PAS", "COL" },
  /* ENC_KEY4_EMB     */ { "EMB", "PLACE", "ERA", "--", "--", "--", "--", "--" },
  /* ENC_KEY4_STEP    */ { "OCT", "DEG", "INV", "VOICE", "--", "--", "--", "--" },
  /* ENC_KEY4_MENU    */ { "KEY", "SCALE", "GRAV", "MOD", "MODTGT", "MODLEN", "FSCALE", "CADSTR" },
  /* ENC_KEY4_MENU2   */ { "BORROW", "PREP", "SD", "MIX", "DIAT", "UNRES", "SURP", "STRICT" },
  /* ENC_KEY4_MENU3   */ { "PARAL", "BASS", "LEAP", "DOUBL", "CMNTN", "TOP", "LEAD", "SPREAD" },
  /* ENC_KEY4_MENU4   */ { "RATE", "ELIDE", "CADWIN", "CELL", "VAR", "ANTE", "MOTIF", "PHRASE" },
  /* ENC_ABLETON      */ { "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8" },
};

static const char* encSetNames[ENC_SET_COUNT] = {
  "SEQENC", "SEQENC2", "SEQENC3", "SEQENC4",
  "ENCENC", "ENCENC2", "ENCENC3", "ENCENC4",
  "KEYENC", "KEYENC2", "KEYENC3", "KEYENC4",
  "SEQENC_STEP_HELD", "SEQENC_ROT_OV", "SEQENC_STEPOV", "SEQENC_SKANK_STEP",
  "SEQENC_BAR_OV",
  "KEYENC2_PREV", "KEYENC4_PREV",
  "KEYENC4_EMB", "KEYENC4_STEP", "KEYENC4_MENU", "KEYENC4_MENU2",
  "KEYENC4_MENU3", "KEYENC4_MENU4",
  "ENCABLETON"
};
static const char* ctrlSetNames[CTRL_SET_COUNT] = {
  "SEQCTRL", "SEQCTRL2", "SEQCTRL3", "SEQCTRL4",
  "ENCCTRL2", "ENCCTRL3", "ENCCTRL4",
  "ENCABLECTRL"
};
static const char* stepSetNames[STEP_SET_COUNT] = {
  "SEQSTEPS", "SEQSTEPS3", "SEQSTEPS4",
  "ENCSTEPS2", "ENCSTEPS3", "ENCSTEPS4",
  "KEYSTEPS3", "KEYSTEPS4",
  "ENCABLECLIP"
};
static const char* keySetNames[KEY_SET_COUNT] = {
  "KEYBTNS", "KEYBTNS2", "KEYBTNS3", "KEYBTNS4"
};

// single momentary override slot per group
static bool    overlayOn[4] = { false, false, false, false };
static uint8_t overlaySet[4] = { 0, 0, 0, 0 };

const ModeBinding* baseBinding() {
  uint8_t row = 0;
  if (modeBase == MODE_ENC) row = 0;
  else if (modeBase == MODE_SEQ) row = 1;
  else if (modeBase == MODE_KEY) row = 2;
  else row = 1;  // MODE_NONE -> SEQ row is irrelevant; variant lookup still safe

  uint8_t col = (modeVariant >= 1 && modeVariant <= 4) ? modeVariant - 1 : 0;
  return &table[row][col];
}

void setOverlay(Group g, uint8_t setId) {
  overlayOn[g] = true;
  overlaySet[g] = setId;
}

void clearOverlay(Group g) {
  overlayOn[g] = false;
}

ModeBinding currentBinding() {
  ModeBinding b = *baseBinding();
  if (overlayOn[G_ENC])  b.enc  = (EncSet)overlaySet[G_ENC];
  if (overlayOn[G_CTRL]) b.ctrl = (CtrlSet)overlaySet[G_CTRL];
  if (overlayOn[G_STEP]) b.step = (StepSet)overlaySet[G_STEP];
  if (overlayOn[G_KEY])  b.key  = (KeySet)overlaySet[G_KEY];
  // ENCABLETONMODE (the NEXT overlay) owns the three DAW-facing groups while
  // it is up: its labels are P1..P8 / the F action row / the clip pads
  if (abltOn() && modeBase == MODE_ENC) {
    b.enc = ENC_ABLETON;
    b.ctrl = CTRL_ABLETON;
    b.step = STEP_ABLETON;
  }
  return b;
}

const char* encLabel(EncSet s, uint8_t i) {
  if (s >= ENC_SET_COUNT || i >= 8) return "?";
  return encLabels[s][i];
}
const char* encSetName(EncSet s)   { return (s < ENC_SET_COUNT)  ? encSetNames[s]  : "?"; }
const char* ctrlSetName(CtrlSet s) { return (s < CTRL_SET_COUNT) ? ctrlSetNames[s] : "?"; }
const char* stepSetName(StepSet s) { return (s < STEP_SET_COUNT) ? stepSetNames[s] : "?"; }
const char* keySetName(KeySet s)   { return (s < KEY_SET_COUNT)  ? keySetNames[s]  : "?"; }
