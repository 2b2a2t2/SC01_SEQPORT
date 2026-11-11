#pragma once

#include <Arduino.h>

// which group an event routes to
enum Group : uint8_t { G_ENC, G_CTRL, G_STEP, G_KEY };

// one id per distinct set in the mode matrix; * cells reuse the base id
enum EncSet : uint8_t {
  ENC_SEQ, ENC_SEQ2, ENC_SEQ3, ENC_SEQ4,
  ENC_ENC, ENC_ENC2, ENC_ENC3, ENC_ENC4,
  ENC_KEY, ENC_KEY2, ENC_KEY3, ENC_KEY4,
  ENC_SEQ_STEP_HELD,           // overlay: step held in SEQMODE
  ENC_SEQ_ROT_OV,              // overlay: PREV held on a SEQ page (SHIFT/TRANS;
                               //   GNR8 page reuses it - ROT on ENC1 there)
  ENC_SEQ_STEPOV,              // overlay: step held in SEQMODE (per-step TRANS)
  ENC_SEQ_SKANK_STEP,           // overlay: step held on a POLY+SKANK track
                               //   (per-step derivation locks, ENC8 = TRANS)
  ENC_SEQ_BAR_OV,              // overlay: bar pad held on the SEQMODE3
                               //   keyboard row (bar OCT / TRANS)
  ENC_KEY2_PREV,               // overlay: PREV held in KEYMODE2
  ENC_KEY4_PREV,               // overlay: PREV held in KEYMODE4
  ENC_KEY4_EMB,                // sticky: EMB menu open in KEYMODE4
  ENC_KEY4_STEP,               // overlay: step held in KEYMODE4 (OCT/DEG/INV/VOICE)
  ENC_KEY4_MENU,               // sticky: the M2 TONALITY menu, bank 1
  ENC_KEY4_MENU2,              //   ...bank 2 (F2 while the page is open)
  ENC_KEY4_MENU3,              //   ...bank 3 (F3)
  ENC_KEY4_MENU4,              //   ...bank 4 (F4)
  ENC_ABLETON,                 // overlay: ENCABLETONMODE (NEXT) - eight Live
                               //   parameters, labels sent by the script
  ENC_SET_COUNT
};
enum CtrlSet : uint8_t {
  CTRL_SEQ, CTRL_SEQ2, CTRL_SEQ3, CTRL_SEQ4,
  CTRL_ENC2, CTRL_ENC3, CTRL_ENC4,
  CTRL_ABLETON,                // overlay: ENCABLETONMODE control row
  CTRL_SET_COUNT
};
enum StepSet : uint8_t {
  STEP_SEQ, STEP_SEQ3, STEP_SEQ4,
  STEP_ENC2, STEP_ENC3, STEP_ENC4,
  STEP_KEY3, STEP_KEY4,
  STEP_ABLETON,                // overlay: ENCABLETONMODE clip pads
  STEP_SET_COUNT
};
enum KeySet : uint8_t {
  KEYSET_BASE, KEYSET_2, KEYSET_3, KEYSET_4,
  KEY_SET_COUNT
};

struct ModeBinding {
  EncSet  enc;
  CtrlSet ctrl;
  StepSet step;
  KeySet  key;
};

const ModeBinding* baseBinding();            // static row for modeBase+variant
void setOverlay(Group g, uint8_t setId);     // momentary override (on press)
void clearOverlay(Group g);                  // restore (on release)
ModeBinding currentBinding();                // base + active overlays

const char* encLabel(EncSet s, uint8_t i);   // encoder i's name in that set
const char* encSetName(EncSet s);
const char* ctrlSetName(CtrlSet s);
const char* stepSetName(StepSet s);
const char* keySetName(KeySet s);
