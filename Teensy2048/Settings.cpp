#include "Config.h"
#include "Settings.h"

#include "Buttons.h"     // ledsShow()
#include "Display.h"     // headerShow / headerAnnounceInt / refreshMainScreen
#include "Encoders.h"    // encDrain - 1 count = 1 %
#include "Events.h"      // cfgStoreCommit()
#include "Modes.h"       // modeBase / modeVariant: a mode change closes the page
#include "Screen.h"      // scrSetBrightness / scrTapHeader
#include "SeqClock.h"    // seqIsSyncInternal / seqSetSyncInternal

#include <EEPROM.h>
#include <stdio.h>

// The store: magic | LED % | screen % | checksum = 4 bytes at 2184, the end
// of the block CFG_EE_BYTES (2200) opens.  A blank or corrupt record leaves
// the boot defaults, exactly like the other two blocks.
#define DEV_EE_ADDR   2184
#define DEV_EE_MAGIC  0xD5
#define DEV_EE_LEN    4

static bool pageUp = false;
static ModeBase openBase = MODE_NONE;   // the screen the page was opened on
static uint8_t openVariant = 0;

static uint8_t ledPct = 10;             // 10 % == the strip's usual 25/255
static uint8_t scrPct = 100;            // full backlight

static void applyLedBrightness() {
  FastLED.setBrightness((uint8_t)((uint16_t)ledPct * 255 / 100));
  ledsShow();                           // otherwise it lands on the next frame
}

static void applyScrBrightness() { scrSetBrightness(scrPct); }

static void settingsSave() {
  uint8_t b[DEV_EE_LEN];
  b[0] = DEV_EE_MAGIC;
  b[1] = ledPct;
  b[2] = scrPct;
  b[3] = (uint8_t)(b[0] + b[1] + b[2]);
  EEPROM.put(DEV_EE_ADDR, b);   // put() writes only the bytes that changed
  cfgStoreCommit();             // ...and commit() is what reaches NVS
}

static void settingsLoad() {
  uint8_t b[DEV_EE_LEN];
  EEPROM.get(DEV_EE_ADDR, b);
  if (b[0] != DEV_EE_MAGIC) return;
  if ((uint8_t)(b[0] + b[1] + b[2]) != b[3]) return;
  if (b[1] <= 100) ledPct = b[1];
  if (b[2] <= 100) scrPct = b[2];
}

void settingsBegin() {
  settingsLoad();
  applyLedBrightness();
  applyScrBrightness();
}

bool settingsOpen() { return pageUp; }

// Leaving the page restores the screen it covered: the header recomputes its
// left slot (mode / open page) and the main band redraws from the live
// bindings, so nothing of the page is left behind.
static void settingsClose() {
  pageUp = false;
  refreshHeader();
  refreshMainScreen();
}

void settingsTouchPoll() {
  // a mode (or variant) change takes the page down with it
  if (pageUp && (modeBase != openBase || modeVariant != openVariant)) {
    settingsClose();
    return;
  }
  if (!scrTapHeader()) return;
  if (pageUp) {
    settingsClose();
    return;
  }
  pageUp = true;
  openBase = modeBase;
  openVariant = modeVariant;
  headerShow("Tap Header to Close");
  refreshHeader();
  refreshMainScreen();
}

void settingsAdjust(uint8_t ei, int16_t counts) {
  if (ei == 0) {                            // LED brightness
    static int16_t res = 0;
    int16_t d = encDrain(counts, &res);
    if (!d) return;
    int v = (int)ledPct + d;
    ledPct = (uint8_t)(v < 0 ? 0 : (v > 100 ? 100 : v));
    applyLedBrightness();
    headerAnnounceInt("LED Brightness", ledPct, "%");
    settingsSave();
    refreshMainScreen();
  } else if (ei == 1) {                     // screen brightness
    static int16_t res = 0;
    int16_t d = encDrain(counts, &res);
    if (!d) return;
    int v = (int)scrPct + d;
    scrPct = (uint8_t)(v < 0 ? 0 : (v > 100 ? 100 : v));
    applyScrBrightness();
    headerAnnounceInt("Screen Brightness", scrPct, "%");
    settingsSave();
    refreshMainScreen();
  } else if (ei == 4) {                     // clock source
    // two-state, NO wrap (CW = EXTERNAL, CCW = INTERNAL), like SEQMODE3 ENC4
    bool wantInternal = (counts < 0);
    if (wantInternal == seqIsSyncInternal()) return;
    seqSetSyncInternal(wantInternal);
    headerShow(wantInternal ? "Clock Internal" : "Clock External");
    refreshMainScreen();
  }
}

void settingValueText(uint8_t cell, char* buf, uint8_t n) {
  switch (cell) {
    case 0: snprintf(buf, (size_t)n, "%u%%", (unsigned)ledPct); break;
    case 1: snprintf(buf, (size_t)n, "%u%%", (unsigned)scrPct); break;
    case 4:
      snprintf(buf, (size_t)n, "%s",
               seqIsSyncInternal() ? "INTERNAL" : "EXTERNAL");
      break;
    default: buf[0] = '\0'; break;
  }
}
