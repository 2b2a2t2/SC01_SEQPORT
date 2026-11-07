#include "TouchChips.h"

#include <Wire.h>

Adafruit_MPR121 chips[NUM_CHIPS];
uint16_t currTouched[NUM_CHIPS] = {0};
static uint16_t prevTouched[NUM_CHIPS] = {0};
static bool chipOk[NUM_CHIPS] = {false};

static const uint8_t i2cAddrs[NUM_CHIPS] = {0x5A, 0x5B, 0x5C, 0x5D};

uint8_t touchChipsFound() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < NUM_CHIPS; i++)
    if (chipOk[i]) n++;
  return n;
}

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
    Serial.print("Chip ");
    Serial.print(i);
    Serial.print(" baselines: ");
    for (uint8_t e = 0; e < PADS_PER_CHIP; e++) {
      Serial.print(chips[i].baselineData(e));
      if (e < PADS_PER_CHIP - 1) Serial.print(',');
    }
    Serial.println();
  }
}

// A pad whose baseline never came up (register < 16, while a healthy resting
// pad sits near 520..800) reports nothing - the chip ACKs fine, its buttons
// are just dead. The last chip started loses the autoconfig race most often.
static bool chipBaselinesSane(uint8_t i) {
  for (uint8_t e = 0; e < PADS_PER_CHIP; e++)
    if (chips[i].baselineData(e) < 64) return false;
  return true;
}

// stop -> autoconfig + thresholds -> restart
static void recalibrateChip(uint8_t i) {
  chips[i].writeRegister(0x5E, 0x00);  // ECR = 0 (stop mode)
  delay(10);
  chips[i].setAutoconfig(true);
  chips[i].setThresholds(TOUCH_THRESH, TOUCH_REL);
  chips[i].writeRegister(0x5E, 0x8C);  // ECR = 0x8C (CL=5, 12 electrodes)
  currTouched[i] = chips[i].touched();
  prevTouched[i] = currTouched[i];
}

bool initTouchChips() {
  // setPins first: Adafruit_MPR121::begin() calls the Wire object's begin(),
  // and without stored pins that would reopen the bus on the core's default
  // SDA/SCL - GPIO 8 and 9 here, which are two of the panel's data lines.
  Wire.setPins(MPR_SDA, MPR_SCL);
  Wire.begin(MPR_SDA, MPR_SCL, MPR_FREQ);
  Wire.setTimeOut(20);

  // Every address is tried: a chip with an unset ADDR strap must not stop the
  // other two from working, and the missing one is what the report names.
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    chipOk[i] = beginChip(chips[i], i2cAddrs[i]);
    if (chipOk[i]) {
      Serial.print("MPR121 #");
      Serial.print(i + 1);
      Serial.print(" OK at 0x");
      Serial.println(i2cAddrs[i], HEX);
    }
  }

  const uint8_t found = touchChipsFound();
  if (found == 0) return false;

  // Autoconfig runs asynchronously after begin(); polling before it completes
  // leaves electrodes at baseline 0, which then never report a touch.
  delay(500);
  printBaselines();

  for (uint8_t attempt = 0; attempt < 3; attempt++) {
    bool anyBad = false;
    for (uint8_t i = 0; i < NUM_CHIPS; i++) {
      if (!chipOk[i] || chipBaselinesSane(i)) continue;
      anyBad = true;
      Serial.print("MPR121 #");
      Serial.print(i + 1);
      Serial.println(" bad baseline -> recalibrating");
      recalibrateChip(i);
    }
    if (!anyBad) break;
    delay(500);
    printBaselines();
  }

  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    prevTouched[i] = currTouched[i] = chipOk[i] ? chips[i].touched() : 0;
  }
  return found == NUM_CHIPS;
}

void pollTouchChips() {
  static uint32_t lastPoll = 0;
  const uint32_t now = millis();
  if (now - lastPoll < TOUCH_POLL_MS) return;
  lastPoll = now;

  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;  // a dead chip would just burn I2C timeouts
    currTouched[i] = chips[i].touched();
    if (currTouched[i] == prevTouched[i]) continue;

    const uint16_t was = prevTouched[i];
    prevTouched[i] = currTouched[i];
    // Print every edge: the fastest way to learn which pad a physical pad is
    for (uint8_t e = 0; e < PADS_PER_CHIP; e++) {
      const bool isDown = currTouched[i] & (1u << e);
      if (isDown == (bool)(was & (1u << e))) continue;
      Serial.print("pad ");
      Serial.print(i * PADS_PER_CHIP + e);
      Serial.println(isDown ? " down" : " up");
    }
  }
}

bool padDown(uint8_t pad) {
  if (pad >= NUM_PADS) return false;
  return (currTouched[pad / PADS_PER_CHIP] >> (pad % PADS_PER_CHIP)) & 1u;
}

uint8_t padsDown() {
  uint8_t n = 0;
  for (uint8_t p = 0; p < NUM_PADS; p++)
    if (padDown(p)) n++;
  return n;
}

void touchRecalibrate() {
  Serial.println("Forcing MPR121 recalibration...");
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;
    recalibrateChip(i);
    Serial.print("MPR121 #");
    Serial.print(i + 1);
    Serial.println(" recalibrated");
  }
  delay(500);
  printBaselines();
}

void touchPrintDiag() {
  for (uint8_t i = 0; i < NUM_CHIPS; i++) {
    if (!chipOk[i]) continue;
    Serial.print("Chip ");
    Serial.print(i);
    Serial.print(" baselines: ");
    for (uint8_t e = 0; e < PADS_PER_CHIP; e++) {
      Serial.print(chips[i].baselineData(e));
      if (e < PADS_PER_CHIP - 1) Serial.print(',');
    }
    Serial.println();
    Serial.print("Chip ");
    Serial.print(i);
    Serial.print(" filtered:  ");
    for (uint8_t e = 0; e < PADS_PER_CHIP; e++) {
      Serial.print(chips[i].filteredData(e));
      if (e < PADS_PER_CHIP - 1) Serial.print(',');
    }
    Serial.println();
  }
}
