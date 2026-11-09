#include "Link.h"
#include "Display.h"
#include "Modes.h"
#include "SeqClock.h"
#include "Keyboard.h"
#include "Encoders.h"
#include "UsbMidiShim.h"    // usbMIDI (Teensy core on the Teensy, facade on SC01)

// ---------------------------------------------------------------- framing --
#define LINK_MFR   0x7D
#define LINK_TAG0  0x37   // '7'
#define LINK_TAG1  0x32   // '2'
#define LINK_TAG2  0x34   // '4'
#define LINK_TAG3  0x38   // '8'

// commands (device -> host unless marked)
enum : uint8_t {
  CMD_HELLO      = 0x01,   // ver
  CMD_ACK        = 0x02,   // host: ver, flags
  CMD_TRACK_SEL  = 0x10,   // idx
  CMD_TRACK      = 0x11,   // host: idx, name...
  CMD_CLIP_ACT   = 0x12,   // row, action (0 launch 1 stop 2 fill)
  CMD_CLIP_STATE = 0x13,   // host: firstRow, 4 x bits
  CMD_PAGE_STEP  = 0x14,   // dir (0 = previous, 1 = next)
  CMD_DEV_STEP   = 0x15,   // dir
  CMD_STOP_TRACK = 0x16,   //
  CMD_STOP_ALL   = 0x17,   //
  CMD_SEL_ROW    = 0x18,   // host: row
  CMD_KNOB       = 0x19,   // slot, value (the CC already went out for learn)
  CMD_PAT_STEP   = 0x20,   // ch, patLen, first, count, steps...
  CMD_FILL_DONE  = 0x21,   // host: ch, ok, notes lo, notes hi
  CMD_PARAM      = 0x30,   // host: slot, page, value, flags, name[8], text[7]
  CMD_PARAM_SHORT= 0x31,   // host: slot, value
  CMD_PAGE       = 0x32,   // host: page, count
  CMD_BANK       = 0x33,   // host: name[0..7]
};

// send queue: one message leaves per linkPump, so nothing ever blocks
#define Q_SLOTS 24
#define Q_LEN   40
static uint8_t qBuf[Q_SLOTS][Q_LEN];
static uint8_t qLen[Q_SLOTS];
static uint8_t qHead = 0, qTail = 0;

// receive assembly (usbMIDI delivers a whole SysEx per callback on this core,
// but the buffer is kept so a split frame cannot lose a message)
#define RX_MAX 320
static uint8_t  rxBuf[RX_MAX];
static uint16_t rxLen = 0;

static bool     alive = false;
static uint32_t lastHostMs = 0;
static uint32_t lastHelloMs = 0;
static uint32_t firstHelloMs = 0;
static bool     helloSent = false;

// host state cache
static uint8_t  trkIdx = 255;
static char     trkName[18] = "";
static uint8_t  clipBits[16] = {0};
static uint8_t  selRow = 255;
static char     pName[8][9];
static char     pText[8][8];
static uint8_t  pVal[8] = {0};
static bool     pValid[8] = {false};
static uint8_t  pageCur = 0;
static uint8_t  pageCount = 0;
static char     bankName[9] = "";

// pattern dump cursor: one chunk per pump
static bool     dumpOn = false;
static uint16_t dumpMask = 0;
static uint8_t  dumpCh = 0;
static uint8_t  dumpStep = 0;

// --------------------------------------------------------------- helpers ---
static uint8_t mk(uint8_t *b, uint8_t cmd) {
  b[0] = LINK_MFR;
  b[1] = LINK_TAG0;
  b[2] = LINK_TAG1;
  b[3] = LINK_TAG2;
  b[4] = LINK_TAG3;
  b[5] = cmd;
  return 6;
}

static void queue(const uint8_t *p, uint8_t len) {
  if (len == 0 || len > Q_LEN) return;
  uint8_t next = (uint8_t)((qHead + 1) % Q_SLOTS);
  if (next == qTail) return;          // full: drop, never block
  memcpy(qBuf[qHead], p, len);
  qLen[qHead] = len;
  qHead = next;
}

static bool flushOne() {
  if (qTail == qHead) return false;
  usbMIDI.sendSysEx(qLen[qTail], qBuf[qTail], false);
  qTail = (uint8_t)((qTail + 1) % Q_SLOTS);
  return true;
}

static void copyText(char *dst, uint8_t cap, const uint8_t *src, uint16_t n) {
  if (cap == 0) return;
  uint8_t i = 0;
  while (i < n && i + 1 < cap && src[i] >= 0x20 && src[i] < 0x7F) {
    dst[i] = (char)src[i];
    i++;
  }
  dst[i] = '\0';
}

// ------------------------------------------------------------ host state ---
const char* linkTrackName() { return trkName; }
uint8_t linkTrackIndex() { return trkIdx; }
uint8_t linkClipBits(uint8_t row) { return (row < 16) ? clipBits[row] : 0; }
uint8_t linkHighlightRow() { return selRow; }
const char* linkParamName(uint8_t slot) {
  return (slot < 8 && pValid[slot]) ? pName[slot] : "";
}
const char* linkParamText(uint8_t slot) {
  return (slot < 8 && pValid[slot]) ? pText[slot] : "";
}
uint8_t linkParamValue(uint8_t slot) { return (slot < 8) ? pVal[slot] : 0; }
bool linkParamValid(uint8_t slot) { return slot < 8 && pValid[slot]; }
uint8_t linkPage() { return pageCur; }
uint8_t linkPageCount() { return pageCount; }
const char* linkBankName() { return bankName; }

void linkParamTouch(uint8_t slot, uint8_t v) {
  if (slot < 8) pVal[slot] = (uint8_t)(v & 0x7F);
}

// the automap half of a knob turn: the CC went out on the wire for MIDI
// learn, this tells the script which parameter to write (a remote script
// cannot rely on hearing a CC on every channel, a SysEx always arrives)
void linkKnob(uint8_t slot, uint8_t v) {
  if (slot >= 8) return;
  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_KNOB);
  b[n++] = slot;
  b[n++] = (uint8_t)(v & 0x7F);
  queue(b, n);
}

bool linkAlive() {
  return alive && (millis() - lastHostMs) < 2000;
}

// --------------------------------------------------------- device -> host --
void linkTrackSelect(uint8_t idx) {
  if (idx > 15) return;
  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_TRACK_SEL);
  b[n++] = idx;
  queue(b, n);
}

void linkClipAct(uint8_t row, uint8_t act) {
  if (row > 15) return;
  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_CLIP_ACT);
  b[n++] = row;
  b[n++] = act;
  queue(b, n);
}

void linkStopTrack() {
  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_STOP_TRACK);
  queue(b, n);
}

void linkStopAll() {
  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_STOP_ALL);
  queue(b, n);
}

void linkBankStep(int8_t dir) {
  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_PAGE_STEP);
  b[n++] = (dir < 0) ? 0 : 1;
  queue(b, n);
}

void linkDeviceStep(int8_t dir) {
  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_DEV_STEP);
  b[n++] = (dir < 0) ? 0 : 1;
  queue(b, n);
}

// start streaming one track's stored pattern (callers send the CLIP_ACT that
// tells the host which slot to fill)
void linkDumpTrack(uint8_t ch) {
  dumpMask = (uint16_t)(1u << (ch & 0x0F));
  dumpCh = (uint8_t)(ch & 0x0F);
  dumpStep = 0;
  dumpOn = true;
}

void linkDumpAll() {
  dumpMask = 0xFFFF;
  dumpCh = 0;
  dumpStep = 0;
  dumpOn = true;
}

// one PAT_STEP chunk per pump: 2 steps (a full 8-note chord fits the frame)
static void pumpDump() {
  if (!dumpOn) return;
  if (dumpMask == 0) { dumpOn = false; return; }

  uint8_t guard = 0;
  while (guard++ < 20 && !(dumpMask & (uint16_t)(1u << dumpCh))) {
    dumpCh = (uint8_t)((dumpCh + 1) & 0x0F);
    dumpStep = 0;
  }
  if (!(dumpMask & (uint16_t)(1u << dumpCh))) { dumpOn = false; return; }

  uint8_t patLen = seqPatLenOf(dumpCh);
  if (patLen == 0) patLen = 16;
  if (patLen > 64) patLen = 64;
  if (dumpStep >= patLen) {
    dumpMask &= (uint16_t)~(uint16_t)(1u << dumpCh);
    dumpCh = (uint8_t)((dumpCh + 1) & 0x0F);
    dumpStep = 0;
    return;
  }

  uint8_t count = (uint8_t)(patLen - dumpStep);
  if (count > 2) count = 2;

  uint8_t b[Q_LEN];
  uint8_t n = mk(b, CMD_PAT_STEP);
  b[n++] = dumpCh;
  b[n++] = patLen;
  b[n++] = dumpStep;
  b[n++] = count;

  int shift = 12 * seqOctaveShiftOf((uint8_t)(dumpCh + 1)) +
              (int)seqTransOf(dumpCh);
  uint8_t prob = (uint8_t)seqProbOf((uint8_t)(dumpCh + 1));
  uint8_t vel = (uint8_t)seqVelocityOf((uint8_t)(dumpCh + 1));
  uint8_t accVel = (uint8_t)seqAccentVelocityOf((uint8_t)(dumpCh + 1));

  for (uint8_t s = 0; s < count; s++) {
    uint8_t st = (uint8_t)(dumpStep + s);
    uint8_t len = 0;
    const uint8_t *notes = stepChordAtCh((uint8_t)(dumpCh + 1), st, &len);
    if (len > 8) len = 8;
    if ((uint8_t)(n + 5 + len) > Q_LEN) break;   // never split a frame
    b[n++] = len;
    for (uint8_t i = 0; i < len; i++) {
      int p = (int)notes[i] + shift;
      if (p < 0) p = 0;
      if (p > 127) p = 127;
      b[n++] = (uint8_t)p;
    }
    uint8_t attr = stepAttrsOf(dumpCh, st);
    b[n++] = (uint8_t)(attr & 0x0F);
    b[n++] = (uint8_t)((attr & 0x08) ? prob : 100);
    b[n++] = (uint8_t)((attr & 0x04) ? accVel : vel);
  }
  queue(b, n);
  dumpStep = (uint8_t)(dumpStep + count);
}

// ------------------------------------------------------------ host -> dev --
static void parseCmd(const uint8_t *p, uint16_t n) {
  if (n == 0) return;
  lastHostMs = millis();
  // Any tagged frame proves the surface is there: a lost ACK must not lock
  // the link out for good, and the script's TRACK/CLIP_STATE/PARAM burst on
  // connect is just as good a sign of life as the ACK itself.
  if (!alive) {
    alive = true;
    if (modeBase == MODE_ENC) headerShow("Live Link On");
    refreshMainScreen();
  }

  switch (p[0]) {
    case CMD_ACK:
      break;   // liveness is set for any tagged frame, above

    case CMD_TRACK:
      if (n >= 2) {
        uint8_t prev = trkIdx;
        trkIdx = p[1];
        copyText(trkName, sizeof(trkName), p + 2, (uint16_t)(n - 2));
        if (prev != p[1]) linkTrackApply(p[1]);   // only on a real move
      }
      break;

    case CMD_CLIP_STATE:
      if (n >= 6) {
        uint8_t first = p[1];
        for (uint8_t i = 0; i < 4; i++) {
          uint8_t row = (uint8_t)(first + i);
          if (row < 16) clipBits[row] = (uint8_t)(p[2 + i] & 0x0F);
        }
      }
      break;

    case CMD_SEL_ROW:
      if (n >= 2) selRow = (p[1] < 16) ? p[1] : 255;
      break;

    case CMD_PARAM:
      if (n >= 20) {
        uint8_t slot = (uint8_t)(p[1] & 0x07);
        pageCur = p[2];
        pVal[slot] = (uint8_t)(p[3] & 0x7F);
        copyText(pName[slot], sizeof(pName[slot]), p + 5, 8);
        copyText(pText[slot], sizeof(pText[slot]), p + 13, 7);
        pValid[slot] = (pName[slot][0] != '\0');
      }
      break;

    case CMD_PARAM_SHORT:
      if (n >= 3) {
        uint8_t slot = (uint8_t)(p[1] & 0x07);
        pVal[slot] = (uint8_t)(p[2] & 0x7F);
      }
      break;

    case CMD_PAGE:
      if (n >= 3) {
        pageCur = p[1];
        pageCount = p[2];
      }
      break;

    case CMD_BANK:
      copyText(bankName, sizeof(bankName), p + 1, (uint16_t)(n - 1));
      break;

    case CMD_FILL_DONE:
      if (n >= 5) {
        uint16_t notes = (uint16_t)p[3] | (uint16_t)((uint16_t)p[4] << 7);
        linkFillDone(p[1], p[2], notes);
      }
      break;

    default:
      break;   // unknown command: ignore (protocol is versioned forward)
  }
}

// The Teensy USB stack hands a SysEx over with its F0/F7 bytes, but a stripped
// frame (starting at the manufacturer id) is accepted too: the tag is what
// identifies the protocol, not the delimiters
static void handleFrame(const uint8_t *d, uint16_t n) {
  for (uint16_t i = 0; i + 5 < n; i++) {
    uint16_t m = (d[i] == 0xF0) ? (uint16_t)(i + 1) : i;
    if (m + 5 >= n) return;
    if (d[m] != LINK_MFR) continue;
    if (d[m + 1] != LINK_TAG0 || d[m + 2] != LINK_TAG1 ||
        d[m + 3] != LINK_TAG2 || d[m + 4] != LINK_TAG3) continue;
    parseCmd(d + m + 5, (uint16_t)(n - m - 5));
    return;
  }
}

static void onSysEx(const uint8_t *data, uint16_t length, bool complete) {
  if (rxLen + length > RX_MAX) rxLen = 0;         // overflow: drop the frame
  if (rxLen + length <= RX_MAX) {
    memcpy(rxBuf + rxLen, data, length);
    rxLen = (uint16_t)(rxLen + length);
  }
  if (!complete) return;
  handleFrame(rxBuf, rxLen);
  rxLen = 0;
}

// ------------------------------------------------------------------ setup --
void linkInit() {
  usbMIDI.setHandleSysEx(onSysEx);
  rxLen = 0;
  qHead = qTail = 0;
  dumpOn = false;
  dumpMask = 0;
}

void linkPump() {
  flushOne();
  pumpDump();
}

void linkTick() {
  uint32_t now = millis();
  if (alive && (now - lastHostMs) > 2000) {
    alive = false;
    if (modeBase == MODE_ENC) headerShow("Live Link Off");
  }
  // Linked: a slow heartbeat.  Connecting: retry fast for 5 s, then 1 s, so a
  // missing script cannot flood Live's MIDI monitor with HELLO frames.
  uint32_t period = 2000u;
  if (!alive) period = (!helloSent || (now - firstHelloMs) < 5000u) ? 200u : 1000u;
  if (now - lastHelloMs >= period) {
    lastHelloMs = now;
    if (!helloSent) { helloSent = true; firstHelloMs = now; }
    uint8_t b[Q_LEN];
    uint8_t n = mk(b, CMD_HELLO);
    b[n++] = LINK_PROTO_VER;
    queue(b, n);
  }
}
