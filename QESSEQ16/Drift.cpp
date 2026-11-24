#include "Drift.h"

#include <Arduino.h>
#include <string.h>

#include "Keyboard.h"   // stepChordAtCh / stepChordWrite / stepGenClear /
                        // stepIsActive / stepChordSame / stepSnapshotTake
#include "SeqClock.h"   // seqEditChannel, seqPatLen(Of), seqFollow, POLY types
#include "Chords.h"     // CHORD_Q_*, CHORD_EXT_* (via the Key4 bridge)
#include "Key4.h"       // k4Fw*, k4TransitionScore, k4VoicedChordIn, decodeChord
#include "Gen.h"        // seqGenFillOf / seqGenAlgoOf / genSeedOnce
#include "Encoders.h"   // encStep / encDrain (the SEQ knob feel)
#include "bjorklund.h"  // Euclidean placement, the same roll the HARM bake uses

// ---------------------------------------------------------------------------
// per-track settings (16 slots, RAM only - like the HARM page)
// ---------------------------------------------------------------------------
#define DRIFT_BLOCK_MAX 64
#define DRIFT_STEP_MAX  64

static uint8_t sHow[16]    = {0};    // DRIFT_HOW_SAME
static uint8_t sAnchor[16] = {50, 50, 50, 50, 50, 50, 50, 50,
                              50, 50, 50, 50, 50, 50, 50, 50};
static uint8_t sSpan[16]   = {0};    // 0 = AUTO
static uint8_t sRead[16]   = {0};    // DRIFT_READ_CHRD
static uint8_t sRhythm[16] = {1, 1, 1, 1, 1, 1, 1, 1,
                              1, 1, 1, 1, 1, 1, 1, 1};  // FILL = today's roll
static uint8_t sShape[16]  = {0};    // close
static uint8_t sSize[16]   = {0};    // triad
static uint8_t sSpread[16] = {0};
static uint8_t sInv[16]    = {0};    // 0 = nearest voice leading
// REPLY draws deterministically from this seed, so the automatic update
// after a feeder edit produces the SAME answer (idempotent); M3 bumps it,
// so every write is a fresh answer
static uint8_t sSeed[16] = {0};

static int16_t rHow, rAnchor, rSpan, rRead, rRhythm, rShape, rSize, rSpread,
               rInv;

static uint8_t driftIdx() {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  return ch < 16 ? ch : 0;
}

// small enums: one list step per value, clamped, no wrap
static void driftEnum(uint8_t *slot, int16_t *res, int16_t counts, uint8_t hi) {
  int16_t step = encStep(counts, res);
  if (!step) return;
  int v = (int)*slot + (int)step;
  if (v < 0) v = 0;
  if (v > (int)hi) v = hi;
  *slot = (uint8_t)v;
}

// 0..100% dials: 1 count = 1 unit (the SEQ percent feel)
static void driftPct(uint8_t *slot, int16_t *res, int16_t counts) {
  int16_t step = encDrain(counts, res);
  if (!step) return;
  int v = (int)*slot + (int)step;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  *slot = (uint8_t)v;
}

void driftHowAdjust(int16_t c)  { driftEnum(&sHow[driftIdx()], &rHow, c, 2); }
uint8_t driftHow()              { return sHow[driftIdx()]; }
const char* driftHowName() {
  static const char* n[3] = { "SAME", "WALK", "REPLY" };
  return n[driftHow()];
}

void driftAnchorAdjust(int16_t c) {
  driftPct(&sAnchor[driftIdx()], &rAnchor, c);
}
uint8_t driftAnchor() { return sAnchor[driftIdx()]; }

void driftSpanAdjust(int16_t c) { driftEnum(&sSpan[driftIdx()], &rSpan, c, 8); }
uint8_t driftSpan()             { return sSpan[driftIdx()]; }
const char* driftSpanName() {
  static const char* n[9] = { "AUTO", "1", "2", "3", "4",
                              "5", "6", "7", "8" };
  return n[driftSpan()];
}

void driftReadAdjust(int16_t c) { driftEnum(&sRead[driftIdx()], &rRead, c, 1); }
uint8_t driftRead()             { return sRead[driftIdx()]; }
const char* driftReadName() {
  return driftRead() == DRIFT_READ_ROOT ? "ROOT" : "CHRD";
}

void driftRhythmAdjust(int16_t c) {
  driftEnum(&sRhythm[driftIdx()], &rRhythm, c, 2);
}
uint8_t driftRhythm() { return sRhythm[driftIdx()]; }
const char* driftRhythmName() {
  static const char* n[3] = { "BLOCK", "FILL", "STAB" };
  return n[driftRhythm()];
}

void driftShapeAdjust(int16_t c) { driftEnum(&sShape[driftIdx()], &rShape, c, 2); }
uint8_t driftShape() { return sShape[driftIdx()]; }
const char* driftShapeName() {
  static const char* n[3] = { "Close", "Open", "Drop" };
  return n[sShape[driftIdx()]];
}
void driftSizeAdjust(int16_t c) { driftEnum(&sSize[driftIdx()], &rSize, c, 2); }
uint8_t driftSize() { return sSize[driftIdx()]; }
const char* driftSizeName() {
  static const char* n[3] = { "Triad", "7th", "9th" };
  return n[sSize[driftIdx()]];
}
void driftSpreadAdjust(int16_t c) {
  driftEnum(&sSpread[driftIdx()], &rSpread, c, 3);
}
uint8_t driftSpread() { return sSpread[driftIdx()]; }
void driftInvAdjust(int16_t c) { driftEnum(&sInv[driftIdx()], &rInv, c, 3); }
uint8_t driftInv() { return sInv[driftIdx()]; }
const char* driftInvName() {
  static const char* n[4] = { "Nat", "1st", "2nd", "3rd" };
  return n[sInv[driftIdx()]];
}

// ---------------------------------------------------------------------------
// the feeder progression, cut into blocks
// ---------------------------------------------------------------------------
// one block = one chord and how many feeder steps it lasts; consecutive
// equal chords merge, an empty feeder step extends the block before it
static uint8_t sBRoot[DRIFT_BLOCK_MAX];
static uint8_t sBQ[DRIFT_BLOCK_MAX];
static uint8_t sBSpan[DRIFT_BLOCK_MAX];
static bool    sBChrom[DRIFT_BLOCK_MAX];
static uint8_t sBCount;

// the answer per block (SAME/WALK keep the feeder chord, REPLY may replace)
static uint8_t sTRoot[DRIFT_BLOCK_MAX];
static uint8_t sTQ[DRIFT_BLOCK_MAX];
static bool    sTChrom[DRIFT_BLOCK_MAX];

// block index per DRIFT step
static uint8_t sMap[DRIFT_STEP_MAX];

static int driftBlocks() {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  if (ch >= 16) return -1;
  // FOLLOW gates the source exactly like the HARM bake: OFF means DRIFT
  // works from its own stored chords instead of the feeder
  uint8_t fch = seqFollow() ? seqFeederChannel() : (uint8_t)(ch + 1);
  if (fch < 1) fch = (uint8_t)(ch + 1);
  uint8_t N = seqPatLenOf((uint8_t)(fch - 1));
  sBCount = 0;
  uint8_t curQ = CHORD_Q_MAJ;
  bool curValid = false;
  uint8_t read = driftRead();
  for (uint8_t t = 0; t < N && t < DRIFT_STEP_MAX; t++) {
    uint8_t n = 0;
    const uint8_t* nt = stepChordAtCh(fch, t, &n);
    if (!nt || n == 0) {                 // rest: the harmony keeps going
      if (sBCount) sBSpan[sBCount - 1]++;
      continue;
    }
    uint8_t rootPc = 0, q = CHORD_Q_MAJ;
    if (read == DRIFT_READ_ROOT) {
      uint8_t low = nt[0];
      for (uint8_t i = 1; i < n; i++)
        if (nt[i] < low) low = nt[i];
      rootPc = (uint8_t)(low % 12);
      bool inKey = false;
      uint8_t deg = k4FwDegree(rootPc, &inKey);
      q = inKey ? k4FwQualityOf(deg) : (curValid ? curQ : CHORD_Q_MAJ);
    } else if (n < 3) {                  // single note / dyad: no triad to read
      rootPc = (uint8_t)(nt[0] % 12);
      q = curValid ? curQ : CHORD_Q_MAJ;
    } else {
      decodeChord(nt, n, &rootPc, &q);
      rootPc = (uint8_t)(rootPc % 12);
    }
    if (sBCount && sBRoot[sBCount - 1] == rootPc &&
        sBQ[sBCount - 1] == q) {
      sBSpan[sBCount - 1]++;
    } else if (sBCount < DRIFT_BLOCK_MAX) {
      bool inKey = false;
      k4FwDegree(rootPc, &inKey);
      sBRoot[sBCount] = rootPc;
      sBQ[sBCount] = q;
      sBChrom[sBCount] = !inKey;         // borrowed roots stay literal
      sBSpan[sBCount] = 1;
      sBCount++;
    }
    curQ = q;
    curValid = true;
  }
  return sBCount;
}

// fit the blocks onto L steps: AUTO keeps the feeder's rhythm of chord
// changes, 1..8 forces a fixed block length (the progression cycles)
static void driftFit(uint8_t L) {
  if (!sBCount || !L) return;
  uint8_t span = driftSpan();
  if (span) {
    for (uint8_t s = 0; s < L; s++)
      sMap[s] = (uint8_t)((s / span) % sBCount);
    return;
  }
  uint16_t total = 0;
  for (uint8_t i = 0; i < sBCount; i++) total += sBSpan[i];
  if (!total) total = sBCount;
  if (L < sBCount) {                     // too few steps for every block
    for (uint8_t s = 0; s < L; s++) sMap[s] = (uint8_t)(s % sBCount);
    return;
  }
  uint16_t len[DRIFT_BLOCK_MAX];
  uint16_t sum = 0;
  for (uint8_t i = 0; i < sBCount; i++) {
    uint32_t v = ((uint32_t)sBSpan[i] * L + total / 2) / total;
    if (v < 1) v = 1;
    len[i] = (uint16_t)v;
    sum = (uint16_t)(sum + v);
  }
  uint16_t guard = 0;                    // rounding error -> longest block
  while (sum < L && guard++ < 512) {
    uint8_t bi = 0;
    uint16_t bs = 0;
    for (uint8_t i = 0; i < sBCount; i++)
      if (sBSpan[i] > bs) { bs = sBSpan[i]; bi = i; }
    len[bi]++;
    sum++;
  }
  guard = 0;
  while (sum > L && guard++ < 512) {
    uint8_t bi = 0;
    uint16_t bl = 0;
    for (uint8_t i = 0; i < sBCount; i++)
      if (len[i] > bl && len[i] > 1) { bl = len[i]; bi = i; }
    if (!bl) break;
    len[bi]--;
    sum--;
  }
  uint8_t s = 0;
  for (uint8_t i = 0; i < sBCount && s < L; i++)
    for (uint16_t j = 0; j < len[i] && s < L; j++) sMap[s++] = i;
  while (s < L) sMap[s++] = (uint8_t)(sBCount - 1);
}

// REPLY: draw a related root through the genre transition table (the same
// grammar KEYMODE4 and the SKANK substitution use), biased toward the
// relative, the subdominant and the neighbouring degrees so the answer
// still belongs to the progression
static uint8_t driftDrawReply(uint8_t fromPc, uint8_t pos, uint8_t fallbackQ,
                              uint8_t* outQ, uint32_t h) {
  uint32_t w[12];
  uint32_t total = 0;
  for (uint8_t pc = 0; pc < 12; pc++) {
    uint32_t s = k4TransitionScore(fromPc, pc, pos);
    uint8_t df = (uint8_t)((pc + 12 - fromPc) % 12);
    if (df == 3 || df == 9) s += 4000;        // relative minor / major
    else if (df == 5) s += 3000;              // subdominant
    else if (df == 2 || df == 10) s += 1500;  // neighbouring degree
    if (s == 0) s = 1;
    w[pc] = s;
    total += s;
  }
  if (!total) { *outQ = fallbackQ; return fromPc; }
  uint32_t r = h % total;
  uint8_t pc = 0;
  for (; pc < 12; pc++) {
    if (r < w[pc]) break;
    r -= w[pc];
  }
  if (pc >= 12) pc = 0;
  bool inKey = false;
  uint8_t deg = k4FwDegree(pc, &inKey);
  *outQ = inKey ? k4FwQualityOf(deg) : fallbackQ;
  return pc;
}

static void driftTargets() {
  uint8_t how = driftHow();
  uint8_t anchor = driftAnchor();
  for (uint8_t i = 0; i < sBCount; i++) {
    sTRoot[i] = sBRoot[i];
    sTQ[i] = sBQ[i];
    sTChrom[i] = sBChrom[i];
    if (how != DRIFT_HOW_REPLY) continue;
    if (i + 1 == sBCount) continue;        // the phrase always comes home
    uint32_t h = 0x9E3779B9u ^
                 ((uint32_t)sSeed[driftIdx()] * 0x85EBCA6Bu) ^
                 ((uint32_t)i * 0xC2B2AE35u) ^
                 ((uint32_t)sBRoot[i] << 8) ^ (uint32_t)sBQ[i];
    if ((uint8_t)((h >> 8) % 100) < anchor) continue;
    uint8_t prevPc = (i > 0) ? sTRoot[i - 1] : sBRoot[sBCount - 1];
    uint8_t q = sBQ[i];
    uint8_t pc = driftDrawReply(prevPc, i, sBQ[i], &q, h >> 3);
    bool inKey = false;
    k4FwDegree(pc, &inKey);
    sTRoot[i] = pc;
    sTQ[i] = q;
    sTChrom[i] = !inKey;
  }
}

// what one step of the block plays: the target on the head, then chords that
// lead into the NEXT block's target (one scale step below, and the dominant
// of it on the last step of a block with room for three)
static void driftChordAt(uint8_t s, uint8_t L, uint8_t* rootPc, uint8_t* q,
                         bool* chrom) {
  uint8_t bi = sMap[s];
  uint8_t start = s;
  while (start > 0 && sMap[start - 1] == bi) start--;
  uint8_t end = s;
  while (end + 1 < L && sMap[end + 1] == bi) end++;
  uint8_t len = (uint8_t)(end - start + 1);
  uint8_t pos = (uint8_t)(s - start);
  *rootPc = sTRoot[bi];
  *q = sTQ[bi];
  *chrom = sTChrom[bi];
  if (driftHow() == DRIFT_HOW_SAME || pos == 0) return;
  uint8_t nj = (uint8_t)((bi + 1) % sBCount);
  uint8_t nr = sTRoot[nj];
  if (len >= 3 && pos == len - 1) {      // dominant of the next target
    *rootPc = (uint8_t)((nr + 7) % 12);
    *q = CHORD_Q_MAJ;
    bool inKey = false;
    k4FwDegree(*rootPc, &inKey);
    *chrom = !inKey;
    return;
  }
  bool nin = false;
  uint8_t nd = k4FwDegree(nr, &nin);
  if (nin) {                             // passing chord one degree below
    uint8_t d = (uint8_t)((nd + 6) % 7);
    *rootPc = k4FwPc(d);
    *q = k4FwQualityOf(d);
    *chrom = false;
  } else {
    *rootPc = (uint8_t)((nr + 10) % 12);
    *q = sTQ[nj];
    *chrom = sTChrom[nj];
  }
}

// voice one chord in the track's own register.  INV Nat (0) picks the
// inversion nearest the previous chord (voice leading); 1..3 force a seat
static int driftVoice(uint8_t rootPc, uint8_t q, uint8_t* out, int* prevLow) {
  uint8_t inv = driftInv();
  uint8_t first = inv ? inv : 0;
  uint8_t last = inv ? inv : 3;
  uint8_t best[8];
  int bestN = 0, bestDist = 9999;
  for (uint8_t k = first; k <= last; k++) {
    uint8_t tmp[8];
    int n = k4VoicedChordIn(seqEditChannel(), rootPc, q, driftSize(), k,
                            driftSpread(), driftShape(), tmp);
    if (n <= 0) continue;
    int low = tmp[0];
    for (int i = 1; i < n; i++)
      if (tmp[i] < low) low = tmp[i];
    int d = 0;
    if (*prevLow >= 0) d = (low > *prevLow) ? low - *prevLow : *prevLow - low;
    if (!bestN || d < bestDist) {
      bestDist = d;
      bestN = n;
      memcpy(best, tmp, (size_t)n);
    }
  }
  if (!bestN) return 0;
  memcpy(out, best, (size_t)bestN);
  int low = best[0];
  for (int i = 1; i < bestN; i++)
    if (best[i] < low) low = best[i];
  *prevLow = low;
  return bestN;
}

// what a write is for - the gestures differ only in the seed, whether they
// roll the rhythm, whether they touch a BLOCK's rests, and the undo snapshot
enum DriftWriteKind : uint8_t {
  DRIFT_WRITE_BAKE = 0,  // M3: new answer, FILL roll, BLOCK rests, snapshot
  DRIFT_WRITE_RELAY,     // RHYTHM knob: same answer, FILL roll, rests, snapshot
  DRIFT_WRITE_AUTO,      // feeder edit: active steps only, no roll, no snapshot
  DRIFT_WRITE_APPLY      // F4: active steps only, RHYTHM KEPT, snapshot
};

static int driftWrite(uint8_t mode) {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  if (ch >= 16) return -1;
  int L = seqPatLen();
  if (L <= 0 || L > DRIFT_STEP_MAX) return -1;
  if (mode == DRIFT_WRITE_BAKE) sSeed[ch]++;  // M3 re-rolls the REPLY answer
  if (driftBlocks() <= 0) return -1;
  driftFit((uint8_t)L);
  driftTargets();
  uint8_t fill = seqGenFillOf(ch);
  bool euclid = seqGenAlgoOf(ch) == SEQ_GEN_EUCLID;
  // BAKE and RELAY lay down a rhythm (the FILL roll); AUTO and APPLY only
  // re-chord the steps the track already uses
  bool roll = (mode == DRIFT_WRITE_BAKE || mode == DRIFT_WRITE_RELAY);
  bool activeOnly = (mode == DRIFT_WRITE_AUTO || mode == DRIFT_WRITE_APPLY);
  uint64_t bits = 0;
  if (mode != DRIFT_WRITE_AUTO)      // every explicit gesture can be undone
    stepSnapshotTake();
  if (roll) {
    genSeedOnce();
    if (euclid) {
      Bjorklund<uint64_t, 10> eu;
      int16_t pulses = (int16_t)(((uint32_t)L * fill + 50) / 100);
      if (pulses > L) pulses = L;
      bits = eu.compute((int8_t)L, (int8_t)pulses);
    }
  }
  int written = 0;
  int prevLow = -1;
  uint8_t rhythm = driftRhythm();
  for (int s = 0; s < L; s++) {
    if (activeOnly && !stepIsActive((uint8_t)s)) continue;
    // the first step of a run of the same block (SPAN re-cycles, so a block
    // that comes round again starts a new run and re-strikes)
    bool head = (s == 0) || (sMap[s] != sMap[s - 1]);
    // BLOCK: only the head carries a chord; the rest of the block is rests,
    // so an automatic re-derive empties them too (it still never ACTIVATES
    // a step the player cleared)
    // APPLY is the exception: it fits chords onto whatever rhythm is there,
    // so it never empties a step
    if (rhythm == DRIFT_RHY_BLOCK && !head && mode != DRIFT_WRITE_APPLY) {
      if (stepIsActive((uint8_t)s)) stepGenClear((uint8_t)s);
      continue;
    }
    // FILL rolls the steps (the HARM bake's rhythm); BLOCK and STAB ignore
    // FILL/ALGO, because which steps land is the point of the setting
    if (roll && rhythm == DRIFT_RHY_FILL) {
      bool on = euclid ? (((bits >> s) & 1) != 0)
                       : ((uint8_t)random(100) < fill);
      if (!on) {
        if (stepIsActive((uint8_t)s)) stepGenClear((uint8_t)s);
        continue;
      }
    }
    uint8_t rootPc = 0, q = CHORD_Q_MAJ;
    bool chrom = false;
    driftChordAt((uint8_t)s, (uint8_t)L, &rootPc, &q, &chrom);
    (void)chrom;
    uint8_t notes[8];
    int n = driftVoice(rootPc, q, notes, &prevLow);
    if (n <= 0) continue;
    if (activeOnly && stepChordSame((uint8_t)s, notes, n)) {
      written++;
      continue;
    }
    if (stepChordWrite((uint8_t)s, notes, (uint8_t)n) < 0) continue;
    written++;
  }
  return written;
}

int driftBake()   { return driftWrite(DRIFT_WRITE_BAKE); }
int driftRebake() { return driftWrite(DRIFT_WRITE_AUTO); }
int driftRelay()  { return driftWrite(DRIFT_WRITE_RELAY); }
int driftApply()  { return driftWrite(DRIFT_WRITE_APPLY); }

uint8_t driftStepChord(uint8_t step, uint8_t* notes) {
  if (!notes) return 0;
  int L = seqPatLen();
  if (L <= 0 || L > DRIFT_STEP_MAX || (int)step >= L) return 0;
  if (driftBlocks() <= 0) return 0;
  driftFit((uint8_t)L);
  driftTargets();
  uint8_t bi = sMap[step];
  uint8_t start = step;
  while (start > 0 && sMap[start - 1] == bi) start--;
  // under BLOCK only a block's first step plays (see driftWrite)
  if (driftRhythm() == DRIFT_RHY_BLOCK && start != step) return 0;
  int prevLow = -1;
  for (uint8_t s = start; s <= step; s++) {
    uint8_t rootPc = 0, q = CHORD_Q_MAJ;
    bool chrom = false;
    driftChordAt(s, (uint8_t)L, &rootPc, &q, &chrom);
    (void)chrom;
    uint8_t tmp[8];
    int n = driftVoice(rootPc, q, tmp, &prevLow);
    if (s == step) {
      if (n <= 0) return 0;
      memcpy(notes, tmp, (size_t)n);
      return (uint8_t)n;
    }
  }
  return 0;
}
