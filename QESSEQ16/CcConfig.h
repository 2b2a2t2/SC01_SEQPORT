#pragma once

#include <stdint.h>

/*****************************************************************************
 ENCMODE CC MAP - edit this file to remap what the eight knobs send.

 Nothing else has to change: encCcNum() in Events.cpp only looks the numbers up
 here, and it is the one place the MIDI send, the dial captions, the CC CONFIG
 page (PREV in ENCMODE) and the DAW-feedback mapping read.

 The map is the panel's own shape:

   ENC1..ENC4   one fixed CC each - the same number whatever bank or page
                group is armed
   ENC5..ENC8   four CCs per armed F bank (F1..F4), in the page group M3
                selects, so 4 knobs x 4 banks x 2 groups = 32 numbers, 36 in
                total with the four fixed ones

 Every number has to be 1..CC_MAP_MAX (36): the firmware keeps a value per CC
 number per channel (36 x 16 in RAM, the 579-byte EEPROM block), so a CC
 outside that range would have nowhere to live.  A hand-edited table is
 clamped by ccMapChecked() instead of trusted - a typo can never reach outside
 the store.

 Two slots may share a number if that is really what you want (both knobs then
 move the same value); nothing in the firmware requires the map to be
 injective.
*****************************************************************************/

// CCs the value store holds - ccVal[CC_MAP_MAX][16] in Events.cpp
#define CC_MAP_MAX 36

// ENC1 .. ENC4 - fixed, any F bank / page group
static const uint8_t CC_ENC_FIXED[4] = { 17, 18, 19, 20 };

// ENC5 .. ENC8 - CC_ENC_BANK[page group][F bank][knob]
//   page group 0 = M3 off, 1 = M3 on
//   F bank 0..3  = F1..F4
//   knob 0..3    = ENC5..ENC8
static const uint8_t CC_ENC_BANK[2][4][4] = {
  // group 1 (M3 off) - the default CC1..16
  { {  29,  30,  31,  32 },
    {  5,  6,  7,  8 },
    {  21, 22, 23, 24 },
    { 25, 26, 27, 28 } },
  // group 2 (M3 on) - the default CC21..36 (CC17..20 stay ENC1..ENC4)
  { { 1, 2, 3, 4 },
    { 5, 6, 7, 8 },
    { 9, 10, 11, 12 },
    { 13, 14, 15, 16 } },
};

// 1..CC_MAP_MAX, or 1 for an entry outside it (never an out-of-bounds store)
static inline uint8_t ccMapChecked(uint8_t cc) {
  return (cc >= 1 && cc <= CC_MAP_MAX) ? cc : 1;
}
