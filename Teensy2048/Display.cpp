#include "Config.h"
#include "Display.h"
#include "Screen.h"      // the panel-specific drawing layer (128x64 / 480x320)
#include "Settings.h"    // device settings page (LED / screen brightness, clock)
#include "Encoders.h"
#include "Bindings.h"
#include "SeqClock.h"
#include "Modes.h"
#include "Keyboard.h"
#include "Events.h"
#include "Chords.h"
#include "Gen.h"         // GNR8 page values (fill/acc/glide/tie)
#include "Key4.h"       // KEYMODE4 genre/knob values + control legend
#include "Skank.h"      // SKANK page: shape name (LIN/PP/RND)
#include "Drum.h"       // DRUM page: genre/var/lane values
#include "Link.h"       // ENCABLETONMODE: Live track/clip/parameter state
#include <Wire.h>

static char headerLeft[24] = "";
static char headerText[40] = "";
static unsigned long headerDeadline = 0;
static bool headerActive = false;
static char headerStick[24] = "";
static bool headerStickOn = false;

// erase the header band
void clearHeader() {
  scrHeader("", "", false);
}

// erase the knob cells and the control legend.  Every screen then names only
// what it carries, so nothing from the previous mode is left behind.
void clearMainScreen() {
  scrClear();
}

// main screen layout:
//  encoders grid 4x4:  label/value rows at y16,24  (enc1-4)
//                               and y32,40         (enc5-8)
//  control buttons grid: row1 y54  PREV/NEXT/M1/M2/M3
//                        row2 y62  the F row - labels follow the context

// context-sensitive legend: what each control does RIGHT NOW
// (i 0..4 = PREV NEXT M1 M2 M3 top row, i 5..8 = F row below)
static const char* ctrlLegend(uint8_t i) {
  if (i > 8) return "";
  bool key2 = keyBehaviorVariant() == 2;   // quality / extension momentaries
  bool key4 = (modeBase == MODE_KEY && modeVariant == 4);  // genre row
  // ENCABLETONMODE renames its whole control row: PREV = tracks, M3 = the
  // clip-action set, F1-F4 = bank/device nav or the four clip actions
  if (abltOn() && modeBase == MODE_ENC && modeVariant != 3) {
    if (i >= 5) {
      if (!abltActionSet()) {
        static const char* bl[4] = { "BANK<", "BANK>", "DEV<", "DEV>" };
        return bl[i - 5];
      }
      static const char* cl[4] = { "FILL", "SCENE", "STOP", "ALL" };
      return cl[i - 5];
    }
    static const char* al[5] = { "TRACKS", "LIVE", "DRUM", "MUTE", "CLIPS" };
    return al[i];
  }
  if (i >= 5) {
    // SEQMODE + strum ADDS row (open with M2 on a POLY STRUM/HAND track):
    // F1..F4 = sus4 / add6 / add9 / clear - wins over the step-edit legend
    if (strumFRowActive()) {
      static const char* ad[4] = { "sus4", "add6", "add9", "off" };
      return ad[i - 5];
    }
    // SEQMODE + ARP page (open with M2 on a MONO ARP track): F1..F4 arm
    // that page's encoder sub-pages - order & shape / motion / generative /
    // engine opts.  Each sub-page owns the whole 8-knob row
    if (arpPageActive()) {
      static const char* ar[4] = { "ORD", "MOT", "GEN", "OPT" };
      return ar[i - 5];
    }
    // SEQMODE + GNR8 page (open with M2 on a MONO BASS track): F1 = the
    // full OPTS page, F2 = per-step OCT + MAP, F3 = per-step BRW,
    // F4 = per-step MOD PROB - wins over the step-edit legend
    if (genPageActive()) {
      static const char* gn[4] = { "OPTS", "OCT", "BRW", "PROB" };
      return gn[i - 5];
    }
    // SEQMODE + MONO step-edit: F1-F4 select TIE/GLIDE/ACCENT/PROB
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        seqTrack() == SEQ_TRACK_MONO) {
      static const char* ed[4] = { "TIE", "GLIDE", "ACC", "PROB" };
      return ed[i - 5];
    }
    // SEQMODE + DRUM: only F3/F4 edit (accent / probability)
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        seqTrack() == SEQ_TRACK_DRUM && i >= 7) {
      static const char* ed[2] = { "ACC", "PROB" };
      return ed[i - 7];
    }
    // SEQMODE + POLY: F1 = FX (sus/ext/color per step), F2 = TYPE
    // (STEP/STRUM/HAND override per step), F3/F4 edit accent / probability
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        seqTrack() == SEQ_TRACK_POLY) {
      static const char* ed[4] = { "FX", "TYPE", "ACC", "PROB" };
      return ed[i - 5];
    }
    // ENCMODE: F1-F4 = the ENC5..8 CC-bank radio
    if (modeBase == MODE_ENC) {
      static const char* fr[4] = { "F1", "F2", "F3", "F4" };
      return fr[i - 5];
    }
    // ...with the M2 TONALITY menu open, F1..F4 pick that page's four banks,
    // so the strip names them instead of the row's usual VAR/LOCK/LEAD/AUTO
    if (key4 && k4Tonality()) {
      return k4TonaBankNameAt((uint8_t)(i - 5));
    }
    if (key4) {          // KEYMODE4: VAR / LOCK / LEAD / HOLD
      static const char* k4r2[4] = { "VAR", "LOCK", "LEAD", "AUTO" };
      return k4r2[i - 5];
    }
    if (!key2) {
      static const char* row2[4] = { "M1", "M2", "M3", "M4" };
      return row2[i - 5];
    }
    static const char* ext[4] = { "6", "m7", "M7", "9" };
    return ext[i - 5];
  }
  // KEYMODE4 advertises the EMB menu on the NEXT key itself
  if (i == 1) return key2 ? "Sus" : (key4 ? "EMB" : "NEXT");
  if (i == 2) {
    // hold M1 = DRUM part-select (beats the KEYMODE2 quality, like Events);
    // SEQMODE+MONO = step-edit radio; ENCMODE hold M1 = drum-VOICE mute
    if (modeBase == MODE_SEQ && modeVariant != 3 && seqTrack() == SEQ_TRACK_DRUM)
      return "PART";
    if (modeBase == MODE_SEQ && modeVariant != 3 && seqTrack() == SEQ_TRACK_MONO)
      return "EDIT";
    if (modeBase == MODE_ENC) return "DRUM";
    if (key4) return "MODE";
    return key2 ? "Maj" : "M1";
  }
  if (i == 3) {
    // hold M2 = ENCMODE channel mute; SEQMODE3 click = erase the track;
    // SEQMODE 1/2/4 + MONO/ARP = the ARP encoder page, + MONO/BASS = the
    // GNR8 page (all keep priority over the quality key, same as the handler)
    if (modeBase == MODE_ENC) return "MUTE";
    if (modeBase == MODE_SEQ && modeVariant == 3) return "ERASE";
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        seqTrack() == SEQ_TRACK_MONO && seqMonoType() == SEQ_MONO_ARP)
      return "ARP";
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        seqTrack() == SEQ_TRACK_MONO && seqMonoType() == SEQ_MONO_BASS)
      return "GNR8";
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        seqTrack() == SEQ_TRACK_MONO && seqMonoType() == SEQ_MONO_LEAD)
      return "LEAD";
    if (key4) return "MENU";
    return key2 ? "Min" : "M2";
  }
  if (i == 4) {
    // GNR8/LEAD track: M3 = generate (tap) / undo (hold), like KEYMODE4 APPLY
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        seqTrack() == SEQ_TRACK_MONO &&
        (seqMonoType() == SEQ_MONO_BASS || seqMonoType() == SEQ_MONO_LEAD))
      return "GEN";
    // ENCMODE M3 = the ENC5..8 page-group toggle (CC1..16 / CC21..36)
    if (modeBase == MODE_ENC) return "PAGE";
    if (key4) return "APPLY";
    return key2 ? "Dim" : "M3";
  }
  return "PREV";
}

// The nine control-column labels, and which of them is ARMED.
//
// The strip used to name the buttons and nothing else, so an open page or an
// armed radio was visible only on the pads.  The band can say it too: the
// chosen F1..F4 (the ENC CC bank, an ARP/GNR8 option, the step-edit attribute,
// the open TONALITY bank) and the M button whose menu is open are filled.
// This reads the same state Modes.cpp lights the pads from, so the two agree.
static void writeLegends() {
  for (uint8_t c = 0; c < 5; c++) {
    scrLegend(c, ctrlLegend(c));
    scrLegend((uint8_t)(5 + c), ctrlLegend((uint8_t)(5 + c)));
  }

  const bool k4m = (modeBase == MODE_KEY && modeVariant == 4);
  const bool polyRadio = (modeBase == MODE_SEQ && modeVariant != 3 &&
                          seqTrack() == SEQ_TRACK_POLY);
  // any encoder page of the open track: it owns the M2 button and the row
  const bool pageOwns = drumPageActive() || arpPageActive() ||
                        genPageActive() || leadPageActive() ||
                        harmPageActive() || compPageActive() ||
                        skankPageActive();
  // SEQMODE MONO: M1's label is EDIT - the edit radio's own button.  It is
  // filled while the radio sits on STEPS (target 0); once TIE/GLIDE/... is
  // chosen that F label carries the mark instead, still on the same radio
  const bool seqMono = (modeBase == MODE_SEQ && modeVariant != 3 &&
                        seqTrack() == SEQ_TRACK_MONO);

  // F row: 1..4 = the armed slot, 0 = this screen has no radio
  uint8_t wantF = 0;
  if (modeBase == MODE_ENC && modeVariant != 3 && !abltOn()) {
    wantF = (uint8_t)(encBank() + 1);             // F1..F4 = the CC bank
  } else if (arpPageActive()) {
    const uint8_t s = arpOptionSel();
    if (s < 4) wantF = (uint8_t)(s + 1);
  } else if (genPageActive()) {
    const uint8_t s = genOptionSel();
    if (s < 4) wantF = (uint8_t)(s + 1);
  } else if (k4m && k4Tonality()) {
    wantF = (uint8_t)(k4TonaBank() + 1);          // the open TONALITY bank
  } else if (stepEditIsOn()) {
    // the same mapping Modes.cpp uses for the pads: MONO/DRUM = target 1..4,
    // POLY = F1 STEP FX (5), F2 STEP TYPE (6), then 3/4
    const uint8_t sel = stepEditSel();
    if (polyRadio && sel == 5) wantF = 1;
    else if (polyRadio && sel == 6) wantF = 2;
    else if (sel >= 1 && sel <= 4) wantF = sel;
  }
  for (uint8_t c = 0; c < 4; c++)
    scrLegendSel((uint8_t)(5 + c), wantF == (uint8_t)(c + 1));

  // M row: M2's menu (the TONALITY page, or the track page in SEQMODE) and
  // KEYMODE4's M1 write state
  scrLegendSel(3, k4m ? k4Tonality() : pageOwns);
  scrLegendSel(2, k4m ? k4Write()
                      : (seqMono && stepEditIsOn() && stepEditSel() == 0 &&
                         !pageOwns));
}

void refreshMainScreen() {
  // A frame, not a wipe: every scr*() below compares against what is already
  // on the panel and repaints only the cells, dials and legend slots that
  // really differ.  Clearing first blanked the whole model, which turned each
  // knob turn into a full-band repaint: a flash on screen, and a block in the
  // loop long enough to delay the sequencer's notes.
  scrFrameBegin();

  // DEVICE SETTINGS: three knobs and nothing else, so the turns can never
  // reach a binding behind the page.  Every other cell stays idle ("---" with
  // no value) and the legend band is blanked - there is nothing to press
  if (settingsOpen()) {
    static const char *const sl[SCR_CELLS] = { "LEDS", "SCREEN", "---", "---",
                                               "CLOCK", "---", "---", "---" };
    for (uint8_t i = 0; i < SCR_CELLS; i++) {
      char v[16];
      settingValueText(i, v, sizeof(v));
      scrCellLabel(i, sl[i]);
      scrCellValue(i, v);
    }
    for (uint8_t i = 0; i < SCR_LEGENDS; i++) {
      scrLegend(i, "");
      scrLegendSel(i, false);
    }
    return;
  }

  if (modeBase == MODE_ENC && abltOn() && modeVariant != 3 && linkAlive()) {
    // the eight knobs are the selected device's parameters: the script's
    // short name over Live's display value (or the raw number), two rows of
    // four in the same cells the CC dials occupy.  Only while the link is
    // alive: a dead script falls through to the CC screen below, which never
    // lies
    for (uint8_t c = 0; c < 4; c++) {
      char num[8];
      const char* nm = linkParamName(c);
      const char* tx = linkParamText(c);
      if (!nm[0]) nm = "--";
      if (!tx[0]) {
        snprintf(num, sizeof(num), "%u", (unsigned)linkParamValue(c));
        tx = num;
      }
      scrCellLabelSmall(c, nm);
      scrCellValue(c, tx);

      char num2[8];
      const char* nm2 = linkParamName((uint8_t)(4 + c));
      const char* tx2 = linkParamText((uint8_t)(4 + c));
      if (!nm2[0]) nm2 = "--";
      if (!tx2[0]) {
        snprintf(num2, sizeof(num2), "%u",
                 (unsigned)linkParamValue((uint8_t)(4 + c)));
        tx2 = num2;
      }
      scrCellLabelSmall((uint8_t)(4 + c), nm2);
      scrCellValue((uint8_t)(4 + c), tx2);
    }
    writeLegends();
    return;
  }

  // ENCMODE (every variant but the channel picker, which keeps its own
  // screen): one CC knob per cell - a 270-degree ring open at 6 o'clock, a
  // pointer across the rim and the value inside it, with the CC number in the
  // ring's bottom notch.  The control legend stays: the OLED dropped it
  // because 128 px had no room under eight dials, the panel has a band of
  // its own for it
  if (modeBase == MODE_ENC && modeVariant != 3) {
    char ccLab[8];
    for (uint8_t c = 0; c < 4; c++) {
      snprintf(ccLab, sizeof(ccLab), "CC%u", encCcNum(c));
      scrDial(c, ccLab, encCcValue(c));
      snprintf(ccLab, sizeof(ccLab), "CC%u", encCcNum((uint8_t)(4 + c)));
      scrDial((uint8_t)(4 + c), ccLab, encCcValue((uint8_t)(4 + c)));
    }
    writeLegends();
    return;
  }

  ModeBinding b = currentBinding();
  char val[12];
  char ccL2[8];              // ENCMODE row-2 CC labels (follow the F bank)
  // SEQMODE3 encoders show state, not the raw shared counter
  bool seq3 = (modeBase == MODE_SEQ && modeVariant == 3);
  // SEQMODE + MONO + radio on: ENC5/6/8 shape the track (gate/vel/prob)
  bool seqGe = (modeBase == MODE_SEQ && modeVariant != 3 &&
                seqTrack() == SEQ_TRACK_MONO && stepEditIsOn());
  // SEQMODE + M2 arp page: ENC2/3/4/7 take span/skip/mode/insert, the
  // other four keep their knobs (ENC1 octave, 5/6/8 gate/vel/prob)
  bool arpP = arpPageActive();
  uint8_t arpSeq = (uint8_t)(seqEditChannel() - 1);
  // SEQMODE + M2 GNR8 page: ENC2/3/4/7 take fill/accent/glide/tie, the
  // other four keep their knobs (ENC1 octave, 5/6/8 gate/vel/prob)
  bool genP = genPageActive();
  uint8_t genSeq = (uint8_t)(seqEditChannel() - 1);
  // SEQMODE + M2 LEAD page: ENC2/3/4 take contour/low/range, ENC5/6 take
  // CT/GD, ENC7 takes density; ENC1 (octave) and ENC8 (prob) keep their
  // knobs (GATE/VEL live in the M1 edit menu)
  bool leadP = leadPageActive();
  uint8_t leadSeq = (uint8_t)(seqEditChannel() - 1);
  // SEQMODE + M2 HARM page (POLY TYPE=HARM): ENC1..4 take genre/fill/src/
  // hold, the other four keep their knobs
  bool harmP = harmPageActive();
  // SEQMODE + M2 COMP page (POLY TYPE=STEP): ENC1 = HITS, the rest idle
  bool compP = compPageActive();
  // SEQMODE + M2 SKANK page (POLY TYPE=SKANK): ENC1..4 take TES/MOT/SHP/
  // THN, ENC5..8 take EXT/SUB/LEN/SPEED (the whole row is the page)
  bool skankP = skankPageActive();
  // SEQMODE + M2 DRUM page (DRUM track): the eight knobs of the pattern
  // page, regrouped by the armed F set (0xFF = the base row)
  bool drumP = drumPageActive();
  uint8_t drumSet = drumSetSelGet();
  // ...or a held step on a POLY+SKANK track arms the per-step lock row
  // (encoders 1..6 override that step's knob, 8 = TRANS)
  bool skankStepOv = (b.enc == ENC_SEQ_SKANK_STEP);
  // ...and with PREV held the SHIFT/TRANS row takes the encoders (ENC1 =
  // rotate, ENC2 = semitone transpose).  The GNR8 page reuses this same
  // overlay, so the rotate value below still applies there
  bool genOv = (b.enc == ENC_SEQ_ROT_OV);
  // ...or a held step in SEQMODE (non-DRUM) arms the per-step TRANS overlay
  bool seqTrOv = (b.enc == ENC_SEQ_STEPOV);
  // ...and in SEQMODE3 a held C D E F bar pad arms the bar OCT/TRANS row
  bool barOv = (b.enc == ENC_SEQ_BAR_OV);
  // either SEQ-page overlay wins: suppress the M2-page label overrides so
  // SHIFT/TRANS (or the held-step TRANS / SKANK locks) keeps the row
  bool seqOv = genOv || seqTrOv || skankStepOv || barOv;
  // ...or with an F radio armed the whole row is the option sub-page
  // (F1 = the full page: TONES/RNG/LOW/ALGO on ENC1-4; F2 = OCT on
  // ENC2 + MAP on ENC3, F3 = BRW on ENC3, F4 = PROB on ENC4 - the rest
  // idle; the rotate row still wins whenever PREV is held)
  uint8_t gSub = (genP && !seqOv) ? genOptionSel() : 0xFF;
  // SEQMODE + DRUM/POLY + radio on: only ENC8 (track probability) is bound
  bool seqDrumGe = (modeBase == MODE_SEQ && modeVariant != 3 &&
                    seqTrack() != SEQ_TRACK_MONO && stepEditIsOn());
  // KEYMODE2 rows: the main chord row, or the PREV overlay row
  bool key2 = (modeBase == MODE_KEY && modeVariant == 2 && b.enc != ENC_KEY2_PREV);
  bool keyOv = (b.enc == ENC_KEY2_PREV);
  // KEYMODE4: the genre row, or its OCT/SCALE/KEY overlay
  bool key4 = (modeBase == MODE_KEY && modeVariant == 4 && b.enc == ENC_KEY4);
  bool key4Ov = (b.enc == ENC_KEY4_PREV);
  // ...or its sticky EMB menu (EMB / PLACE)
  bool key4Emb = (b.enc == ENC_KEY4_EMB);
  // ...or the M2 TONALITY page - four banks, F1..F4 pick one
  bool key4Menu = (b.enc >= ENC_KEY4_MENU && b.enc <= ENC_KEY4_MENU4);
  // ...or the held-step row (OCT/DEG/INV/VOICE editing ONE step)
  bool key4Step = (b.enc == ENC_KEY4_STEP);

  for (uint8_t c = 0; c < 4; c++) {
    const char *l1 = encLabel(b.enc, c);
    if (arpP && !seqOv) {
      // an armed F sub-page owns its slots; the rest keep the plain row
      int8_t slot = arpSlotAt(arpOptionSel(), c);
      if (slot >= 0) l1 = seqArpPName((uint8_t)slot);
      else if (c >= 1 && c <= 3) {
        static const char *al[4] = { "OCT", "SPAN", "SKIP", "MODE" };
        l1 = al[c];
      }
    }
    if (genP && !seqOv && c >= 1 && c <= 3) {
      static const char *gl[4] = { "OCT", "FILL", "ACC", "GLIDE" };
      l1 = gl[c];
    }
    if (leadP && !seqOv && c >= 1 && c <= 3) {
      static const char *ll[4] = { "OCT", "CONTOUR", "LOW", "RANGE" };
      l1 = ll[c];
    }
    if (harmP && !seqOv) {
      static const char *hl[4] = { "GENRE", "FILL", "SRC", "HOLD" };
      l1 = hl[c];
    }
    if (drumP && !seqOv) {
      // base row: GENRE / VAR / FILL / MOT, then the four lane selectors on
      // row 2; the F sets swap the four lane selectors onto ENC1..4
      static const char *const dl[4] = { "GENRE", "VAR", "FILL", "MOT" };
      static const char *const dn[4] = { "KICK", "SNARE", "HAT", "PERC" };
      l1 = (drumSet == 0xFF || drumSet == 0) ? dl[c] : dn[c];
    }
    if (compP && !seqOv) {
      static const char *cl[4] = { "HITS", "---", "---", "---" };
      l1 = cl[c];
    }
    if (skankP && !seqOv) {
      static const char *kl[4] = { "TES", "MOT", "SHP", "THN" };
      l1 = kl[c];
    }
    if (key4Emb) {
      static const char *el[4] = { "EMB", "PLACE", "---", "---" };
      l1 = el[c];
    }
    if (gSub == 0) {             // F1 full page: the four option knobs
      static const char *fl[4] = { "TONES", "RNG", "LOW", "ALGO" };
      l1 = fl[c];
    } else if (gSub != 0xFF) {   // F2..F4 mod pages: which knob binds
      static const char *mOc[4] = { "---", "OCT", "MAP", "---" };
      static const char *mBr[4] = { "---", "---", "BRW", "---" };
      static const char *mPb[4] = { "---", "---", "---", "PROB" };
      l1 = (gSub == 1) ? mOc[c] : (gSub == 2) ? mBr[c] : mPb[c];
    }
    // DRUM track: the row-1 record options do not exist - keep the idle
    // "--" legend so the slot reads unbound (only the DRUM page or an
    // overlay owns this row there, both guarded above)
    if (modeBase == MODE_SEQ && (modeVariant == 1 || modeVariant == 2) &&
        seqTrack() == SEQ_TRACK_DRUM && c >= 1 && c <= 3 &&
        !seqOv && !drumP)
      l1 = "--";
    scrCellLabel(c, l1);
    if (genOv) {
      // SHIFT/TRANS row: ENC1 = the rotation position, ENC2 = the track's
      // semitone transpose, ENC3 = per-track scale-lock (Klock), the rest
      // stay idle
      if (c == 0) snprintf(val, sizeof(val), "%u", seqGenShiftPos() + 1);
      else if (c == 1) {
        int tv = seqTrans();
        if (tv == 0) snprintf(val, sizeof(val), "0");
        else snprintf(val, sizeof(val), "%+d", tv);
      } else if (c == 2) {
        snprintf(val, sizeof(val), "%s", seqKlock() ? "ON" : "OFF");
      } else snprintf(val, sizeof(val), "");
    } else if (seqTrOv) {
      // held-step TRANS row: ENC1 = that step's offset (0 = "0")
      if (c == 0) {
        int8_t tv = stepTrAt((uint8_t)seqTrStepSelGet());
        if (tv == 0) snprintf(val, sizeof(val), "0");
        else snprintf(val, sizeof(val), "%+d", tv);
      } else snprintf(val, sizeof(val), "");
    } else if (skankStepOv) {
      // SKANK lock row: that step's override, "inherit" = the page knob
      int8_t ss = seqTrStepSelGet();
      int16_t lk = (ss >= 0) ? stepSkankLockAt((uint8_t)ss, (uint8_t)c) : -1;
      if (lk < 0) snprintf(val, sizeof(val), "inherit");
      else snprintf(val, sizeof(val), "%d", lk);
    } else if (barOv) {
      // held-bar row: ENC1 = that bar's octave, ENC2 = its semitone TRANS
      uint8_t bd = seqBarEditSel();
      if (bd > 3) bd = 3;
      if (c == 0) snprintf(val, sizeof(val), "%+d", (int)seqBarOctAt(bd));
      else if (c == 1) snprintf(val, sizeof(val), "%+d", (int)seqBarTrAt(bd));
      else snprintf(val, sizeof(val), "");
    } else if (gSub == 0) {
      // F1 full options page: ENC1 TONES, ENC2 RANGE, ENC3 LOW, ENC4 ALGO
      if (c == 0) {
        snprintf(val, sizeof(val), "%s",
                 seqGenTonesName(seqGenTonesOf(genSeq)));
      } else if (c == 1) {
        snprintf(val, sizeof(val), "%uoct", seqGenRangeOf(genSeq));
      } else if (c == 2) {
        snprintf(val, sizeof(val), "%s",
                 seqGenLowName(seqGenLowOf(genSeq)));
      } else {
        snprintf(val, sizeof(val), "%s",
                 seqGenAlgoName(seqGenAlgoOf(genSeq)));
      }
    } else if (gSub != 0xFF) {
      // F2..F4 mod pages: the selected step's value (nothing picked yet
      // reads "pick"); MAP on F2/ENC3 needs no selection
      int8_t ss = genStepSelStep();
      if (gSub == 1) {
        if (c == 1) {
          if (ss < 0) snprintf(val, sizeof(val), "pick");
          else {
            int ov = stepOctAt((uint8_t)ss);
            if (ov == 0) snprintf(val, sizeof(val), "0");
            else snprintf(val, sizeof(val), "%+d", ov);
          }
        } else if (c == 2) {
          snprintf(val, sizeof(val), "%s", seqMapName(seqMap()));
        } else snprintf(val, sizeof(val), "");
      } else if (gSub == 2) {
        if (c == 2) {
          if (ss < 0) snprintf(val, sizeof(val), "pick");
          else if (stepBrwAt((uint8_t)ss) == 0)
            snprintf(val, sizeof(val), "off");
          else snprintf(val, sizeof(val), "%u", stepBrwAt((uint8_t)ss));
        } else snprintf(val, sizeof(val), "");
      } else {
        if (c == 3) {
          if (ss < 0) snprintf(val, sizeof(val), "pick");
          else snprintf(val, sizeof(val), "%u%%", stepModPAt((uint8_t)ss));
        } else snprintf(val, sizeof(val), "");
      }
    } else if (skankP && c == 0) {
      snprintf(val, sizeof(val), "%u", skankPrm(0));         // ENC1 = TES
    } else if (skankP && c == 1) {
      snprintf(val, sizeof(val), "%u", skankPrm(1));         // ENC2 = MOT
    } else if (skankP && c == 2) {
      snprintf(val, sizeof(val), "%s",
               skankShpName(skankPrm(2)));                   // ENC3 = SHP
    } else if (skankP && c == 3) {
      snprintf(val, sizeof(val), "%u", skankPrm(3));         // ENC4 = THN
    } else if (drumP && !seqOv) {
      // DRUM page row 1 (ENC1..4): GENRE / VAR / FILL / MOT on the base row
      // and on the F1 GROOVE set, the four lane values on F2 LINES, F3
      // FILL% and F4 MOT% (the F sets move the lane knobs onto row 1, so
      // every label/value pair stays in the same column)
      if (drumSet == 0xFF || drumSet == 0) {
        if (c == 0)      snprintf(val, sizeof(val), "%s",
                                  drumGenreName(drumGenre()));
        else if (c == 1) snprintf(val, sizeof(val), "%s",
                                  drumVarName(drumVar()));
        else if (c == 2) snprintf(val, sizeof(val), "%u%%",
                                  (unsigned)drumFill());
        else             snprintf(val, sizeof(val), "%u%%",
                                  (unsigned)drumMot());
      } else if (drumSet == 1) {
        snprintf(val, sizeof(val), "%s",
                 drumLineName(drumLine((uint8_t)c)));
      } else if (drumSet == 2) {
        snprintf(val, sizeof(val), "%u%%",
                 (unsigned)drumLaneFill((uint8_t)c));
      } else {
        snprintf(val, sizeof(val), "%u%%",
                 (unsigned)drumLaneMot((uint8_t)c));
      }
    } else if (modeBase == MODE_SEQ && c == 0 && !compP &&
               !(arpP && arpSlotAt(arpOptionSel(), 0) >= 0)) {
      if (modeVariant == 4) {
        // variant 4 is the timing screen: ENC1 = the per-track swing style
        snprintf(val, sizeof(val), "%s", seqSwingStyleName(seqSwingStyle()));
      } else {
        snprintf(val, sizeof(val), "%d", seqOctaveShift()); // ENC1 = octave shift
      }
    } else if (modeBase == MODE_SEQ && c == 1 && modeVariant == 4 && !genP &&
               !leadP && !harmP && !compP && !skankP && !drumP &&
               !(arpP && arpSlotAt(arpOptionSel(), 1) >= 0)) {
      // variant 4 timing screen: ENC2 = the swing amount (0..100)
      snprintf(val, sizeof(val), "%u%%", seqSwingAmt());
    } else if (modeBase == MODE_SEQ && (c == 2 || c == 3) && modeVariant == 4 &&
               !genP && !leadP && !harmP && !compP && !skankP && !drumP &&
               !arpP) {
      // variant 4 input row: ENC3 = EXT IN mode, ENC4 = source (every page
      // owns these slots when open, so the guard mirrors the plain row)
      if (c == 2)
        snprintf(val, sizeof(val), "%s", seqExtInModeName(seqExtInMode()));
      else
        snprintf(val, sizeof(val), "%s", seqExtInSrcName(seqExtInSrc()));
    } else if (genP && c == 1) {
      snprintf(val, sizeof(val), "%u%%", seqGenFillOf(genSeq));   // ENC2
    } else if (genP && c == 2) {
      snprintf(val, sizeof(val), "%u%%", seqGenAccOf(genSeq));    // ENC3
    } else if (genP && c == 3) {
      snprintf(val, sizeof(val), "%u%%", seqGenGlideOf(genSeq));  // ENC4
    } else if (arpP && arpSlotAt(arpOptionSel(), c) >= 0) {
      uint8_t slot = (uint8_t)arpSlotAt(arpOptionSel(), c);
      snprintf(val, sizeof(val), "%s",
               seqArpPValue(slot, seqArpPOf(arpSeq, slot)));  // sub-page knob
    } else if (arpP && c == 1) {
      snprintf(val, sizeof(val), "%u",
               seqArpSpanOf(arpSeq));           // ENC2 = span (octaves)
    } else if (arpP && c == 2) {
      snprintf(val, sizeof(val), "%s",
               seqArpSkipOf(arpSeq) ? "ON" : "OFF");   // ENC3 = skip rests
    } else if (arpP && c == 3) {
      snprintf(val, sizeof(val), "%s",
               seqArpModeName(seqArpModeOf(arpSeq)));  // ENC4 = mode
    } else if (leadP && c == 1) {
      snprintf(val, sizeof(val), "%s",
               seqLeadContourName(seqLeadContourOf(leadSeq))); // ENC2 contour
    } else if (leadP && c == 2) {
      snprintf(val, sizeof(val), "%s",
               seqGenLowName(seqGenLowOf(leadSeq)));          // ENC3 = LOW
    } else if (leadP && c == 3) {
      snprintf(val, sizeof(val), "%uoct",
               seqGenRangeOf(leadSeq));                       // ENC4 = RANGE
    } else if (harmP && c == 0) {
      snprintf(val, sizeof(val), "%s", k4GenreShort());       // ENC1 = genre
    } else if (harmP && c == 1) {
      snprintf(val, sizeof(val), "%u%%",
               seqGenFillOf((uint8_t)(seqEditChannel() - 1))); // ENC2 = fill
    } else if (harmP && c == 2) {
      snprintf(val, sizeof(val), "%s", k4HarmInputName());    // ENC3 = src
    } else if (harmP && c == 3) {
      snprintf(val, sizeof(val), "%s", k4HarmHoldName());     // ENC4 = hold
    } else if (compP && c == 0) {
      int h = seqCompHits();                                  // ENC1 = hits
      if (h <= 1) snprintf(val, sizeof(val), "off");
      else snprintf(val, sizeof(val), "%d", h);
    } else if (modeBase == MODE_SEQ &&
               (modeVariant == 1 || modeVariant == 2) &&
               c >= 1 && c <= 3 &&
               seqTrack() != SEQ_TRACK_DRUM &&
               !genP && !leadP && !harmP && !compP && !skankP && !drumP &&
               !arpP) {
      // M1 step-edit row, variants 1/2: ENC2 = LISTEN (CHORD/NOTE),
      // ENC3 = PLAYBACK (GATE/RECORDED), ENC4 = OVERDUB (OFF/ON).  A DRUM
      // track has no record options - blank, like an idle slot
      if (c == 1)
        snprintf(val, sizeof(val), "%s", seqRecListenName(seqRecListen()));
      else if (c == 2)
        snprintf(val, sizeof(val), "%s",
                 seqRecPlaybackName(seqRecPlayback()));
      else
        snprintf(val, sizeof(val), "%s", seqRecOverdubName(seqRecOverdub()));
    } else if (seq3 && c == 1) {
      snprintf(val, sizeof(val), "%d", seqPatLen());      // ENC2 = steps
    } else if (seq3 && c == 2) {
      static const char *folN[3] = { "OFF", "ON", "FREEZE" };
      snprintf(val, sizeof(val), "%s", folN[seqFollowMode()]);
    } else if (seq3 && c == 3) {
      snprintf(val, sizeof(val), "%s",
               seqSpeedName(seqSpeedIdxOf((uint8_t)(seqEditChannel() - 1))));
    } else if (key4 && c == 0) {
      snprintf(val, sizeof(val), "%s", k4GenreShort()); // ENC1 = genre
    } else if (key4 && c == 1) {
      snprintf(val, sizeof(val), "%s", k4MoodName());   // ENC2 = MOOD
    } else if (key4 && c == 2) {
      snprintf(val, sizeof(val), "%u", k4Knob(1));      // ENC3 = TENS
    } else if (key4 && c == 3) {
      snprintf(val, sizeof(val), "%u", k4Knob(2));      // ENC4 = MOTION
    } else if (key4Menu) {
      snprintf(val, sizeof(val), "%s",   // the open bank's value for this knob
               k4TonaLabel((uint8_t)(k4TonaBank() * 8 + c)));
    } else if (key4Ov && c == 3) {
      snprintf(val, sizeof(val), "%s",
               chordFScale() ? "ON" : "OFF");       // overlay ENC4 = FScale
    } else if (key4Ov && c == 4) {
      snprintf(val, sizeof(val), "%u", k4EmbSubProb());  // overlay EN5 = SUB
    } else if (key4Ov && c == 5) {
      snprintf(val, sizeof(val), "%u", k4EmbInvBias());  // overlay EN6 = INV
    } else if (key4Ov && c == 6) {
      snprintf(val, sizeof(val), "%u", k4EmbPassAmt());  // overlay EN7 = PAS
    } else if (key4Ov && c == 7) {
      snprintf(val, sizeof(val), "%u", k4EmbColorAmt()); // overlay EN8 = COL
    } else if (key4Emb && c == 0) {
      snprintf(val, sizeof(val), "%s", k4EmbModeName());  // ENC1 = EMB
    } else if (key4Emb && c == 1) {
      snprintf(val, sizeof(val), "%s", k4EmbPlaceName()); // ENC2 = PLACE
    } else if (key4Emb && c == 2) {
      snprintf(val, sizeof(val), "%u", k4Knob(5));         // ENC3 = ERA
    } else if (key4Emb) {
      snprintf(val, sizeof(val), "");                   // ENC3..8 idle
    } else if (key4Step) {
      // held-step values: the step's own OCT / degree / inversion / spread
      int8_t ss = k4StepSelGet();
      char dgs[8], ivs[8];
      bool has = (ss >= 0 &&
                  k4StepLabels((uint8_t)ss, dgs, sizeof(dgs),
                               ivs, sizeof(ivs)));
      if (!has) {
        snprintf(val, sizeof(val), "");                 // no step / empty
      } else if (c == 0) {
        int ov = stepOctAt((uint8_t)ss);                   // ENC1 = octave
        if (ov == 0) snprintf(val, sizeof(val), "0");
        else snprintf(val, sizeof(val), "%+d", ov);
      } else if (c == 1) {
        snprintf(val, sizeof(val), "%s", dgs);             // ENC2 = degree
      } else if (c == 2) {
        snprintf(val, sizeof(val), "%s", ivs);             // ENC3 = inversion
      } else {
        // ENC4 = voice: octave spread between bass and top ("close" when
        // they sit inside one octave)
        uint8_t n = 0;
        const uint8_t *nt = stepChordAt((uint8_t)ss, &n);
        uint8_t lo = 0, hi = 0;
        for (uint8_t i = 0; i < n; i++) {
          if (i == 0 || nt[i] < lo) lo = nt[i];
          if (i == 0 || nt[i] > hi) hi = nt[i];
        }
        uint8_t sp = (uint8_t)((hi - lo) / 12);
        if (sp == 0) snprintf(val, sizeof(val), "close");
        else snprintf(val, sizeof(val), "%uoct", sp);
      }
    } else if (modeBase == MODE_KEY && c == 0) {
      snprintf(val, sizeof(val), "%d", keyOctave());         // ENC1 = octave
    } else if (modeBase == MODE_KEY && c == 1) {
      snprintf(val, sizeof(val), "%s", keyScaleName());      // ENC2 = scale
    } else if (modeBase == MODE_KEY &&
               (modeVariant == 1 || modeVariant == 2 || key4Ov) && c == 2) {
      snprintf(val, sizeof(val), "%s", keyRootName());       // ENC3 = root
    } else if (keyOv && c == 3) {
      snprintf(val, sizeof(val), "%s", chordLatch() ? "ON" : "OFF");
    } else if (key2 && c == 3) {
      snprintf(val, sizeof(val), "%s", chordFScale() ? "ON" : "OFF"); // ENC4
    } else if (modeBase == MODE_KEY && modeVariant == 3 && c == 2) {
      snprintf(val, sizeof(val), "%s",      // ENC3 = ENCOD link
               chanEncLocked() ? "locked" : "free");
    } else if (modeBase == MODE_KEY && modeVariant == 3 && c == 3) {
      snprintf(val, sizeof(val), "%s",      // ENC4 = KBOARD link
               chanKeyLocked() ? "locked" : "free");
    } else if (modeBase == MODE_ENC) {
      snprintf(val, sizeof(val), "%u", encCcValue(c));       // CC value
    } else if (l1[0] == '-' && l1[1] == '-') {
      snprintf(val, sizeof(val), "");      // "--" legend: no position noise
    } else {
      snprintf(val, sizeof(val), "%ld", encPosition(c));
    }
    scrCellValue(c, val);
    // row-2 labels follow the step-edit bindings while the radio is on
    // (the idle default row would contradict the live values)
    const char* l2 = encLabel(b.enc, 4 + c);
    if (drumP && !seqOv) {
      // row 2 only carries the four lane line selectors of the base row;
      // every F set keeps its knobs on row 1, so row 2 stays idle
      static const char *const dn[4] = { "KICK", "SNARE", "HAT", "PERC" };
      l2 = (drumSet == 0xFF) ? dn[c] : "---";
    } else if (seqGe) {
      static const char* sl[4] = { "GATE", "VEL", "---", "PROB" };
      l2 = sl[c];
    } else if (seqDrumGe) {
      static const char* sl[4] = { "---", "---", "---", "PROB" };
      l2 = sl[c];
    } else if (modeBase == MODE_ENC) {
      // ENC5..8 follow the F1..F4 CC bank in the active M3 page group
      snprintf(ccL2, sizeof(ccL2), "CC%u", encCcNum(4 + c));
      l2 = ccL2;
    }
    if (c == 1 && f3AccentActive()) l2 = "ACC";   // F3 held: ENC6 = accent
    if (!drumP && modeBase == MODE_SEQ && modeVariant == 3 &&
        seqTrack() == SEQ_TRACK_DRUM && 4 + c == 6)
      l2 = "MAP";           // DRUM track, SEQMODE3 only: the kit mapping sits
                             // on the dead TYPE knob (ENC7)
    if (arpP) {                                   // arp page: sub-page slots
      int8_t slot = arpSlotAt(arpOptionSel(), (uint8_t)(4 + c));
      if (slot >= 0) l2 = seqArpPName((uint8_t)slot);
      else if (4 + c == 6) l2 = "INST";           // plain page: ENC7 = insert
    }
    if (genP && 4 + c == 6) l2 = "TIE";           // gen page: ENC7 = tie %
    if (leadP && 4 + c == 4) l2 = "CT";           // lead page: ENC5 = ct blend
    if (leadP && 4 + c == 5) l2 = "GD";           // lead page: ENC6 = gd set
    if (leadP && 4 + c == 6) l2 = "DENS";         // lead page: ENC7 = density
    if (genOv) l2 = "---";                        // SHIFT/TRANS row: idle
    if (seqTrOv) l2 = "---";                      // held-step TRANS: idle
    if (barOv) l2 = "---";                        // held-bar row: idle
    if (seqDrumGe && 4 + c == 6 && stepEditSel() == 5)
      l2 = "FX";                                  // POLY edit: ENC7 = FX type
    if (seqDrumGe && 4 + c == 6 && stepEditSel() == 6)
      l2 = "TYPE";                                // POLY edit: ENC7 = TYPE arm
    if (seqDrumGe && 4 + c <= 5 && stepEditSel() == 7)
      l2 = (c == 0) ? "SUB" : (c == 1) ? "INV" : (c == 2) ? "PAS" : "COL"; // POLY edit: ENC5-8 = EMB params
    if (gSub != 0xFF) l2 = "---";            // option sub-page: row 2 idle
    if (skankStepOv) {          // lock row: EXT/SUB, TES% lock, TRANS (wins
      static const char *s2[4] = { "EXT", "SUB", "TES%", "TRANS" }; // over radio)
      l2 = s2[c];
    }
    if (skankP && !seqOv) {     // SKANK page: the whole second row follows
      static const char *s2[4] = { "EXT", "SUB", "TES%", "NOT%" };
      l2 = s2[c];
    }
    scrCellLabel((uint8_t)(4 + c), l2);
    if (!drumP && modeBase == MODE_SEQ && modeVariant == 3 &&
        seqTrack() == SEQ_TRACK_DRUM && 4 + c == 6) {
      snprintf(val, sizeof(val), "%s", drumMapName(drumMap())); // CUSTOM / GM
    } else if (drumP && !seqOv) {
      // DRUM page row 2 (ENC5..8): the base row's four lane selectors.
      // The F sets own row 1, so this row is idle there
      if (drumSet == 0xFF)
        snprintf(val, sizeof(val), "%s",
                 drumLineName(drumLine((uint8_t)c)));
      else snprintf(val, sizeof(val), "");
    } else if (genOv) {
      snprintf(val, sizeof(val), "");              // SHIFT/TRANS row idle
    } else if (seqTrOv) {
      snprintf(val, sizeof(val), "");              // held-step TRANS idle
    } else if (barOv) {
      snprintf(val, sizeof(val), "");              // held-bar row: row 2 idle
    } else if (skankStepOv) {
      // lock row: EXT / SUB / TES% override, ENC8 = the step's TRANS
      int8_t ss = seqTrStepSelGet();
      if (c <= 2) {
        int16_t lk = (ss >= 0)
                         ? stepSkankLockAt((uint8_t)ss, (uint8_t)(c + 4))
                         : -1;
        if (lk < 0) snprintf(val, sizeof(val), "inherit");
        else if (c == 2) snprintf(val, sizeof(val), "%d%%", lk);
        else snprintf(val, sizeof(val), "%d", lk);
      } else if (c == 3) {
        int8_t tv = (ss >= 0) ? stepTrAt((uint8_t)ss) : 0;
        if (tv == 0) snprintf(val, sizeof(val), "0");
        else snprintf(val, sizeof(val), "%+d", tv);
      } else snprintf(val, sizeof(val), "");
    } else if (gSub != 0xFF) {
      snprintf(val, sizeof(val), "");        // option sub-page: idle
    } else if (c == 1 && f3AccentActive()) {
      snprintf(val, sizeof(val), "%d", seqAccentVelocity()); // ENC6 = acc vel
    } else if (arpP && arpSlotAt(arpOptionSel(), (uint8_t)(4 + c)) >= 0) {
      uint8_t slot = (uint8_t)arpSlotAt(arpOptionSel(), (uint8_t)(4 + c));
      snprintf(val, sizeof(val), "%s",
               seqArpPValue(slot, seqArpPOf(arpSeq, slot)));  // sub-page knob
    } else if (arpP && 4 + c == 4) {
      snprintf(val, sizeof(val), "%d%%", seqGatePct());   // ENC5 = gate %
    } else if (arpP && 4 + c == 5) {
      snprintf(val, sizeof(val), "%d", seqVelocity());    // ENC6 = velocity
    } else if (arpP && 4 + c == 7) {
      snprintf(val, sizeof(val), "%d%%", seqProbPct());   // ENC8 = probability
    } else if (arpP && 4 + c == 6) {
      snprintf(val, sizeof(val), "%s",
               seqArpInstName(seqArpInstOf(arpSeq)));  // ENC7 = insert mode
    } else if (genP && 4 + c == 6) {
      snprintf(val, sizeof(val), "%u%%", seqGenTieOf(genSeq));  // ENC7 tie
    } else if (leadP && 4 + c == 4) {
      snprintf(val, sizeof(val), "%u%%", seqLeadCtOf(leadSeq));  // ENC5 ct
    } else if (leadP && 4 + c == 5) {
      snprintf(val, sizeof(val), "%s",
               seqLeadGdName(seqLeadGdOf(leadSeq)));             // ENC6 gd
    } else if (leadP && 4 + c == 6) {
      snprintf(val, sizeof(val), "%u%%", seqGenFillOf(leadSeq)); // ENC7 dens
    } else if (skankP && 4 + c == 4) {
      snprintf(val, sizeof(val), "%u%%", skankPrm(4));      // ENC5 = EXT
    } else if (skankP && 4 + c == 5) {
      snprintf(val, sizeof(val), "%u%%", skankPrm(5));      // ENC6 = SUB
    } else if (skankP && 4 + c == 6) {
      snprintf(val, sizeof(val), "%u%%", skankPrm(6));      // ENC7 = TES%
    } else if (skankP && 4 + c == 7) {
      snprintf(val, sizeof(val), "%u%%", skankPrm(7));      // ENC8 = NOT%
    } else if (seq3 && 4 + c == 4) {
      snprintf(val, sizeof(val), "%u", seqInputChannel()); // ENC5 = input ch
    } else if (seq3 && 4 + c == 5) {
      snprintf(val, sizeof(val), "%s", seqTrackName());   // ENC6 = TRACK
    } else if (seq3 && 4 + c == 6 && seqTrack() == SEQ_TRACK_MONO) {
      snprintf(val, sizeof(val), "%s", seqMonoTypeName()); // ENC7 = TYPE
    } else if (seq3 && 4 + c == 6 && seqTrack() == SEQ_TRACK_POLY) {
      snprintf(val, sizeof(val), "%s", seqPolyTypeName()); // ENC7 = TYPE
    } else if (seq3 && 4 + c == 7) {
      snprintf(val, sizeof(val), "%u", seqTempo());         // ENC8 = BPM
    } else if (seqGe && 4 + c == 4) {
      snprintf(val, sizeof(val), "%d%%", seqGatePct());     // ENC5 = gate
    } else if (seqGe && 4 + c == 5) {
      snprintf(val, sizeof(val), "%d", seqVelocity());      // ENC6 = vel
    } else if (seqGe && 4 + c == 6) {
      snprintf(val, sizeof(val), "");                    // ENC7 = free
    } else if (seqGe && 4 + c == 7) {
      snprintf(val, sizeof(val), "%d%%", seqProbPct());  // ENC8 = track probability
    } else if (seqDrumGe && 4 + c == 4) {
      snprintf(val, sizeof(val), "");                    // ENC5 free
    } else if (seqDrumGe && 4 + c == 5) {
      snprintf(val, sizeof(val), "");                    // ENC6 free
    } else if (seqDrumGe && 4 + c == 6) {
      if (stepEditSel() == 7) {
        // EMB target: show SUB/INV/PAS/COL for focused step
        static const char* embLabels[4] = { "SUB", "INV", "PAS", "COL" };
        uint8_t p = stepEmbGet(k4Focus(), c);
        if (p == 255) p = (c == 0) ? k4EmbSubProb() : (c == 1) ? k4EmbInvBias()
            : (c == 2) ? k4EmbPassAmt() : k4EmbColorAmt();
        snprintf(val, sizeof(val), "%s=%u", embLabels[c], p);
      } else {
        snprintf(val, sizeof(val), "%s",
                 stepEditSel() == 5 ? stepFxSelName()
                 : stepEditSel() == 6 ? stepTypeArmName() : ""); // ENC7
      }
    } else if (seqDrumGe && 4 + c == 7) {
      snprintf(val, sizeof(val), "%d%%", seqProbPct());  // ENC8 = track probability
    } else if (key4 && 4 + c == 4) {
      snprintf(val, sizeof(val), "%s", k4CadencePair());    // CAD
    } else if (key4 && 4 + c == 5) {
      snprintf(val, sizeof(val), "%u", k4Knob(4));          // CPX
    } else if (key4 && 4 + c == 6) {
      snprintf(val, sizeof(val), "%s", k4PhraseName());      // PHRASE
    } else if (key4 && 4 + c == 7) {
      snprintf(val, sizeof(val), "%u", k4Knob(6));          // SURP
    } else if (key4Ov || key4Emb || key4Menu) {
      snprintf(val, sizeof(val), "");          // overlay row 2 idle
    } else if (keyOv && 4 + c == 4) {
      snprintf(val, sizeof(val), "%s", chordKeyLock() ? "ON" : "OFF");
    } else if (keyOv && 4 + c == 5) {
      snprintf(val, sizeof(val), "%s", chordPerf() ? "ON" : "OFF");
    } else if (key2 && 4 + c == 4) {
      snprintf(val, sizeof(val), "%u", chordParam(1));      // ENC5 = bass
    } else if (key2 && 4 + c == 5) {
      snprintf(val, sizeof(val), "%u", chordParam(2));      // ENC6 = spread
    } else if (key2 && 4 + c == 6) {
      snprintf(val, sizeof(val), "%u", chordParam(3));      // ENC7 = density
    } else if (key2 && 4 + c == 7) {
      snprintf(val, sizeof(val), "%s", chordLatch() ? "ON" : "OFF"); // ENC8
    } else if (modeBase == MODE_ENC) {
      snprintf(val, sizeof(val), "%u", encCcValue(4 + c)); // CC value
    } else if (l2[0] == '-' && l2[1] == '-') {
      snprintf(val, sizeof(val), "");      // "--" legend: no position noise
    } else {
      snprintf(val, sizeof(val), "%ld", encPosition(4 + c));
    }
    scrCellValue((uint8_t)(4 + c), val);
  }

  writeLegends();
}

void drawHeaderText() {
  // SEQMODE step-edit: while an attribute target is engaged, the left
  // header slot shows it instead of the mode name (TIE/GLIDE/ACCENT on MONO,
  // STEP FX/STEP TYPE/EMBED on POLY); an open page wins over both so the
  // rebind is always visible.  Every label here is a whole word - the strip
  // is read at a glance during a set, so "PROB" is not a label
  const char* left = headerLeft;
  // ENCABLETONMODE names the Live track it is bound to in the left slot
  char liveLeft[22];
  if (abltOn() && modeBase == MODE_ENC) {
    if (linkTrackName()[0]) {
      snprintf(liveLeft, sizeof(liveLeft), "LIVE %.12s", linkTrackName());
    } else {
      snprintf(liveLeft, sizeof(liveLeft), "LIVE LINK");
    }
    left = liveLeft;
  }
  if (settingsOpen()) {
    left = "DEVICE SETTINGS";    // the header tap opened the settings page
  } else if (drumPageActive()) {
    left = "DRUM PATTERN";       // the drum pattern page owns the row
  } else if (genPageActive()) {
    left = "GENERATOR";          // the generator page owns the encoder row
  } else if (currentBinding().enc >= ENC_KEY4_MENU &&
             currentBinding().enc <= ENC_KEY4_MENU4) {
    left = k4TonaBankName();    // the M2 page owns the row: which bank
  } else if (arpPageActive()) {
    left = arpOptionName(arpOptionSel());   // ARP / ARP ORDER .. ARP OPTIONS
  } else if (strumFRowActive()) {
    left = "STRUM ADDS";        // strum ADDS row: the F row edits colors
  } else if (stepEditActive()) {
    static const char* ed[4] = { "TIE", "GLIDE", "ACCENT", "PROBABILITY" };
    if (stepEditSel() == 5) left = "STEP FX";
    else if (stepEditSel() == 6) left = "STEP TYPE";
    else if (stepEditSel() == 7) left = "EMBED";
    else left = ed[stepEditSel() - 1];
  }
  // A transient message owns the whole strip; the persistent one (the KEY4
  // progression) sits on the right and yields room to the left label.  The
  // drawing layer does the fitting, because that strip is 128 px on the OLED
  // and 480 px on the panel
  const char* right = "";
  bool transient = false;
  if (headerActive && headerText[0]) {
    right = headerText;
    transient = true;
  } else if (headerStickOn && headerStick[0]) {
    right = headerStick;
  }
  scrHeader(left, right, transient);

  // the panel's mode row follows the active family (no-op on the OLED)
  scrModeRow(modeBase == MODE_NONE ? SCR_MODE_NONE : (uint8_t)(modeBase - 1));
}

void headerSetLeft(const char* text) {
  strncpy(headerLeft, text, sizeof(headerLeft) - 1);
  headerLeft[sizeof(headerLeft) - 1] = '\0';
  drawHeaderText();
}

// top strip only: the left slot tracks state that can change without a
// mode switch (ARP page closed, radio target reset on a channel change)
void refreshHeader() {
  drawHeaderText();
}

void headerShow(const char* name) {
  strncpy(headerText, name, sizeof(headerText) - 1);
  headerText[sizeof(headerText) - 1] = '\0';
  headerDeadline = millis() + HEADER_MS;
  headerActive = true;
  drawHeaderText();
}

// the one announcement shape: a full parameter name, then its value.
// Every knob, switch and page in the firmware goes through here, so the
// strip can never drift back into "klock ON" / "mot 30%" codes
void headerAnnounce(const char* name, const char* value) {
  char buf[sizeof(headerText)];
  snprintf(buf, sizeof(buf), "%s %s", name, value ? value : "");
  headerShow(buf);
}

void headerAnnounceInt(const char* name, int value, const char* suffix) {
  char buf[sizeof(headerText)];
  snprintf(buf, sizeof(buf), "%s %d%s", name, value, suffix ? suffix : "");
  headerShow(buf);
}

void headerSetStick(const char* text) {
  strncpy(headerStick, text, sizeof(headerStick) - 1);
  headerStick[sizeof(headerStick) - 1] = '\0';
  headerStickOn = headerStick[0] != '\0';
  drawHeaderText();
}

void headerStickClear() {
  if (!headerStickOn) return;
  headerStickOn = false;
  headerStick[0] = '\0';
  drawHeaderText();
}

void headerHide() {
  if (!headerActive) return;
  headerActive = false;
  drawHeaderText();
}

void displayMarkDirty() {
  refreshHeader();
  refreshMainScreen();
}

void initDisplay() {
  scrBegin();               // panel or strip up, drawing surface cleared
  refreshMainScreen();      // the first draw reads the restored knob values
  refreshHeader();
  displayUpdate();
}

// Bring-up report: the reset reason, the bring-up stage the previous boot
// died in, the failure count, the heap and what the hardware layer found, in
// the cells every mode uses.  It bypasses refreshMainScreen() on purpose:
// none of the firmware's mode or track state is up when it is drawn.
void displayShowBoot(bool safeMode, const char* reset, const char* stage,
                     const char* boots, const char* heap, const char* devices,
                     const char* note) {
  clearMainScreen();
  scrHeader("BOOT", note ? note : "", true);
  scrModeRow(SCR_MODE_NONE);

  scrCellLabel(0, "RESET");
  scrCellValue(0, reset ? reset : "--");
  scrCellLabel(1, "LAST STAGE");
  scrCellValue(1, stage ? stage : "--");
  scrCellLabel(2, "FAILED BOOTS");
  scrCellValue(2, boots ? boots : "0");
  scrCellLabel(3, "FREE HEAP");
  scrCellValue(3, heap ? heap : "--");
  scrCellLabel(4, "MPR121 PADS");
  scrCellValue(4, devices ? devices : "--");
  scrCellLabel(5, "STATE");
  scrCellValue(5, safeMode ? "SAFE" : "normal");
  scrCellLabel(6, "STEP");
  scrCellValue(6, note ? note : "");
  scrCellLabel(7, "SAFE MODE");
  scrCellValue(7, safeMode ? "2 failed boots" : "after 2 fails");

  // the legend band stays empty: bring-up has no controls, and the panel's
  // legend strip is the control row of a running mode
  scrUpdate();
}

void displayUpdate() {
  if (headerActive && (long)(millis() - headerDeadline) >= 0) {
    headerHide();
  }
  scrUpdate();   // OLED: one page per call; panel: only the dirty regions
}
