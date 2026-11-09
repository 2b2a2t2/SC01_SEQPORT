#include "Skank.h"
#include "Chords.h"   // keyIsInScale (the FScale key rule), chordBuild, CHORD_Q_*
#include "Key4.h"     // k4BorrowSample - the 18-genre transition matrix

// ---------------------------------------------------------------------------
// internal shaping (all paths end sorted, deduped, clamped 0..127)
// ---------------------------------------------------------------------------
static void skankFinalize(Chord &c) {
  if (c.count > 8) c.count = 8;
  for (uint8_t i = 0; i < c.count; i++)
    if (c.notes[i] > 127) c.notes[i] = 127;
  for (uint8_t i = 1; i < c.count; i++) {          // insertion sort
    uint8_t v = c.notes[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && c.notes[j] > v) {
      c.notes[j + 1] = c.notes[j];
      j--;
    }
    c.notes[j + 1] = v;
  }
  uint8_t m = 0;
  for (uint8_t i = 0; i < c.count; i++)            // drop duplicates
    if (m == 0 || c.notes[i] != c.notes[m - 1])
      c.notes[m++] = c.notes[i];
  c.count = m;
}

// ENC6 SUB: at the last chord slot of every bar the voicing becomes the
// root-position diatonic triad of a degree sampled from the current
// genre's transition row (nearest degree of the sounding bass), built at
// that bass register - a passing chord kicked down the progression
static void skankSubstitute(Chord &c) {
  if (c.count == 0) return;
  uint8_t pc = 0, q = CHORD_Q_MAJ;
  if (!k4BorrowSample((uint8_t)(c.notes[0] % 12), &pc, &q)) return;
  int d = ((int)c.notes[0] - (int)pc) % 12;        // nearest pc at/below bass
  if (d < 0) d += 12;
  int root = (int)c.notes[0] - d;
  c.count = chordBuild((uint8_t)root, q, 0, 0, c.notes);  // triad, root pos
}

// ENC5 EXT: an in-scale color tone (6th / major 7th / 9th over the chord
// root - the candidates the FScale key rule keeps) joins above the
// voicing, rolled per hit; a pc already present is not a color, and the
// note lands on the nearest pitch above the top that carries the pc
static void skankExtend(Chord &c) {
  if (c.count == 0 || c.count >= 8) return;
  uint8_t rootPc = (uint8_t)(c.notes[0] % 12);
  uint8_t top = c.notes[0];
  for (uint8_t i = 1; i < c.count; i++)
    if (c.notes[i] > top) top = c.notes[i];
  static const uint8_t col[3] = { 9, 11, 2 };      // 6th, maj7, 9th
  uint8_t cands[3], n = 0;
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t pc = (uint8_t)((rootPc + col[i]) % 12);
    if (!keyIsInScale(pc)) continue;
    bool present = false;
    for (uint8_t j = 0; j < c.count; j++)
      if ((uint8_t)(c.notes[j] % 12) == pc) present = true;
    if (!present) cands[n++] = pc;
  }
  if (n == 0) return;
  uint8_t pc = cands[(uint8_t)random(n)];
  int note = (int)top + (int)((pc - (top % 12) + 12) % 12);
  if (note > 127) note -= 12;
  if (note < 0 || note > 127) return;
  c.notes[c.count++] = (uint8_t)note;
}

// inversion: rotate the k lowest tones up an octave - k = inv mod the
// tone count, so a triad has no third inversion (inv 3 lands home) while
// a four-tone chord walks all four positions
static void skankInvert(Chord &c, int inv) {
  if (c.count == 0) return;
  inv = ((inv % 4) + 4) % 4;
  int k = inv % (int)c.count;
  for (int i = 0; i < k; i++) {
    int v = (int)c.notes[i] + 12;
    if (v > 127) v = 127;
    c.notes[i] = (uint8_t)v;
  }
}

// ENC4 THN: drop the lowest tones progressively -
// dropped = round(amt/100 * (count - 2)), so 0 = full, 50 = drop the
// lowest, 100 = keep only the top two
static void skankThin(Chord &c, uint8_t amt) {
  if (amt == 0 || c.count <= 2) return;
  int drop = ((int)amt * ((int)c.count - 2) + 50) / 100;
  if (drop <= 0) return;
  for (uint8_t i = 0; (int)i + drop < (int)c.count; i++)
    c.notes[i] = c.notes[i + drop];
  c.count = (uint8_t)(c.count - drop);
}

// ENC8 NOT%: one tone of the voicing mutates to a random in-scale tone at
// the same register (the nearest pitch carrying the picked pc, so the stab
// keeps its size; finalize dedups whatever collapses)
static void skankMutate(Chord &c) {
  if (c.count == 0) return;
  uint8_t pcs[12], n = 0;
  for (uint8_t pc = 0; pc < 12; pc++)
    if (keyIsInScale(pc)) pcs[n++] = pc;
  if (n == 0) return;
  uint8_t k = (uint8_t)random(c.count);
  uint8_t pc = pcs[(uint8_t)random(n)];
  int note = (int)c.notes[k] - (int)(((int)c.notes[k] - (int)pc + 12) % 12);
  if (note < 0) note += 12;
  if (note > 127) note = 127;
  c.notes[k] = (uint8_t)note;
}

// ---------------------------------------------------------------------------
// the derivation itself (pure besides random() - the locks, feeder and
// register choice stay the caller's business)
// ---------------------------------------------------------------------------
void deriveSkankChord(const Chord &feeder, Chord &skankOut, uint8_t stepIndex,
                      uint8_t encTessiture, uint8_t encMotion, uint8_t encShape,
                      uint8_t encThinning, uint8_t encExtension,
                      uint8_t encSubstitution, uint8_t encTesRand,
                      uint8_t encNotRand) {
  skankOut = feeder;
  if (skankOut.count > 8) skankOut.count = 8;
  if (skankOut.count == 0) return;

  uint8_t idx = (uint8_t)((stepIndex & 15) >> 2);  // chord slot in the bar

  if (idx == 3 && encSubstitution &&
      (uint8_t)random(100) < encSubstitution)
    skankSubstitute(skankOut);
  if (encExtension && (uint8_t)random(100) < encExtension)
    skankExtend(skankOut);

  // TES anchors the walk, MOT walks away from it per chord slot, SHP
  // picks the slot map: LIN rises through the bar (0,1,2,3), PP ping-
  //pongs over the chord slots (0,1,2,3,2,1), RND rolls fresh each hit.
  // MOT at center 50 parks on the anchor (delta 0 also skips the RND
  // roll - nothing would show it).  TES% then may replace the anchored
  // result with a fully random inversion (the chaos roll)
  int base = ((int)encTessiture * 4) / 100;
  if (base > 3) base = base % 4;                   // 100 wraps home
  int inv = base;
  int delta = (int)encMotion - 50;
  if (delta) {
    int eff = idx;
    if (encShape > 66) eff = (int)random(4);
    else if (encShape > 33) {
      static const uint8_t tri[6] = { 0, 1, 2, 3, 2, 1 };
      eff = tri[(stepIndex >> 2) % 6];
    }
    int scaled = base * 50 + delta * eff;          // 50 = one inversion
    inv = (scaled >= 0) ? (scaled + 25) / 50 : (scaled - 25) / 50;
  }
  if (encTesRand && (uint8_t)random(100) < encTesRand)
    inv = (int)random(4);
  skankInvert(skankOut, inv);
  skankThin(skankOut, encThinning);
  if (encNotRand && (uint8_t)random(100) < encNotRand)
    skankMutate(skankOut);
  skankFinalize(skankOut);
}

const char *skankShpName(uint8_t encShape) {
  if (encShape <= 33) return "LIN";
  if (encShape <= 66) return "PP";
  return "RND";
}
