#include "Modes.h"
#include "Buttons.h"
#include "Display.h"
#include "SeqClock.h"
#include "Keyboard.h"
#include "Encoders.h"
#include "Bindings.h"
#include "Chords.h"
#include "Events.h"
#include "Key4.h"      // KEYMODE4 proposal LEDs (rank / focus / locks)
#include "Link.h"      // ENCABLETONMODE: clip-slot states the script feeds

ModeBase modeBase = MODE_NONE;
uint8_t modeVariant = 0;

// hold (variant 3) is momentary: remember what to restore on release
static ModeBase heldPrevBase = MODE_NONE;
static uint8_t heldPrevVariant = 0;
static bool heldActive = false;
static uint8_t heldPad = 255;        // which mode button is being held
static bool seq3ChSel = true;        // SEQ3 step-pad channel overlay up
static uint8_t seq3KbCh = 0;         // keyboard latched to a track's SEQ3 row
                                     // (0 = not latched)

static const uint8_t modeLeds[3] = { 32, 33, 34 };  // ENC, SEQ, KEY
static const ModeBase modeOfLed[3] = { MODE_ENC, MODE_SEQ, MODE_KEY };
static const char* baseNames[4] = { "---", "ENC", "SEQ", "KEY" };
// the header's left slot never shows the internal code (ENC3 / SEQ1 / KEY2) -
// whole words, and variant 3 says what it actually is: a channel picker
static const char* fullNames[4] = { "---", "ENCODER", "SEQUENCER", "KEYBOARD" };

#define MODE_DIM 64  // 25% brightness for unselected mode buttons
#define KEY_DIM 128  // keyboard idle brightness in KEY mode (dark blue)
#define DRUM_KEY_IDLE 40  // drum-track keyboard: idle dim orange-red
#define DRUM_KEY_HIT  56  // ...a note the playhead sounds: dim green blink
#define DRUM_KEY_MUTE 48  // ...a muted drum voice: dim orange
#define STEP_DIM 32   // step pad background (dark blue, barely visible)

// semitone index (0=C..11=B) for LEDs 25..41; -1 = LEDs 32..36 (not keys)
static const int8_t kbPitchIdx[17] = {
  0, 2, 4, 5, 7, 9, 11,      // 25..31: C D E F G A B
  -1, -1, -1, -1, -1,        // 32..36: mode/transport (not keys)
  1, 3, 6, 8, 10             // 37..41: C# D# F# G# A#
};

#define PULSE_PERIOD_MS 1000  // variant 2: breathing pulse
#define BLINK_MS        250   // variant 4: on/off half-period

// last non-momentary variant per family (index = ModeBase), default 1
static uint8_t familyMem[4] = { 0, 1, 1, 1 };

// per-channel KEYMODE memory: each keyboard channel recalls the KEY
// variant last latched on it (1/2/4 - the momentary hold, 3, is never
// stored).  RAM only, like familyMem
static uint8_t keyModeMem[16] = { 1, 1, 1, 1, 1, 1, 1, 1,
                                  1, 1, 1, 1, 1, 1, 1, 1 };

// ENCABLETONMODE (NEXT in ENCMODE): latched overlay, RAM only
static bool abltLatched = false;
static bool abltPrev = false;      // PREV held = pads pick a track
static bool abltAct = false;       // M3 set = F1-F4 are clip actions

static void applyModeLeds();     // defined below; recall repaints too

void keyModeRecall() {
  uint8_t v = keyModeMem[keyChannel() - 1];
  if (v < 1 || v > 4 || v == 3) v = 1;  // 3 = momentary, never a target
  familyMem[MODE_KEY] = v;    // next KEYMODE entry lands on it too
  // skip while the KEY3 hold is mid-gesture (variant 3): the release
  // path applies the channel's variant then
  if (modeBase == MODE_KEY && modeVariant != 3 && modeVariant != v) {
    // live switch while KEYMODE is showing (the keyboard moved to a
    // channel with a different remembered variant): same chord/K4
    // housekeeping modeOnEvent does on a variant change
    bool wasK4 = (modeVariant == 4);
    modeVariant = v;
    if (v == 4 && !wasK4) k4Enter();
    if (wasK4 && v != 4) k4Stop();
    applyModeLeds();
    headerSetLeft(modeHeaderLabel());
    refreshMainScreen();
  }
}

// EEPROM accessors (the SEQ block stores the per-channel memory)
uint8_t keyModeMemOf(uint8_t ch) {
  if (ch < 1 || ch > 16) return 1;
  uint8_t v = keyModeMem[ch - 1];
  return (v >= 1 && v <= 4 && v != 3) ? v : 1;
}

void keyModeMemSet(uint8_t ch, uint8_t v) {
  if (ch < 1 || ch > 16) return;
  keyModeMem[ch - 1] = (v >= 1 && v <= 4 && v != 3) ? v : 1;
}

// playback blink phase: set by pollModeLeds(), read by modeLedHook() -
// while the playhead sounds notes on the keyboard channel, their pitch
// classes flash dim green on the keys (blink wins over the idle colors)
static bool kbBlinkOn = false;

// same driver for the F-row: while the playhead reads an attributed step
// of the MONO edit channel, the F1-F4 LEDs flash dim green
static bool fBlinkOn = false;

// is the playhead sounding this exact note on the keyboard channel?
// (drums match note for note - the pitch-class form below would light a
// pad for any note of the same name, and the drum map cares which one)
static bool kbDrumPlays(uint8_t note) {
  uint8_t n = seqSoundingCount(keyChannel());
  for (uint8_t i = 0; i < n; i++)
    if (seqSoundingNote(keyChannel(), i) == note) return true;
  return false;
}

// is the playhead currently playing this pitch class on the keyboard channel?
static bool kbSeqPlays(uint8_t pc) {
  uint8_t n = seqSoundingCount(keyChannel());
  pc %= 12;
  for (uint8_t i = 0; i < n; i++)
    if (seqSoundingNote(keyChannel(), i) % 12 == pc) return true;
  return false;
}

static int8_t activeModeLed() {
  for (uint8_t i = 0; i < 3; i++) {
    if (modeBase == modeOfLed[i]) return modeLeds[i];
  }
  return -1;
}

// color for the selected mode LED: solid / pulsing / blinking
static CRGB selectedModeColor() {
  if (modeVariant == 2) {
    uint8_t s = sin8((uint32_t)millis() * 256UL / PULSE_PERIOD_MS);
    uint8_t b = 24 + scale8(s, 220);   // 24..244
    return CRGB(b, b, b);
  }
  if (modeVariant == 4) {
    return ((millis() / BLINK_MS) & 1) ? CRGB::White : CRGB::Black;
  }
  return CRGB::White;
}

static CRGB dimColorFor(ModeBase b) {
  CRGB c;
  switch (b) {
    case MODE_ENC: c = CRGB::Purple;       break;
    case MODE_SEQ: c = CRGB::Blue;         break;
    case MODE_KEY: c = CRGB::MediumSeaGreen; break;
    default:       c = CRGB::Black;        break;
  }
  c.nscale8(MODE_DIM);
  return c;
}

// SEQMODE track-type palette: ONE hue per track type.  The M2 page pad wears
// it (so the pad says which page it opens), and the M2 page's own F row
// wears it too.  RED / ORANGE / YELLOW ARE OFF LIMITS - they already mean
// REC, the drum / encoder MUTE overlays and the channel + attribute markers -
// and the step-edit attribute row keeps its blue (TIE / GLIDE / ACCENT /
// PROB), so every track hue lives in the cool arc from spring green through
// cyan and blue to indigo and pink.  Two pale tints (STRUM lavender, COMP
// pale sky) buy extra separation from their vivid neighbours by lightness
// rather than by hue.  SEQ_PAGE_DIM is the idle brightness (the page-open
// state breathes on top of it).
#define SEQ_PAGE_DIM 64
static uint8_t seqPagePulse() {          // breathing brightness 64..200
  uint8_t s = sin8((uint32_t)millis() * 256UL / PULSE_PERIOD_MS);
  return (uint8_t)(SEQ_PAGE_DIM + scale8(s, (uint8_t)(200 - SEQ_PAGE_DIM)));
}
static CRGB seqPageColor() {
  if (seqTrack() == SEQ_TRACK_DRUM) return CRGB(120, 0, 255);     // indigo
  if (seqTrack() == SEQ_TRACK_MONO) {
    if (seqMonoType() == SEQ_MONO_ARP) return CRGB(255, 0, 170);  // pink
    if (seqMonoType() == SEQ_MONO_BASS) return CRGB(0, 70, 200);  // blue
    return CRGB(46, 120, 80);                                     // LEAD sea green
  }
  if (seqPolyType() == SEQ_POLY_HARM)  return CRGB(0, 215, 215);  // aqua
  if (seqPolyType() == SEQ_POLY_STEP)  return CRGB(135, 195, 255); // pale sky
  if (seqPolyType() == SEQ_POLY_SKANK) return CRGB(0, 205, 125);  // spring green
  return CRGB(205, 135, 255);                                     // STRUM / HAND lavender
}
// does the edit track own a page on M2?  (POLY+STEP is conditional and is
// handled at the call site: its COMP page is optional)
static bool seqPagePadHas() {
  if (seqTrack() == SEQ_TRACK_DRUM) return true;
  if (seqTrack() == SEQ_TRACK_MONO) {
    return seqMonoType() == SEQ_MONO_ARP || seqMonoType() == SEQ_MONO_BASS ||
           seqMonoType() == SEQ_MONO_LEAD;
  }
  uint8_t t = seqPolyType();
  return t == SEQ_POLY_HARM || t == SEQ_POLY_SKANK || t == SEQ_POLY_STRUM ||
         t == SEQ_POLY_HAND;
}
static bool seqPagePadOpen() {
  return arpPageActive() || genPageActive() || leadPageActive() ||
         harmPageActive() || compPageActive() || skankPageActive() ||
         strumFRowActive() || drumPageActive();
}

static CRGB modeLedHook(uint8_t pad, bool isTouched, CRGB defaultColor) {
  int8_t led = ledForPad[pad];
  for (uint8_t i = 0; i < 3; i++) {
    if (led == modeLeds[i]) {
      if (isTouched) return CRGB::White;
      if (modeBase == modeOfLed[i]) return selectedModeColor();
      return dimColorFor(modeOfLed[i]);
    }
  }
  // step pads = LEDs 0..15 (groupSteps is identity 0..15): green playhead
  // owned by SEQMODE but inherited by every mode; recorded steps dimmed
  // green, empty steps dimmed dark blue
  if (led >= 0 && led < 16) {
    if (isTouched) return CRGB::White;
    // ENC mute overlay (hold M2): pad N = output channel N+1, dim
    // orange-red = playing, bright orange-red = muted (wins over the
    // variant-3 channel selector while the overlay is up)
    if (encMuteActive()) {
      CRGB c = CRGB::OrangeRed;
      if (!seqIsMuted((uint8_t)led)) c.nscale8(32);
      return c;
    }
    // ENCMODE drum-voice mute (hold M1): pad N = drum note 36+N, dim
    // orange-red = voice playing, bright = muted (all drum channels)
    if (drumVoiceMuteActive()) {
      CRGB c = CRGB::OrangeRed;
      if (!drumVoiceMuted((uint8_t)(36 + led))) c.nscale8(32);
      return c;
    }
    // ENCABLETONMODE: the pads are Live's 16 clip slots of the selected
    // track (PREV held: the 16 tracks).  Empty reads dark, a clip dim green,
    // the playing one bright green, a triggered one blinks, armed dim red -
    // every bit comes from the script's CLIP_STATE
    if (modeBase == MODE_ENC && abltOn() && modeVariant != 3) {
      if (isTouched) return CRGB::White;
      if (abltPrevPick()) {
        if (led == (int8_t)(encChannel() - 1)) return CRGB::Yellow;
        CRGB y = CRGB::Yellow;
        y.nscale8(16);
        return y;
      }
      uint8_t bits = linkClipBits((uint8_t)led);
      if (bits & 0x02) return CRGB::Green;              // clip playing
      if (bits & 0x04) {                                // triggered: blink
        if ((millis() / BLINK_MS) & 1) return CRGB::Green;
        CRGB td = CRGB::Green;
        td.nscale8(40);
        return td;
      }
      if (bits & 0x01) {                                // has a clip
        CRGB hc = CRGB::Green;
        hc.nscale8(64);
        return hc;
      }
      if (bits & 0x08) {                                // track armed
        CRGB ar = CRGB::Red;
        ar.nscale8(48);
        return ar;
      }
      CRGB em = CRGB::DarkBlue;
      em.nscale8(32);
      return em;
    }
    // channel overlays: the 16 steps become the MIDI channel selector -
    // dimmed yellow background, selected channel in bright yellow
    // (KEY = keyboard output, SEQ = sequencer edit, ENC = encoder CCs).
    // SEQ3 only until an encoder moves: the pads then show the sequencer.
    // plain ENCMODE keeps it up for the whole variant (locked picker)
    if (chSelOverlay()) {
      uint8_t sel = (modeBase == MODE_KEY) ? keyChannel() :
                     (modeBase == MODE_SEQ) ? seqEditChannel() : encChannel();
      if (led == (int8_t)(sel - 1)) return CRGB::Yellow;
      CRGB y = CRGB::Yellow;
      y.nscale8(16);
      return y;
    }
    // DRUM part-select overlay (hold M1): dim orange, selected part bright
    // orange (pad N = drum note 36+N, pad0 = C1 ... pad15 = D#2) - orange is
    // reserved for these mute/select overlays, never for a track hue
    if (modeBase == MODE_SEQ && modeVariant != 3 &&
        drumPartSelectActive() && seqTrack() == SEQ_TRACK_DRUM) {
      if (led == (int8_t)(drumPartNote() - 36)) return CRGB(255, 72, 0);
      CRGB o = CRGB(255, 72, 0);
      o.nscale8(16);
      return o;
    }
    uint8_t vst = (uint8_t)(led + seqStepViewBase());
    // past the pattern length: the pad is dark (touch still lights it white)
    if (!seqStepUsed(vst)) return CRGB::Black;
    {
      // playhead of the edit window (the pads show one 16-step bar at a
      // time; the sequence itself may run up to 64 steps): green when the
      // bar on the pads is the one being read, dark-blue ghost at the same
      // beat position while playback runs in another bar
    uint8_t ph = seqStepPlayPad();
    if (ph < 16 && led == groupSteps[ph]) return CRGB::Green;
    uint8_t gh = seqStepPlayGhost();
    if (gh < 16 && led == groupSteps[gh]) return CRGB::DarkBlue;
    // KEYMODE4: the write target (focus) and the steps apply skips (locked)
    // stay visible under the playhead colors
    if (modeBase == MODE_KEY && modeVariant == 4) {
      bool foc = (vst == k4Focus());
      if (foc && k4Locked(vst)) return CRGB(255, 200, 40);   // amber
      if (foc) return k4Write() ? CRGB::Red : CRGB::White;   // write target
      if (k4Locked(vst)) {
        CRGB a = CRGB(255, 200, 40);                          // dim amber
        a.nscale8(64);
        return a;
      }
    }
    }
    // GNR8 mod pages (F2/F3/F4): the step armed for the dial breathes
    // violet - the playhead above still wins so the head stays readable
    // (the page's own 30 ms refresh loop drives the pulse)
    if (genPageActive() && genOptionSel() >= 1 && genOptionSel() <= 3 &&
        vst == genStepSelStep()) {
      uint8_t s = sin8((uint32_t)millis() * 256UL / PULSE_PERIOD_MS);
      uint8_t b = 56 + scale8(s, 160);             // 56..216 breathing
      CRGB p = CRGB::Purple;
      p.nscale8(b);
      return p;
    }
    // STEP OCT page (F2): steps carrying an octave shift light up on a
    // dark-purple -> bright-pink ramp keyed to the value (-2 .. +2; the
    // picked step above outranks the ramp, unshifted steps stay as they
    // were)
    if (genPageActive() && genOptionSel() == 1) {
      int8_t ov = stepOctAt((uint8_t)vst);
      if (ov) {
        uint8_t k = (uint8_t)(ov + 2);             // -2..-1,+1..+2 -> 0..4
        return CHSV((uint8_t)(196 + k * 10), 255,
                    (uint8_t)(56 + k * 48));       // 196/56 .. 236/248
      }
    }
    // SKANK lock hold (POLY+SKANK + hold step): the step being locked
    // breathes violet, same gesture as the GNR8 armed dial (the playhead
    // above still wins so the head stays readable; the lock row's own
    // 30 ms refresh loop drives the pulse)
    if (currentBinding().enc == ENC_SEQ_SKANK_STEP &&
        seqTrStepSelGet() == (int8_t)vst) {
      uint8_t s = sin8((uint32_t)millis() * 256UL / PULSE_PERIOD_MS);
      uint8_t b = 56 + scale8(s, 160);             // 56..216 breathing
      CRGB p = CRGB::Purple;
      p.nscale8(b);
      return p;
    }
    CRGB c;
    if (stepIsActive(vst)) {
      // selected-attribute pulse: while an attribute target is engaged,
      // the steps carrying it breathe YELLOW instead of dim static green
      if (stepEditActive() &&
          (stepAttrs(vst) &
           (uint8_t)(1 << (stepEditSel() - 1)))) {
        uint8_t s = sin8((uint32_t)millis() * 256UL / PULSE_PERIOD_MS);
        uint8_t b = 48 + scale8(s, 120);           // 48..168 breathing
        c = CRGB::Yellow;
        c.nscale8(b);
      } else {
        c = CRGB::Green;
        c.nscale8(64);
      }
    } else {
      c = CRGB::DarkBlue;
      c.nscale8(STEP_DIM);
    }
    return c;
  }
  // keyboard pads (white keys 25..31, black keys 37..41): visible in every
  // mode (behavior inherited); color follows the KEY family variant —
  // current variant while in KEYMODE, last latched KEY variant elsewhere
  if ((led >= 25 && led <= 31) || (led >= 37 && led <= 41)) {
    // hold-M2 overdub-layer overlay: the row is the layer panel - white
    // C D E F = layers 1..4 (bright hue = holds notes, dim = empty, red =
    // muted), the five black keys = MUTE / ERASE / UNDO / REDO / RESET with
    // the selected one in its own colour (mute = dim orange pulsing, erase
    // = dim dark red), G A B dark, gap LEDs black.  Touch still
    // flashes white; the panel wins over the bar row and every inherited
    // key color while it is up
    if (layOvActive()) {
      if (isTouched) return CRGB::White;
      int8_t lk = kbPitchIdx[led - 25];
      if (lk < 0) return CRGB::Black;          // gap LEDs between the keys
      CRGB c = seqPageColor();                 // the M2 hue the hold rode in on
      if (lk == 0 || lk == 2 || lk == 4 || lk == 5) {   // white = layers 1..4
        uint8_t layer = (lk == 0) ? 0 : (lk == 2) ? 1 : (lk == 4) ? 2 : 3;
        uint8_t info = layInfo(layer);
        if (info & 2) {                        // muted: red instead of hue
          CRGB r = CRGB::Red;
          r.nscale8(72);
          return r;
        }
        c.nscale8((info & 1) ? 200 : 56);      // holds notes / empty
        return c;
      }
      if (lk == 1 || lk == 3 || lk == 6 || lk == 8 || lk == 10) {
        // black = commands; the SELECTED one wears its own colour instead
        // of the track hue - mute = dim orange, breathing; erase = dim dark
        // red (static).  Unselected commands keep the dim track hue
        if (lk == 1 && layArmCmd() == LAY_CMD_MUTE) {
          uint8_t s = sin8((uint32_t)millis() * 256UL / PULSE_PERIOD_MS);
          CRGB o = CRGB::Orange;
          o.nscale8((uint8_t)(48 + ((uint16_t)s * 112) / 255));  // dim 48..160
          return o;
        }
        if (lk == 3 && layArmCmd() == LAY_CMD_ERASE) {
          CRGB r = CRGB::DarkRed;            // dim dark red, no pulse
          r.nscale8(176);
          return r;
        }
        c.nscale8(72);
        return c;
      }
      c.nscale8(40);                           // G A B: nothing assigned
      return c;
    }
    // SEQMODE3: the keyboard is not an instrument here - C D E F become
    // the pattern bars (C = steps 1-16 ... F = 49-64): dim blue background
    // with the EDITED bar bright; the bar the playhead reads breathes
    // white (follows playback across bars, patterns > 16 steps only) and a
    // looping bar breathes green - the keys still play, just invisible
    if (seq3KbRole()) {
      if (isTouched) return CRGB::White;
      // one pulse phase for the whole row (verbs below, bars after)
      uint8_t s = sin8((uint32_t)millis() * 256UL / PULSE_PERIOD_MS);

      // the black keys of the bar octave are the track verbs, so they read
      // as a second row: the length pair pulses LIGHT BLUE (C# dim, D# bright
      // - one colour, two intensities, the way the pair works together),
      // copy / paste sit dim MediumSeaGreen, and follow is dim green that
      // breathes - still dim - while it is ON, so its state shows at a glance
      if (led >= 37 && led <= 41) {
        if (led == 37 || led == 38) {
          CRGB b = CRGB(140, 200, 255);            // light blue
          // D# is the faster breath of the pair, C# the slow one.  Both stay
          // well below a bar LED: this row is a reminder of which verb sits
          // under the finger, not a status light - the peak used to glare
          uint16_t per = (led == 38) ? 550 : 1500;  // ms per breath
          uint8_t pv = sin8((uint32_t)millis() * 256UL / per);
          uint8_t hi = (led == 38) ? 104 : 56;     // pulse ceiling
          b.nscale8((uint8_t)(12 + scale8(pv, hi)));
          return b;
        }
        if (led == 40) {                           // G#: bar follow
          CRGB g = CRGB::Green;
          g.nscale8(seqStepViewChaseGet() ? (uint8_t)(32 + scale8(s, 96))
                                          : (uint8_t)48);
          return g;
        }
        CRGB m = CRGB(60, 179, 113);               // F# / A#: copy / paste
        m.nscale8(96);
        return m;
      }
      // the white keys are the bars - any other LED falls through
      int8_t bar = (led == 25) ? 0 : (led == 26) ? 1 :
                   (led == 27) ? 2 : (led == 28) ? 3 : -1;
      if (bar < 0) return CRGB::Black;
      if (seqBarLooped((uint8_t)bar)) {
        CRGB g = CRGB::Green;         // looping bar: breathing green
        g.nscale8((uint8_t)(48 + scale8(s, 192)));
        return g;
      }
      // one-bar patterns never cross a bar edge: keep the LED static
      uint8_t pb = (seqPatLen() > 16) ? seqPlayBar() : 0xFF;
      if (bar == (int8_t)pb && seqIsRunning()) {
        CRGB w = CRGB::White;         // played bar: breathing white
        w.nscale8((uint8_t)(48 + scale8(s, 192)));
        return w;
      }
      if (bar == (int8_t)seqStepPage())
        return CRGB::DarkBlue;        // edited bar: full bright
      CRGB c = CRGB::DarkBlue;        // the other bars recede
      c.nscale8(32);
      return c;
    }
    int8_t k = kbPitchIdx[led - 25];          // -1 for non-key LEDs in range
    uint8_t kv = keyBehaviorVariant();
    bool inScale = (keyScale() == 0) || keyIsInScale((uint8_t)k);
    // DRUM track: these pads are drum notes (C..B of the kit), not pitches -
    // there is no KEY and no SCALE to show, so the whole row reads as one
    // dim orange-red pad grid and every scale / chord / proposal color is
    // skipped (a pad is never dark for being out of scale)
    bool drum = keyDrumTrack();
    if (isTouched) {
      // HAND strum zone: every key touch is a pluck/toggle feedback flash
      // (plucks bypass the scale filter, black keys toggle options)
      if (handLive()) return CRGB::White;
      // a key the scale filter silences flashes red (chord mode / KeyLock
      // play it, so they flash white; KEYMODE4 plays any PROPOSED key -
      // out-of-scale keys included, so they never flash red either)
      if (!drum && kv != 2 && kv != 4 && !chordKeyLock() && !inScale)
        return CRGB(40, 0, 0);
      return CRGB::White;
    }
    if (drum) {
      if (k < 0) return CRGB::Black;          // 32..36 are not keys
      // the pad plays its chromatic note C..B at the keyboard's octave, so
      // the kit's C1 = 36 sits at octave 1 - those drum-map ids are what the
      // playhead sounds and what the voice mutes hold
      uint8_t dn = (uint8_t)(12L * keyOctave() + 24L + k);
      // a muted voice is never heard at all, so it reads dim orange and the
      // blink never lights it (mute wins)
      if (drumVoiceMuted(dn)) {
        CRGB m = CRGB::Orange;
        m.nscale8(DRUM_KEY_MUTE);
        return m;
      }
      // the playhead sounding this pad's note right now: dim green blink
      if (kbBlinkOn && kbDrumPlays(dn)) {
        CRGB g = CRGB::Green;
        g.nscale8(DRUM_KEY_HIT);
        return g;
      }
      CRGB d = CRGB::OrangeRed;            // otherwise one dim orange-red row
      d.nscale8(DRUM_KEY_IDLE);
      return d;
    }
    // sequence playing on the keyboard channel: those pitch classes blink
    // dim green - wins over the chord tint and the scale (playback feedback).
    // HAND strums instead of hearing playback - its strings keep the display
    if (kbBlinkOn && kbSeqPlays((uint8_t)k) && !handLive()) {
      CRGB g = CRGB::Green;
      g.nscale8(64);
      return g;
    }
    // HAND strum zone: the 7 white keys are the strings - a pink ->
    // seagreen gradient left to right (C..B, hue 247..107); strings without
    // a chord or with a guitar gap dim to almost nothing.  The 5 black keys
    // are the silent Le Strum option toggles: violet when on, dim dark
    // violet when off.  Wins over the kv2 chord tint and the kv4 proposal
    // rainbow so the keyboard reads as one instrument (touch flashes white)
    if (handLive()) {
      static const int8_t wStr[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
      static const int8_t bIdx[12] = { -1, 0, -1, 1, -1, -1, 2, -1, 3, -1, 4, -1 };
      static const uint8_t obit[5] = { STRUM_OPT_GUITAR, STRUM_OPT_GUITAR2,
                                       STRUM_OPT_ADDS, STRUM_OPT_SUS,
                                       STRUM_OPT_SUSCOM };
      if (k < 0) return CRGB::Black;          // gap LEDs between the keys
      int8_t bi = bIdx[k];
      if (bi >= 0)                           // black key = option toggle
        return (strumOptsGet() & obit[bi]) ? CRGB(110, 0, 190)
                                           : CRGB(14, 0, 26);
      uint8_t note = handStrNote((uint8_t)wStr[k]);
      CRGB g = CHSV((uint8_t)(247 - (uint8_t)((led - 25) * 140 / 6)),
                    255, 255);
      g.nscale8(KEY_DIM);
      if (note == 0 || note == 0xFF) g.nscale8(32);   // silent / gap string
      return g;
    }
    // sounding chord keys tint cyan (they win over the scale's dark keys)
    if (kv == 2 && chordPlaysPitch((uint8_t)k)) return CRGB(0, 90, 160);
    // KEYMODE4: the whole scored degree set lights - hue-mapped by key as
    // originally designed (C=blue ... B=red), rank 0 (the next chord from
    // the highest probability) hottest, brightness stepping down the list;
    // borrowed proposals (open by TENS) stay orange, closed ones stay dark
    if (kv == 4) {
      int8_t rk = k4KeyRank((uint8_t)k);
      if (rk < 0) return CRGB::Black;
      CRGB c;
      if (k4KeyBorrowed((uint8_t)k)) {
        c = CRGB(255, 120, 0);
      } else {
        uint8_t hue = (uint8_t)(170 + ((uint16_t)k * 85 + 5) / 11);
        c = CHSV(hue, 255, 255);
      }
      int b = 255 - (int)rk * 64;     // steep: rank 0 unmistakably hottest
      if (b < 40) b = 40;             // rank 4+ stay dim but visible
      c.nscale8((uint8_t)b);
      c.nscale8(KEY_DIM);
      return c;
    }
    if (!inScale) return CRGB::Black;         // scale display: dead keys dark
    CRGB c;
    if (kv == 2) {
      c = CRGB(60, 0, 255);                   // blue-violet
    } else {
      c = CRGB::DarkBlue;                     // KEYMODE / KEYMODE3
    }
    c.nscale8(KEY_DIM);
    return c;
  }
  // SEQMODE step-edit radio: M1 + F1-F4 (MONO) or M1 + F3/F4 (DRUM) keep
  // their mode visible at all times; POLY lights F1-F4 too (FX/TYPE/ACC/
  // PROB) - bright white = the target the pads currently edit (M1 while no
  // attribute is selected), dim dark blue background otherwise.  The M2
  // ARP/GNR8 pages are part of the same radio on MONO tracks: open, they
  // drop the edit selection (and dim M1), an armed target drops the page.
  //  Touch wins over everything
  bool seqMonoRadio = (modeBase == MODE_SEQ && modeVariant != 3 &&
                       seqTrack() == SEQ_TRACK_MONO);
  bool seqPolyRadio = (modeBase == MODE_SEQ && modeVariant != 3 &&
                       seqTrack() == SEQ_TRACK_POLY);
  bool seqDrumRadio = (modeBase == MODE_SEQ && modeVariant != 3 &&
                       seqTrack() == SEQ_TRACK_DRUM);
  // SEQMODE + DRUM page open: the F row is the page's KNOB-SET radio
  // (F1 GROOVE / F2 LINES / F3 FILL% / F4 MOT%), so it lights in the DRUM
  // track hue - dim when idle, bright for the armed set (base row = all dim)
  if (modeBase == MODE_SEQ && modeVariant != 3 && drumPageActive() &&
      led >= groupControls[5] && led <= groupControls[8]) {
    if (isTouched) return CRGB::White;
    uint8_t ssel = drumSetSelGet();
    bool armed = (ssel != 0xFF &&
                  ssel == (uint8_t)(led - groupControls[5]));
    CRGB p = seqPageColor();              // same hue as the DRUM M2 pad
    p.nscale8(armed ? 176 : 48);
    return p;
  }
  if ((seqMonoRadio &&
       (led == groupControls[2] ||
        (led >= groupControls[5] && led <= groupControls[8]))) ||
      ((seqDrumRadio || seqPolyRadio) &&
       (led == groupControls[2] ||
        (seqPolyRadio && led >= groupControls[5] &&
                        led <= groupControls[8]) ||
        (!seqPolyRadio && (led == groupControls[5] + 2 ||
                           led == groupControls[5] + 3))))) {
    if (isTouched) return CRGB::White;
    // strum ADDS row open: F1..F3 burn the STRUM/HAND hue when their color
    // is armed (sus4/add6/add9), F4 lights while anything is armed (it
    // clears), a dim version of the same hue otherwise - the step-edit
    // radio and its blue stay out of the way
    if (strumFRowActive() && led >= groupControls[5] &&
        led <= groupControls[8]) {
      uint8_t fi = (uint8_t)(led - groupControls[5]);
      uint8_t ext = strumExtGet();
      bool lit = (fi < 3) ? (((ext >> fi) & 1) != 0) : (ext != 0);
      CRGB e = seqPageColor();           // the STRUM/HAND hue of the M2 pad
      e.nscale8(lit ? 176 : 48);
      return e;
    }
    // ARP page open: the F row = the four encoder sub-pages (order &
    // shape / motion / generative / engine opts) - dim TRACK COLOR marks the
    // borrowed row, the armed page burns bright, so the row's owner is
    // always visible and the row matches the M2 pad that opened it
    if (arpPageActive() && led >= groupControls[5] &&
        led <= groupControls[8]) {
      uint8_t sel = arpOptionSel();
      bool armed = (sel != 0xFF &&
                    sel == (uint8_t)(led - groupControls[5]));
      CRGB p = seqPageColor();
      p.nscale8(sel == 0xFF ? 48 : (armed ? 176 : 32));
      return p;
    }
    // GNR8 page open: the F row = option radios (F1 = full OPTS page,
    // F2 = OCT/MAP, F3 = BRW, F4 = PROB) - dim TRACK COLOR marks the
    // borrowed row (no target blink, no chain mask), the ARMED radio
    // burns bright so the sub-page it owns is visible
    if (genPageActive() && led >= groupControls[5] &&
        led <= groupControls[8]) {
      uint8_t sel = genOptionSel();
      bool armed = (sel != 0xFF &&
                    sel == (uint8_t)(led - groupControls[5]));
      CRGB p = seqPageColor();
      p.nscale8(sel == 0xFF ? 48 : (armed ? 176 : 32));
      return p;
    }
    if ((seqMonoRadio || stepEditActive()) &&
        led >= groupControls[5] && fBlinkOn &&
        (seqFMask() & (uint8_t)(1 << (led - groupControls[5])))) {
      CRGB g = CRGB::Green;
      g.nscale8(64);
      return g;
    }
    // which F pad carries the selected target (MONO: F1-F4 = targets 1-4;
    // DRUM: F3/F4 = 3/4; POLY: F1 = FX (5), F2 = TYPE (6), F3/F4 = 3/4).
    // M1 is white only while it owns the radio: with the M2 ARP/GNR8 page
    // open the keys are one radio, so M1 dims like every unselected member
    uint8_t sel = stepEditSel();
    uint8_t wantF = 0;
    if (sel != 0) {
      if (seqPolyRadio && sel == 5) wantF = 1;
      else if (seqPolyRadio && sel == 6) wantF = 2;
      else wantF = sel;
    }
    bool pageOwns = arpPageActive() || genPageActive() || leadPageActive() ||
                    harmPageActive() || compPageActive() || skankPageActive() ||
                    drumPageActive();
    if ((sel == 0 && !pageOwns && led == groupControls[2]) ||
        (wantF != 0 && led >= groupControls[5] &&
         wantF == (uint8_t)(led - groupControls[5] + 1))) {
      CRGB s = CRGB::White;
      s.nscale8(80);
      return s;
    }
    CRGB d = CRGB::DarkBlue;
    d.nscale8(32);
    return d;
  }
  // SEQMODE M2 on a track that owns an encoder page: the pad carries that
  // TRACK TYPE's hue (seqPageColor) and BREATHES while its page is open
  // instead of sitting dim - so DRUM indigo, ARP pink, BASS blue, LEAD sea
  // green, STRUM/HAND lavender, HARM aqua, COMP pale sky and SKANK spring
  // green are all tellable apart at a glance, and no page is a blind key
  // (the F row of the open page wears the same hue: see above).
  // POLY+STEP is the one conditional case: the COMP page is optional, so
  // with it closed the pad keeps its generic M2 look
  if (modeBase == MODE_SEQ && modeVariant != 3 && led == groupControls[3]) {
    bool polyStep = (seqTrack() == SEQ_TRACK_POLY &&
                     seqPolyType() == SEQ_POLY_STEP);
    bool live = seqPagePadHas() || (polyStep && compPageActive());
    if (live) {
      if (isTouched) return CRGB::White;
      CRGB c = seqPageColor();
      c.nscale8(seqPagePadOpen() ? seqPagePulse() : SEQ_PAGE_DIM);
      return c;
    }
  }
  // ...and M3 on the generator tracks is the GENERATOR button (tap =
  // generate, hold = undo): a slow dim hue cycle, mirroring KEYMODE4's
  // APPLY - BASS, LEAD and the HARM harmonizer
  if (modeBase == MODE_SEQ && modeVariant != 3 && led == groupControls[4] &&
      ((seqTrack() == SEQ_TRACK_MONO &&
        (seqMonoType() == SEQ_MONO_BASS || seqMonoType() == SEQ_MONO_LEAD)) ||
       (seqTrack() == SEQ_TRACK_POLY && seqPolyType() == SEQ_POLY_HARM))) {
    if (isTouched) return CRGB::White;
    return CHSV((uint8_t)(millis() >> 2), 255, 100);  // full cycle ~1s
  }
  // ENCMODE function pads: always show their identity colors even when
  // idle - M1 dim orange, M2 dim orange-red (touch still wins).  M3 is
  // the page-group toggle: dim medium sea green = lower group (CC1..16),
  // purple = upper group (CC21..36).  The F row is the ENC5..8 CC-bank
  // radio: dim purple (lower) / dim medium sea green (upper) idle, bright
  // white = the armed bank (F1 selected by default on entry)
  if (modeBase == MODE_ENC &&
      (led == groupControls[2] || led == groupControls[3] ||
       led == groupControls[4] ||
       (led >= groupControls[5] && led <= groupControls[8]))) {
    if (isTouched) return CRGB::White;
    if (led >= groupControls[5] && led <= groupControls[8]) {
      if (led == groupControls[5] + encBank()) return CRGB::White;
      CRGB p = encM3Page() ? CRGB::MediumSeaGreen : CRGB::Purple;
      p.nscale8(48);
      return p;
    }
    if (led == groupControls[4]) {
      CRGB m = encM3Page() ? CRGB::Purple : CRGB::MediumSeaGreen;
      m.nscale8(encM3Page() ? 176 : 48);   // upper group burns purple
      return m;
    }
    CRGB c = (led == groupControls[2]) ? CRGB::Orange : CRGB::OrangeRed;
    c.nscale8(48);
    return c;
  }
  // KEYMODE2 chord row: the quality momentaries (NEXT/M1/M2/M3) and the
  // extensions (F1-F4) show the KEY button's dim green while idle - the
  // extensions a touch darker (both are inert once KeyLock overrides the
  // chord).  Touch still wins; ENCMODE/SEQMODE keep their own colors above
  if (modeBase == MODE_KEY && modeVariant == 2 &&
      led >= groupControls[1] && led <= groupControls[8]) {
    if (isTouched) return CRGB::White;
    CRGB c = dimColorFor(MODE_KEY);
    if (led > groupControls[4]) {     // extensions: same green, less R+B
      c.r = c.r / 2;                  //   (purer and a touch darker)
      c.b = c.b / 2;
    }
    return c;
  }
  // KEYMODE4 M1: the write state - dim red = AUDITION (proposals only,
  // nothing lands), bright red = WRITE (keys land on the focus step)
  if (modeBase == MODE_KEY && modeVariant == 4 &&
      led == groupControls[2]) {
    if (isTouched) return CRGB::White;
    if (k4Write()) return CRGB::Red;
    CRGB r = CRGB::Red;
    r.nscale8(48);
    return r;
  }
  // KEYMODE4 M3 (APPLY): a slow dim hue cycle marks the generator button
  if (modeBase == MODE_KEY && modeVariant == 4 &&
      led == groupControls[4]) {
    if (isTouched) return CRGB::White;
    return CHSV((uint8_t)(millis() >> 2), 255, 100);  // full cycle ~1s
  }
  // KEYMODE4 TONALITY page (F1..F4 = the four banks): the open bank is a
  // solid white pad, the other three drift through one slow hue cycle, so the
  // row reads as "one chosen, three to try".  The value stays well clear of
  // the bottom of the range: the strip runs at FastLED brightness 25, and a
  // dim value down at ~50 gets torn up by the temporal dithering (the pad
  // blinks colour/black instead of gliding through the hues)
  if (modeBase == MODE_KEY && modeVariant == 4 && k4Tonality() &&
      led >= groupControls[5] && led <= groupControls[8]) {
    uint8_t bi = (uint8_t)(led - groupControls[5]);
    if (bi == k4TonaBank()) return CRGB::White;
    return CHSV((uint8_t)(millis() >> 4), 255, isTouched ? 255 : 130);
  }
  // KEYMODE4 M2 (TONALITY menu): the same hue-cycle marker, four times
  // slower, brighter while the page is open - the button reads as "menu"
  // without borrowing the hue of any SEQMODE page
  if (modeBase == MODE_KEY && modeVariant == 4 &&
      led == groupControls[3]) {
    if (isTouched) return CRGB::White;
    return CHSV((uint8_t)(millis() >> 4), 255, k4Tonality() ? 200 : 110);
  }
  // PLAY transport pad: dim dark green = stopped, bright green = running
  // (touch always wins)
  if (led == groupTransport[0]) {
    if (isTouched) return CRGB::White;
    if (seqIsRunning()) return CRGB::Green;
    CRGB g = CRGB::Green;
    g.nscale8(48);
    return g;
  }
  // REC transport pad: dim dark red = idle, bright red = live record armed
  // / running (a touch always wins)
  if (led == groupTransport[1]) {
    if (isTouched) return CRGB::White;
    if (seqRecState()) return CRGB::Red;
    CRGB r = CRGB::Red;
    r.nscale8(48);
    return r;
  }
  return defaultColor;
}

static void applyModeLeds() {
  int8_t active = activeModeLed();
  for (uint8_t i = 0; i < 3; i++) {
    leds[modeLeds[i]] = (modeLeds[i] == active)
      ? selectedModeColor()
      : dimColorFor(modeOfLed[i]);
  }
  ledsShow();
  refreshAllLeds();   // keyboard/step pads follow the new mode via the hook
}

void pollModeLeds() {
  static unsigned long lastPulseShow = 0;
  static int8_t lastBlink = -1;

  // ENCABLETONMODE: clip states and the triggered-clip blink come from the
  // script, so the pad row is repainted at ~30 fps while the overlay is up
  if (modeBase == MODE_ENC && abltOn()) {
    static unsigned long abltFlip = 0;
    if ((long)(millis() - abltFlip) >= 0) {
      abltFlip = millis() + 30;
      refreshAllLeds();
    }
  }

  // playback blink driver: flash the keys the playhead sounds on the
  // keyboard channel (edge on/off repaints immediately - a finished chord
  // never keeps its green; the phase flip drives the flashing)
  static bool kbActive = false;
  static unsigned long kbFlip = 0;
  bool kbNow = (seqSoundingCount(keyChannel()) > 0);
  if (kbNow != kbActive) {
    kbActive = kbNow;
    if (!kbActive) kbBlinkOn = false;
    refreshAllLeds();
  }
  if (kbActive && (long)(millis() - kbFlip) >= 0) {
    kbFlip = millis() + BLINK_MS;
    kbBlinkOn = !kbBlinkOn;
    refreshAllLeds();
  }

  // F-row attribute blink driver (same cadence as the keyboard blink):
  // mask comes from the playhead, edge on/off repaints immediately so a
  // finished attributed step never keeps its green
  static bool fActive = false;
  static unsigned long fFlip = 0;
  bool fNow = (seqFMask() != 0);
  if (fNow != fActive) {
    fActive = fNow;
    if (!fActive) fBlinkOn = false;
    refreshAllLeds();
  }
  if (fActive && (long)(millis() - fFlip) >= 0) {
    fFlip = millis() + BLINK_MS;
    fBlinkOn = !fBlinkOn;
    refreshAllLeds();
  }

  // live-record edge: repaint the REC LED on every state change and show
  // the one-shot message when a record pass finished by itself
  static uint8_t recLast = 0;
  uint8_t recNow = seqRecState();
  if (recNow != recLast) {
    recLast = recNow;
    refreshAllLeds();
  }
  if (seqRecTakeDone()) headerShow("Record Done");

  // transport edge: repaint the PLAY LED on every start/stop (button,
  // external clock, INT transport alike)
  static bool playLast = false;
  bool playNow = seqIsRunning();
  if (playNow != playLast) {
    playLast = playNow;
    refreshAllLeds();
  }

  // attribute-step pulse driver: while an attribute target is engaged,
  // repaint at ~30 fps so the marked steps breathe (edge restores static
  // colors when the radio closes or the target goes back to STEPS)
  static bool attrActive = false;
  static unsigned long attrFlip = 0;
  bool attrNow = stepEditActive();
  if (attrNow != attrActive) {
    attrActive = attrNow;
    refreshAllLeds();
  }
  if (attrActive && (long)(millis() - attrFlip) >= 0) {
    attrFlip = millis() + 30;
    refreshAllLeds();
  }

  // SEQMODE3 pattern-bar pulse: while SEQ3 is up, repaint at ~30 fps so
  // the played / looping bars breathe (edge restores static colors)
  static bool barActive = false;
  static unsigned long barFlip = 0;
  bool barNow = seq3KbRole();
  if (barNow != barActive) {
    barActive = barNow;
    refreshAllLeds();
  }
  if (barActive && (long)(millis() - barFlip) >= 0) {
    barFlip = millis() + 30;
    refreshAllLeds();
  }

  // M2 arp-page pulse: while the ARP encoder page is open, repaint at
  // ~30 fps so that pad breathes purple (edge restores the static dim)
  static bool arpActive = false;
  static unsigned long arpFlip = 0;
  bool arpNow = arpPageActive();
  if (arpNow != arpActive) {
    arpActive = arpNow;
    refreshAllLeds();
  }
  if (arpActive && (long)(millis() - arpFlip) >= 0) {
    arpFlip = millis() + 30;
    refreshAllLeds();
  }

  // M2 GNR8-page pulse: same breathing purple for the generator page
  // (and it keeps the M3 hue cycle moving while the page is open)
  static bool genActive = false;
  static unsigned long genFlip = 0;
  bool genNow = genPageActive();
  if (genNow != genActive) {
    genActive = genNow;
    refreshAllLeds();
  }
  if (genActive && (long)(millis() - genFlip) >= 0) {
    genFlip = millis() + 30;
    refreshAllLeds();
  }

  // M2 LEAD-page pulse: same breathing sea green + M3 hue cycle while the
  // LEAD generator page is open (mirrors the GNR8 pulse driver)
  static bool leadActive = false;
  static unsigned long leadFlip = 0;
  bool leadNow = leadPageActive();
  if (leadNow != leadActive) {
    leadActive = leadNow;
    refreshAllLeds();
  }
  if (leadActive && (long)(millis() - leadFlip) >= 0) {
    leadFlip = millis() + 30;
    refreshAllLeds();
  }

  // M2 HARM-page pulse: breathing teal + M3 hue cycle while the HARM
  // generator page is open (mirrors the GNR8/LEAD pulse drivers)
  static bool harmActive = false;
  static unsigned long harmFlip = 0;
  bool harmNow = harmPageActive();
  if (harmNow != harmActive) {
    harmActive = harmNow;
    refreshAllLeds();
  }
  if (harmActive && (long)(millis() - harmFlip) >= 0) {
    harmFlip = millis() + 30;
    refreshAllLeds();
  }

  // M2 COMP-page pulse: breathing teal while the COMP encoder page is open
  static bool compActive = false;
  static unsigned long compFlip = 0;
  bool compNow = compPageActive();
  if (compNow != compActive) {
    compActive = compNow;
    refreshAllLeds();
  }
  if (compActive && (long)(millis() - compFlip) >= 0) {
    compFlip = millis() + 30;
    refreshAllLeds();
  }

  // M2 SKANK-page pulse: breathing orange while the SKANK encoder page is
  // open (the lock row's held step pulses through the step LED path)
  static bool skankActive = false;
  static unsigned long skankFlip = 0;
  bool skankNow = skankPageActive();
  if (skankNow != skankActive) {
    skankActive = skankNow;
    refreshAllLeds();
  }
  if (skankActive && (long)(millis() - skankFlip) >= 0) {
    skankFlip = millis() + 30;
    refreshAllLeds();
  }

  // M2 DRUM-page pulse: breathing indigo while the pattern page is open
  static bool drumActive = false;
  static unsigned long drumFlip = 0;
  bool drumNow = drumPageActive();
  if (drumNow != drumActive) {
    drumActive = drumNow;
    refreshAllLeds();
  }
  if (drumActive && (long)(millis() - drumFlip) >= 0) {
    drumFlip = millis() + 30;
    refreshAllLeds();
  }

  // hold-M2 layer panel: the selected MUTE key breathes dim orange, so the
  // row needs a repaint at ~30 fps while the panel is up (the edge restores
  // the static colours on release)
  static bool layActive = false;
  static unsigned long layFlip = 0;
  bool layNow = layOvActive();
  if (layNow != layActive) {
    layActive = layNow;
    refreshAllLeds();
  }
  if (layActive && (long)(millis() - layFlip) >= 0) {
    layFlip = millis() + 30;
    refreshAllLeds();
  }

  // KEYMODE4: the M3 generator marker (1 s cycle) and the M2 menu marker
  // (4 s) both move, so the row repaints at ~30 fps while the variant is up
  static bool k4Active = false;
  static unsigned long k4Flip = 0;
  bool k4Now = (modeBase == MODE_KEY && modeVariant == 4);
  if (k4Now != k4Active) {
    k4Active = k4Now;
    refreshAllLeds();
  }
  if (k4Active && (long)(millis() - k4Flip) >= 0) {
    k4Flip = millis() + 30;
    refreshAllLeds();
  }

  int8_t active = activeModeLed();
  if (active < 0) { lastBlink = -1; return; }

  if (modeVariant == 2) {
    unsigned long now = millis();
    if (now - lastPulseShow < 30) return;   // ~30 fps
    lastPulseShow = now;
    leds[active] = selectedModeColor();
    ledsShow();
  } else if (modeVariant == 4) {
    int8_t phase = (millis() / BLINK_MS) & 1;
    if (phase == lastBlink) return;
    lastBlink = phase;
    leds[active] = selectedModeColor();
    ledsShow();
  } else {
    lastBlink = -1;
  }
}

void initModes() {
  modeBase = MODE_KEY;      // boot into KEYMODE
  modeVariant = 1;
  k4Refresh();              // proposal cache valid before the first LED pass
  setLedColorHook(modeLedHook);
  applyModeLeds();
  headerSetLeft(modeHeaderLabel());
  refreshMainScreen();      // main screen shows the KEY binding from boot
}

uint8_t keyBehaviorVariant() {
  if (modeBase == MODE_KEY) return modeVariant;
  return familyMem[MODE_KEY];
}

bool keyChordMode() {
  uint8_t kv = keyBehaviorVariant();
  return kv == 2 || kv == 4;
}

bool seq3ChSelActive() { return seq3ChSel; }

void seq3ChSelEnter() { seq3ChSel = true; }

bool seq3ChSelDismiss() {
  if (!seq3ChSel) return false;
  seq3ChSel = false;
  return true;
}

// the channel picker owns the step pads: variant 3 (momentary, all three
// families) and plain ENCMODE (the locked one - no button to hold; the user
// asked for the inverse of the first cut, which put it on ENCMODE4 and hid
// the running sequence there)
bool chSelOverlay() {
  if (modeBase == MODE_ENC && modeVariant == 1) return true;
  return modeVariant == 3 &&
         (modeBase == MODE_KEY || modeBase == MODE_ENC ||
          (modeBase == MODE_SEQ && seq3ChSelActive()));
}

// Does the keyboard owe its pads to the SEQ3 bar / verb row right now?  True
// while SEQ3 is actually held, and true afterwards for the track a bar or a
// verb was pressed on: touching the row COMMITS it, because picking a bar and
// then releasing SEQ only to press the next one is the whole workflow.  The
// keys stay on duty in every mode until KEYMODE3 picks the keyboard's track
// (that gesture means "be an instrument again"), see seq3KbRelease
bool seq3KbRole() {
  if (modeBase == MODE_SEQ && modeVariant == 3) return true;
  return seq3KbCh != 0;
}

void seq3KbLatch(uint8_t ch) {
  if (ch >= 1 && ch <= 16) seq3KbCh = ch;
}

void seq3KbRelease() { seq3KbCh = 0; }

// --- ENCABLETONMODE state --------------------------------------------------
bool abltOn() { return abltLatched; }

void abltSet(bool on) {
  if (!on) abltPrev = false;      // never leave the pad row on the picker
  abltLatched = on;
}

void abltToggle() { abltSet(!abltLatched); }

bool abltPrevPick() { return abltLatched && abltPrev; }

void abltPrevHold(bool on) { abltPrev = abltLatched && on; }

bool abltActionSet() { return abltAct; }

void abltActionToggle() { abltAct = !abltAct; }

// ENCABLETONMODE bridge callbacks (Link.cpp asks for these)
// the host moved Live's track selection: follow it on the encoder channel so
// the device's channel picker and the DAW agree
void linkTrackApply(uint8_t idx) {
  if (idx > 15) return;
  encSetChannel((uint8_t)(idx + 1));
  if (modeBase == MODE_ENC) {
    char hb[24];
    snprintf(hb, sizeof(hb), "Live %.12s", linkTrackName());
    headerShow(hb);
    refreshMainScreen();
  }
}

void linkFillDone(uint8_t ch, uint8_t ok, uint16_t notes) {
  (void)ch;
  if (!ok) {
    headerShow("Clip Fill Failed");
    return;
  }
  char fb[24];
  snprintf(fb, sizeof(fb), "Clip Filled %u", (unsigned)notes);
  headerShow(fb);
}

void modeOnEvent(uint8_t pad, uint8_t ev) {
  if (ev != EV_CLICKED && ev != EV_HOLD && ev != EV_DOUBLE && ev != EV_RELEASED) return;

  int8_t led = ledForPad[pad];
  ModeBase b = MODE_NONE;
  for (uint8_t i = 0; i < 3; i++) {
    if (led == modeLeds[i]) b = modeOfLed[i];
  }
  if (b == MODE_NONE) return;

  // was the keyboard sitting in KEYMODE4 when this click/hold began?  The
  // variant mutations below must not be able to lose that fact
  bool wasK4 = (modeBase == MODE_KEY && modeVariant == 4);
  ModeBase wasBase = modeBase;

  if (ev == EV_RELEASED) {
    // releasing the held mode button ends the hold.  ENCMODE3 always exits
    // INTO ENCMODE and SEQMODE3 always exits INTO SEQMODE: a hold started
    // inside that family restores that variant, a hold started anywhere
    // else lands on that family's variant 1.  KEYMODE3 is the only one
    // that restores the previous base/variant wherever it came from
    if (heldActive && pad == heldPad) {
      heldActive = false;
      if (modeBase == b && modeVariant == 3) {
        if (b == MODE_ENC || b == MODE_SEQ) {
          modeBase = b;
          modeVariant = (heldPrevBase == b) ? heldPrevVariant : 1;
        } else {
          modeBase = heldPrevBase;         // KEY3: back to the previous
          modeVariant = heldPrevVariant;   // family - but on the channel
          if (modeBase == MODE_KEY) {      // the overlay left us on, that
            modeVariant = keyModeMemOf(keyChannel());  // channel's own
            familyMem[MODE_KEY] = modeVariant;         // variant wins
          }
        }
        applyModeLeds();
        // the channel picker is gone the instant the key comes up: drop its
        // message too, or the strip would keep claiming "Channel Select"
        // over a pad row that is already the sequencer / keyboard again
        headerHide();
        headerSetLeft(modeHeaderLabel());
        if (modeBase == MODE_KEY && modeVariant == 4)
          k4Enter();         // back into KEYMODE4: re-seed walk + header
        refreshMainScreen();   // labels/values belong to the restored binding
      }
    }
    return;
  }

  if (ev == EV_HOLD) {
    if (!heldActive) {
      heldPrevBase = modeBase;
      heldPrevVariant = modeVariant;
      heldActive = true;
      heldPad = pad;
      seq3ChSelEnter();    // (re)entry: SEQ3 gets its channel overlay back
    }
    modeVariant = 3;
    // the momentary variant swaps the whole pad row for the channel
    // picker: say so, or the only cue is 16 pads turning yellow
    headerShow("Channel Select");
  } else if (ev == EV_DOUBLE) {
    modeVariant = 4;
    familyMem[b] = 4;
  } else {
    // click: same family — 1->2, 2->1, 4->1; otherwise recall last variant
    if (modeBase == b) {
      if (modeVariant == 1) modeVariant = 2;
      else modeVariant = 1;          // 2 or 4 -> 1
    } else {
      modeVariant = familyMem[b] ? familyMem[b] : 1;
    }
    familyMem[b] = modeVariant;
  }
  modeBase = b;
  // a latched KEY variant belongs to the keyboard channel it was set on
  // (the channel recalls it next time - keyModeRecall).  Click AND
  // double-click: KEYMODE4's entry must land in its channel's memory too;
  // the momentary hold (3) is never stored
  if (b == MODE_KEY && (ev == EV_CLICKED || ev == EV_DOUBLE))
    keyModeMem[keyChannel() - 1] = modeVariant;

  // entering the ENCMODE family from outside takes the page (F bank +
  // group) the CURRENT encoder channel last used - the per-channel
  // memory, not a hard F1 reset (variant hops inside the family keep
  // the armed bank / group as before)
  if (modeBase == MODE_ENC && wasBase != MODE_ENC) {
    encPageRecall();
  }

  // the engine resets when the keyboard leaves the chord variants OR when
  // KEYMODE4 is (re)entered - it starts every visit with a clean gesture
  // (the KEYMODE2 latch/KeyLock/params globals survive, only the sound
  // and the held keys reset).  The PREV overlay is momentary - it never
  // outlives the binding it remaps
  bool nowK4 = (modeBase == MODE_KEY && modeVariant == 4);
  if (!keyChordMode() || nowK4) chordStopAll();
  // fresh KEYMODE4 visit: seed the progression - its first chord is the
  // chord the engine still holds (the KEYMODE2 chord just played), else
  // the stored step before the focus
  if (nowK4 && !wasK4) k4Enter();
  // leaving KEYMODE4 into another chord context: hand the engine's saved
  // KEYMODE2 state (KeyLock, latch, params) back before anyone else uses it
  if (wasK4 && !nowK4) k4Stop();
  clearOverlay(G_ENC);
  abltSet(false);           // ...and neither does the NEXT Ableton overlay
  encMuteHold(false);       // the M2 mute overlay never outlives the ENC mode
  drumMuteHold(false);      // nor the M1 drum-voice mute overlay
  arpPageDismiss();         // nor the M2 arp encoder page
  f3AccentHold(false);      // nor the F3 accent-velocity rebind of ENC6
  strumFRowDismiss();       // nor the M2 strum ADDS row
  genPageDismiss();         // nor the M2 GNR8 encoder page
  k4TonalityDismiss();      // nor the KEYMODE4 tonality menu
  stepChainDisarm();        // ...nor an armed attribute chain span

  applyModeLeds();
  headerSetLeft(modeHeaderLabel());
  refreshMainScreen();   // encoder labels belong to the new binding
}

const char* modeName() {
  static char name[8];
  if (modeBase == MODE_NONE || modeVariant == 0) return "---";
  if (modeVariant == 1) return baseNames[modeBase];
  snprintf(name, sizeof(name), "%s%u", baseNames[modeBase], modeVariant);
  return name;
}

// what the OLED's left header slot prints: the same state in whole words.
// variant 3 is the channel picker in all three families - and on SEQMODE3
// only while its pad overlay is actually up, since an encoder turn hands
// the pads back to the sequencer
const char* modeHeaderLabel() {
  static char name[20];
  if (modeBase == MODE_NONE || modeVariant == 0) return "---";
  if (chSelOverlay()) return "CHANNEL SELECT";
  if (modeVariant == 1) return fullNames[modeBase];
  snprintf(name, sizeof(name), "%s %u", fullNames[modeBase], modeVariant);
  return name;
}
