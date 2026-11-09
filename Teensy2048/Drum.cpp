#include "Drum.h"
#include "Keyboard.h"     // stepSnapshotTake / stepDrumWrite, CHORD_MAX
#include "SeqClock.h"     // seqEditChannel, seqTrackOf, seqPatLenOf
#include "Gen.h"          // genSeedOnce
#include "Encoders.h"     // encDrain - accelerated FILL / MOT dials

// parts written per step (the engine stores up to CHORD_MAX = 8 notes per
// step; stepDrumWrite clamps to its own limit, this keeps the page's buffer
// the same size)
#define DRUM_MAX_PARTS 8

// --- authoring tables -------------------------------------------------------
// THE KIT MAP (what this page's lanes play - the DRUM track's 16 parts are
// notes 36 (C1) .. 51 (D#2), device naming: MIDI 24 = C0):
//   36 C1  bass drum        44 G#1 low tom
//   37 C#1 hand clap        45 A1  mid tom
//   38 D1  closed hat       46 A#1 hi tom
//   39 D#1 open hat         47 B1  (unused)
//   40 E1  snare            48 C2  cow bell
//   41 F1  low conga        49 C#2 claves
//   42 F#1 mid conga        50 D2  maracas
//   43 G1  hi conga         51 D#2 cymbal
//
// Voice map per lane, indexed by the step char's letter (a=0 ... g=6).
// Only the letters a lane's lines actually use are hit; the slots exist so
// a line can reach a doubling voice (clap under a snare, claves as a click,
// open hat in place of a crash, a cowbell in the perc lane, ...).
// [0] = CUSTOM (this kit, the default), [1] = General MIDI percussion.
static const uint8_t kLaneVoice[2][DRUM_LANES][DRUM_VOICES] = {
  { // --- CUSTOM ---------------------------------------------------------
    { 36, 36, 36, 36, 36, 36, 36 }, // KICK : a = 36 bass drum
    { 40, 37, 49, 40, 40, 40, 40 }, // SNARE: a = 40 snare   b = 37 hand clap
                                    //        c = 49 claves  d = 40 snare (ghost)
                                    //        e..g = 40 snare
    { 38, 39, 50, 51, 38, 38, 38 }, // HAT  : a = 38 closed  b = 39 open
                                    //        c = 50 maracas d = 51 cymbal
                                    //        e..g = 38 closed
    { 43, 42, 39, 41, 46, 45, 48 }, // PERC : a = 43 hi conga  b = 42 mid conga
                                    //        c = 39 open hat  d = 41 low conga
                                    //        e = 46 hi tom    f = 45 mid tom
                                    //        g = 48 cow bell
                                    // the bar-start accent ('c') is an OPEN
                                    // HAT on this kit - the D#2 cymbal reads
                                    // as a crash and rings over the whole bar
  },
  { // --- General MIDI percussion -----------------------------------------
    { 36, 36, 36, 36, 36, 36, 36 }, // KICK : a = 36 bass drum
    { 38, 39, 37, 40, 38, 38, 38 }, // SNARE: a = 38 snare  b = 39 hand clap
                                    //        c = 37 side stick d = 40 e.snare
                                    //        e..g = 38 snare
    { 42, 46, 44, 51, 42, 42, 42 }, // HAT  : a = 42 closed b = 46 open
                                    //        c = 44 pedal  d = 51 ride
                                    //        e..g = 42 closed
    { 45, 47, 46, 48, 50, 49, 51 }, // PERC : a = 45 low tom  b = 47 low-mid tom
                                    //        c = 46 open hat d = 48 hi-mid tom
                                    //        e = 50 high tom f = 49 crash
                                    //        g = 51 ride
  }
};

// bar-end VAR fill voices per map: snare + the tom run (high -> low)
static const uint8_t kFillSnare[2] = { 40, 38 };
static const uint8_t kFillTomHi[2] = { 46, 50 };
static const uint8_t kFillTomLo[2] = { 44, 45 };

// One 16-step bar per line: '-' rest, 'a'..'g' core hit, 'A'..'G' optional
// (rolled by that lane's FILL %).  Lane order: KICK, SNARE, HAT, PERC;
// per lane [0] = line A, [1] = line B.  Line B is never a copy of line A:
// the line selector is a musical choice on every lane, percussion most of
// all (the perc lane carries real conga / tom / cowbell lines, not just the
// bar-start accent).
static const char *const kGenre[DRUM_GENRES][DRUM_LANES][2] = {
  // HOUSE - four on the floor, offbeat open hat, congas on the offbeats
  { { "a---a---a---a---", "a---a---a-a-a-a-" },
    { "----a-------a---", "----a---a-a-a---" },
    { "--b---b---b---b-", "aAaAaAaAaAaAaAaA" },
    { "c-a---a---a---a-", "ca-d--a--d-a--a-" } },
  // TECHNO - driving, ghosted 16th hats, tom stabs + cowbell
  { { "a---a---a---a---", "a---a---a-a---a-" },
    { "----a-------a---", "----a---a-a-a---" },
    { "--b---b---b---b-", "AaAaAaAaAaAaAaAa" },
    { "c--e-------f----", "c-g---a---g---a-" } },
  // BREAKS - broken kick, backbeat snare, tom chops
  { { "a-----a---a-----", "a--a--a---a--a--" },
    { "----a-------a---", "----a-----a-a--a" },
    { "aAaAaAaAaAaAaAaA", "--b---b---b---b-" },
    { "c---e---f---e---", "--a-d--a--d-a---" } },
  // HIPHOP - laid-back kick, ghost snares, lazy congas
  { { "a-----a-a-------", "a--a--a---a--a--" },
    { "----a-------a---", "----a--Aa-a-a--A" },
    { "aAaAaAaAaAaAaAaA", "--b---b---b---b-" },
    { "c-------d---a---", "---a--d----a--d-" } },
  // TRAP - sparse kick, hat rolls, 808 cowbell
  { { "a-----a---a--a--", "a---a---a-a---a-" },
    { "----a-------a---", "----a--a---a--a-" },
    { "aaaaaaaaaaaaaaaa", "aAaAaAaAaAaAaAaA" },
    { "c--g----g---g---", "--a-a--a--a-A--A" } },
  // JUNGLE - chopped break, busy ghosts, tom chops
  { { "a-----a---a--a--", "a--a-a----a-a---" },
    { "----a-------a---", "----a--a-a--a-A-" },
    { "aAaAaAaAaAaAaAaA", "aa-aa-aa-aa-aa--" },
    { "c-e---f---e---f-", "--a-d--a--a-d--a" } },
  // FUNK - syncopated kick, ghost snare, conga push
  { { "a-----a---a--a--", "a--a--a---a-a-a-" },
    { "----a-------a---", "----a--a-a--a--a" },
    { "aAaAaAaAaAaAaAaA", "--b---b---b---b-" },
    { "c--d--a---d--a--", "---a--d-a---a-d-" } },
  // ROCK - straight backbeat, ghosted 16ths, cowbell + tom fill
  { { "a-----a---a-----", "a-----a---a--a-a" },
    { "----a-------a---", "----a----a--a-a-" },
    { "aAaAaAaAaAaAaAaA", "--b---b---b---b-" },
    { "c-----g-----g---", "c--e--f--e--f---" } },
  // JAZZ - swing ride: quarters plus the skip note on the last 16th of
  // beats 2 and 4 (steps 7/15 read as the swung "a" of the ride pattern on
  // a straight grid), sparse kick, comping snare instead of a backbeat
  { { "a-------a-------", "a------aa--a----" },
    { "---a-----a--A---", "---a--a---a--a--" },
    { "a-A-a--aa-A-a--a", "a-a-a-aa-a-a-a-a" },
    { "c-------e---f---", "----e--f---e--f-" } },
  // LATIN - son clave on the snare lane, conga tumbao
  { { "a--a--a---a--a--", "a--a--a---a-a-a-" },
    { "c--c--c---c-c---", "----b-------b---" },
    { "aAaAaAaAaAaAaAaA", "--b---b---b---b-" },
    { "c--d--a---d--a--", "--a-a--d--a-a--d" } },
  // AFRO - 3-2 feel, conga conversation
  { { "a-----a---a--a--", "a--a--a---a-a---" },
    { "----a-------a---", "----a-----a---a-" },
    { "aAaAaAaAaAaAaAaA", "--b---b---b---b-" },
    { "c-a--d-a--a--d-a", "--a-a--d--a-a--d" } },
  // ELECTRO - machine kick, clap backbeat, cowbell
  { { "a-----a---a-----", "a--a--a-a-a-----" },
    { "----b-------b---", "----b--b---b--b-" },
    { "aAaAaAaAaAaAaAaA", "--b---b---b---b-" },
    { "c------g---g----", "--g--g---g--g---" } },
  // DNB - two-step: kick 1 and 3&, snare 2 and 4, tom and hat chops
  { { "a---------a-----", "a--a------a--a--" },
    { "----a-------a---", "----a--a---a--a-" },
    { "a-a-a-a-a-a-a-a-", "AaAaAaAaAaAaAaAa" },
    { "c----------e--f-", "--------e--f--e-" } },
  // GARAGE - 2-step shuffle: swung 8ths on the hats, skipped kick
  { { "a---------a-----", "a-----a---a---a-" },
    { "----a-------a---", "----a--a----a--a" },
    { "a--aa--aa--aa--a", "a-a-a-a-a-a-a-a-" },
    { "c---a---d---a---", "--a--d-a--a--d-a" } },
  // PUNK - fast straight 8ths, no-nonsense backbeat
  { { "a-------a-------", "a-----a-a-----a-" },
    { "----a-------a---", "----a--a----a--a" },
    { "a-a-a-a-a-a-a-a-", "AaAaAaAaAaAaAaAa" },
    { "c-----e-----f---", "----e--f----e--f" } },
  // SKA - offbeat skank, cowbell on the up-beats
  { { "a-------a-------", "a---a---a---a---" },
    { "----a-------a---", "----a---a---a-a-" },
    { "--a---a---a---a-", "--b---b---b---b-" },
    { "c-g---g---g---g-", "c-a-a--a-a--a-a-" } },
  // DISCO - four on the floor, open hats, conga groove
  { { "a---a---a---a---", "a---a---a-a-a-a-" },
    { "----b-------b---", "----b---b---b--b" },
    { "--b---b---b---b-", "aAaAaAaAaAaAaAaA" },
    { "c--a---a---a---a", "---a-d-a---a-d-a" } },
  // REGGAE - one drop (kick + snare on 3), skank hats, cowbell
  { { "--------a-------", "------a-a-----a-" },
    { "--------a-------", "----c-------c---" },
    { "--a---a---a---a-", "a-a-a-a-a-a-a-a-" },
    { "c-----g-----g---", "--a-a--a--a-a--a" } },
};

static const char *const kGenreName[DRUM_GENRES] = {
  "HOUSE", "TECHNO", "BREAKS", "HIPHOP", "TRAP", "JUNGLE",
  "FUNK", "ROCK", "JAZZ", "LATIN", "AFRO", "ELECTRO",
  "DNB", "GARAGE", "PUNK", "SKA", "DISCO", "REGGAE"
};

// --- per-track parameters ---------------------------------------------------
static uint8_t dGenre[16]  = { 0 };                    // HOUSE
static uint8_t dVar[16]    = { 0 };                    // straight
static uint8_t dLine[16][DRUM_LANES] = {
  { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
  { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
  { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 },
  { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }
};
static uint8_t dFill[16][DRUM_LANES] = {               // ghost density
  { 60, 60, 60, 60 }, { 60, 60, 60, 60 }, { 60, 60, 60, 60 },
  { 60, 60, 60, 60 }, { 60, 60, 60, 60 }, { 60, 60, 60, 60 },
  { 60, 60, 60, 60 }, { 60, 60, 60, 60 }, { 60, 60, 60, 60 },
  { 60, 60, 60, 60 }, { 60, 60, 60, 60 }, { 60, 60, 60, 60 },
  { 60, 60, 60, 60 }, { 60, 60, 60, 60 }, { 60, 60, 60, 60 },
  { 60, 60, 60, 60 }
};
static uint8_t dMot[16][DRUM_LANES] = {                // accent chance
  { 20, 20, 20, 20 }, { 20, 20, 20, 20 }, { 20, 20, 20, 20 },
  { 20, 20, 20, 20 }, { 20, 20, 20, 20 }, { 20, 20, 20, 20 },
  { 20, 20, 20, 20 }, { 20, 20, 20, 20 }, { 20, 20, 20, 20 },
  { 20, 20, 20, 20 }, { 20, 20, 20, 20 }, { 20, 20, 20, 20 },
  { 20, 20, 20, 20 }, { 20, 20, 20, 20 }, { 20, 20, 20, 20 },
  { 20, 20, 20, 20 }
};
static uint8_t dMap[16]   = { 0 };                     // DRUM_MAP_CUSTOM
static uint8_t dFillM[16] = { 60, 60, 60, 60, 60, 60, 60, 60,
                              60, 60, 60, 60, 60, 60, 60, 60 };
static uint8_t dMotM[16]  = { 20, 20, 20, 20, 20, 20, 20, 20,
                              20, 20, 20, 20, 20, 20, 20, 20 };

// encoder residuals (4 counts = 1 detent)
static int16_t rGenre, rVar, rFill, rMot;
static int16_t rLine[DRUM_LANES], rLF[DRUM_LANES], rLM[DRUM_LANES];
static unsigned long lastBake = 0;
#define DRUM_BAKE_GAP 600                    // ms: one undo per knob burst

static uint8_t drumIdx() {
  uint8_t ch = seqEditChannel() - 1;
  return ch < 16 ? ch : 0;
}

// 4 counts = 1 detent, residual kept (same shape as every SEQ knob)
static int16_t det(int16_t *res, int16_t counts) {
  *res += counts;
  int16_t s = 0;
  while (*res >= 4) { *res -= 4; s++; }
  while (*res <= -4) { *res += 4; s--; }
  return s;
}

void drumGenreAdjust(int16_t counts) {
  int16_t s = det(&rGenre, counts);
  if (!s) return;
  uint8_t ch = drumIdx();
  int v = (int)dGenre[ch] + s;
  if (v < 0) v = 0;
  if (v >= DRUM_GENRES) v = DRUM_GENRES - 1;
  dGenre[ch] = (uint8_t)v;
}

void drumVarAdjust(int16_t counts) {
  int16_t s = det(&rVar, counts);
  if (!s) return;
  uint8_t ch = drumIdx();
  int v = (int)dVar[ch] + s;
  if (v < 0) v = 0;
  if (v > DRUM_VAR_8) v = DRUM_VAR_8;
  dVar[ch] = (uint8_t)v;
}

// the master knobs drive all four lanes (so the per-lane F sets start from
// the same place) and keep their own value for the display
void drumFillAdjust(int16_t counts) {
  int16_t s = encDrain(counts, &rFill);
  if (!s) return;
  uint8_t ch = drumIdx();
  int v = (int)dFillM[ch] + s;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  dFillM[ch] = (uint8_t)v;
  for (uint8_t l = 0; l < DRUM_LANES; l++) dFill[ch][l] = (uint8_t)v;
}

void drumLaneFillAdjust(uint8_t lane, int16_t counts) {
  if (lane >= DRUM_LANES) return;
  int16_t s = encDrain(counts, &rLF[lane]);
  if (!s) return;
  uint8_t ch = drumIdx();
  int v = (int)dFill[ch][lane] + s;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  dFill[ch][lane] = (uint8_t)v;
}

void drumMotAdjust(int16_t counts) {
  int16_t s = encDrain(counts, &rMot);
  if (!s) return;
  uint8_t ch = drumIdx();
  int v = (int)dMotM[ch] + s;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  dMotM[ch] = (uint8_t)v;
  for (uint8_t l = 0; l < DRUM_LANES; l++) dMot[ch][l] = (uint8_t)v;
}

void drumLaneMotAdjust(uint8_t lane, int16_t counts) {
  if (lane >= DRUM_LANES) return;
  int16_t s = encDrain(counts, &rLM[lane]);
  if (!s) return;
  uint8_t ch = drumIdx();
  int v = (int)dMot[ch][lane] + s;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  dMot[ch][lane] = (uint8_t)v;
}

void drumLineAdjust(uint8_t lane, int16_t counts) {
  if (lane >= DRUM_LANES) return;
  int16_t s = det(&rLine[lane], counts);
  if (!s) return;
  uint8_t ch = drumIdx();
  int v = (int)dLine[ch][lane] + s;
  if (v < 0) v = 0;
  if (v > DRUM_LINE_OFF) v = DRUM_LINE_OFF;
  dLine[ch][lane] = (uint8_t)v;
}

uint8_t drumGenre()        { return dGenre[drumIdx()]; }
uint8_t drumVar()          { return dVar[drumIdx()]; }
uint8_t drumFill()         { return dFillM[drumIdx()]; }
uint8_t drumLaneFill(uint8_t l) { return l < DRUM_LANES ? dFill[drumIdx()][l] : 0; }
uint8_t drumMot()          { return dMotM[drumIdx()]; }
uint8_t drumLaneMot(uint8_t l) { return l < DRUM_LANES ? dMot[drumIdx()][l] : 0; }
uint8_t drumLine(uint8_t l)    { return l < DRUM_LANES ? dLine[drumIdx()][l] : 0; }

// two-state, NO wrap: CW = GM, CCW = CUSTOM; true when the flag moved
bool drumMapSetDir(int16_t dir) {
  uint8_t ch = drumIdx();
  uint8_t want = (dir > 0) ? DRUM_MAP_GM : DRUM_MAP_CUSTOM;
  if (dMap[ch] == want) return false;
  dMap[ch] = want;
  return true;
}
uint8_t drumMap() { return dMap[drumIdx()]; }
const char* drumMapName(uint8_t v) {
  return (v == DRUM_MAP_GM) ? "GM" : "CUSTOM";
}

const char* drumGenreName(uint8_t v) {
  return v < DRUM_GENRES ? kGenreName[v] : kGenreName[0];
}

const char* drumVarName(uint8_t v) {
  static const char *const n[3] = { "off", "fill4", "fill8" };
  return v <= DRUM_VAR_8 ? n[v] : n[0];
}

const char* drumLineName(uint8_t v) {
  static const char *const n[3] = { "A", "B", "OFF" };
  return v <= DRUM_LINE_OFF ? n[v] : n[0];
}

// --- persistence ------------------------------------------------------------
void drumGenreSet(uint8_t ch, uint8_t v) {
  if (ch < 16) dGenre[ch] = (v < DRUM_GENRES) ? v : 0;
}
void drumVarSet(uint8_t ch, uint8_t v) {
  if (ch < 16) dVar[ch] = (v <= DRUM_VAR_8) ? v : 0;
}
void drumFillSet(uint8_t ch, uint8_t v) {
  if (ch < 16) dFillM[ch] = (v <= 100) ? v : 60;
}
void drumLaneFillSet(uint8_t ch, uint8_t lane, uint8_t v) {
  if (ch < 16 && lane < DRUM_LANES) dFill[ch][lane] = (v <= 100) ? v : 60;
}
void drumMotSet(uint8_t ch, uint8_t v) {
  if (ch < 16) dMotM[ch] = (v <= 100) ? v : 20;
}
void drumLaneMotSet(uint8_t ch, uint8_t lane, uint8_t v) {
  if (ch < 16 && lane < DRUM_LANES) dMot[ch][lane] = (v <= 100) ? v : 20;
}
void drumLineSet(uint8_t ch, uint8_t lane, uint8_t v) {
  if (ch < 16 && lane < DRUM_LANES) dLine[ch][lane] = (v <= DRUM_LINE_OFF) ? v : 0;
}
void drumMapSet(uint8_t ch, uint8_t v) {
  if (ch < 16) dMap[ch] = (v <= DRUM_MAP_GM) ? v : DRUM_MAP_CUSTOM;
}
uint8_t drumMapOf(uint8_t ch) { return ch < 16 ? dMap[ch] : 0; }
uint8_t drumGenreOf(uint8_t ch)      { return ch < 16 ? dGenre[ch] : 0; }
uint8_t drumVarOf(uint8_t ch)        { return ch < 16 ? dVar[ch] : 0; }
uint8_t drumFillOf(uint8_t ch)       { return ch < 16 ? dFillM[ch] : 0; }
uint8_t drumLaneFillOf(uint8_t ch, uint8_t l) {
  return (ch < 16 && l < DRUM_LANES) ? dFill[ch][l] : 0;
}
uint8_t drumMotOf(uint8_t ch)        { return ch < 16 ? dMotM[ch] : 0; }
uint8_t drumLaneMotOf(uint8_t ch, uint8_t l) {
  return (ch < 16 && l < DRUM_LANES) ? dMot[ch][l] : 0;
}
uint8_t drumLineOf(uint8_t ch, uint8_t l) {
  return (ch < 16 && l < DRUM_LANES) ? dLine[ch][l] : 0;
}

// --- bake -------------------------------------------------------------------
static bool drumAdd(uint8_t *notes, uint8_t *n, uint8_t voice) {
  for (uint8_t i = 0; i < *n; i++)
    if (notes[i] == voice) return false;        // already in this step
  if (*n >= DRUM_MAX_PARTS) return false;
  notes[(*n)++] = voice;
  return true;
}

int drumProduce() {
  uint8_t ch = (uint8_t)(seqEditChannel() - 1);
  if (ch >= 16 || seqTrackOf(ch) != SEQ_TRACK_DRUM) return -1;
  uint16_t n = seqPatLenOf(ch);
  if (n == 0) return -1;
  if (n > 64) n = 64;

  // one undo per knob burst (a continuous turn restores the whole gesture)
  if ((unsigned long)(millis() - lastBake) > DRUM_BAKE_GAP) stepSnapshotTake();
  lastBake = millis();
  genSeedOnce();

  uint8_t genre = dGenre[ch] < DRUM_GENRES ? dGenre[ch] : 0;
  uint8_t var   = dVar[ch] <= DRUM_VAR_8 ? dVar[ch] : 0;
  uint8_t map   = (dMap[ch] <= DRUM_MAP_GM) ? dMap[ch] : 0;
  uint8_t fill[DRUM_LANES], mot[DRUM_LANES], line[DRUM_LANES], len[DRUM_LANES];
  for (uint8_t l = 0; l < DRUM_LANES; l++) {
    fill[l] = dFill[ch][l];
    mot[l]  = dMot[ch][l];
    line[l] = dLine[ch][l] <= DRUM_LINE_OFF ? dLine[ch][l] : 0;
    // length guard: a short authored line must not be read past its NUL.
    // An OFF lane has no line at all - DRUM_LINE_OFF is one past the A/B
    // pair - so it must never be dereferenced: on the LAST genre that slot
    // reads past kGenre into whatever the linker put next (kTriad, whose
    // bytes make a bogus pointer) and the strlen() faults, rebooting the
    // device.  It is also why lanes 0..2 got a neighbour's line length
    // instead of their own.
    len[l]  = (line[l] < DRUM_LINE_OFF)
                ? (uint8_t)strlen(kGenre[genre][l][line[l]]) : 0;
  }

  // bar-end fill zone: the last beat or the last half bar
  uint8_t vStart = (var == DRUM_VAR_8) ? 8 : (var == DRUM_VAR_4) ? 12 : 16;

  // roll the 16-step cell ONCE, then stamp it over the track's length - a
  // longer pattern repeats the same bar instead of re-rolling (and the
  // FILL/MOT rolls stay identical across the repeats)
  uint8_t cellNotes[16][DRUM_MAX_PARTS];
  uint8_t cellCnt[16];
  uint16_t cellAcc[16];

  for (uint8_t cell = 0; cell < 16; cell++) {
    uint8_t *notes = cellNotes[cell];
    uint8_t cnt = 0;
    uint16_t acc = 0;

    for (uint8_t l = 0; l < DRUM_LANES; l++) {
      if (line[l] >= DRUM_LINE_OFF) continue;              // lane off
      if (cell >= len[l]) continue;                        // short line
      char c = kGenre[genre][l][line[l]][cell];
      if (c == '-' || c == 0) continue;
      bool opt = (c >= 'A' && c <= 'G');
      uint8_t vx = opt ? (uint8_t)(c - 'A') : (uint8_t)(c - 'a');
      if (vx >= DRUM_VOICES) continue;
      if (opt && (uint8_t)random(100) >= fill[l]) continue; // ghost roll
      uint8_t voice = kLaneVoice[map][l][vx];
      drumAdd(notes, &cnt, voice);
      if ((uint8_t)random(100) < mot[l])
        acc |= (uint16_t)1 << (voice - 36);                 // accent this hit
    }

    // bar-end fill: a rolled snare/tom run through the zone, but only for
    // the lanes that are actually on (an OFF lane stays silent)
    if (cell >= vStart) {
      if (line[DRUM_SNARE] < DRUM_LINE_OFF &&
          (uint8_t)random(100) < fill[DRUM_SNARE]) {
        uint8_t sn = kFillSnare[map];
        drumAdd(notes, &cnt, sn);
        if ((uint8_t)random(100) < mot[DRUM_SNARE])
          acc |= (uint16_t)1 << (sn - 36);
      }
      if (cell >= 14 && line[DRUM_PERC] < DRUM_LINE_OFF &&
          (uint8_t)random(100) < fill[DRUM_PERC]) {
        // high tom into low tom (A#1 -> G#1 on the custom kit)
        uint8_t tv = (cell == 14) ? kFillTomHi[map] : kFillTomLo[map];
        drumAdd(notes, &cnt, tv);
      }
    }

    cellCnt[cell] = cnt;
    cellAcc[cell] = acc;
  }

  for (uint16_t i = 0; i < n; i++) {
    uint8_t cell = (uint8_t)(i & 15);
    stepDrumWrite((uint8_t)i, cellNotes[cell], cellCnt[cell], cellAcc[cell]);
  }
  return (int)n;
}
