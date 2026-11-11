#pragma once

#include <Arduino.h>

// Display content layer.
//
// Every screen in this firmware is described in the OLED's vocabulary: a
// header strip, eight knob cells (four per row), and nine control-legend
// slots.  The drawing itself lives behind Screen.h, so the same content runs
// on the Teensy's 128x64 SSD1306 and on the SC01 Plus 480x320 panel - the
// latter with the layout SC01_SEQPORT.ino was designed on (header band, mode
// row, 4x2 cells of 120x75, three rows of legend buttons, CC dials).

#define HEADER_MS   2500

void initDisplay();
void headerSetLeft(const char* text);           // permanent, top-left (e.g. mode)
void headerShow(const char* name);              // temporary, right side, HEADER_MS
// "<Parameter> <value>" - the temporary strip names WHAT changed and its
// value in full words, so no knob turn is silent and no message is a code
// ("Pattern Length 16", not "LEN 16").  Too-wide text is pinned left, so the
// start of a message is always readable
void headerAnnounce(const char* name, const char* value);
void headerAnnounceInt(const char* name, int value, const char* suffix = "");
void headerSetStick(const char* text);          // persistent, right side (KEY4 progression)
void headerStickClear();                        // erase the persistent right text
void headerHide();                              // erase temporary right text now
void clearHeader();                  // erase the header band
void clearMainScreen();              // erase the knob cells + the legend band
void refreshMainScreen();            // redraw encoder/control grids (changed cells only)
void refreshHeader();                // redraw the top strip (mode/ARP/radio)
void displayMarkDirty();             // redraw everything
void displayUpdate();                // call every loop: timeout + push changes

// Bring-up report: the cells name the reset reason, the last bring-up stage
// reached, how many boots in a row failed and the free heap, so a board that
// resets tells you where it died on the panel itself.  Available before any
// of the firmware's state is up (it bypasses refreshMainScreen()).
void displayShowBoot(bool safeMode, const char* reset, const char* stage,
                     const char* boots, const char* heap, const char* devices,
                     const char* note);
