#pragma once

#include <Arduino.h>

// Display port of Teensy2048's SSD1306 screen onto the SC01 Plus panel.
//
// The OLED layout is banded; the panel keeps that structure, scaled up and
// re-proportioned for 480x320 landscape:
//
//   OLED 128x64                  SC01 Plus 480x320
//   ------------------------------------------------------------------
//   y  0..15  header strip   ->  y   0..33   header (34 px, accent rule)
//   (no OLED counterpart)    ->  y  34..67   ENC / SEQ / KEY mode row
//   y 16..47  two knob rows  ->  y  68..255  4 columns x 2 rows, 120x94
//   y 48..63  control legend ->  y 256..319  5 + 4 slots
//
// The mode row is new: the OLED showed the mode word in the header, which on
// a 480 px strip left no room for the parameter announcements, so the three
// modes get their own always-visible buttons with the active one filled.
//
// Fonts: the OLED's 6x12 header font maps to the 26 px panel font, its 4x6
// label font to 16 px, and the legend uses FreeSans12pt7b (~22 px) as the
// middle step between them. A value or label that would overflow its column
// drops a size instead of running into the next column.
//
// As on the Teensy, everything is immediate mode: the setters below record
// what a screen should say, and displayUpdate() draws whatever is dirty. The
// modules that will feed it (Encoders, Bindings, Modes, ...) are not ported
// yet, so the content comes from the sketch.

#define SCREEN_W 480
#define SCREEN_H 320
#define HEADER_H 34
#define HEADER_MS 2500

#define MODE_COUNT 3
#define MODE_NONE 0xFF  // no mode button highlighted
#define MODE_Y HEADER_H
#define MODE_H 34

#define CELL_COUNT 8
#define LEGEND_COUNT 9  // 0..4 = top control row, 5..8 = F row

void initDisplay();  // panel init, rotation, backlight, first full draw

// --- header band -----------------------------------------------------------
void headerSetLeft(const char* text);  // permanent, top-left (e.g. track)
void headerShow(const char* text);     // temporary, right side, HEADER_MS
// "<Parameter> <value>": the transient strip names WHAT changed and its value
void headerAnnounce(const char* name, const char* value);
void headerAnnounceInt(const char* name, int value, const char* suffix = "");
void headerSetStick(const char* text);  // persistent, right side
void headerStickClear();                // erase the persistent right text
void headerHide();                      // erase the temporary right text now

void clearHeader();      // erase the header band
void clearMainScreen();  // erase the knob cells + legend

// --- mode row --------------------------------------------------------------
// ENC / SEQ / KEY buttons; the active one is filled with the accent colour.
void displaySetModes(const char* m0, const char* m1, const char* m2);
void displaySetActiveMode(uint8_t index);  // 0..2, or MODE_NONE

// --- knob cells (the OLED label/value pairs) -------------------------------
// Text cell: small caption over a large value, e.g. "GATE" over "75%".
void displaySetCell(uint8_t i, const char* label, const char* value);
void displaySetCellValue(uint8_t i, const char* value);
// Dial cell: the ENCMODE CC knob - 270-degree ring, pointer, value inside and
// the CC number underneath (port of drawCcDial()).
void displaySetCellDial(uint8_t i, const char* caption, uint8_t value);

// --- control legend --------------------------------------------------------
void displaySetLegend(uint8_t i, const char* text);  // "" blanks the slot

// Touch marker, drawn over the main band while the panel is pressed.
void displaySetTouch(bool pressed, uint16_t x, uint16_t y);

void refreshMainScreen();  // redraw knob cells + legend
void refreshHeader();      // redraw the header band
void displayMarkDirty();   // redraw everything on the next displayUpdate()
void displayUpdate();      // call every loop: announce timeout + draw if dirty
