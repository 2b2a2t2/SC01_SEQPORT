/*********************************************************
SC01 Plus (ESP32-S3, 3.5" 480x320 ST7796 + FT5x06 touch)
Teensy2048 port - step 1: display, step 2: pads + LEDs.

Hardware on the board, brought up here:
  50 WS2812B on GPIO 12          (Leds.cpp)
  4 x MPR121 on Wire, GPIO 10/11 (TouchChips.cpp, 48 pads)
  TCA9548A @0x71 + 8 AS5600      (Encoders.cpp, same 10/11 bus)
  FT5x06 touch on I2C 6/5        (PanelSC01.h)

Boot diagnosis
--------------
A reset reason and the last bring-up stage reached are kept in NVS and shown
on page 0, so a board that reboots tells you where it died without a serial
console. NVS and not RTC memory, on purpose: a brownout clears the RTC domain,
so the failure count restarted at zero on every boot and safe mode could never
engage - which is what made a rebooting board reboot for ever. Each stage is
recorded *before* the step runs, so the value shown names the step that never
returned.

  RESET  power on / brownout / panic / int wdt / task wdt / sw reset
  STAGE  which init step the previous boot last completed
  BOOTS  consecutive boots that did not finish

Two failed boots in a row and the sketch starts in SAFE MODE with the LEDs,
pad controllers and encoders left off, so a fault in any of them cannot hide
the display or the reason. Tap through the pages to confirm it is alive. A boot
that gets all the way through clears the count, so power cycling leaves safe
mode - and if the fault is still there it comes back after two more boots.

Pages (none of them rotate on their own - tap to change):
  0  boot + panel self-test (header tap opens it)
  1  SEQ screen  - knob label/value grid + step-edit F row
  2  ENC screen  - eight CC dials (ring + pointer + value)
  3  KEY screen  - KEYMODE 2 chord row
  4  devices     - what answered on both I2C buses, live pad + strip state

Serial: r = recalibrate MPR121, d = print baselines, e = encoder diag,
        s = scan both I2C buses, L = run the LED sweep again (works in safe mode).
**********************************************************/
#include <Arduino.h>
#include <esp_system.h>
#include <FS.h>
#include <SD.h>
#include <Preferences.h>
#include <SPI.h>
#include <stdlib.h>

#include "Display.h"
#include "Encoders.h"
#include "Leds.h"
#include "PanelSC01.h"
#include "TouchChips.h"

// Panel instance: PanelSC01.h declares it, Display.cpp draws on it.
PanelSC01 tft;

// SD card on the board's SPI header (separate from the panel's parallel bus)
#define SD_CS 41
#define SPI_MOSI 40
#define SPI_MISO 38
#define SPI_SCK 39

#define STATS_MS 250
// ENCMODE redraw cadence. The dial cells used to share STATS_MS, which capped
// the pointers at 4 Hz and made a slow turn look like it was catching up in
// jumps; STATS_MS still paces the device census and the draw-time cell.
#define ENC_UI_MS 33

// --- boot bookkeeping, kept in NVS so it survives a brownout --------------
// RTC_NOINIT was the wrong home for this: a rail that drops far enough to reset
// the chip also loses the RTC domain, so the count came back as zero every time
// and a rebooting board never reached safe mode. NVS is flash and keeps the
// count across anything short of erasing the chip.
#define BOOT_MAGIC 0x53433032u  // "SC02"; bump it when the stage list changes

enum Stage : uint32_t {
  ST_BOOT = 0,
  ST_DISPLAY = 1,
  ST_LEDS = 2,     // addLeds + the first clear
  ST_LEDTEST = 3,  // the one-at-a-time sweep
  ST_PADS = 4,
  ST_ENCODERS = 5,
  ST_DONE = 6
};

static Preferences bootPrefs;
static uint32_t bootStage = ST_BOOT;  // stage stored by the previous boot
static uint32_t bootCount = 0;        // boots in a row that did not finish

static bool safeMode = false;
static uint8_t chipsFound = 0;

static const char* stageName(uint32_t s) {
  switch (s) {
    case ST_DISPLAY: return "display";
    case ST_LEDS: return "leds";
    case ST_LEDTEST: return "ledsweep";
    case ST_PADS: return "pads";
    case ST_ENCODERS: return "encoders";
    case ST_DONE: return "ready";
    default: return "boot";
  }
}

static const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "ext reset";
    case ESP_RST_SW: return "sw reset";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int wdt";
    case ESP_RST_TASK_WDT: return "task wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

// Record the step that is about to run. Writing before the work, not after, is
// what makes the stored value name the step that died: the one in the record is
// the one that never returned. The serial echo is flushed so a panic backtrace
// arrives after it and not instead of it.
static void bootMark(uint32_t s) {
  bootPrefs.putUInt("stage", s);
  Serial.printf("  stage %s\n", stageName(s));
  Serial.flush();
}

// One OLED screen's worth of content.
struct Page {
  const char* header;             // header left label
  const char* note;               // transient announcement on entry
  int8_t mode;                    // highlighted mode button, -1 for none
  bool dials;                     // eight CC dials instead of label/value cells
  const char* legend[LEGEND_COUNT];
  const char* label[CELL_COUNT];
  const char* value[CELL_COUNT];  // the dial value, as text, when dials = true
};

static const Page pages[] = {
    // 0 - boot report + panel self-test (cells 6/7 are live)
    { "BOOT + TEST",
      "DISPLAY ready",
      -1,
      false,
      { "PREV", "NEXT", "M1", "M2", "M3", "M1", "M2", "M3", "M4" },
      { "RESET", "STAGE", "BOOTS", "HEAP", "LEDS", "PADS", "DRAW", "PRESS" },
      { "--", "--", "--", "--", "--", "--", "--", "idle" } },

    // 1 - SEQMODE, MONO track with the step-edit F row
    { "SEQMODE",
      "GATE 75%",
      1,
      false,
      { "PREV", "NEXT", "EDIT", "ARP", "GEN", "TIE", "GLIDE", "ACC", "PROB" },
      { "OCT", "GATE", "VEL", "PROB", "GLIDE", "ACC", "TIE", "---" },
      { "0", "75%", "100", "90%", "30%", "20", "40%", "" } },

    // 2 - ENCMODE, the eight CC knobs as dials
    { "ENCMODE",
      "CC 74",
      0,
      true,
      { "PREV", "NEXT", "M1", "M2", "M3", "F1", "F2", "F3", "F4" },
      { "CC74", "CC71", "CC91", "CC93", "CC1", "CC11", "CC21", "CC31" },
      { "64", "100", "40", "20", "0", "127", "32", "96" } },

    // 3 - KEYMODE 2 chord row
    { "KEYMODE 2",
      "SCALE MINOR",
      2,
      false,
      { "PREV", "Sus", "Maj", "Min", "Dim", "6", "m7", "M7", "9" },
      { "OCT", "SCALE", "ROOT", "FSCL", "BASS", "SPREAD", "DENS", "LATCH" },
      { "3", "MINOR", "C", "ON", "40", "60", "50", "OFF" } },

    // 4 - device census: what answered on 10/11 and 6/5, plus live pad state.
    // Cells 0..3, 6 and 7 are overwritten by updatePadCells(); the F row of the
    // legend carries the four chip touch masks.
    { "DEVICES",
      "10/11 + 6/5",
      -1,
      false,
      { "PREV", "NEXT", "RECAL", "M2", "M3", "", "", "", "" },
      { "MPR121", "MUX", "AS5600", "DOWN", "PAD I2C", "LEDS", "TOUCH 6/5", "DRAW" },
      { "--", "--", "--", "0", "10/11", "50", "--", "--" } },
};

static const uint8_t PAGE_COUNT = sizeof(pages) / sizeof(pages[0]);
// Mode button (ENC, SEQ, KEY) -> page index
static const uint8_t MODE_PAGE[MODE_COUNT] = {2, 1, 3};

// Next page when the user taps to advance, wrapping to page 1: page 0 (the
// boot report) is never part of the sequence, because it is opened on purpose
// from the header and has nothing to cycle to.
static uint8_t nextPage(uint8_t p) {
  const uint8_t n = p + 1;
  return n >= PAGE_COUNT ? 1 : n;
}

static uint8_t page = 0;
static unsigned long lastStats = 0;
static unsigned long lastEncUi = 0;
static unsigned long worstDrawUs = 0;
static unsigned long lastEncPoll = 0;
static bool lastPressed = false;
static bool touchSeen = false;  // a press has been read since boot

static void applyPage(uint8_t p) {
  const Page& pg = pages[p];
  headerSetLeft(pg.header);
  headerShow(pg.note);
  displaySetActiveMode(pg.mode < 0 ? MODE_NONE : (uint8_t)pg.mode);

  for (uint8_t i = 0; i < LEGEND_COUNT; i++) displaySetLegend(i, pg.legend[i]);
  for (uint8_t i = 0; i < CELL_COUNT; i++) {
    if (pg.dials) displaySetCellDial(i, pg.label[i], (uint8_t)atoi(pg.value[i]));
    else displaySetCell(i, pg.label[i], pg.value[i]);
  }
  refreshMainScreen();
}

static void showPage(uint8_t p) {
  page = p % PAGE_COUNT;
  applyPage(page);
  if (page == 0) updateBootCells();
}

static void updateBootCells() {
  char buf[16];

  displaySetCellValue(0, resetName(esp_reset_reason()));
  displaySetCellValue(1, stageName(bootStage > ST_DONE ? ST_BOOT : bootStage));
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)bootCount);
  displaySetCellValue(2, buf);
  snprintf(buf, sizeof(buf), "%uK", (unsigned)(ESP.getFreeHeap() / 1024));
  displaySetCellValue(3, buf);
  displaySetCellValue(4, safeMode ? "safe" : "on");
  if (safeMode) displaySetCellValue(5, "safe");
  else {
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)chipsFound, (unsigned)NUM_CHIPS);
    displaySetCellValue(5, buf);
  }
}

// Page 0: draw cost and touch coordinates
static void updateTestCells(bool pressed, uint16_t tx, uint16_t ty) {
  char buf[16];

  snprintf(buf, sizeof(buf), "%lu ms", (worstDrawUs + 500) / 1000);
  displaySetCellValue(6, buf);

  if (pressed) snprintf(buf, sizeof(buf), "%u,%u", (unsigned)tx, (unsigned)ty);
  else snprintf(buf, sizeof(buf), "idle");
  displaySetCellValue(7, buf);
}

// Page 4: what answered on both I2C buses, then the live pad state. The three
// chip touch masks move to the F row so the grid can name each device.
static void updatePadCells() {
  char buf[16];

  snprintf(buf, sizeof(buf), "%u/%u", (unsigned)chipsFound, (unsigned)NUM_CHIPS);
  displaySetCellValue(0, buf);
  // The mux address is a strap, so show the one that actually answered.
  if (encMuxPresent()) snprintf(buf, sizeof(buf), "0x%02X", encMuxAddr());
  else snprintf(buf, sizeof(buf), "--");
  displaySetCellValue(1, buf);
  snprintf(buf, sizeof(buf), "%u/%u", (unsigned)encodersFound(), (unsigned)NUM_ENCS);
  displaySetCellValue(2, buf);
  snprintf(buf, sizeof(buf), "%u", (unsigned)padsDown());
  displaySetCellValue(3, buf);

  if (safeMode)
    displaySetCellValue(6, "safe");
  else
    displaySetCellValue(6, touchSeen ? "ok" : "none");

  snprintf(buf, sizeof(buf), "%lu ms", (worstDrawUs + 500) / 1000);
  displaySetCellValue(7, buf);

  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    snprintf(buf, sizeof(buf), "C%u %03X", (unsigned)(i + 1), (unsigned)currTouched[i]);
    displaySetLegend((uint8_t)(5 + i), buf);
  }
}

// Page 2: the eight CC dials follow the real encoders once they answer. Until
// then the page's static table keeps the layout visible without hardware.
// displaySetCellDial() already skips a cell whose content is unchanged, so
// there is deliberately no "last shown" cache here: one kept drawing the
// previous live value over applyPage()'s table after returning to ENCMODE,
// because applyPage() does not know what the cache holds.
static void updateEncoderDials() {
  if (encodersFound() == 0) return;

  for (uint8_t i = 0; i < NUM_ENCS; i++)
    displaySetCellDial(i, pages[2].label[i], encOk[i] ? encValue[i] : 0);
}

// 's' - every address that answers, on both I2C buses. The TCA9548A address is
// set by the A0..A2 straps, so a board strapped differently (or a mux on the
// panel bus) shows up here instead of only as "not found at 0x70".
static void i2cScanBus(TwoWire& bus, const char* name) {
  Serial.printf("i2c %s:", name);
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    bus.beginTransmission(a);
    if (bus.endTransmission() != 0) continue;
    Serial.printf(" 0x%02X", a);
    n++;
  }
  Serial.println(n ? "" : "  (nothing)");
}

static void i2cScan() {
  i2cScanBus(Wire, "10/11 pad bus");   // MPR121 + mux + AS5600
  i2cScanBus(Wire1, "6/5  panel bus"); // FT5x06, in case the mux landed here
}

// 'L' - bring the strip up and run the sweep on demand. In safe mode this is
// the only way to touch the LEDs, which is what makes it worth having: if the
// board resets here, the strip or its driver is the fault and not the pads or
// the encoders. initLeds() is idempotent, so pressing it twice is harmless.
static void ledSelfTest() {
  bootMark(ST_LEDTEST);
  initLeds();
  ledStartupTest();
  bootMark(ST_DONE);
  Serial.println("led test: done");
}

static void handleSerial() {
  while (Serial.available()) {
    const int c = Serial.read();
    if (c == 'r' || c == 'R') touchRecalibrate();
    else if (c == 'd' || c == 'D') touchPrintDiag();
    else if (c == 'e' || c == 'E') encPrintDiag();
    else if (c == 's' || c == 'S') i2cScan();
    else if (c == 'l' || c == 'L') ledSelfTest();
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);

  // Did the previous run get all the way through bring-up?
  bootPrefs.begin("sc01boot", false);
  const bool known = bootPrefs.getUInt("magic", 0) == BOOT_MAGIC;
  bootStage = known ? bootPrefs.getUInt("stage", ST_BOOT) : ST_BOOT;
  bootCount = known ? bootPrefs.getUInt("boots", 0) : 0;
  if (known && bootStage != ST_DONE) bootCount++;
  else bootCount = 0;
  safeMode = known && bootCount >= 2;
  bootPrefs.putUInt("magic", BOOT_MAGIC);
  bootPrefs.putUInt("boots", bootCount);

  Serial.print("boot ");
  Serial.print(bootCount);
  Serial.print(" reset=");
  Serial.print(resetName(esp_reset_reason()));
  Serial.print(" last stage=");
  Serial.println(stageName(bootStage > ST_DONE ? ST_BOOT : bootStage));
  if (safeMode) Serial.println("SAFE MODE: LEDs and pads disabled");

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  bool sdOk = SD.begin(SD_CS);
  (void)sdOk;

  bootMark(ST_DISPLAY);
  initDisplay();
  displaySetModes("ENC", "SEQ", "KEY");
  if (safeMode) headerSetStick("SAFE MODE");
  showPage(0);

  if (!safeMode) {
    bootMark(ST_LEDS);
    initLeds();
    bootMark(ST_LEDTEST);
    ledStartupTest();

    bootMark(ST_PADS);
    initTouchChips();
    chipsFound = touchChipsFound();
    if (chipsFound < NUM_CHIPS)
      Serial.printf("MPR121: %u of %u answered - check 10/11 and the ADDR straps\n",
                    (unsigned)chipsFound, (unsigned)NUM_CHIPS);

    i2cScan();  // show what the encoder probe is about to look for
    bootMark(ST_ENCODERS);
    initEncoders();
  }

  updateBootCells();
  bootPrefs.putUInt("boots", 0);  // a full bring-up clears the failure count
  bootMark(ST_DONE);
}

void loop() {
  if (!safeMode) {
    pollTouchChips();
    ledPadFeedback();
    if (millis() - lastEncPoll >= ENC_POLL_MS) {
      lastEncPoll = millis();
      pollEncoders();
    }
  }
  handleSerial();

  uint16_t tx = 0;
  uint16_t ty = 0;
  const bool pressed = tft.getTouch(&tx, &ty);
  if (pressed) touchSeen = true;

  if (pressed && !lastPressed) {
    // Header: open the boot report - the one page nothing else can reach.
    // Mode row: jump straight to that screen. Elsewhere: step to the next page.
    if (ty < HEADER_H)
      showPage(0);
    else if (ty >= MODE_Y && ty < MODE_Y + MODE_H)
      showPage(MODE_PAGE[tx / (SCREEN_W / MODE_COUNT)]);
    else
      showPage(nextPage(page));
  }
  lastPressed = pressed;

  // No automatic page cycling: the panel stays wherever it was left until the
  // user taps. A hand resting on the board therefore cannot flip the screen
  // mid-test, and a page left up keeps being updated instead of expiring.

  displaySetTouch(pressed, tx, ty);

  unsigned long t0 = micros();
  displayUpdate();
  const unsigned long us = micros() - t0;
  if (us > worstDrawUs) worstDrawUs = us;

  const bool statsDue = millis() - lastStats >= STATS_MS;
  if (statsDue) lastStats = millis();
  const bool encUiDue = millis() - lastEncUi >= ENC_UI_MS;
  if (encUiDue) lastEncUi = millis();

  if (page == 0) {
    updateTestCells(pressed, tx, ty);
  } else if (page == 2) {
    if (encUiDue) updateEncoderDials();
  } else if (page == 4 && statsDue) {
    updatePadCells();
  }
  if (statsDue) worstDrawUs = 0;
}
