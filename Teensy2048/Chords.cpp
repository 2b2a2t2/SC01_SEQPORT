#include "Chords.h"
#include "Keyboard.h"   // keyChannel(), keyPendingRefresh()
#include "UsbMidiShim.h"    // usbMIDI (Teensy core on the Teensy, facade on SC01)

#define CHORD_VEL   100
#define CHORD_N     8      // notes sounding at once
#define CHORD_ENC   4      // encoder counts per param step

// 54 scales; patterns are 12-bit masks rooted at C (bit0 = C) so the
// keyRoot rotation lands on the right pitch classes
static const char* const kScaleName[KEY_SCALE_COUNT] = {
  // diatonic modes
  "Chr", "Major", "Minor", "HarmMin", "MelMin", "Dorian", "Phryg",
  "Lydian", "Mixoly", "Locrian",
  // pentatonic & blues
  "MajPent", "MinPent", "Blues",
  // symmetric & exotic
  "WhoTone", "DimHW", "DimWH", "DbHarm", "PhrDom", "HunMin", "Mishbrk",
  "LydDom", "Altered", "BebopD", "BebopM", "LydAug", "Mixb6", "LocN2",
  "Dor#4", "Lyd#2", "Ultra", "Persian", "NeaMin", "NeaMaj", "Augmnt",
  "Promth", "Enigm", "Insen", "Yo", "Hirajo",
  // harmonic major
  "HarmMaj",
  // Messiaen modes of limited transposition 3..7
  "Mesia3", "Mesia4", "Mesia5", "Mesia6", "Mesia7",
  // maqam / gamelan
  "Span8", "Saba", "Iwato", "Kumoi", "PelogS", "PelogT",
  // maqam (12-TET renderings; the jins half-flats are rounded)
  "Rast", "Bayati", "Hijaz"
};
// one-word mood shown in the header when a scale is selected
static const char* const kScaleMood[KEY_SCALE_COUNT] = {
  "All Notes", "Happy", "Sad", "Exotic", "Dramatic", "Jazzy", "Spanish",
  "Dreamy", "Groovy", "Tense",
  "Open", "Soulful", "Bluesy",
  "Floating", "Spooky", "Unstable", "Arabic", "Flamenco", "Gypsy", "Klezmer",
  "Bluesy", "Restless", "Swinging", "Breezy", "Shimmer", "Moody", "Eerie",
  "Mystic", "Bright", "Bizarre", "Mystical", "Lament", "Cinematic",
  "Surreal", "Airy", "Cryptic", "Haunting", "Gentle", "Tranquil",
  "Grand",
  "Hypnotic", "Alien", "Ritual", "Spectral", "Cluster",
  "Spanish", "Mourning", "Bleak", "Koto", "Gamelan", "Bronze",
  "Noble", "Intimate", "Yearning"
};
static const uint16_t kScalePat[KEY_SCALE_COUNT] = {
  0xFFF,  // Chr       {0..11}
  0xAB5,  // Major     {0,2,4,5,7,9,11}
  0x5AD,  // Minor     {0,2,3,5,7,8,10}
  0x9AD,  // HarmMin   {0,2,3,5,7,8,11}
  0xAAD,  // MelMin    {0,2,3,5,7,9,11}
  0x6AD,  // Dorian    {0,2,3,5,7,9,10}
  0x5AB,  // Phryg     {0,1,3,5,7,8,10}
  0xAD5,  // Lydian    {0,2,4,6,7,9,11}
  0x6B5,  // Mixoly    {0,2,4,5,7,9,10}
  0x56B,  // Locrian   {0,1,3,5,6,8,10}
  0x295,  // MajPent   {0,2,4,7,9}
  0x4A9,  // MinPent   {0,3,5,7,10}
  0x4E9,  // Blues     {0,3,5,6,7,10}
  0x555,  // WhoTone   {0,2,4,6,8,10}
  0x6DB,  // DimHW     {0,1,3,4,6,7,9,10}
  0xB6D,  // DimWH     {0,2,3,5,6,8,9,11}
  0x9B3,  // DbHarm    {0,1,4,5,7,8,11}
  0x5B3,  // PhrDom    {0,1,4,5,7,8,10}
  0x9CD,  // HunMin    {0,2,3,6,7,8,11}
  0x5CD,  // Mishbrk   {0,2,3,6,7,8,10} (Misheberakh = 4th mode of harmonic minor)
  0x6D5,  // LydDom    {0,2,4,6,7,9,10}
  0x55B,  // Altered   {0,1,3,4,6,8,10}
  0xEB5,  // BebopD    {0,2,4,5,7,9,10,11}
  0xBB5,  // BebopM    {0,2,4,5,7,8,9,11}
  0xB55,  // LydAug    {0,2,4,6,8,9,11}
  0x5B5,  // Mixb6     {0,2,4,5,7,8,10}
  0x56D,  // LocN2     {0,2,3,5,6,8,10}
  0x6CD,  // Dor#4     {0,2,3,6,7,9,10}
  0xAD9,  // Lyd#2     {0,3,4,6,7,9,11}
  0x2DB,  // Ultra     {0,1,3,4,6,7,9}
  0x973,  // Persian   {0,1,4,5,6,8,11}
  0x9AB,  // NeaMin    {0,1,3,5,7,8,11}
  0xAAB,  // NeaMaj    {0,1,3,5,7,9,11}
  0x999,  // Augmnt    {0,3,4,7,8,11}
  0x655,  // Promth    {0,2,4,6,9,10}
  0xD53,  // Enigm     {0,1,4,6,8,10,11}
  0x4A3,  // Insen     {0,1,5,7,10}
  0x4A9,  // Yo        {0,3,5,7,10} (as listed)
  0x1A3,  // Hirajo    {0,1,5,7,8}
  0x9B5,  // HarmMaj   {0,2,4,5,7,8,11}
  0xDDD,  // Mesia3    {0,2,3,4,6,7,8,10,11} (2-1-1 x3)
  0x9E7,  // Mesia4    {0,1,2,5,6,7,8,11}
  0x8E3,  // Mesia5    {0,1,5,6,7,11}
  0xD75,  // Mesia6    {0,2,4,5,6,8,10,11}
  0xBEF,  // Mesia7    {0,1,2,3,5,6,7,8,9,11}
  0x57B,  // Span8     {0,1,3,4,5,6,8,10}
  0x5DB,  // Saba      {0,1,3,4,6,7,8,10}
  0x463,  // Iwato     {0,1,5,6,10}
  0x28D,  // Kumoi     {0,2,3,7,9}
  0x18B,  // PelogS    {0,1,3,7,8}
  0x18D,  // PelogT    {0,2,3,7,8}
  0x6AD,  // Rast      {0,2,3,5,7,9,10} (= Dorian; jins Rast, E half-flat rounded)
  0x6AB,  // Bayati    {0,1,3,5,7,9,10} (jins Bayati, 1.5-1.5-2 off the 2nd)
  0x5B3,  // Hijaz     {0,1,4,5,7,8,10} (= PhrDom; jins Hijaz)
};
static const char* const kRootName[12] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// quality -> row in the triad table below (NONE plays no triad of its own;
// it only appears through the diatonic/KeyLock paths)
static const uint8_t kQualityRow[5] = { 0, 0, 1, 2, 5 };
static const char* const kQualityName[5] = { "", "Maj", "Min", "Dim", "Sus" };
static const uint8_t kTriad[6][3] = {
  {0, 4, 7},  // 0: Major
  {0, 3, 7},  // 1: Minor
  {0, 3, 6},  // 2: Diminished
  {0, 4, 8},  // 3: Augmented
  {0, 2, 7},  // 4: Sus2
  {0, 5, 7},  // 5: Sus4
};

// scale state (shared by every KEY variant)
static uint8_t sScale = 0;            // 0 = Chrom: no filter, no LED change
static uint8_t sRoot  = 0;            // 0 = C
static int16_t sScaleRes = 0, sRootRes = 0;

// chord engine state
static uint8_t sQuality = CHORD_Q_NONE;   // momentary quality button
static uint8_t sExt = 0;                  // extension bitmask (F1..F4)
static int8_t  sParam[4] = {0, 0, 0, 0};  // voicing, bass, spread, density
static int16_t sParamRes[4] = {0};
static bool    sLatch = false, sLock = false, sPerf = false;
static bool    sFScale = false;      // KEYMODE2 ENC4: force chord tones to scale

static uint16_t sHeld = 0;                // keys held during the gesture
static uint8_t  sHeldCount = 0;
static int16_t  sRootNote = -1;           // root of the current gesture
static uint8_t  sPressNote[12];           // note each key was pressed with

static uint8_t  sNotes[CHORD_N];          // notes currently sounding
static uint8_t  sCount = 0;
static int16_t  sPlayedRoot = -1;         // root of the built chord (name)
static uint8_t  sPlayed = CHORD_Q_NONE;   // quality actually built
static bool     sCut = false;             // silenced by chordCut()
static bool     sMute = false;            // live MONO-track voice owns sound

static uint16_t rotPattern() {
  uint16_t p = kScalePat[sScale];
  return (uint16_t)(((p << sRoot) | (p >> (12 - sRoot))) & 0xFFF);
}

static uint8_t clampNote(int n) {
  if (n < 0) n = 0;
  if (n > 127) n = 127;
  return (uint8_t)n;
}

// ---------------------------------------------------------------------------
// scale API
// ---------------------------------------------------------------------------

void keyScaleAdjust(int16_t counts) {
  sScaleRes += counts;
  int8_t step = 0;
  while (sScaleRes >= CHORD_ENC) { sScaleRes -= CHORD_ENC; step++; }
  while (sScaleRes <= -CHORD_ENC) { sScaleRes += CHORD_ENC; step--; }
  if (!step) return;
  int v = (int)sScale + step;
  if (v < 0) v = 0;
  if (v > KEY_SCALE_COUNT - 1) v = KEY_SCALE_COUNT - 1;
  sScale = (uint8_t)v;
}

int keyScale() { return sScale; }
const char* keyScaleName() { return kScaleName[sScale]; }

// the scale's mood is what the header says when it changes ("Scale Happy"):
// the knob's own value line already carries kScaleName, so the long spelling
// would only repeat it and squeeze the interesting word off the strip
const char* keyScaleMood() { return kScaleMood[sScale]; }

void keyRootAdjust(int16_t counts) {
  sRootRes += counts;
  int8_t step = 0;
  while (sRootRes >= CHORD_ENC) { sRootRes -= CHORD_ENC; step++; }
  while (sRootRes <= -CHORD_ENC) { sRootRes += CHORD_ENC; step--; }
  if (!step) return;
  int v = (int)sRoot + step;
  if (v < 0) v = 0;
  if (v > 11) v = 11;
  sRoot = (uint8_t)v;
}

int keyRoot() { return sRoot; }
const char* keyRootName() { return kRootName[sRoot]; }

bool keyIsInScale(uint8_t kidx) {
  if (sScale == 0) return true;
  return (rotPattern() & (uint16_t)(1u << (kidx % 12))) != 0;
}

// ---------------------------------------------------------------------------
// internals: quality resolution
// ---------------------------------------------------------------------------

// diatonic quality of a root pitch class: I/IV/V major, ii/iii/vi minor,
// vii diminished (the pressed quality button overrides this upstream)
static uint8_t diatonicQuality(uint8_t rootPC) {
  if (sScale == 0) return CHORD_Q_MAJ;
  uint16_t rot = rotPattern();
  int dist = ((int)rootPC - (int)sRoot + 12) % 12;
  int deg = 0;
  for (int s = 0; s <= dist && s < 12; s++) {
    if (rot & (uint16_t)(1u << ((sRoot + s) % 12))) {
      if (s == dist) break;
      deg++;
    }
  }
  if (deg == 0 || deg == 3 || deg == 4) return CHORD_Q_MAJ;
  if (deg == 6) return CHORD_Q_DIM;
  return CHORD_Q_MIN;
}

// KeyLock: nearest scale degree of the played pitch class
static int keylockDegree(uint8_t pc) {
  uint16_t rot = rotPattern();
  int bestDeg = 0, bestDist = 12, deg = 0;
  for (int s = 0; s < 12; s++) {
    int tpc = (sRoot + s) % 12;
    if (rot & (uint16_t)(1u << tpc)) {
      int d = (int)pc - tpc;
      if (d < 0) d = -d;
      if (d > 6) d = 12 - d;
      if (d < bestDist) { bestDist = d; bestDeg = deg; }
      deg++;
    }
  }
  return bestDeg;
}

// KeyLock: the triad quality built on that degree
static uint8_t keylockQuality(int deg) {
  if (sScale == 0) return CHORD_Q_MAJ;
  uint16_t rot = rotPattern();
  int pcs[12], n = 0;
  for (int s = 0; s < 12; s++) {
    int tpc = (sRoot + s) % 12;
    if (rot & (uint16_t)(1u << tpc)) pcs[n++] = tpc;
  }
  if (n < 3) return CHORD_Q_MAJ;
  int d = deg % n;
  int r = pcs[d], i3 = (pcs[(d + 2) % n] - r + 12) % 12,
                 i5 = (pcs[(d + 4) % n] - r + 12) % 12;
  if (i3 == 4 && i5 == 7) return CHORD_Q_MAJ;
  if (i3 == 3 && i5 == 7) return CHORD_Q_MIN;
  if (i3 == 3 && i5 == 6) return CHORD_Q_DIM;
  return CHORD_Q_MAJ;
}

// ---------------------------------------------------------------------------
// internals: play / stop
// ---------------------------------------------------------------------------

static void stopNotes() {
  if (!sMute)                            // muted: the notes never went out
    for (uint8_t i = 0; i < sCount; i++)
      usbMIDI.sendNoteOff(sNotes[i], 0, keyChannel());
  sCount = 0;
}

static void sendOut() {
  if (sMute) return;
  for (uint8_t i = 0; i < sCount; i++)
    usbMIDI.sendNoteOn(sNotes[i], CHORD_VEL, keyChannel());
}

// extensions above the triad - fixed intervals so every scale (chromatic,
// pentatonic, ...) gets proper 6/7/9/11/13 colors; the 9 implies a 7th when no
// 7th bit is set
static void addExtensions(uint8_t *out, uint8_t &n, int root, uint8_t qType,
                          uint8_t ext) {
  if (!ext) return;
  uint8_t add[6];
  uint8_t an = 0;
  if (ext & CHORD_EXT_6)
    add[an++] = (qType == CHORD_Q_MIN || qType == CHORD_Q_DIM) ? 8 : 9;
  if (ext & CHORD_EXT_m7) add[an++] = 10;
  if (ext & CHORD_EXT_M7) add[an++] = 11;
  if (ext & CHORD_EXT_9) {
    if (!(ext & (CHORD_EXT_m7 | CHORD_EXT_M7)))
      add[an++] = (qType == CHORD_Q_MIN) ? 10 : 11;
    add[an++] = 14;
  }
  if (ext & CHORD_EXT_11) add[an++] = 17;
  if (ext & CHORD_EXT_13) add[an++] = 21;
  for (uint8_t i = 0; i < an && n < CHORD_N; i++) {
    uint8_t v = clampNote(root + add[i]);
    bool dup = false;
    for (uint8_t j = 0; j < n; j++) if (out[j] == v) dup = true;
    if (!dup) out[n++] = v;
  }
}

// FScale: pull an out-of-scale chord tone onto the nearest scale pitch.
// Ties go DOWN first (a sharp tends to flatten), and chromatic has nothing
// to do so it passes everything through
static uint8_t forceScaleNote(uint8_t note) {
  if (!sFScale || keyIsInScale(note)) return note;
  for (int d = 1; d <= 6; d++) {
    int dn = (int)note - d;
    if (dn >= 0 && keyIsInScale((uint8_t)dn)) return (uint8_t)dn;
    int up = (int)note + d;
    if (up <= 127 && keyIsInScale((uint8_t)up)) return (uint8_t)up;
  }
  return note;
}

// decode the root note and quality from a stored chord: match the triad
// templates by pitch class (order = major, minor, diminished, sus4, sus2,
// the same five families the MONO ROOT type knows), the LOWEST matching
// note wins (root-position bias, inversions cannot fool it).  A chord with
// no triad (dyads, quartal stacks) falls back to its lowest note as a
// major root - the sus/ext transforms still land somewhere musical
static void fxRootQ(const uint8_t *ch, uint8_t n, int *root, uint8_t *q) {
  bool pc[12] = {false, false, false, false, false, false,
                 false, false, false, false, false, false};
  for (uint8_t i = 0; i < n; i++) pc[ch[i] % 12] = true;
  int lo = ch[0];
  for (uint8_t i = 1; i < n; i++) if (ch[i] < lo) lo = ch[i];
  int best = -1;
  uint8_t bq = CHORD_Q_MAJ;
  for (uint8_t i = 0; i < n; i++) {
    uint8_t r = (uint8_t)(ch[i] % 12);
    uint8_t qTry = CHORD_Q_MAJ;
    if (pc[(r + 4) % 12] && pc[(r + 7) % 12])      qTry = CHORD_Q_MAJ;
    else if (pc[(r + 3) % 12] && pc[(r + 7) % 12]) qTry = CHORD_Q_MIN;
    else if (pc[(r + 3) % 12] && pc[(r + 6) % 12]) qTry = CHORD_Q_DIM;
    else if (pc[(r + 5) % 12] && pc[(r + 7) % 12]) qTry = CHORD_Q_SUS;
    else if (pc[(r + 2) % 12] && pc[(r + 7) % 12]) qTry = CHORD_Q_SUS;
    else continue;                                 // this note is no root
    if (best < 0 || ch[i] < best) { best = ch[i]; bq = qTry; }
  }
  *root = (best >= 0) ? best : lo;
  *q = bq;
}

// playback-time step FX for POLY tracks (the FX byte's SUS/COLOR/EXT bits;
// STRUM is timing, handled by the caller).  Core = SUS + extensions,
// color = one raw chromatic approach appended at the end.  See Chords.h
uint8_t chordFxApply(const uint8_t *in, uint8_t n, uint8_t fx,
                     uint8_t *out, uint8_t *nCore) {
  if (nCore) *nCore = n;
  if (!in || !out || !n) return 0;
  if (n > CHORD_N) n = CHORD_N;
  for (uint8_t i = 0; i < n; i++) out[i] = in[i];
  uint8_t m = n;

  int root;
  uint8_t q;
  fxRootQ(in, n, &root, &q);

  if (fx & (CHORD_FX_SUS2 | CHORD_FX_SUS4)) {
    // the sus re-voice removes the THIRD (by pitch class - a voicing may
    // park it anywhere) and adds the 4th/2nd in the root's octave
    uint8_t t3 = (uint8_t)((root + 3) % 12);
    uint8_t t4 = (uint8_t)((root + 4) % 12);
    uint8_t k = 0;
    for (uint8_t i = 0; i < m; i++) {
      uint8_t p = (uint8_t)(out[i] % 12);
      if (p != t3 && p != t4) out[k++] = out[i];
    }
    m = k;
    uint8_t w = clampNote(root + ((fx & CHORD_FX_SUS4) ? 5 : 2));
    bool dup = false;
    for (uint8_t i = 0; i < m; i++) if (out[i] == w) dup = true;
    if (!dup && m < CHORD_N) out[m++] = w;
  }
  uint8_t ext = (uint8_t)((fx >> 4) & 0x0F);
  if (ext) addExtensions(out, m, root, q, ext);
  if (fx & CHORD_FX_COLOR) {
    // one quiet approach note a half-step under the root - appended RAW,
    // so the caller must not FScale-snap it (the chromatic IS the color)
    uint8_t w = clampNote(root - 1);
    bool dup = false;
    for (uint8_t i = 0; i < m; i++) if (out[i] == w) dup = true;
    if (!dup && m < CHORD_N) out[m++] = w;
  }
  if (nCore) *nCore = m;
  return m;
}

// build the notes of a chord: triad from the quality row, extensions, then
// the explicit spread/density/bass/voicing transforms and the FScale snap.
// Shared by the live engine (playChordAt) and the KEYMODE4 apply path
static uint8_t buildNotes(int root, uint8_t qType, uint8_t ext, uint8_t bass,
                          uint8_t voicing, uint8_t spread, uint8_t density,
                          uint8_t *out) {
  uint8_t n = 0;
  out[n++] = (uint8_t)root;

  uint8_t q = kQualityRow[qType];
  for (uint8_t t = 0; t < 3 && n < CHORD_N; t++) {
    uint8_t v = clampNote(root + kTriad[q][t]);
    bool dup = false;
    for (uint8_t j = 0; j < n; j++) if (out[j] == v) dup = true;
    if (!dup) out[n++] = v;
  }
  addExtensions(out, n, root, qType, ext);

  // spread: raise everything above the root one octave per step
  if (spread > 0)
    for (uint8_t j = 1; j < n; j++)
      out[j] = clampNote((int)out[j] + spread * 12);

  // density: add octave copies of the chord until full
  if (density > 0) {
    uint8_t base = n;
    for (uint8_t j = 0; j < base && n < CHORD_N; j++) {
      out[n] = clampNote((int)out[j] + 12);
      n++;
    }
  }

  // bass: move the idx-th note down an octave to the bottom
  if (bass > 0 && n > 1 && n < CHORD_N) {
    uint8_t src = out[bass % n];
    if (src >= 12) {
      uint8_t low = (uint8_t)(src - 12);
      for (int j = n; j > 0; j--) out[j] = out[j - 1];
      out[0] = low;
      n++;
    }
  }

  // voicing: rotate the top notes down (voicing times)
  if (voicing > 0 && n > 3) {
    for (int8_t v = 0; v < voicing && v < n - 1; v++) {
      uint8_t top = out[0];
      for (uint8_t j = 0; j + 1 < n; j++) out[j] = out[j + 1];
      out[n - 1] = top;
    }
  }

  // FScale: every tone lands inside the scale, collapsed to unique notes
  // (so a forced third can never double the root silently)
  if (sFScale) {
    uint8_t m = 0;
    for (uint8_t i = 0; i < n; i++) {
      uint8_t v = forceScaleNote(out[i]);
      bool dup = false;
      for (uint8_t j = 0; j < m; j++) if (out[j] == v) dup = true;
      if (!dup) out[m++] = v;
    }
    n = m;
  }
  return n;
}

// build and play the chord on a root note with a fixed quality
static void playChordAt(int note, uint8_t qType) {
  stopNotes();
  int root = note;
  if (root < 36) root = 36;
  if (root > 84) root = 84;

  uint8_t out[CHORD_N];
  uint8_t n = buildNotes(root, qType, sExt, (uint8_t)sParam[1],
                         (uint8_t)sParam[0], (uint8_t)sParam[2],
                         (uint8_t)sParam[3], out);

  for (uint8_t i = 0; i < n; i++) sNotes[i] = out[i];
  sCount = n;
  sPlayedRoot = (int16_t)root;
  sPlayed = qType;
  sendOut();
}

// chromatic + no quality: the pad alone (still through the engine so the
// pending chord, assign mode and the LEDs all see it)
static void playSingle(int note) {
  stopNotes();
  sNotes[0] = clampNote(note);
  sCount = 1;
  sPlayedRoot = (int16_t)sNotes[0];
  sPlayed = CHORD_Q_NONE;
  sendOut();
}

// KeyLock: snap the root to the nearest scale tone, then play - a held
// quality button wins over that degree's triad (KeyLock still quantizes
// the root, the quality row stays live)
static void playKeyLockAt(int note) {
  uint8_t pc = (uint8_t)(((note % 12) + 12) % 12);
  int deg = keylockDegree(pc);
  uint8_t q = (sQuality != CHORD_Q_NONE) ? sQuality : keylockQuality(deg);

  uint16_t rot = rotPattern();
  int bestDist = 12, snap = pc;
  for (int s = 0; s < 12; s++) {
    int tpc = (sRoot + s) % 12;
    if (rot & (uint16_t)(1u << tpc)) {
      int d = (int)pc - tpc;
      if (d < 0) d = -d;
      if (d > 6) d = 12 - d;
      if (d < bestDist) { bestDist = d; snap = tpc; }
    }
  }
  playChordAt(note - (int)pc + snap, q);
}

// the current gesture as a chord (press path; quality > diatonic > single)
static void triggerRoot() {
  if (sRootNote < 0) return;
  if (sLock) playKeyLockAt(sRootNote);
  else if (sQuality != CHORD_Q_NONE) playChordAt(sRootNote, sQuality);
  else if (sScale != 0) playChordAt(sRootNote, diatonicQuality((uint8_t)(sRootNote % 12)));
  else playSingle(sRootNote);
}

// re-trigger only while a chord is actually sounding (quality/ext/param
// change, octave shift)
static void retrigger() {
  if (sRootNote < 0 || sCount == 0) return;
  triggerRoot();
  keyPendingRefresh();
}

// ---------------------------------------------------------------------------
// gesture
// ---------------------------------------------------------------------------

int chordPadPress(uint8_t kidx, uint8_t note) {
  if (kidx >= 12) return -1;
  if (!(sHeld & (uint16_t)(1u << kidx))) {
    sHeld |= (uint16_t)(1u << kidx);
    sHeldCount++;
  }
  sPressNote[kidx] = note;
  sRootNote = note;
  triggerRoot();
  keyPendingRefresh();          // the press is the gesture snapshot
  return (sCount > 0) ? sNotes[0] : -1;
}

int chordPadRelease(uint8_t kidx) {
  if (kidx >= 12 || !(sHeld & (uint16_t)(1u << kidx))) return -1;
  sHeld &= (uint16_t)~(1u << kidx);
  if (sHeldCount) sHeldCount--;

  if (sHeld == 0) {
    if (sCount == 0) { sRootNote = -1; return 0; }
    if (sLatch) return sNotes[0];          // latch: keep sounding
    stopNotes();
    sRootNote = -1;
    return 0;
  }
  // keys remain: re-trigger from the first held key (the pending chord is
  // NOT re-fed - the gesture snapshot was taken on press, like the
  // single-note path)
  for (uint8_t k = 0; k < 12; k++) {
    if (sHeld & (uint16_t)(1u << k)) {
      sRootNote = sPressNote[k];
      triggerRoot();
      return (sCount > 0) ? sNotes[0] : -1;
    }
  }
  return -1;
}

// ---------------------------------------------------------------------------
// params
// ---------------------------------------------------------------------------

void chordSetQuality(uint8_t q) {
  if (q > CHORD_Q_SUS) q = CHORD_Q_NONE;
  if (q == sQuality) return;
  sQuality = q;
  retrigger();
}

void chordSetExt(uint8_t ext, bool on) {
  if (on) sExt |= ext;
  else sExt &= (uint8_t)~ext;
  retrigger();
}

void chordParamAdjust(uint8_t idx, int16_t counts) {
  if (idx >= 4) return;
  sParamRes[idx] += counts;
  int8_t step = 0;
  while (sParamRes[idx] >= CHORD_ENC) { sParamRes[idx] -= CHORD_ENC; step++; }
  while (sParamRes[idx] <= -CHORD_ENC) { sParamRes[idx] += CHORD_ENC; step--; }
  if (!step) return;
  int v = (int)sParam[idx] + step;
  if (v < 0) v = 0;
  if (v > 3) v = 3;
  if (v == sParam[idx]) return;
  sParam[idx] = (int8_t)v;
  retrigger();
}

void chordLatchAdjust(int16_t counts) {
  bool want = (counts > 0);          // CW = on, CCW = off (no wrap)
  if (want == sLatch) return;
  sLatch = want;
  // latch off with nobody holding a key = silence now (pending keeps the chord)
  if (!sLatch && sHeldCount == 0 && sCount > 0) {
    stopNotes();
    sRootNote = -1;
  }
}

void chordKeyLockAdjust(int16_t counts) {
  bool want = (counts > 0);
  if (want != sLock) sLock = want;   // takes effect on the next press
}

void chordPerfAdjust(int16_t counts) {
  bool want = (counts > 0);
  if (want != sPerf) sPerf = want;   // state only (the reference has no behavior)
}

void chordFScaleAdjust(int16_t counts) {
  bool want = (counts > 0);          // CW = on, CCW = off (no wrap)
  if (want == sFScale) return;
  sFScale = want;
  retrigger();                       // live: rebuild the chord it silences
}

bool chordFScale() { return sFScale; }

uint8_t chordForceNote(uint8_t note) { return forceScaleNote(note); }

uint8_t chordParam(uint8_t idx) { return (idx < 4) ? (uint8_t)sParam[idx] : 0; }
bool chordLatch()  { return sLatch; }
bool chordKeyLock() { return sLock; }
bool chordPerf()   { return sPerf; }

// ---------------------------------------------------------------------------
// read accessors
// ---------------------------------------------------------------------------

const uint8_t* chordNotes() { return sNotes; }
uint8_t chordCount() { return sCount; }
bool chordSounding() { return sCount > 0; }

bool chordPlaysPitch(uint8_t pc) {
  pc %= 12;
  for (uint8_t i = 0; i < sCount; i++)
    if (sNotes[i] % 12 == pc) return true;
  return false;
}

const char* chordName() {
  static char buf[16];
  buf[0] = '\0';
  if (sPlayedRoot < 0) return buf;
  char ext[8] = "";
  if (sExt & CHORD_EXT_6)  strcat(ext, "6");
  if (sExt & CHORD_EXT_m7) strcat(ext, "m7");
  if (sExt & CHORD_EXT_M7) strcat(ext, "M7");
  if (sExt & CHORD_EXT_9)  strcat(ext, "9");
  snprintf(buf, sizeof(buf), "%s%s%s",
           kRootName[((sPlayedRoot % 12) + 12) % 12],
           kQualityName[sPlayed], ext);
  return buf;
}

// ---------------------------------------------------------------------------
// KEYMODE4: the genre engine owns the build state per gesture
// ---------------------------------------------------------------------------

// saved KEYMODE2 state (latch/KeyLock/params persist as globals - KEY4 must
// leave them exactly as it found them)
static bool    k4Saved = false;
static bool    k4LockWas = false, k4LatchWas = false;
static int8_t  k4ParamWas[4];

static void k4Force(uint8_t q, uint8_t ext, uint8_t bass) {
  sQuality = q;
  sExt = ext;
  sParam[0] = 0;
  sParam[1] = (int8_t)bass;
  sParam[2] = 0;
  sParam[3] = 0;
}

int chordKey4Press(uint8_t kidx, uint8_t note, uint8_t q, uint8_t ext,
                   uint8_t bass) {
  if (kidx >= 12) return -1;
  if (!k4Saved) {
    k4Saved = true;
    k4LockWas = sLock;
    k4LatchWas = sLatch;
    for (uint8_t i = 0; i < 4; i++) k4ParamWas[i] = sParam[i];
  }
  sLock = false;      // the proposal set decides the roots, not KeyLock
  sLatch = false;     // an audition releases with the key, like a single note
  if (!(sHeld & (uint16_t)(1u << kidx))) {
    sHeld |= (uint16_t)(1u << kidx);
    sHeldCount++;
  }
  sPressNote[kidx] = note;
  sRootNote = note;
  k4Force(q, ext, bass);
  playChordAt(note, q);        // explicit quality: never the KeyLock path
  keyPendingRefresh();
  return (sCount > 0) ? sNotes[0] : -1;
}

// the gesture re-voices from its first held key on release: swap the build
// state to that key's proposal before chordPadRelease rebuilds
void chordKey4Bucket(uint8_t q, uint8_t ext, uint8_t bass) {
  k4Force(q, ext, bass);
}

// gesture over / leaving KEYMODE4: the engine is back to KEYMODE2 state
void chordKey4Clear() {
  sQuality = CHORD_Q_NONE;
  sExt = 0;
  if (k4Saved) {
    sLock = k4LockWas;
    sLatch = k4LatchWas;
    for (uint8_t i = 0; i < 4; i++) sParam[i] = k4ParamWas[i];
    k4Saved = false;
  }
}

// the notes of a proposal without touching the sounding gesture (apply path:
// same builder as the live engine, KEY2's spread/density/voicing at neutral)
uint8_t chordBuild(uint8_t root, uint8_t q, uint8_t ext, uint8_t bass,
                   uint8_t *out) {
  int r = root;
  if (r < 36) r = 36;
  if (r > 84) r = 84;
  return buildNotes(r, q, ext, bass, 0, 0, 0, out);
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

void chordShift(int semitones) {
  if (semitones == 0) return;
  for (uint8_t k = 0; k < 12; k++) {
    if (!(sHeld & (uint16_t)(1u << k))) continue;
    sPressNote[k] = clampNote((int)sPressNote[k] + semitones);
  }
  if (sRootNote < 0) return;
  sRootNote = (int16_t)clampNote((int)sRootNote + semitones);
  retrigger();
}

void chordCut() {
  if (sMute) { sCut = true; return; }   // muted: nothing is out to cut
  if (sCount == 0) return;
  for (uint8_t i = 0; i < sCount; i++)
    usbMIDI.sendNoteOff(sNotes[i], 0, keyChannel());
  sCut = true;
}

void chordRestore() {
  if (!sCut) return;
  sCut = false;
  sendOut();
}

// live keyboard through a MONO track: keep building the gesture chord
// (pending, LEDs, recording all see it) but silence the engine - the mono
// voice (ROOT/BASS/ARP) sounds on the same channel instead
void chordMute(bool on) {
  if (on == sMute) return;
  sMute = on;
  if (on) {
    if (!sCut) {                         // fresh engage: cut what is out
      for (uint8_t i = 0; i < sCount; i++)
        usbMIDI.sendNoteOff(sNotes[i], 0, keyChannel());
      sCut = true;
    }
  } else {
    sCut = false;
    sendOut();                           // re-send whatever sounds now
  }
}

void chordStopAll() {
  stopNotes();
  sCut = false;
  sHeld = 0;
  sHeldCount = 0;
  sRootNote = -1;
  // quality/extensions are momentary (cleared when their buttons release);
  // latch/KeyLock/Perf/voicing params persist like the reference's globals
}
