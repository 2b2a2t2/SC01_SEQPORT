#pragma once

#include <Arduino.h>

// ENCABLETONMODE bridge: a small SysEx protocol between the Teensy and the
// companion remote script in Remote Scripts/Teensy2048/.  It carries track
// selection, clip-slot state and actions, pattern dumps and device-parameter
// metadata.  Timing never goes through here - clock stays the per-port Sync
// checkbox and MIDI Clock.
//
// Framing: F0 7D '7' '2' '4' '8' <cmd> <args...> F7   (all payload 7-bit)
// (0x7D = the non-commercial manufacturer id, so the tag can never collide
// with the Tap script's numeric message ids on the same cable.)
//
// Every device -> host message is queued and drained ONE per linkPump(), so a
// call from the event path can never block on USB, and a pattern dump (up to
// 64 steps x 16 tracks) is spread over loop passes instead of one burst.

#define LINK_PROTO_VER 1

void linkInit();          // install the SysEx handler; call once in setup
void linkPump();          // drain the send queue + stream pattern chunks
void linkTick();          // HELLO retry + liveness timeout (call every loop)
bool linkAlive();         // the script answered within the last 2 s

// --- device -> host --------------------------------------------------------
void linkTrackSelect(uint8_t idx);           // 0-based track index
void linkClipAct(uint8_t row, uint8_t act);  // 0 launch, 1 stop, 2 fill
void linkStopTrack();                        // stop the selected track's clips
void linkStopAll();                          // stop every clip
void linkDumpTrack(uint8_t ch);              // stream track ch (0..15)
void linkDumpAll();                          // stream all 16 tracks (scene)
void linkBankStep(int8_t dir);               // parameter page -1 / +1
void linkDeviceStep(int8_t dir);             // previous / next device
void linkParamTouch(uint8_t slot, uint8_t v);// a knob moved (cache + display)
void linkKnob(uint8_t slot, uint8_t v);      // ...and the automap write (SysEx)

// --- host -> device (state the UI reads) ----------------------------------
const char* linkTrackName();                 // "" until the script says hello
uint8_t linkTrackIndex();                    // 255 = unknown
uint8_t linkClipBits(uint8_t row);           // bit0 has clip, bit1 playing,
                                             // bit2 triggered, bit3 armed
uint8_t linkHighlightRow();                  // selected clip slot's row, 255
const char* linkParamName(uint8_t slot);     // "" = no parameter in that slot
const char* linkParamText(uint8_t slot);     // Live's display value
uint8_t linkParamValue(uint8_t slot);        // 0..127 dial position
bool linkParamValid(uint8_t slot);
uint8_t linkPage();                          // 0-based page of the device
uint8_t linkPageCount();
const char* linkBankName();

// --- callbacks implemented in Events.cpp ----------------------------------
void linkTrackApply(uint8_t idx);            // host moved the track selection
void linkFillDone(uint8_t ch, uint8_t ok, uint16_t notes);
