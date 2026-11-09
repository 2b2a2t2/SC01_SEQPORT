#include "Config.h"
#include "TouchChips.h"
#include "Buttons.h"
#include <Wire.h>

Adafruit_MPR121 chips[NUM_CHIPS];
uint16_t lastTouched[NUM_CHIPS] = {0, 0, 0, 0};
uint16_t currTouched[NUM_CHIPS] = {0, 0, 0, 0};

// which chips actually answered at boot; the helpers below skip the rest, so
// a missing chip costs its own 12 pads and nothing else
static bool chipOk[NUM_CHIPS] = {false, false, false, false};

static const uint8_t i2cAddrs[NUM_CHIPS] = { 0x5A, 0x5B, 0x5C, 0x5D };

// the report the boot page and the serial log both read: a short chip count
// is what a missing ADDR strap or a dead bus looks like
uint8_t touchChipsFound() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < NUM_CHIPS; i++)
    if (chipOk[i]) n++;
  return n;
}

#ifdef use_Teensy
static const uint8_t irqPins[NUM_CHIPS] = {
  MPR121_IRQ1, MPR121_IRQ2, MPR121_IRQ3, MPR121_IRQ4
};

static volatile bool chipIrq[NUM_CHIPS] = {false, false, false, false};
static void isrChip0() { chipIrq[0] = true; }
static void isrChip1() { chipIrq[1] = true; }
static void isrChip2() { chipIrq[2] = true; }
static void isrChip3() { chipIrq[3] = true; }
static void (*isrs[NUM_CHIPS])() = { isrChip0, isrChip1, isrChip2, isrChip3 };
#endif

static bool beginChip(Adafruit_MPR121 &chip, uint8_t addr) {
  if (!chip.begin(addr, &Wire)) {
    Serial.print("MPR121 not found at 0x");
    Serial.println(addr, HEX);
    return false;
  }
  chip.setAutoconfig(true);
  chip.setThresholds(TOUCH_THRESH, TOUCH_REL);
  return true;
}

static void printBaselines() {
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;
    Serial.print("Chip "); Serial.print(i); Serial.print(" baselines: ");
    for (uint8_t e = 0; e < 12; e++) {
      uint16_t bl = chips[i].baselineData(e);
      Serial.print(bl);
      if (e < 11) Serial.print(",");
    }
    Serial.println();
  }
}

// A pad whose baseline never came up (below 64 = register < 16, while a
// healthy resting pad sits near 520..800) reports nothing - the chip ACKs
// fine, its buttons are just dead
static bool chipBaselinesSane(uint8_t i) {
  for (uint8_t e = 0; e < 12; e++)
    if (chips[i].baselineData(e) < 64) return false;
  return true;
}

// stop -> autoconfig + thresholds -> restart, and clear a stale-low IRQ
// line by reading the touch status once
static void recalibrateChip(uint8_t i) {
  chips[i].writeRegister(0x5E, 0x00);  // ECR = 0 (stop mode)
  delay(10);
  chips[i].setAutoconfig(true);
  chips[i].setThresholds(TOUCH_THRESH, TOUCH_REL);
  chips[i].writeRegister(0x5E, 0x8C);  // ECR = 0x8C (CL=5, 12 electrodes)
  currTouched[i] = chips[i].touched();
#ifdef use_Teensy
  chipIrq[i] = false;
#endif
}

bool initTouchChips() {
#ifdef use_SC01Plus
  uint8_t found = 0;
  // setPins first: Adafruit_MPR121::begin() calls the Wire object's begin(),
  // and without stored pins that would reopen the bus on the core's default
  // SDA/SCL - GPIO 8 and 9 here, which are two of the panel's data lines.
  Wire.setPins(MPR_SDA, MPR_SCL);
  Wire.begin(MPR_SDA, MPR_SCL, MPR_FREQ);
  Wire.setTimeOut(20);

  // Every address is tried: a chip with an unset ADDR strap must not stop
  // the others from working, and the missing one is what the report names.
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    chipOk[i] = beginChip(chips[i], i2cAddrs[i]);
    if (chipOk[i]) {
      found++;
      Serial.print("MPR121 #");
      Serial.print(i + 1);
      Serial.print(" OK at 0x");
      Serial.println(i2cAddrs[i], HEX);
    }
  }
  if (found == 0) return false;
#else
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!beginChip(chips[i], i2cAddrs[i])) return false;
    chipOk[i] = true;
    Serial.print("MPR121 #");
    Serial.print(i + 1);
    Serial.println(" OK");
  }
#endif

  // Wait for autoconfig to stabilize baselines on all electrodes.
  // Autoconfig runs asynchronously after begin(); polling before it
  // completes leaves some electrodes at baseline 0 (never touch).
  delay(500);

  // Diagnostic: check baseline data for all electrodes
  printBaselines();

  // A chip can pass begin() yet come up with dead baselines (autoconfig
  // race - the last chip started loses this race most often).  Re-run
  // autoconfig per bad chip, bounded so boot can never hang.
  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    bool anyBad = false;
    for (uint8_t i = 0; i < NUM_CHIPS; i++) {
      if (!chipOk[i] || chipBaselinesSane(i)) continue;
      anyBad = true;
      Serial.print("MPR121 #"); Serial.print(i + 1);
      Serial.println(" bad baseline -> recalibrating");
      recalibrateChip(i);
    }
    if (!anyBad) break;
    delay(500);
    printBaselines();
  }

#ifdef use_Teensy
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    pinMode(irqPins[i], INPUT_PULLUP);
  }

  // The IRQ line stays LOW until the touch status register is read.  Any
  // status change during the settle leaves it low, and attaching on
  // FALLING would then never fire again - the whole chip goes silent for
  // the session.  Read once to clear it; the value lands in currTouched so
  // the first poll still reports a pad that is already held down.
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (digitalRead(irqPins[i]) == LOW) {
      Serial.print("MPR121 #"); Serial.print(i + 1);
      Serial.println(" IRQ low at boot -> cleared");
    }
    chipIrq[i] = false;
    currTouched[i] = chips[i].touched();
  }

  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    attachInterrupt(digitalPinToInterrupt(irqPins[i]), isrs[i], FALLING);
  }
  return true;
#else
  // Same boot state as the IRQ path: read every chip once so a pad already
  // held down is reported by the first poll instead of being swallowed.
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    currTouched[i] = chipOk[i] ? chips[i].touched() : 0;
  }
  return found == NUM_CHIPS;
#endif
}

void pollTouchChips() {
#ifdef use_Teensy
  // heartbeat: recovers a chip whose edge was lost (also unsticks a line
  // left low, since reading the status clears it)
  static uint32_t lastBeat = 0;
  uint32_t now = millis();
  bool beat = (now - lastBeat) >= IRQ_HEARTBEAT_MS;
  if (beat) lastBeat = now;
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;
    if (!chipIrq[i] && !beat && digitalRead(irqPins[i]) != LOW) continue;
    chipIrq[i] = false;
    currTouched[i] = chips[i].touched();
    // Diagnostic: print touch status when changed
    if (currTouched[i] != lastTouched[i]) {
      Serial.print("Chip "); Serial.print(i); Serial.print(" touched: 0x");
      Serial.println(currTouched[i], HEX);
    }
    handleTouches(i, currTouched[i], lastTouched[i]);
  }
#else
  // no IRQ line on this board: poll every TOUCH_POLL_MS and act only on a
  // change, which is what the IRQ edge did on the Teensy
  static uint32_t lastPoll = 0;
  uint32_t now = millis();
  if (now - lastPoll < TOUCH_POLL_MS) return;
  lastPoll = now;

  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;   // a dead chip would just burn I2C timeouts
    currTouched[i] = chips[i].touched();
    if (currTouched[i] == lastTouched[i]) continue;
    // Diagnostic: print touch status when changed
    Serial.print("Chip "); Serial.print(i); Serial.print(" touched: 0x");
    Serial.println(currTouched[i], HEX);
    handleTouches(i, currTouched[i], lastTouched[i]);
  }
#endif
}

void touchRecalibrate() {
  Serial.println("Forcing MPR121 recalibration...");
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;
    recalibrateChip(i);
    Serial.print("MPR121 #"); Serial.print(i + 1); Serial.println(" recalibrated");
  }
  delay(500);
  // Print baselines after recalibration
  printBaselines();
}

void touchPrintDiag() {
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;
    Serial.print("Chip "); Serial.print(i); Serial.print(" baselines: ");
    for (uint8_t e = 0; e < 12; e++) {
      uint16_t bl = chips[i].baselineData(e);
      Serial.print(bl);
      if (e < 11) Serial.print(",");
    }
    Serial.println();
    // Also print filtered data for reference
    Serial.print("Chip "); Serial.print(i); Serial.print(" filtered:  ");
    for (uint8_t e = 0; e < 12; e++) {
      uint16_t fd = chips[i].filteredData(e);
      Serial.print(fd);
      if (e < 11) Serial.print(",");
    }
    Serial.println();
  }
}
