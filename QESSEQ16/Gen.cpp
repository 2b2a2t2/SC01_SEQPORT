#include "Gen.h"
#include "Keyboard.h"      // stepGenWrite / stepGenClear / stepSnapshot*
#include "SeqClock.h"      // seqPatLenOf, track types
#include "Chords.h"        // keyIsInScale (notes stay in the KEY + SCALE)
#include "Display.h"       // headerShow feedback
#include "Encoders.h"      // encDrain - accelerated 0-100% dials
#include "bjorklund.h"     // Euclidean placement (Aciduino V2's dependency)

// per-sequence parameter slots (index = sequence 0..15); boot defaults
static uint8_t genFill[16]   = { 50, 50, 50, 50, 50, 50, 50, 50,
                                 50, 50, 50, 50, 50, 50, 50, 50 };
static uint8_t genAcc[16]    = { 30, 30, 30, 30, 30, 30, 30, 30,
                                 30, 30, 30, 30, 30, 30, 30, 30 };
static uint8_t genGlide[16]  = { 20, 20, 20, 20, 20, 20, 20, 20,
                                 20, 20, 20, 20, 20, 20, 20, 20 };
static uint8_t genTie[16]    = { 10, 10, 10, 10, 10, 10, 10, 10,
                                 10, 10, 10, 10, 10, 10, 10, 10 };
static uint8_t genTones[16];     // SEQ_GEN_TONES_ALL (0 = valid)
static uint8_t genAlgo[16];      // SEQ_GEN_CLASSIC (0 = valid)
// explicit boot defaults: zero-init would mean LOW C0 and - fatal - RANGE
// 0 octaves, which degenerated generate() to a single repeated pitch on a
// blank/legacy EEPROM (RangeOf validates on top of this)
static uint8_t genLow[16]   = { 1, 1, 1, 1, 1, 1, 1, 1,
                                1, 1, 1, 1, 1, 1, 1, 1 };   // SEQ_GEN_LOW_C1
static uint8_t genRange[16] = { 2, 2, 2, 2, 2, 2, 2, 2,
                                2, 2, 2, 2, 2, 2, 2, 2 };   // 2 octaves
// LEAD melodic contour (M2 LEAD page).  Boot ARCH everywhere.  Shares the
// FILL/LOW/RANGE slots with GNR8 (a track is one TYPE or the other), so
// only the contour is new storage.
static uint8_t leadContour[16] = { 0 };          // SEQ_LEAD_ARCH
static int16_t genLeadRes;                       // contour residual (4 = 1)

// LEAD page CT/GD knobs: the chord-tone blend (per-note chance to keep
// the skeleton's scale-degree identity instead of snapping to the feeder
// chord) and the guide-tone set depth (which extensions gd may use).  The
// knobs live on the LEAD page ENC5/6 and only bite when the global MAP
// mode is ct / gd respectively
static uint8_t leadCtBlend[16] = { 0 };          // 0..100%, 0 = pure ct
static uint8_t leadGdDepth[16] = { 0 };          // 0..3 = 3/7, +9, +13, EXT
static int16_t genLeadCtRes, genLeadGdRes;       // residuals (4 = 1)

// live contour flip cache (RAM, session only): per track and per contour
// the last pattern that contour produced, so turning CONTOUR on the M2
// LEAD page swaps the stored sequence between the ARCH/DESC/WALK versions
// in place - the version being left is kept, the target's last version is
// restored (or generated fresh the first time) and every contour stays
// revisitable.  0xFF marks a rest.
#define LEAD_CACHE_REST 0xFF
static uint8_t leadCache[16][3][SEQ_GEN_STEPS];
static uint8_t leadCacheLen[16][3];      // length at cache time (0 = none)
static unsigned long leadContourLast;    // burst gap for the undo snapshot

// encoder residuals: 4 counts = 1 unit, kept between channels like the
// other SEQ knobs (a stray count lands on the next turn of the same knob)
static int16_t genFillRes, genAccRes, genGlideRes, genTieRes;
static int16_t genOptRes[4];                // option dials (F radio pages)
static int16_t genShiftRes;                 // rotate residual (4 counts = 1)
static uint8_t genShiftPos = 0;             // cumulative rotation in-pattern
static unsigned long genShiftLast = 0;      // burst gap for the undo snapshot

static uint8_t genIdx() {
  uint8_t ch = seqEditChannel() - 1;
  return ch < 16 ? ch : 0;
}

// 4 counts = 1 step at a slow dial and more per detent when spun (encDrain),
// clamped (shared shape of every GEN knob)
static void genAdjust(uint8_t *slot, int16_t *res, int16_t counts,
                      uint8_t lo, uint8_t hi) {
  int16_t step = encDrain(counts, res);
  if (!step) return;
  int16_t v = (int16_t)*slot + step;
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  *slot = (uint8_t)v;
}

void seqGenFillAdjust(int16_t c)   { genAdjust(&genFill[genIdx()], &genFillRes, c, 0, 100); }
void seqGenAccAdjust(int16_t c)    { genAdjust(&genAcc[genIdx()], &genAccRes, c, 0, 100); }
void seqGenGlideAdjust(int16_t c)  { genAdjust(&genGlide[genIdx()], &genGlideRes, c, 0, 100); }
void seqGenTieAdjust(int16_t c)    { genAdjust(&genTie[genIdx()], &genTieRes, c, 0, 100); }

static uint8_t genClamp100(uint8_t v, uint8_t dflt) {
  return v <= 100 ? v : dflt;           // blank EEPROM (0xFF) -> default
}
void seqGenFillSet(uint8_t i, uint8_t v)   { if (i < 16) genFill[i]   = genClamp100(v, 50); }
void seqGenAccSet(uint8_t i, uint8_t v)    { if (i < 16) genAcc[i]    = genClamp100(v, 30); }
void seqGenGlideSet(uint8_t i, uint8_t v)  { if (i < 16) genGlide[i]  = genClamp100(v, 20); }
void seqGenTieSet(uint8_t i, uint8_t v)    { if (i < 16) genTie[i]    = genClamp100(v, 10); }

uint8_t seqGenFillOf(uint8_t i)   { return i < 16 ? genFill[i] : 50; }
uint8_t seqGenAccOf(uint8_t i)    { return i < 16 ? genAcc[i] : 30; }
uint8_t seqGenGlideOf(uint8_t i)  { return i < 16 ? genGlide[i] : 20; }
uint8_t seqGenTieOf(uint8_t i)    { return i < 16 ? genTie[i] : 10; }

void seqGenTonesSet(uint8_t i, uint8_t v) {
  if (i >= 16) return;
  genTones[i] = v <= SEQ_GEN_TONES_3 ? v : SEQ_GEN_TONES_ALL;
}
void seqGenLowSet(uint8_t i, uint8_t v) {
  if (i >= 16) return;
  genLow[i] = v <= SEQ_GEN_LOW_C2 ? v : SEQ_GEN_LOW_C1;
}
void seqGenRangeSet(uint8_t i, uint8_t v) {
  if (i >= 16) return;
  genRange[i] = (v >= 1 && v <= 5) ? v : 2;
}
void seqGenAlgoSet(uint8_t i, uint8_t v) {
  if (i >= 16) return;
  genAlgo[i] = v <= SEQ_GEN_EUCLID ? v : SEQ_GEN_CLASSIC;
}

uint8_t seqGenTonesOf(uint8_t i)  { return i < 16 ? genTones[i] : SEQ_GEN_TONES_ALL; }
uint8_t seqGenLowOf(uint8_t i)    { return i < 16 ? genLow[i]   : SEQ_GEN_LOW_C1; }
uint8_t seqGenRangeOf(uint8_t i)  {   // 0 (unset) must never reach produce()
  return (i < 16 && genRange[i] >= 1 && genRange[i] <= 5) ? genRange[i] : 2; }
uint8_t seqGenAlgoOf(uint8_t i)   { return i < 16 ? genAlgo[i]  : SEQ_GEN_CLASSIC; }

// F-radio option dials: 1 detent = 4 counts = 1 value, clamped (no wrap)
void seqGenTonesAdjust(int16_t c) {
  genAdjust(&genTones[genIdx()], &genOptRes[0], c,
            SEQ_GEN_TONES_ALL, SEQ_GEN_TONES_3);
}
void seqGenLowAdjust(int16_t c) {
  genAdjust(&genLow[genIdx()], &genOptRes[1], c,
            SEQ_GEN_LOW_C0, SEQ_GEN_LOW_C2);
}
void seqGenRangeAdjust(int16_t c) {
  genAdjust(&genRange[genIdx()], &genOptRes[2], c, 1, 5);
}
void seqGenAlgoAdjust(int16_t c) {
  genAdjust(&genAlgo[genIdx()], &genOptRes[3], c,
            SEQ_GEN_CLASSIC, SEQ_GEN_EUCLID);
}

const char* seqGenTonesName(uint8_t v) {
  static const char* n[4] = { "all", "5", "4", "3" };
  return n[v <= SEQ_GEN_TONES_3 ? v : SEQ_GEN_TONES_ALL];
}
const char* seqGenLowName(uint8_t v) {
  static const char* n[3] = { "C0", "C1", "C2" };   // MIDI 24 / 36 / 48
  return n[v <= SEQ_GEN_LOW_C2 ? v : SEQ_GEN_LOW_C1];
}
const char* seqGenAlgoName(uint8_t v) {
  static const char* n[2] = { "CLASSIC", "EUCLID" };
  return n[v <= SEQ_GEN_EUCLID ? v : SEQ_GEN_CLASSIC];
}

// LEAD contour knob (per track, RAM; 4 counts = 1 step, clamped no wrap)
void seqLeadContourAdjust(int16_t c) {
  genAdjust(&leadContour[genIdx()], &genLeadRes, c,
            SEQ_LEAD_ARCH, SEQ_LEAD_WALK);
}
void seqLeadContourSet(uint8_t i, uint8_t v) {
  if (i >= 16) return;
  leadContour[i] = v <= SEQ_LEAD_WALK ? v : SEQ_LEAD_ARCH;
}
uint8_t seqLeadContourOf(uint8_t i) {
  return i < 16 ? leadContour[i] : SEQ_LEAD_ARCH;
}
const char* seqLeadContourName(uint8_t v) {
  static const char* n[3] = { "ARCH", "DESC", "WALK" };
  return n[v <= SEQ_LEAD_WALK ? v : SEQ_LEAD_ARCH];
}

// LEAD page CT knob: chord-tone vs scale blend, 0..100% (4 counts = 1)
void seqLeadCtAdjust(int16_t c) {
  genAdjust(&leadCtBlend[genIdx()], &genLeadCtRes, c, 0, 100);
}
void seqLeadCtSet(uint8_t i, uint8_t v) {
  if (i < 16) leadCtBlend[i] = v <= 100 ? v : 0;
}
uint8_t seqLeadCtOf(uint8_t i) {
  return i < 16 ? leadCtBlend[i] : 0;
}
// LEAD page GD knob: guide-tone set depth, 0..3 (4 counts = 1 step)
void seqLeadGdAdjust(int16_t c) {
  genAdjust(&leadGdDepth[genIdx()], &genLeadGdRes, c, 0, 3);
}
void seqLeadGdSet(uint8_t i, uint8_t v) {
  if (i < 16) leadGdDepth[i] = v <= 3 ? v : 0;
}
uint8_t seqLeadGdOf(uint8_t i) {
  return i < 16 ? leadGdDepth[i] : 0;
}
const char* seqLeadGdName(uint8_t v) {
  static const char* n[4] = { "3/7", "+9", "+13", "EXT" };
  return n[v <= 3 ? v : 0];
}

// ---------------------------------------------------------------------------
// generate
// ---------------------------------------------------------------------------

// pitch classes the generator may pick: the current KEY + SCALE,
// subsampled to TONES count (all / 5 / 4 / 3 degrees, kept spread evenly
// over the scale - Chromatic + "all" = plain 12 semitones)
static uint8_t genAllowed[12];
static uint8_t genAllowedN = 12;

static void genBuildAllowed() {
  uint8_t cand[12], n = 0;
  for (uint8_t i = 0; i < 12; i++)
    if (keyIsInScale(i)) cand[n++] = i;
  if (n == 0) {                       // never happens (chromatic fallback)
    for (uint8_t i = 0; i < 12; i++) cand[i] = i;
    n = 12;
  }
  uint8_t want = n;
  switch (seqGenTonesOf(genIdx())) {
    case SEQ_GEN_TONES_5: want = 5; break;
    case SEQ_GEN_TONES_4: want = 4; break;
    case SEQ_GEN_TONES_3: want = 3; break;
    default: break;                   // ALL
  }
  if (want > n) want = n;
  for (uint8_t j = 0; j < want; j++)
    genAllowed[j] = cand[(uint16_t)j * n / want];
  genAllowedN = want;
}

// nearest in-scale pitch class (down first, then up - forceScaleNote style)
static uint8_t genSnapNote(uint8_t note) {
  for (uint8_t d = 0; d < 12; d++) {
    if (d == 0) {
      for (uint8_t i = 0; i < genAllowedN; i++)
        if (genAllowed[i] == (uint8_t)(note % 12)) return note;
      continue;
    }
    if (note >= d) {
      uint8_t dn = (uint8_t)(note - d);
      for (uint8_t i = 0; i < genAllowedN; i++)
        if (genAllowed[i] == (uint8_t)(dn % 12)) return dn;
    }
    if (note + d <= 127) {
      uint8_t up = (uint8_t)(note + d);
      for (uint8_t i = 0; i < genAllowedN; i++)
        if (genAllowed[i] == (uint8_t)(up % 12)) return up;
    }
  }
  return note;
}

// one lazy randomSeed() for every generator's rolls (GNR8/LEAD produce,
// the HARM bake's FILL gate): the first roll of a boot picks the seed
void genSeedOnce() {
  static bool seeded = false;
  if (!seeded) {                      // one lazy seed: differs per boot
    randomSeed((uint32_t)(micros() ^ (millis() << 16)));
    seeded = true;
  }
}

int seqGenProduce() {
  uint8_t ch = seqEditChannel() - 1;
  uint16_t n = seqPatLenOf(ch);
  if (n == 0 || n > SEQ_GEN_STEPS) return -1;
  genSeedOnce();
  uint8_t idx = genIdx();
  uint8_t fill = seqGenFillOf(idx), accp = seqGenAccOf(idx);
  uint8_t glp = seqGenGlideOf(idx), tiep = seqGenTieOf(idx);
  uint8_t low = (uint8_t)(24 + 12 * seqGenLowOf(idx));      // 24 / 36 / 48
  uint8_t octs = seqGenRangeOf(idx);                        // 1..5
  if (octs < 1) octs = 1;                 // never an empty note window
  int hi = (int)low + 12 * (int)octs - 1;                   // inclusive
  if (hi > 127) hi = 127;
  genBuildAllowed();

  bool euclid = seqGenAlgoOf(idx) == SEQ_GEN_EUCLID;
  uint64_t bits = 0;
  if (euclid) {
    Bjorklund<uint64_t, 10> eu;
    int16_t pulses = (int16_t)(((uint32_t)n * fill + 50) / 100);   // round
    if (pulses > (int16_t)n) pulses = n;
    bits = eu.compute((int8_t)n, (int8_t)pulses);
  }

  stepSnapshotTake();                 // M3 hold = undo back to here
  bool on[SEQ_GEN_STEPS];
  int written = 0;
  for (uint16_t i = 0; i < n; i++) {
    on[i] = euclid ? (((bits >> i) & 1) != 0)
                   : ((uint8_t)random(100) < fill);
    if (!on[i]) {
      stepGenClear((uint8_t)i);
      continue;
    }
    uint8_t note = (uint8_t)random((long)low, (long)hi + 1);
    note = genSnapNote(note);
    uint8_t att = 0;
    if ((uint8_t)random(100) < accp)  att |= SEQ_ATT_ACCENT;
    if ((uint8_t)random(100) < glp)   att |= SEQ_ATT_GLIDE;
    if (stepGenWrite((uint8_t)i, note, att) >= 0) written++;
  }
  // TIE rolls on the RESTS (Aciduino's rule): a rest whose previous step
  // holds a note (or continues another tie chain) may hold that note over
  // itself - the flag lands on the ringing step, consecutive rolls chain
  // the hold across several rests
  for (uint16_t i = 0; i < n; i++) {
    if (on[i]) continue;
    uint16_t p = i ? (uint16_t)(i - 1) : (uint16_t)(n - 1);   // ringing step
    uint16_t q = p ? (uint16_t)(p - 1) : (uint16_t)(n - 1);   // step before it
    if (!on[p] && !(stepAttrs((uint8_t)q) & SEQ_ATT_TIE)) continue;
    if ((stepAttrs((uint8_t)p) & SEQ_ATT_TIE)) continue;
    if ((uint8_t)random(100) < tiep) stepAttrToggle((uint8_t)p, SEQ_ATT_TIE);
  }
  return written;
}

// ---------------------------------------------------------------------------
// LEAD generate: walk a melodic CONTOUR over the in-scale ladder of the
// LOW..RANGE window.  Rhythm reuses the shared FILL% density gate (classic
// or Bjorklund/Euclid via ALGO); the pitch follows the contour (ARCH rises
// to a mid-pattern peak then falls, DESC steps down and wraps, WALK random
// -walks ±1/±2 scale steps and reflects at the ends).  Notes stay in the
// KEY + SCALE.  Stored like GNR8 (one note per step, undo snapshot first).
// At playback LEAD + FOLLOW + MAP (ct/gd) re-resolves these skeleton notes
// onto the feeder track's chord - see mapBassNote() in Keyboard.cpp.
// shared LEAD write path: roll the fill rests and walk the contour over
// the in-scale ladder, one note per active step (the undo snapshot is the
// caller's job).  Used by M3 GEN and by the live CONTOUR flip
static int leadEmit(uint8_t idx, uint16_t n, uint8_t contour) {
  genSeedOnce();
  uint8_t fill = seqGenFillOf(idx);
  uint8_t low = (uint8_t)(24 + 12 * seqGenLowOf(idx));      // 24 / 36 / 48
  uint8_t octs = seqGenRangeOf(idx);                        // 1..5
  if (octs < 1) octs = 1;
  int hi = (int)low + 12 * (int)octs - 1;
  if (hi > 127) hi = 127;
  genBuildAllowed();

  // in-scale ladder inside the window: the contour walks these degrees
  uint8_t ladder[48];
  uint8_t ln = 0;
  for (int v = low; v <= hi && ln < 48; v++)
    if (keyIsInScale((uint8_t)(v % 12))) ladder[ln++] = (uint8_t)v;
  if (ln == 0) { ladder[0] = low; ln = 1; }   // never empty
  uint8_t top = (uint8_t)(ln - 1);

  bool euclid = seqGenAlgoOf(idx) == SEQ_GEN_EUCLID;
  uint64_t bits = 0;
  if (euclid) {
    Bjorklund<uint64_t, 10> eu;
    int16_t pulses = (int16_t)(((uint32_t)n * fill + 50) / 100);
    if (pulses > (int16_t)n) pulses = n;
    bits = eu.compute((int8_t)n, (int8_t)pulses);
  }

  bool on[SEQ_GEN_STEPS];
  int written = 0;
  uint16_t half = (n > 1) ? (uint16_t)((n - 1) / 2) : 0;   // ARCH peak
  int pos = ln / 2;                   // WALK running position
  for (uint16_t i = 0; i < n; i++) {
    on[i] = euclid ? (((bits >> i) & 1) != 0)
                   : ((uint8_t)random(100) < fill);
    if (!on[i]) {
      stepGenClear((uint8_t)i);
      continue;
    }
    uint8_t note;
    switch (contour) {
      case SEQ_LEAD_DESC: {          // step down across the pattern, wrap
        uint8_t deg = (uint8_t)(top - (uint16_t)i * top /
                                       (n > 1 ? (uint16_t)(n - 1) : 1));
        note = ladder[deg];
        break;
      }
      case SEQ_LEAD_WALK: {          // random ±1/±2 scale-step walk
        pos += (int)random(-2, 3);   // -2..2
        if (pos < 0) pos = -pos;                        // reflect low
        if (pos > (int)top) pos = 2 * (int)top - pos;   // reflect high
        if (pos < 0) pos = 0;
        if (pos > (int)top) pos = (int)top;
        note = ladder[pos];
        break;
      }
      default: {                     // ARCH: rise to mid peak, fall back
        uint16_t t = (i <= half) ? i : (uint16_t)(n - 1 - i);
        uint8_t deg = half ? (uint8_t)((uint16_t)t * top / half) : 0;
        note = ladder[deg];
        break;
      }
    }
    if (stepGenWrite((uint8_t)i, note, 0) >= 0) written++;
  }
  return written;
}

// keep the pattern just written (or restored) as THIS contour's version
static void leadCacheStore(uint8_t idx, uint8_t contour, uint16_t n) {
  if (idx >= 16 || contour > SEQ_LEAD_WALK) return;
  if (n > SEQ_GEN_STEPS) n = SEQ_GEN_STEPS;
  for (uint16_t i = 0; i < n; i++) {
    uint8_t len = 0;
    const uint8_t *c = stepChordAtCh(idx + 1, (uint8_t)i, &len);
    leadCache[idx][contour][i] = (len > 0) ? c[0] : LEAD_CACHE_REST;
  }
  leadCacheLen[idx][contour] = (uint8_t)n;
}

// stamp a cached version back into the edit sequence (n = live pattern
// length; a shorter cache clears the tail, a longer one is truncated)
static void leadCacheRestore(uint8_t idx, uint8_t contour, uint16_t n) {
  if (idx >= 16 || contour > SEQ_LEAD_WALK) return;
  uint16_t cn = leadCacheLen[idx][contour];
  if (cn > n) cn = n;
  for (uint16_t i = 0; i < n; i++) {
    uint8_t v = (i < cn) ? leadCache[idx][contour][i] : LEAD_CACHE_REST;
    if (v == LEAD_CACHE_REST) stepGenClear((uint8_t)i);
    else                      stepSkeletonWrite((uint8_t)i, v);
  }
}

int seqLeadProduce() {
  uint8_t idx = genIdx();
  uint16_t n = seqPatLenOf(idx);
  if (n == 0 || n > SEQ_GEN_STEPS) return -1;
  stepSnapshotTake();                 // M3 hold = undo back to here
  int w = leadEmit(idx, n, seqLeadContourOf(idx));
  leadCacheStore(idx, seqLeadContourOf(idx), n);  // flip away and back = this
  return w;
}

// deterministic first-visit reshaping: reorder the pattern's existing
// notes along the new contour (same pitch material, new order - no
// randomness, rests kept).  Returns false when there is nothing to
// reshape (empty pattern -> caller falls back to a fresh generation)
static bool leadReshape(uint8_t idx, uint16_t n, uint8_t contour) {
  uint8_t pool[SEQ_GEN_STEPS];
  uint8_t act[SEQ_GEN_STEPS];
  uint8_t pn = 0;
  for (uint16_t i = 0; i < n; i++) {
    uint8_t len = 0;
    const uint8_t *c = stepChordAtCh(idx + 1, (uint8_t)i, &len);
    if (len > 0) { pool[pn] = c[0]; act[pn] = (uint8_t)i; pn++; }
  }
  if (pn == 0) return false;
  // sort the pitch material ascending (the active slots keep their order)
  for (uint8_t i = 1; i < pn; i++) {
    uint8_t v = pool[i];
    uint8_t j = i;
    while (j > 0 && pool[j - 1] > v) { pool[j] = pool[j - 1]; j--; }
    pool[j] = v;
  }
  // walk the pool along the contour, assigning pool[src] to act[k]
  int wpos = pn / 2, wdir = 1, wamp = 1;
  for (uint8_t k = 0; k < pn; k++) {
    uint8_t src;
    switch (contour) {
      case SEQ_LEAD_DESC:            // highest first, descending
        src = (uint8_t)(pn - 1 - k);
        break;
      case SEQ_LEAD_WALK: {          // +1 -1 +2 -2 ... deterministic walk
        if (k > 0) {
          wpos += wdir * wamp;
          if (wpos <= 0 || wpos >= (int)pn - 1) {
            if (wpos < 0) wpos = -wpos;
            if (wpos > (int)pn - 1) wpos = 2 * ((int)pn - 1) - wpos;
            wdir = -wdir;
            wamp++;
          }
          if (wpos < 0) wpos = 0;
          if (wpos > (int)pn - 1) wpos = (int)pn - 1;
        }
        src = (uint8_t)wpos;
        break;
      }
      default: {                     // ARCH: mountain fold of the pool
        uint8_t half = (uint8_t)((pn - 1) / 2);
        src = (k <= half) ? (uint8_t)(2 * k)
                          : (uint8_t)(2 * (pn - 1 - k) + 1);
        break;
      }
    }
    stepSkeletonWrite(act[k], pool[src]);
  }
  return true;
}

// M2 LEAD page ENC2: turning CONTOUR reshapes the stored sequence LIVE -
// no GEN press.  Non-destructive: the version being left is cached under
// its contour and the target's last version is restored (or reshaped
// fresh the first time), so ARCH/DESC/WALK stay revisitable.  Undo (M3
// hold) still returns to the pre-flip pattern; returns true when steps
// changed
bool seqLeadContourTurn(int16_t counts) {
  uint8_t idx = genIdx();
  uint8_t old = seqLeadContourOf(idx);
  seqLeadContourAdjust(counts);
  uint8_t now = seqLeadContourOf(idx);
  if (now == old) return false;             // no step crossed
  uint16_t n = seqPatLenOf(idx);
  if (n == 0 || n > SEQ_GEN_STEPS) return false;

  // one undo snapshot per burst: the first flip of a continuous turn
  // saves the pre-flip pattern (M3 hold restores all of it)
  if (millis() - leadContourLast > 600) stepSnapshotTake();
  leadContourLast = millis();

  leadCacheStore(idx, old, n);              // keep the version we leave
  if (leadCacheLen[idx][now]) {
    leadCacheRestore(idx, now, n);          // bring its last shape back
  } else {
    // first visit: reshape the existing notes onto the new contour
    // (deterministic reorder, no randomness) - an empty pattern has
    // nothing to reshape and falls back to a fresh generation
    if (!leadReshape(idx, n, now)) leadEmit(idx, n, now);
    leadCacheStore(idx, now, n);
  }
  return true;
}

// ---------------------------------------------------------------------------
// rotate
// ---------------------------------------------------------------------------

void seqGenShiftAdjust(int16_t counts) {
  uint8_t ch = seqEditChannel() - 1;
  uint16_t n = seqPatLenOf(ch);
  if (n == 0 || n > SEQ_GEN_STEPS) return;
  genShiftRes += counts;
  int16_t step = 0;
  while (genShiftRes >= 4) { genShiftRes -= 4; step++; }
  while (genShiftRes <= -4) { genShiftRes += 4; step--; }
  if (!step) return;
  // one undo snapshot per burst: the first step of a continuous rotation
  // saves the un-rotated pattern (M3 hold restores all of it)
  if (millis() - genShiftLast > 600) stepSnapshotTake();
  genShiftLast = millis();
  stepShiftPattern((uint8_t)n, (int8_t)step);
  int16_t p = (int16_t)genShiftPos + step;
  while (p >= (int16_t)n) p = (int16_t)n ? p - n : 0;
  while (p < 0) p += n;
  genShiftPos = (uint8_t)p;
}

uint8_t seqGenShiftPos() { return genShiftPos; }
