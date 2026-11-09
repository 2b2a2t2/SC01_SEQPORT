#pragma once

#include <stdint.h>

/*****************************************************************************
 Drawing layer under Display.cpp.

 Display.cpp owns what a screen says: the label and value of each of the eight
 knob cells, the nine control-legend slots, the header label and the header
 note.  It never draws.  Everything below is the one place the active panel is
 chosen:

   ScreenOled.cpp   Teensy 4.1, SSD1306 128x64 - the layout the firmware was
                    written on (header, 4x2 label/value cells, 5+4 legend)
   ScreenPanel.cpp  SC01 Plus, ST7796 480x320 - the SC01_SEQPORT.ino layout
                    (header band, ENC/SEQ/KEY mode row, 4x2 cells of 120x94,
                    5+4 legend slots, CC dials)

 The vocabulary is the OLED's - eight cells (four per row), nine legend slots
 - because that is what Display.cpp's layout logic produces on either panel.
 The 480x320 panel simply gives each cell 120x94 px to say it in.
*****************************************************************************/

#define SCR_CELLS    8
#define SCR_LEGENDS  9
#define SCR_MODE_NONE 0xFF

void scrBegin();                 // panel init + clear (first draw)
void scrClear();                 // blank every cell and legend slot
// A full frame is about to be described.  The framebuffer backend starts from
// a blank buffer; the panel has no framebuffer, so it keeps what is already on
// screen as the reference and each scr*() call then repaints only the element
// that actually differs.  A band-wide blank there repaints everything on every
// knob turn - a visible flash, and a block in the loop long enough to be heard.
void scrFrameBegin();
// Header band: left label, right note.  `transient` = a fresh announcement
// (accent colour, takes the whole strip if long); otherwise the note is the
// persistent one and the left label shares the strip with it.
void scrHeader(const char* left, const char* right, bool transient);
// Mode row: 0 ENC, 1 SEQ, 2 KEY.  No-op on the 128x64 panel, where the family
// is named in the header instead.
void scrModeRow(uint8_t active);

void scrCellLabel(uint8_t i, const char* text);       // caption over the value
void scrCellLabelSmall(uint8_t i, const char* text);  // ENCABLETONMODE names
void scrCellValue(uint8_t i, const char* text);
// Dial cell: 270-degree ring, pointer across the rim, value inside and the
// caption (the CC number) in the ring's bottom notch.
void scrDial(uint8_t i, const char* caption, uint8_t value);

void scrLegend(uint8_t i, const char* text);  // "" blanks the slot
// Which legend slot is ARMED right now: the F1..F4 radio of the open page, or
// the M button whose menu is open.  The panel fills that label the way the mode
// row fills the active mode, so the strip says what the pads already show - a
// chosen F pad burns white there (Modes.cpp), and the legend names which one.
// No-op on the 128x64 panel: its 4x6 strip has no room to invert a label.
void scrLegendSel(uint8_t i, bool on);

void scrUpdate();   // push whatever changed (one OLED page per call)
