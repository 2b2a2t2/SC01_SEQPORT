#include "SeqClock.h"
#include "Modes.h"
#include "Buttons.h"
#include "Keyboard.h"
#include "Events.h"     // encCcIncoming - DAW CC feedback syncs ENCMODE knobs
#include "Link.h"       // ENCABLETONMODE bridge: queued SysEx + pattern dump
#include "Encoders.h"   // encDrain - accelerated 0-100% knobs
#include "UsbMidiShim.h"    // usbMIDI (Teensy core on the Teensy, facade on SC01)
#include <uClock.h>
#include <MIDI.h>       // 5-pin DIN input (Serial2, pin 7 RX) - Teensyduino bundle
#ifdef use_SC01Plus
#include "esp32-hal-timer.h"   // hw_timer_t: the uClock timer fix below
#endif

// DIN MIDI in on Serial2 (RX = pin 7, TX = pin 8): notes only, gated per
// track by the SEQMODE4 SRC knob - clock/transport stays USB-only so two
// clock sources can never double-advance uClock
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2, DIN_MIDI);

#define STEPS_COUNT 16

// written from uClock timer ISR (via usbMIDI.read in loop), read from loop()
static volatile uint8_t playStep = 0;    // 0..15
static volatile bool clockRun = false;

// ISR -> loop handoff: sequenced notes are sent from loop, never the ISR
static volatile uint8_t pendingStep = 255;   // 255 = nothing pending
static volatile bool pendingStop = false;
static volatile bool pendingStart = false;   // transport (re)start: reset heads

// SEQMODE3 sync state: INT (default, master) or EXT (follow incoming clock)
static bool syncInternal = true;
static uint16_t tempoBpm = 120;

// fired from uClock's timer ISR on every output PPQN_24 tick (master mode only)
static void onSync24Callback(uint32_t tick) {
  if (syncInternal) usbMIDI.sendClock();   // 0xF8 to the DAW (Ableton slave)
  (void)tick;
}

static void onStepCallback(uint32_t step) {
  playStep = (uint8_t)(step % STEPS_COUNT);
  pendingStep = playStep;
}

static void onClockStartCb() {
  playStep = 0;
  clockRun = true;
  pendingStart = true;            // loop resets every track's position
  pendingStep = 0;                // play step 1 as soon as the loop wakes
}

static void onClockStopCb() {
  clockRun = false;
  pendingStop = true;           // release sequenced notes from loop context
}

static void midiClock() {
  static uint32_t lastUs = 0;
  uint32_t now = micros();
  if (lastUs != 0) {
    uint32_t gap = now - lastUs;
    // >100ms silence between ticks while running = pump was starved
    if (clockRun && gap > 100000UL && Serial.availableForWrite() > 48) {
      Serial.print("CLKGAP ");
      Serial.println(gap);
    }
    // <1ms apart = draining a backlog (tempo-poisoning burst)
    if (gap < 1000UL && Serial.availableForWrite() > 48) {
      Serial.println("CLKBURST");
    }
  }
  lastUs = now;
  uClock.clockMe();
}
// incoming clock transport is honored only as slave (EXT); in INT master
// mode the Teensy's own start/stop stays authoritative
static void midiStart() { if (!syncInternal) uClock.start(); }
static void midiStop()  { if (!syncInternal) uClock.stop(); }
static void midiContinue() { if (!syncInternal) uClock.start(); }

// incoming notes feed the record workflow (loop context via seqPumpMidi);
// the port argument tells the per-track SRC gate which physical input the
// note arrived on (USB vs the Serial2 DIN input)
static void midiNoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
  if (vel) extNoteOn(EXT_PORT_USB, ch, note, vel);
  else extNoteOff(EXT_PORT_USB, ch, note);   // velocity 0 = note off
}
static void midiNoteOff(uint8_t ch, uint8_t note, uint8_t vel) {
  (void)vel;
  extNoteOff(EXT_PORT_USB, ch, note);
}
static void dinNoteOn(uint8_t ch, uint8_t note, uint8_t vel) {
  if (vel) extNoteOn(EXT_PORT_DIN, ch, note, vel);
  else extNoteOff(EXT_PORT_DIN, ch, note);   // velocity 0 = note off
}
static void dinNoteOff(uint8_t ch, uint8_t note, uint8_t vel) {
  (void)vel;
  extNoteOff(EXT_PORT_DIN, ch, note);
}

// incoming CC feedback (loop context via seqPumpMidi): keeps the ENCMODE
// knob positions in step with the DAW so a knob turn never starts from a
// stale value
static void midiControlChange(uint8_t ch, uint8_t cc, uint8_t val) {
  encCcIncoming(ch, cc, val);
}

// green playhead in every mode (SEQMODE owns it, all modes inherit);
// active (recorded) steps = dimmed green, empty steps = dimmed dark blue
static void drawSteps() {
  // channel overlays (KEY3/SEQ3/ENC3, and the locked plain ENCMODE) own the step
  // LEDs (hook repaints them on mode change), the playhead must not paint
  // over them.  SEQ3 gives them back to the sequencer as soon as an encoder
  // moves; plain ENC keeps them until the mode changes
  if (chSelOverlay()) return;
  // DRUM part-select overlay (hold M1) also owns the step LEDs
  if (modeBase == MODE_SEQ && modeVariant != 3 &&
      drumPartSelectActive() && seqTrack() == SEQ_TRACK_DRUM)
    return;
  // ENC mute overlays (hold M2 / M1) also own the step LEDs
  if (encMuteActive() || drumVoiceMuteActive()) return;
  uint8_t base = seqStepViewBase();        // 16-step window of the edit track
  uint8_t ph = seqStepPlayPad();           // its playhead inside that window
  uint8_t gh = seqStepPlayGhost();         // ghost when it runs in another bar
  bool changed = false;
  for (uint8_t i = 0; i < STEPS_COUNT; i++) {
    uint8_t led = groupSteps[i];
    if (leds[led] == CRGB(CRGB::White)) continue;  // pad currently touched
    CRGB c;
    if (!seqStepUsed((uint8_t)(base + i))) {
      c = CRGB::Black;    // past the pattern length: pad off
    } else if (i == ph) {
      c = CRGB::Green;
    } else if (i == gh) {
      c = CRGB::DarkBlue;   // bar on the pads is not the one being read
    } else if (stepIsActive(base + i)) {
      c = CRGB::Green;
      c.nscale8(64);    // dimmed green = active step
    } else {
      c = CRGB::DarkBlue;
      c.nscale8(32);   // barely visible background (matches STEP_DIM in Modes.cpp)
    }
    if (leds[led] != c) { leds[led] = c; changed = true; }
  }
  if (changed) ledsShow();
}

#ifdef use_SC01Plus
// ---- arduino-esp32 3.x: give uClock a timer whose tick is one microsecond --
// uClock 2.3.0's ESP32 backend (platforms/esp32.h) puts the microsecond value in
// both timer slots:
//
//     _uclockTimer = timerBegin(init_clock);          // "us per tick"
//     timerAlarm(_uclockTimer, init_clock, true, 0);  // ...as a tick count
//     timerAlarm(_uclockTimer, us_interval, ...);     // and again on setTempo()
//
// On core 3.x timerBegin() takes a frequency in Hz and timerAlarm() a count of
// those ticks, so the pair collapses to one tick per second at 120 BPM - 192x
// slower than the 5.208 ms the clock must run at. A step is 24 ticks
// (mod_step_ref = output_ppqn / 4), so it arrived 24 s late: the playhead moved
// once, that step's note never got its note-off (the next step sends it) and
// the sequencer looked frozen on step 2 until STOP flushed the notes.
//
// Handing uClock a 1 MHz timer makes every timerAlarm(us) it issues mean
// exactly microseconds, at every tempo, including its own setTempo(). Only the
// handle is replaced - its ISR, clock task and mutex are untouched, and this
// must stay matched to uClock 2.3.0's backend.
extern hw_timer_t *_uclockTimer;   // uClock's platform timer handle
extern void handlerISR(void);      // the ISR uClock attaches to that timer

static void uClockUseMicrosecondTimer() {
  hw_timer_t *t = timerBegin(1000000);   // 1 tick = 1 us
  if (!t) return;                        // no timer left: keep the slow one
  if (_uclockTimer) timerEnd(_uclockTimer);
  _uclockTimer = t;
  timerAttachInterrupt(_uclockTimer, &handlerISR);
  // the setTempo() at the end of initSeqClock() arms it: 5208 ticks of 1 us
}
#endif

void initSeqClock() {
  // init() FIRST.  On the ESP32 backend uClock creates its hardware timer
  // *and* its mutex in initTimer(), and every setter below runs its work
  // inside ATOMIC() = xSemaphoreTake(_mutex, portMAX_DELAY).  Configuring the
  // clock before init() therefore takes a null semaphore, FreeRTOS asserts and
  // the chip panics - which is exactly what the SC01's boot page reported as
  // "panic / clock": display and strip up, then a reset, for ever.  On the
  // Teensy ATOMIC() is a no-op, which is why the original order worked there.
  uClock.init();
#ifdef use_SC01Plus
  uClockUseMicrosecondTimer();   // core 3.x: see the note above
#endif

  uClock.setOutputPPQN(uClock.PPQN_96);
  uClock.setInputPPQN(uClock.PPQN_24);
  uClock.setOnStep(onStepCallback);
  uClock.setOnClockStart(onClockStartCb);
  uClock.setOnClockStop(onClockStopCb);
  // registered after the PPQN setters: setOnSync drops a resolution above
  // output_ppqn, so the output rate has to be in place first
  uClock.setOnSync(uClock.PPQN_24, onSync24Callback);

  // boot default = INT master; ENC4 SYNC (SEQMODE3) can switch to EXT slave
  uClock.setClockMode(syncInternal ? uClock.INTERNAL_CLOCK : uClock.EXTERNAL_CLOCK);
  uClock.setTempo((float)tempoBpm);

  usbMIDI.setHandleClock(midiClock);
  usbMIDI.setHandleStart(midiStart);
  usbMIDI.setHandleStop(midiStop);
  usbMIDI.setHandleContinue(midiContinue);
  usbMIDI.setHandleNoteOn(midiNoteOn);
  usbMIDI.setHandleNoteOff(midiNoteOff);
  usbMIDI.setHandleControlChange(midiControlChange);

  // DIN input (Serial2): notes only - no clock/transport handlers on purpose
  DIN_MIDI.begin(MIDI_CHANNEL_OMNI);
  DIN_MIDI.setHandleNoteOn(dinNoteOn);
  DIN_MIDI.setHandleNoteOff(dinNoteOff);
}

void seqPumpMidi() {
  uint8_t n = 0;
  while (n < 64 && usbMIDI.read()) n++;   // bounded; 64 covers catch-up bursts
  n = 0;
  while (n < 64 && DIN_MIDI.read()) n++;  // same bound for the DIN input
}

void pollSeqClock() {
  seqPumpMidi();
  linkPump();                  // ENCABLETONMODE: one queued SysEx + a chunk
  linkTick();                  // ...and the HELLO retry / liveness timeout

  // sequencer note housekeeping: the ISR only queues flags, notes go out
  // here in loop context (no USB/MIDI calls from interrupt code)
  noInterrupts();
  uint8_t sAdv = pendingStep;  pendingStep = 255;
  bool st = pendingStop;       pendingStop = false;
  bool sn = pendingStart;      pendingStart = false;
  interrupts();
  if (st) seqStepStop();             // transport stopped: let every note go
  else {
    // due deferred (swung) off-steps run first, so a pass queued for an
    // earlier instant keeps its time order against this tick
    seqSwingPoll();
    if (sn) seqStepAdvance(true);       // start: every track back to its head
    else if (sAdv != 255) seqStepAdvance(false);
  }
  keyMonoPoll();               // live MONO-track keyboard voice (TRACK/TYPE)
  keyOctTrackPoll();           // per-track keyboard octave (KEYMODE, all variants)
  seqGatePoll();               // track gate note-offs (MONO step-edit)
  seqStrumPoll();              // staggered strum note-ons (FX / TYPE=STRUM)
  compPoll();                  // COMP stab re-strikes (POLY STEP, HITS 2..8)

  // redraw when the displayed position moved or the mode changed.  The
  // trigger must include the DOT's own source (edit track head): playStep
  // is written by the uClock timer ISR and can bump between the dispatch
  // and this check, burning a trigger and leaving the dot one step behind
  // until it jumped over the missed position
  static ModeBase lastBase = modeBase;
  static uint8_t lastDrawn = 255;
  static uint8_t lastPh = 255;
  static uint8_t lastGh = 255;
  uint8_t s = playStep;
  uint8_t ph = seqStepPlayPad();
  uint8_t gh = seqStepPlayGhost();
  if (s != lastDrawn || ph != lastPh || gh != lastGh || modeBase != lastBase) {
    lastDrawn = s;
    lastPh = ph;
    lastGh = gh;
    lastBase = modeBase;
    drawSteps();
  }
}

uint8_t seqCurrentStep() { return playStep; }
bool seqIsRunning() { return clockRun; }

// INT = master: uClock generates the ticks and onSync24 streams 0xF8 out.
// EXT = slave: follow incoming USB-MIDI clock (original behavior).
// Switching always stops first so state never leaks across modes.
void seqSetSyncInternal(bool on) {
  if (on == syncInternal) return;
  uClock.stop();
  syncInternal = on;
  uClock.setClockMode(on ? uClock.INTERNAL_CLOCK : uClock.EXTERNAL_CLOCK);
  if (on) uClock.setTempo((float)tempoBpm);   // setTempo is ignored in EXTERNAL
}

bool seqIsSyncInternal() { return syncInternal; }

// tempo is stored always; applied live when the internal clock is running
void seqSetTempo(uint16_t bpm) {
  if (bpm < 40) bpm = 40;
  if (bpm > 240) bpm = 240;
  tempoBpm = bpm;
  if (syncInternal) uClock.setTempo((float)bpm);
}

uint16_t seqTempo() { return tempoBpm; }

// SEQMODE3 TRACK knob: per-sequence track type (clamped, no wrap).
// channel 1 = DRUM by default, channels 2..16 = POLY
static uint8_t trackType[16] = {SEQ_TRACK_DRUM, SEQ_TRACK_POLY, SEQ_TRACK_POLY,
                                SEQ_TRACK_POLY, SEQ_TRACK_POLY, SEQ_TRACK_POLY,
                                SEQ_TRACK_POLY, SEQ_TRACK_POLY, SEQ_TRACK_POLY,
                                SEQ_TRACK_POLY, SEQ_TRACK_POLY, SEQ_TRACK_POLY,
                                SEQ_TRACK_POLY, SEQ_TRACK_POLY, SEQ_TRACK_POLY,
                                SEQ_TRACK_POLY};
static int16_t trackRes = 0;           // leftover encoder counts

void seqTrackAdjust(int16_t counts) {
  trackRes += counts;
  int8_t step = 0;
  while (trackRes >= 4)  { trackRes -= 4;  step++; }
  while (trackRes <= -4) { trackRes += 4;  step--; }
  if (!step) return;
  uint8_t ch = seqEditChannel();
  if (ch < 1 || ch > 16) return;
  uint8_t ec = (uint8_t)(ch - 1);
  uint8_t cur = trackType[ec];
  // leaving or entering the HAND strum mode: cut the ringing strings and
  // drop the buffered chord (the strum state must not outlive the mode)
  bool wasHand = cur == SEQ_TRACK_POLY && seqPolyTypeOf(ec) == SEQ_POLY_HAND;
  int16_t t = (int16_t)cur + step;
  if (t < SEQ_TRACK_DRUM) t = SEQ_TRACK_DRUM;
  if (t > SEQ_TRACK_POLY) t = SEQ_TRACK_POLY;
  trackType[ec] = (uint8_t)t;
  bool nowHand = trackType[ec] == SEQ_TRACK_POLY &&
                 seqPolyTypeOf(ec) == SEQ_POLY_HAND;
  if (wasHand != nowHand) handRingCut(ec);
}

uint8_t seqTrack() { return trackType[seqEditChannel() - 1]; }

const char* seqTrackName() {
  static const char* names[3] = { "DRUM", "MONO", "POLY" };
  return names[seqTrack()];
}

uint8_t seqTrackOf(uint8_t idx) {
  return (idx < 16) ? trackType[idx] : SEQ_TRACK_POLY;
}

void seqSetTrackType(uint8_t idx, uint8_t t) {
  if (idx >= 16) return;
  if (t > SEQ_TRACK_POLY) t = SEQ_TRACK_POLY;
  trackType[idx] = t;
}

// SEQMODE3 TYPE (ENC7): per-channel mono transform (default ROOT = 0)
static uint8_t monoType[16] = {0};
static int16_t monoRes = 0;             // leftover encoder counts

void seqMonoTypeAdjust(int16_t counts) {
  monoRes += counts;
  int8_t step = 0;
  while (monoRes >= 4)  { monoRes -= 4;  step++; }
  while (monoRes <= -4) { monoRes += 4;  step--; }
  if (!step) return;
  // a stale POLY SKANK value (5, the shared byte) starts from ROOT
  int16_t t = (int16_t)seqMonoType() + step;
  if (t < SEQ_MONO_ROOT)  t = SEQ_MONO_ROOT;
  if (t > SEQ_MONO_LEAD)  t = SEQ_MONO_LEAD;
  monoType[seqEditChannel() - 1] = (uint8_t)t;
}

uint8_t seqMonoType() {
  uint8_t t = monoType[seqEditChannel() - 1];
  return (t <= SEQ_MONO_LEAD) ? t : SEQ_MONO_ROOT;  // 5 = stale POLY SKANK
}

const char* seqMonoTypeName() {
  static const char* names[5] = { "ROOT", "BASS", "ARP", "STRUM", "LEAD" };
  return names[seqMonoType()];
}

uint8_t seqMonoTypeOf(uint8_t idx) {
  return (idx < 16) ? monoType[idx] : SEQ_MONO_ROOT;
}

void seqSetMonoType(uint8_t idx, uint8_t m) {
  if (idx >= 16) return;
  // the byte is shared with the POLY types: SEQ_POLY_SKANK (5) rides
  // through on load, everything else clamps into the MONO range
  if (m > SEQ_MONO_LEAD && m != SEQ_POLY_SKANK) m = SEQ_MONO_LEAD;
  monoType[idx] = m;
}

// SEQMODE3 TYPE (ENC7) on a POLY track: the track's default step playback
// type, stored in the same byte as the MONO transform (0..3, 5 = SKANK,
// are valid POLY values; a stale 4 from a MONO/LEAD edit reads as STEP and
// the first adjust snaps back into range).  STEP = chords play as blocks
// (as before), STRUM = every step is raked with the fixed tempo spread,
// HAND = the keyboard performs: silent chord build, white keys strum, REC
// captures the gesture for playback, HARM = the track is a harmonizer (M2
// HARM page bakes a progression from the FOLLOW feeder's bassline; plays
// like STEP), SKANK = the track derives every step's voicing live from the
// feeder's chord (or its own stored chord when FOLLOW points at itself) -
// the M2 SKANK page shapes the walk, played steps gate the rhythm.  A step
// can override the playback style per step (F2 TYPE: STEP/STRUM/HAND only
// - HARM and SKANK are track modes, not per-step styles).
static int16_t polyRes = 0;               // leftover encoder counts (own)

void seqPolyTypeAdjust(int16_t counts) {
  polyRes += counts;
  int8_t step = 0;
  while (polyRes >= 4)  { polyRes -= 4;  step++; }
  while (polyRes <= -4) { polyRes += 4;  step--; }
  if (!step) return;
  uint8_t ec = seqEditChannel();
  if (ec < 1 || ec > 16) return;
  ec = (uint8_t)(ec - 1);
  // a HAND track changes its playback mode: cut ringing strings + buffer
  bool wasHand = seqPolyTypeOf(ec) == SEQ_POLY_HAND;
  int16_t t = (int16_t)seqPolyType() + step;
  if (t < SEQ_POLY_STEP) t = SEQ_POLY_STEP;
  // 4 = the stale MONO/LEAD value never lands: step over it (CW from HARM
  // arrives on SKANK, CCW from SKANK falls back to HARM)
  if (t == 4) t = (step > 0) ? SEQ_POLY_SKANK : SEQ_POLY_HARM;
  if (t > SEQ_POLY_SKANK) t = SEQ_POLY_SKANK;
  monoType[ec] = (uint8_t)t;
  if (wasHand != (seqPolyTypeOf(ec) == SEQ_POLY_HAND)) handRingCut(ec);
}

uint8_t seqPolyType() {
  uint8_t t = monoType[seqEditChannel() - 1];
  return (t == 4 || t > SEQ_POLY_SKANK) ? SEQ_POLY_STEP : t;
}

const char* seqPolyTypeName() {
  static const char* names[6] = { "STEP", "STRUM", "HAND", "HARM",
                                  "?", "SKANK" };   // 4 never surfaces
  return names[seqPolyType()];
}

uint8_t seqPolyTypeOf(uint8_t idx) {
  if (idx >= 16) return SEQ_POLY_STEP;
  uint8_t t = monoType[idx];
  return (t == 4 || t > SEQ_POLY_SKANK) ? SEQ_POLY_STEP : t;
}

// --- ARP options (SEQMODE M2 page), per track -----------------------------
// span = octaves extended on top of the chord (Arpie allows 3 more),
// mode = walk order, inst = Arpie's note-insert patterns, skip = rests
// still advance the arp position.  All read by the arp engine in
// Keyboard.cpp (both the playhead track and the live keyboard voice).
static uint8_t arpSpan[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
static uint8_t arpMode[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint8_t arpInst[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint8_t arpSkip[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static int16_t arpSpanRes = 0, arpModeRes = 0, arpInstRes = 0;

static uint8_t arpEdit() { return seqEditChannel() - 1; }

void seqArpSpanAdjust(int16_t counts) {
  arpSpanRes += counts;
  int8_t step = 0;
  while (arpSpanRes >= 4)  { arpSpanRes -= 4;  step++; }
  while (arpSpanRes <= -4) { arpSpanRes += 4;  step--; }
  if (!step) return;
  int16_t v = (int16_t)arpSpan[arpEdit()] + step;
  if (v < 1) v = 1;
  if (v > 4) v = 4;
  arpSpan[arpEdit()] = (uint8_t)v;
}

uint8_t seqArpSpanOf(uint8_t idx) { return (idx < 16) ? arpSpan[idx] : 1; }

void seqArpSpanSet(uint8_t idx, uint8_t v) {
  if (idx >= 16) return;
  if (v < 1) v = 1;
  if (v > 4) v = 4;
  arpSpan[idx] = v;
}

void seqArpModeAdjust(int16_t counts) {
  arpModeRes += counts;
  int8_t step = 0;
  while (arpModeRes >= 4)  { arpModeRes -= 4;  step++; }
  while (arpModeRes <= -4) { arpModeRes += 4;  step--; }
  if (!step) return;
  int16_t v = (int16_t)arpMode[arpEdit()] + step;
  if (v < SEQ_ARP_UP)     v = SEQ_ARP_UP;
  if (v > SEQ_ARP_DIV)    v = SEQ_ARP_DIV;        // no wrap
  arpMode[arpEdit()] = (uint8_t)v;
}

uint8_t seqArpModeOf(uint8_t idx) {
  return (idx < 16) ? arpMode[idx] : SEQ_ARP_UP;
}

void seqArpModeSet(uint8_t idx, uint8_t v) {
  if (idx >= 16) return;
  if (v > SEQ_ARP_DIV) v = SEQ_ARP_UP;   // an older save only holds 0..4
  arpMode[idx] = v;
}

const char *seqArpModeName(uint8_t v) {
  // all nine walk shapes: the four pivot orders used to live only on the F1
  // ORDER knob, which left them unreachable when ORDER = MODE
  static const char *names[9] = { "UP", "DOWN", "UPDOWN", "RANDOM",
                                  "RANDOM2", "OUTIN", "INOUT", "CONV",
                                  "DIV" };
  if (v > SEQ_ARP_DIV) v = SEQ_ARP_UP;
  return names[v];
}

void seqArpInstAdjust(int16_t counts) {
  arpInstRes += counts;
  int8_t step = 0;
  while (arpInstRes >= 4)  { arpInstRes -= 4;  step++; }
  while (arpInstRes <= -4) { arpInstRes += 4;  step--; }
  if (!step) return;
  int16_t v = (int16_t)arpInst[arpEdit()] + step;
  if (v < SEQ_INST_OFF) v = SEQ_INST_OFF;
  if (v > SEQ_INST_4F2B) v = SEQ_INST_4F2B;       // no wrap
  arpInst[arpEdit()] = (uint8_t)v;
}

uint8_t seqArpInstOf(uint8_t idx) {
  return (idx < 16) ? arpInst[idx] : SEQ_INST_OFF;
}

void seqArpInstSet(uint8_t idx, uint8_t v) {
  if (idx >= 16) return;
  if (v > SEQ_INST_4F2B) v = SEQ_INST_OFF;
  arpInst[idx] = v;
}

const char *seqArpInstName(uint8_t v) {
  static const char *names[5] = { "OFF", "HI", "LOW", "3F1B", "4F2B" };
  if (v > SEQ_INST_4F2B) v = SEQ_INST_OFF;
  return names[v];
}

void seqArpSkipToggle(int16_t counts) {
  arpSkip[arpEdit()] = (counts > 0) ? 1 : 0;   // CW = ON, CCW = OFF
}

bool seqArpSkipOf(uint8_t idx) { return idx < 16 && arpSkip[idx]; }

void seqArpSkipSet(uint8_t idx, bool on) {
  if (idx < 16) arpSkip[idx] = on ? 1 : 0;
}

// --- ARP F sub-pages: one parameter block per track -----------------------
// F1..F4 all write these slots, so a value set on the MOTION page is the
// same value the GENERATIVE page reads.  Ranges are per slot and every
// slot stops at both ends: an enum that wrapped would drop a shape you just
// dialled in (ORDER 8 -> 0) on one detent, so no sub-page knob wraps.
static const uint8_t arpPMaxT[ARP_P_COUNT] = {
  8, 5, 4, 100, 100, 6, 4, 100, 2, 100, 100, 100, 63, 6, 2, 3,
  100, 100, 100, 100
};
static const uint8_t arpPDefT[ARP_P_COUNT] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 50, 30, 50, 7, 0, 0, 1,
  40, 50, 0, 0
};
static uint8_t arpP[16][ARP_P_COUNT];
static bool arpPReady = false;

static void arpPEnsure() {
  if (arpPReady) return;
  arpPReady = true;
  for (uint8_t i = 0; i < 16; i++)
    for (uint8_t p = 0; p < ARP_P_COUNT; p++) arpP[i][p] = arpPDefT[p];
}

uint8_t seqArpPMax(uint8_t p) { return (p < ARP_P_COUNT) ? arpPMaxT[p] : 0; }

uint8_t seqArpPOf(uint8_t idx, uint8_t p) {
  arpPEnsure();
  if (idx >= 16 || p >= ARP_P_COUNT) return 0;
  uint8_t v = arpP[idx][p];
  return (v > arpPMaxT[p]) ? arpPMaxT[p] : v;
}

void seqArpPSet(uint8_t idx, uint8_t p, uint8_t v) {
  arpPEnsure();
  if (idx >= 16 || p >= ARP_P_COUNT) return;
  arpP[idx][p] = (v > arpPMaxT[p]) ? arpPMaxT[p] : v;
}

void seqArpPAdjust(uint8_t p, int16_t counts) {
  arpPEnsure();
  if (p >= ARP_P_COUNT) return;
  static int16_t res[ARP_P_COUNT];        // leftover encoder counts
  int16_t step;
  if (arpPMaxT[p] > 8) {                  // a percentage: accelerated
    step = encDrain(counts, &res[p]);
  } else {                                // small enum: one per detent
    res[p] += counts;
    step = 0;
    while (res[p] >= 4)  { res[p] -= 4;  step++; }
    while (res[p] <= -4) { res[p] += 4;  step--; }
  }
  if (!step) return;
  int16_t v = (int16_t)arpP[arpEdit()][p] + step;
  uint8_t mx = arpPMaxT[p];
  if (v < 0) v = 0;                       // clamped at both ends, no wrap
  if (v > mx) v = mx;
  arpP[arpEdit()][p] = (uint8_t)v;
}

const char *seqArpPName(uint8_t p) {
  static const char *const n[ARP_P_COUNT] = {
    "ORDER", "STEP", "PIVOT", "WEIGHT", "MUTATE", "INSERT", "CYCLE",
    "MORPH", "ENGINE", "RATE", "DEPTH", "MEMORY", "SEED", "BIAS",
    "TARGET", "CLOCK", "JUMP", "SMOOTH", "STAY", "REST"
  };
  if (p >= ARP_P_COUNT) return "---";
  arpPEnsure();
  // the four OPT knobs belong to the armed engine: MARKOV steers with
  // jump/smooth/stay/rest, WALK with step/reflect/drift/rest
  if (p >= ARP_P_OPT1) {
    static const char *const w[4] = { "STEP", "REFLECT", "DRIFT", "REST" };
    uint8_t eng = seqArpPOf(arpEdit(), ARP_P_ENGINE);
    if (eng == SEQ_ARP_ENG_WALK) return w[p - ARP_P_OPT1];
  }
  return n[p];
}

const char *seqArpPValue(uint8_t p, uint8_t v) {
  static char buf[8];
  switch (p) {
    case ARP_P_ORDER: {
      static const char *const o[9] = { "MODE", "UP", "DOWN", "UPDN",
                                        "OUTIN", "INOUT", "CONV", "DIV",
                                        "PLAY" };
      return o[v > 8 ? 0 : v];
    }
    case ARP_P_STEP: {
      static const char *const s[6] = { "1/1", "2/1", "3/1", "2/2",
                                        "3/2", "4/2" };
      return s[v > 5 ? 0 : v];
    }
    case ARP_P_PIVOT: {
      static const char *const q[5] = { "ROOT", "5TH", "TOP", "LOW", "CTR" };
      return q[v > 4 ? 0 : v];
    }
    case ARP_P_INSERT: {
      static const char *const s[7] = { "FOLLOW", "OFF", "HI", "LOW",
                                        "3F1B", "4F2B", "RAND" };
      return s[v > 6 ? 0 : v];
    }
    case ARP_P_CYCLE: {
      static const char *const c[5] = { "1bar", "2bar", "4bar", "8bar",
                                        "16bar" };
      return c[v > 4 ? 0 : v];
    }
    case ARP_P_ENGINE: {
      static const char *const e[3] = { "OFF", "MARKOV", "WALK" };
      return e[v > 2 ? 0 : v];
    }
    case ARP_P_BIAS: {
      static const char *const b[7] = { "NONE", "HIGH", "LOW", "ROOT",
                                        "PIVOT", "SPARSE", "DENSE" };
      return b[v > 6 ? 0 : v];
    }
    case ARP_P_TARGET: {
      static const char *const t[3] = { "ORDER", "PITCH", "BOTH" };
      return t[v > 2 ? 0 : v];
    }
    case ARP_P_CLOCK: {
      static const char *const k[4] = { "STEP", "PASS", "BAR", "PHRASE" };
      return k[v > 3 ? 0 : v];
    }
    default: break;
  }
  snprintf(buf, sizeof(buf), "%u", v);
  return buf;
}

// SEQMODE3 pattern setup, per track: length (ENC2, 0..64 steps, boot 16),
// the 16-step edit window picked by the C D E F bars, and which bar loops
// playback on itself.  Independent per track - every playhead wraps at its
// own length (polymeter); a looped bar plays regardless of the length
static uint8_t patLen[16] = {16, 16, 16, 16, 16, 16, 16, 16,
                             16, 16, 16, 16, 16, 16, 16, 16};
static int16_t patRes = 0;               // leftover encoder counts (ENC2)
static uint8_t seqPage[16] = {0};        // edit window 0..3 (zero = steps 1-16)
static uint8_t loopPage[16] = {0};       // bar looped by its own press
static bool    loopOn[16] = {false};

void seqPatLenAdjust(int16_t counts) {
  patRes += counts;
  int8_t step = 0;
  while (patRes >= 4)  { patRes -= 4;  step++; }
  while (patRes <= -4) { patRes += 4;  step--; }
  if (!step) return;
  uint8_t ch = seqEditChannel() - 1;
  int v = (int)patLen[ch] + step;
  if (v < 0) v = 0;
  if (v > 64) v = 64;
  if ((uint8_t)v == patLen[ch]) return;
  patLen[ch] = (uint8_t)v;
  harmFeederChanged(ch);   // the feeder window moved: HARM tracks that feed
                           // from this channel re-bake (deferred, loop pass)
}

// C# / D# presses: one shared write, so both keys get the same 0..64 clamp,
// the same window fix-up and the same HARM mark as the ENC2 dial
static void patLenApply(uint8_t ch, uint8_t v) {
  if (ch >= 16) return;
  if (v > 64) v = 64;
  if (v == patLen[ch]) return;
  patLen[ch] = v;
  // the edit window follows the length down: a page past the new end would
  // show 16 pads with nothing in them
  if (v) {
    uint8_t last = (uint8_t)((v - 1) / STEPS_COUNT);
    if (last > 3) last = 3;
    if (seqPage[ch] > last) seqPage[ch] = last;
  }
  harmFeederChanged(ch);   // same feeder-window mark as seqPatLenAdjust
}

// public form for the clipboard (paste / undo): the same 0..64 clamp, the
// same window fix-up and the same HARM mark the C# / D# keys get
void seqPatLenApplyOf(uint8_t ch, uint8_t v) { patLenApply(ch, v); }

uint8_t seqPatLenHalf() {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  if (ch >= 16) return 16;
  if (patLen[ch] == 0) return 0;          // silent track: nothing to halve
  uint8_t v = (uint8_t)(patLen[ch] / 2);
  if (v < 1) v = 1;                        // never silences by accident
  patLenApply(ch, v);
  return patLen[ch];
}

uint8_t seqPatLenDouble() {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  if (ch >= 16) return 16;
  uint8_t before = patLen[ch];
  uint8_t v = before ? (uint8_t)(before * 2) : STEPS_COUNT;
  patLenApply(ch, v);                     // 0 = silent track -> one bar
  // the steps the doubling opened up are filled from the old pattern: 4
  // chords on steps 1-4 of a 4-step pattern repeat on 5-8, a full bar
  // becomes a two-bar loop.  A 0-length track has nothing to copy and a
  // length already at the 64 ceiling does not grow, so neither copies
  if (patLen[ch] > before) seqPatRepeatInto(ch, before);
  return patLen[ch];
}

int seqPatLen() { return patLen[seqEditChannel() - 1]; }

uint8_t seqPatLenOf(uint8_t ch) { return (ch < 16) ? patLen[ch] : 16; }

// restore path (seqCfgLoad): same 0..64 clamp as the encoder
void seqPatLenSetOf(uint8_t ch, uint8_t len) {
  if (ch >= 16) return;
  if (len > 64) len = 64;
  patLen[ch] = len;
}

// SEQMODE3 ENC4: per-track pattern speed, clamped 0..8 (no wrap).  The
// ladder doubles each way around the default 6 = normal (one step per
// 16th note): 1/64, 1/32, 1/16, 1/8, 1/4, 1/2, 1, 2, 4 - num/den describe
// how many sub-steps a track takes inside one master tick.  The two
// slowest rungs (1/64, 1/32) were added under the original 7-rung ladder,
// which shifted every older rung up by two - the EEPROM loader translates
// saved values from before the extension (see seqCfgLoad)
static const uint8_t spdNum[9] = { 1, 1, 1, 1, 1, 1, 1, 2, 4 };
static const uint8_t spdDen[9] = { 64, 32, 16, 8, 4, 2, 1, 1, 1 };
static uint8_t patSpeed[16] = { 6, 6, 6, 6, 6, 6, 6, 6,
                                6, 6, 6, 6, 6, 6, 6, 6 };
static int16_t spdRes = 0;               // leftover encoder counts (ENC4)

void seqSpeedAdjust(int16_t counts) {
  spdRes += counts;
  int8_t step = 0;
  while (spdRes >= 4)  { spdRes -= 4;  step++; }
  while (spdRes <= -4) { spdRes += 4;  step--; }
  if (!step) return;
  int v = (int)patSpeed[seqEditChannel() - 1] + step;
  if (v < 0) v = 0;
  if (v > 8) v = 8;
  patSpeed[seqEditChannel() - 1] = (uint8_t)v;
}

uint8_t seqSpeedIdxOf(uint8_t ch) { return (ch < 16) ? patSpeed[ch] : 6; }

void seqSpeedSetOf(uint8_t ch, uint8_t idx) {
  if (ch >= 16) return;
  patSpeed[ch] = (idx > 8) ? 6 : idx;
}

const char *seqSpeedName(uint8_t idx) {
  static const char *const nm[9] = { "1/64", "1/32", "1/16", "1/8",
                                     "1/4", "1/2", "1", "2", "4" };
  return nm[(idx > 8) ? 6 : idx];
}

uint8_t seqSpeedNumOf(uint8_t ch) { return spdNum[seqSpeedIdxOf(ch)]; }
uint8_t seqSpeedDenOf(uint8_t ch) { return spdDen[seqSpeedIdxOf(ch)]; }

// --- per-track swing (SEQMODE 4 ENC1/ENC2) ---------------------------------
// Style = a machine emulation: which grid it swings (pair = steps per swing
// pair: 2 = 16th swing, every odd step late; 4 = 8th shuffle, only the "&"
// late) and how deep the AMT knob can shove it (maxR = the ratio at amt
// 100).  The effective ratio grows linearly from 50% (straight, amt 0) to
// maxR (amt 100); seqSwingDepthOf turns it into the scheduler's delay in
// permille of one pair period.  Boot defaults: style OFF, amt 50 (mid
// character the moment a style is picked) - persisted per track in the SEQ
// EEPROM block
static const char *const swingNameT[SEQ_SWING_COUNT] = {
  "OFF", "MPC60", "MPC3K", "SP1200", "LINN", "TR808", "TR909",
  "SHUFFLE", "TRIPLET"
};
static const uint8_t swingPairT[SEQ_SWING_COUNT] = { 0, 2, 2, 2, 2, 2, 2, 4, 4 };
static const uint8_t swingMaxRT[SEQ_SWING_COUNT]  = { 50, 75, 70, 72, 62,
                                                      66, 64, 75, 67 };
static uint8_t swingStyle[16] = {0};
static uint8_t swingAmt[16] = { 50, 50, 50, 50, 50, 50, 50, 50,
                                50, 50, 50, 50, 50, 50, 50, 50 };
static int16_t swingRes = 0;               // leftover encoder counts (style)
static int16_t swingAmtRes = 0;            // leftover encoder counts (amount)

void seqSwingStyleAdjust(int16_t counts) {
  swingRes += counts;
  int8_t step = 0;
  while (swingRes >= 4)  { swingRes -= 4;  step++; }
  while (swingRes <= -4) { swingRes += 4;  step--; }
  if (!step) return;
  int16_t v = (int16_t)swingStyle[seqEditChannel() - 1] + step;
  if (v < SEQ_SWING_OFF) v = SEQ_SWING_OFF;
  if (v > SEQ_SWING_TRIPLET) v = SEQ_SWING_TRIPLET;   // no wrap
  swingStyle[seqEditChannel() - 1] = (uint8_t)v;
}

uint8_t seqSwingStyle() { return seqSwingStyleOf(seqEditChannel() - 1); }

uint8_t seqSwingStyleOf(uint8_t ch) {
  return (ch < 16 && swingStyle[ch] <= SEQ_SWING_TRIPLET) ? swingStyle[ch]
                                                          : SEQ_SWING_OFF;
}

void seqSwingStyleSet(uint8_t ch, uint8_t v) {
  if (ch < 16)
    swingStyle[ch] = (v <= SEQ_SWING_TRIPLET) ? v : SEQ_SWING_OFF;
}

const char *seqSwingStyleName(uint8_t v) {
  return swingNameT[(v <= SEQ_SWING_TRIPLET) ? v : 0];
}

void seqSwingAmtAdjust(int16_t counts) {
  int16_t step = encDrain(counts, &swingAmtRes);  // 0-100% knob: 1 count = 1
  if (!step) return;
  int16_t v = (int16_t)swingAmt[seqEditChannel() - 1] + step;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  swingAmt[seqEditChannel() - 1] = (uint8_t)v;
}

uint8_t seqSwingAmt() { return seqSwingAmtOf(seqEditChannel() - 1); }

uint8_t seqSwingAmtOf(uint8_t ch) { return (ch < 16) ? swingAmt[ch] : 50; }

void seqSwingAmtSet(uint8_t ch, uint8_t v) {
  if (ch < 16) swingAmt[ch] = (v > 100) ? 50 : v;
}

// effective swing ratio in percent (50 = straight): amt scales 50 .. maxR
uint8_t seqSwingRatioOf(uint8_t ch) {
  uint8_t st = seqSwingStyleOf(ch);
  if (!st) return 50;
  return (uint8_t)(50 + ((uint16_t)seqSwingAmtOf(ch) * (swingMaxRT[st] - 50)) / 100);
}

uint8_t seqSwingPairOf(uint8_t ch) {
  return swingPairT[seqSwingStyleOf(ch)];   // 0 = no swing
}

// delay of the off-step, in permille of one pair period: the ratio sits
// r = depth/1000 into the pair, so the off-step is late by depth/1000 of
// the whole pair (max 250 = ratio 75%, i.e. one pair-step of delay)
uint16_t seqSwingDepthOf(uint8_t ch) {
  return (uint16_t)(seqSwingRatioOf(ch) - 50) * 10;
}


void seqPlayRange(uint8_t ch, uint8_t *lo, uint8_t *hi) {
  if (ch >= 16) { *lo = 0; *hi = STEPS_COUNT; return; }
  if (loopOn[ch]) {                        // a looped bar plays by itself
    *lo = (uint8_t)(loopPage[ch] * STEPS_COUNT);
    *hi = (uint8_t)(*lo + STEPS_COUNT);
  } else {
    *lo = 0;
    *hi = patLen[ch];                      // 0 = silent track
  }
}

bool seqLoopOn(uint8_t ch) { return ch < 16 && loopOn[ch]; }

// bar press on the edit channel: another bar selects its window, the
// SELECTED bar toggles its loop (single loop slot per track)
uint8_t seqBarPress(uint8_t bar) {
  if (bar > 3) return 0;
  uint8_t ch = seqEditChannel() - 1;
  if (seqPage[ch] != bar) { seqPage[ch] = bar; return 0; }
  if (loopOn[ch] && loopPage[ch] == bar) { loopOn[ch] = false; return 2; }
  loopOn[ch] = true;
  loopPage[ch] = bar;
  return 1;
}

uint8_t seqStepViewBase() {
  return (uint8_t)(seqPage[seqEditChannel() - 1] * STEPS_COUNT);
}

void seqStepViewFollow(uint8_t step) {
  uint8_t pg = (uint8_t)(step / STEPS_COUNT);
  if (pg > 3) return;
  seqPage[seqEditChannel() - 1] = pg;
}

uint8_t seqStepPage() { return seqPage[seqEditChannel() - 1]; }

// SEQMODE3 G#: bar follow - that track's edit window chases its own
// playhead instead of staying on the bar you picked, so the pads show the
// bar that is sounding ("the bar moves with the green dot").  RAM only
// like every other view detail; the playhead advance calls the sync
static bool viewChase[16] = {false};

bool seqStepViewChaseGet() { return viewChase[seqEditChannel() - 1]; }

bool seqStepViewChaseToggle() {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  if (ch >= 16) return false;
  viewChase[ch] = !viewChase[ch];
  return viewChase[ch];
}

void seqStepViewChaseSync(uint8_t ch, uint8_t step) {
  if (ch >= 16 || !viewChase[ch]) return;
  uint8_t pg = (uint8_t)(step / STEPS_COUNT);
  if (pg > 3) return;
  seqPage[ch] = pg;
}

bool seqBarLooped(uint8_t page) {
  uint8_t ch = seqEditChannel() - 1;
  return loopOn[ch] && loopPage[ch] == page;
}

// SEQMODE3 FOLLOW (ENC3): ON = the track keeps its own pattern length and
// its active steps as the rhythm mask, but its NOTES are read live from
// the feeding track (input channel's track) at the feeder's playhead - a
// 16-step track then mirrors a 64-step feeder bar by bar; an empty feeder
// step holds the feeder's last chord/note.  RAM only, boot OFF; meaningful
// for MONO/POLY listeners pointing at another track.  Three-state: 0 = OFF,
// 1 = ON (live follow), 2 = FREEZE (keep the LAST incoming chord - the
// follower keeps playing, but the chord it resolves is latched on the first
// frozen step and held until FREEZE is left).  seqFollow()/seqFollowOf()
// treat FREEZE as "following" (mode != 0) so every gate that checks them
// still runs; only the chord resolution changes.
static uint8_t followOn[16] = {0};

bool seqFollow() { return followOn[seqEditChannel() - 1] != 0; }
bool seqFollowOf(uint8_t ch) { return ch < 16 && followOn[ch] != 0; }
uint8_t seqFollowMode() { return followOn[seqEditChannel() - 1]; }
uint8_t seqFollowModeOf(uint8_t ch) {
  return ch < 16 ? followOn[ch] : 0;
}
bool seqFollowFreezeOf(uint8_t ch) { return ch < 16 && followOn[ch] == 2; }
void seqFollowModeSet(uint8_t m) {
  if (m > 2) m = 2;
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  uint8_t prev = followOn[ch];
  followOn[ch] = m;
  // entering FREEZE latches the chord the feeder is sounding at THIS instant
  // (not the one the follower's next active step would happen to catch) and
  // leaving it drops the latch, so every entry recaptures fresh
  if (m == 2 && prev != 2) seqFollowFreezeCapture(ch);
  else if (m != 2)        seqFollowFreezeClear(ch);
}

// restore path (seqCfgLoad); a corrupt byte > 2 reads back as OFF.  Nothing
// is latched at boot: a FREEZE track captures on its first frozen step
void seqFollowModeSetOf(uint8_t ch, uint8_t m) {
  if (ch < 16) {
    followOn[ch] = (m > 2) ? 0 : m;
    seqFollowFreezeClear(ch);
  }
}

// GNR8 BASS mapping mode (see SeqClock.h): global, boot NEAR; the
// reserved config byte b[2] holds it (legacy blocks read 0 = NEAR there,
// old clipboard junk >3 is rejected by seqMapSet)
static uint8_t mapMode = SEQ_MAP_NEAR;
static int16_t mapRes = 0;               // encoder residual (4 counts = 1)

uint8_t seqMap() { return mapMode; }
void seqMapSet(uint8_t m) {
  mapMode = (m <= SEQ_MAP_GD) ? m : SEQ_MAP_NEAR;
}
void seqMapAdjust(int16_t counts) {
  mapRes += counts;
  int16_t step = 0;
  while (mapRes >= 4) { mapRes -= 4; step++; }
  while (mapRes <= -4) { mapRes += 4; step--; }
  if (!step) return;
  int16_t v = (int16_t)mapMode + step;
  if (v < SEQ_MAP_NEAR) v = SEQ_MAP_NEAR;
  if (v > SEQ_MAP_GD) v = SEQ_MAP_GD;
  mapMode = (uint8_t)v;
}
const char *seqMapName(uint8_t m) {
  switch (m) {
    case SEQ_MAP_ROOT: return "root";
    case SEQ_MAP_DEG:  return "deg";
    case SEQ_MAP_IVL:  return "ivl";
    case SEQ_MAP_CT:   return "ct";
    case SEQ_MAP_GD:   return "gd";
    default:           return "near";
  }
}

// System Real-Time Start (0xFA): with SYNC=INT also launch the internal
// transport so Clock ticks flow to the DAW; with SYNC=EXT just emit Start.
void seqSendStart() {
  if (syncInternal) uClock.start();
  usbMIDI.sendStart();
}

// PLAY toggle: while the internal (SYNC=INT) transport is running, PLAY
// stops it and emits Real-Time Stop (0xFC); otherwise behave like Start.
// (usbMIDI.sendStop() is unusable: the Teensy core sends 0xFB = Continue.)
void seqTogglePlay() {
  if (syncInternal && clockRun) {
    uClock.stop();                      // onClockStopCb releases sequenced notes
    usbMIDI.sendRealTime(0xFC);
  } else {
    seqSendStart();
  }
}

// section = 0 pump, 1 touch/inputs, 2 events, 3 display; report worst >3ms once/s
void seqDiagNote(uint8_t section, uint32_t us) {
  static uint32_t worst[4] = {0, 0, 0, 0};
  static uint32_t lastReport = 0;
  if (section > 3) return;
  if (us > worst[section]) worst[section] = us;
  uint32_t now = millis();
  if (now - lastReport < 1000) return;
  lastReport = now;
  if ((worst[0] | worst[1] | worst[2] | worst[3]) > 3000 &&
      Serial.availableForWrite() > 64) {
    Serial.print("DIAG pump=");
    Serial.print(worst[0]);
    Serial.print(" in=");
    Serial.print(worst[1]);
    Serial.print(" ev=");
    Serial.print(worst[2]);
    Serial.print(" dsp=");
    Serial.println(worst[3]);
  }
  worst[0] = worst[1] = worst[2] = worst[3] = 0;
}
