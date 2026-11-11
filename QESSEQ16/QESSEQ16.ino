/*********************************************************
Teensy 2048 - hardware bring-up for the two boards it runs on

  Teensy 4.1  - the original target: SSD1306 128x64 header display, 42 LEDs,
                IRQ-driven MPR121 pads, quadrature encoders
  SC01 Plus   - ESP32-S3 panel: 480x320 ST7796, 50 LEDs, polled MPR121 pads,
                AS5600 knobs behind a TCA9548A mux

Everything except the hardware layer is Teensy2048's own code.

Boot diagnosis (SC01)
---------------------
A board that resets before the panel is up tells you nothing, and one that
resets after it is up shows the screen and then goes black - which is exactly
where this port started. So the bring-up stage reached is written to NVS
*before* each step runs, and shown on the panel by the next boot: the stored
value names the step that never returned.

  RESET   power on / brownout / panic / int wdt / task wdt / sw reset
  STAGE   which bring-up step the previous boot last started
  BOOTS   consecutive boots that did not reach ST_STABLE

A boot only settles after loop() has run for BOOT_SETTLE_MS, so a board that
gets the panel up and still resets is counted too, and "setup done" as the
stage says the fault is in loop() and not in bring-up. Two failed boots in a
row start SAFE MODE with the strip, the knobs and the pads left off, so a
fault in any of them cannot hide the panel. A boot that settles clears the
count, so power cycling leaves safe mode - and if the fault is still there it
comes back after two more boots.

NVS, not RTC memory, on purpose: a rail that sags far enough to reset the
chip also loses the RTC domain, so the count came back as zero every time and
a rebooting board never reached safe mode.

Serial (115200): "recal" + Enter = force MPR121 recalibration,
                 "diag"  + Enter = print electrode baselines.
**********************************************************/
#include <Wire.h>

#include "Config.h"
#include "UsbMidiShim.h"     // usbMIDI: the Teensy core's own, or the SC01 facade
#include "Buttons.h"
#include "TouchChips.h"
#include "Display.h"
#include "Encoders.h"
#include "Modes.h"
#include "Events.h"
#include "SeqClock.h"
#include "Keyboard.h"
#include "Link.h"        // ENCABLETONMODE bridge (SysEx with the remote script)
#include "Settings.h"    // device settings page (header tap)

#ifdef use_SC01Plus
#include <Preferences.h>
#include <esp_system.h>

// --- boot bookkeeping ------------------------------------------------------
#define BOOT_MAGIC     0x53303150u  // "S01P"; bump it when the stage list changes
#define BOOT_SETTLE_MS 5000UL       // loop() ran this long = the boot is good
#define BOOT_REPORT_MS 3000UL       // the boot page then hands over to the UI

enum Stage : uint32_t {
  ST_BOOT = 0,
  ST_DISPLAY,   // panel init + first frame
  ST_STRIP,     // LED driver + the pad->LED map
  ST_ENCODERS,  // mux probe + AS5600 census
  ST_MODES,     // mode layer, LED hook, first mode screen
  ST_CLOCK,     // uClock + USB/DIN MIDI handlers
  ST_LINK,      // ENCABLETONMODE SysEx handler
  ST_PADS,      // MPR121 begin + autoconfig
  ST_READY,     // setup() returned
  ST_STABLE     // setup() returned and loop() ran for BOOT_SETTLE_MS
};

static Preferences bootPrefs;
static uint32_t bootStage = ST_BOOT;   // the stage stored by the previous boot
static uint32_t bootCount = 0;         // boots in a row that did not settle
static bool safeMode = false;
static bool bootDisplayUp = false;     // a report needs a live panel
static bool bootSettled = false;
static bool uiHandedOff = false;
static uint32_t bootStartMs = 0;

static const char* stageName(uint32_t s) {
  switch (s) {
    case ST_DISPLAY:  return "display";
    case ST_STRIP:    return "strip";
    case ST_ENCODERS: return "encoders";
    case ST_MODES:    return "modes";
    case ST_CLOCK:    return "clock";
    case ST_LINK:     return "link";
    case ST_PADS:     return "pads";
    case ST_READY:    return "setup done";
    case ST_STABLE:   return "stable";
    default:          return "boot";
  }
}

static const char* resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "power on";
    case ESP_RST_EXT:       return "ext reset";
    case ESP_RST_SW:        return "sw reset";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "int wdt";
    case ESP_RST_TASK_WDT:  return "task wdt";
    case ESP_RST_WDT:       return "wdt";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
  }
}

// The boot page: reset reason, the stage the previous boot died in, the
// failure count, the heap, and what the hardware layer has answered so far.
static void bootReport(const char* note) {
  if (!bootDisplayUp) return;
  char boots[8];
  char heap[8];
  char devices[24];
  snprintf(boots, sizeof(boots), "%lu", (unsigned long)bootCount);
  snprintf(heap, sizeof(heap), "%uK", (unsigned)(ESP.getFreeHeap() / 1024));
  if (safeMode)
    snprintf(devices, sizeof(devices), "off (safe)");
  else
    snprintf(devices, sizeof(devices), "%u/%u answered",
             (unsigned)touchChipsFound(), (unsigned)NUM_CHIPS);
  displayShowBoot(safeMode, resetName(esp_reset_reason()),
                  stageName(bootStage), boots, heap, devices, note);
}

// Record the step that is about to run. Writing before the work, not after,
// is what makes the stored value name the step that died: the one in the
// record is the one that never returned. The serial echo goes out first so a
// panic backtrace arrives after it and not instead of it.
static void bootMark(uint32_t s) {
  bootStage = s;
  bootPrefs.putUInt("stage", s);
  Serial.printf("  stage %s  heap %uK\n", stageName(s),
                (unsigned)(ESP.getFreeHeap() / 1024));
  Serial.flush();
  bootReport(stageName(s));
}

// A boot is only good once loop() has run for BOOT_SETTLE_MS: ST_READY means
// the panel came up and the board still did not survive, which is a loop()
// fault and not a bring-up one.
static void bootSettle() {
  if (bootSettled || millis() - bootStartMs < BOOT_SETTLE_MS) return;
  bootSettled = true;
  bootStage = ST_STABLE;
  bootPrefs.putUInt("stage", ST_STABLE);
  bootPrefs.putUInt("boots", 0);
  // stack watermark of the task loop() runs in: the number that says whether
  // a later crash is the loop task running out of stack
  Serial.printf("boot: stable  heap %uK  stk %u\n",
                (unsigned)(ESP.getFreeHeap() / 1024),
                (unsigned)uxTaskGetStackHighWaterMark(NULL));
}

// The boot page hands the panel to the normal UI a few seconds after bring-up.
static void bootHandoff() {
  if (uiHandedOff || millis() - bootStartMs < BOOT_REPORT_MS) return;
  uiHandedOff = true;
  refreshHeader();
  refreshMainScreen();
}

static void setupSc01() {
  Serial.begin(115200);
  delay(50);
  bootStartMs = millis();

  bootPrefs.begin("sc01boot", false);
  const bool known = bootPrefs.getUInt("magic", 0) == BOOT_MAGIC;
  bootStage = known ? bootPrefs.getUInt("stage", ST_BOOT) : ST_BOOT;
  bootCount = known ? bootPrefs.getUInt("boots", 0) : 0;
  if (known && bootStage != ST_STABLE) bootCount++;
  else bootCount = 0;
  safeMode = known && bootCount >= 2;
  bootPrefs.putUInt("magic", BOOT_MAGIC);
  bootPrefs.putUInt("boots", bootCount);

  Serial.printf("boot %lu reset=%s last stage=%s heap=%uK\n",
                (unsigned long)bootCount, resetName(esp_reset_reason()),
                stageName(bootStage), (unsigned)(ESP.getFreeHeap() / 1024));
  if (safeMode) Serial.println("SAFE MODE: strip, knobs and pads left off");

  // starts TinyUSB: the MIDI interface plus CDC so Serial keeps working;
  // needs the board option "USB Mode = USB-OTG (TinyUSB)" (see UsbMidiShim.cpp)
  usbMIDI.begin();
  Serial.println("SC01 Plus - Teensy2048");

  // The pad bus is GPIO 10/11: the core's default Wire pins (8/9) are two of
  // the panel's data lines, so the bus has to move before anything uses it.
  Wire.setPins(MPR_SDA, MPR_SCL);
  Wire.begin(MPR_SDA, MPR_SCL, MPR_FREQ);
  Wire.setTimeOut(20);

  // Settings first: the first draw reads the restored knobs. Patterns stay
  // in RAM, as on the Teensy.  cfgStoreBegin() opens the NVS-backed EEPROM
  // before anything reads it - without it the loaders see zeros and every
  // save is silently dropped.
  cfgStoreBegin();
  encCcLoad();
  seqCfgLoad();

  // Display first: a fault in anything below must not be able to hide the
  // panel, and the boot page is the only place the board can report itself
  bootMark(ST_DISPLAY);
  initDisplay();
  bootDisplayUp = true;
  bootReport("display");

  if (!safeMode) {
    bootMark(ST_STRIP);
    initButtons();          // strip driver + pad -> LED map
  }

  bootMark(ST_ENCODERS);
  if (!safeMode) initEncoders();

  // initModes() installs the LED hook and paints the first screen; the pads
  // are only polled, so a missing controller costs its own pads and nothing
  // else
  bootMark(ST_MODES);
  initModes();
  settingsBegin();        // stored LED / screen brightness, applied at boot

  bootMark(ST_CLOCK);
  initSeqClock();

  bootMark(ST_LINK);
  linkInit();

  bootMark(ST_PADS);
  if (!safeMode) {
    // Missing chips used to hang here with while(1), which looked exactly
    // like the fault it was meant to report: the panel kept its last frame
    // and nothing said why.
    if (!initTouchChips())
      Serial.printf("MPR121: %u of %u answered - check 10/11 and the ADDR "
                    "straps\n",
                    (unsigned)touchChipsFound(), (unsigned)NUM_CHIPS);
  }

  bootMark(ST_READY);
  bootReport(safeMode ? "safe mode" : "ready");
}
#endif  // use_SC01Plus

#ifdef use_Teensy
static void setupTeensy() {
  Serial.begin(115200);
  Serial.println("Teensy 4x MPR121 Touch");

  Wire.begin();
  Wire.setClock(400000);

  initButtons();
  cfgStoreBegin();  // no-op on the Teensy, kept for one boot order
  encCcLoad();      // BEFORE initDisplay: the first draw reads the knobs
  seqCfgLoad();     // SEQ settings restore with them (patterns stay RAM)
  initDisplay();
  initEncoders();
  initModes();
  settingsBegin();  // stored LED / screen brightness, applied at boot
  initSeqClock();
  linkInit();       // ENCABLETONMODE bridge: SysEx handler + send queue

  if (!initTouchChips()) while (1);
}
#endif  // use_Teensy

void setup() {
#ifdef use_SC01Plus
  setupSc01();
#else
  setupTeensy();
#endif
}

void loop() {
#ifdef use_SC01Plus
  bootSettle();     // 5 s of loop() = this boot counts as good
  bootHandoff();    // the boot page makes way for the UI after 3 s
#endif

  // pump USB MIDI FIRST: clockMe() only runs when usbMIDI.read() runs.
  // starving it freezes uClock's external phase-lock (the "green dot freeze").
  uint32_t t0 = micros();
  pollSeqClock();
  uint32_t t1 = micros();
  seqDiagNote(0, t1 - t0);

  pollTouchChips();
  btnUpdate();
  pollEncoders();
  settingsTouchPoll();   // header tap opens / closes the device settings page
  pollModeLeds();
  uint32_t t2 = micros();
  seqDiagNote(1, t2 - t1);

  handleButtonEvents();
  handleEncoderEvents();
  uint32_t t3 = micros();
  seqDiagNote(2, t3 - t2);

  // drain ticks queued during touch/Serial/LED work, THEN draw
  pollSeqClock();
  harmRebakePoll();   // HARM tracks re-bake after feeder changes: the draw
                      // below shows the re-derived steps (Keyboard.cpp)
  displayUpdate();
  uint32_t t4 = micros();
  seqDiagNote(3, t4 - t3);

  // Serial command: type "recal" + Enter to force MPR121 recalibration
  //                type "diag"  + Enter to print electrode baselines
  static String serialBuf = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialBuf.trim();
      if (serialBuf == "recal") {
        touchRecalibrate();
      } else if (serialBuf == "diag") {
        touchPrintDiag();
      }
      serialBuf = "";
    } else {
      serialBuf += c;
    }
  }
}
