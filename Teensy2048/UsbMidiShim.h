#pragma once
/*************************************************************
 usbMIDI facade for the SC01 Plus (ESP32-S3) build
 ------------------------------------------------------------
 The Teensy 4 core hands the sketch a global `usbMIDI` object with a
 ready-made API (sendNoteOn, setHandleClock, ...). The ESP32-S3 core has
 the same USB-MIDI functionality, but as the TinyUSB class `USBMIDI`
 (libraries/USB/src/USBMIDI.h) with a different API and no global object.

 This header declares an object with the Teensy's *name*, method names,
 channel numbering (1..16), SysEx framing and handler signatures, so the
 five sketch files that talk to USB MIDI only need this include and
 nothing else changes.

 On the Teensy this file is empty - the core's own usbMIDI is used.

 Board option required: "USB Mode = USB-OTG (TinyUSB)" (see UsbMidiShim.cpp).
*************************************************************/

#include "Config.h"

#ifdef use_SC01Plus

#include <stdint.h>

class TeensyUsbMidi {
public:
  // Starts TinyUSB (the MIDI interface registered at static init, plus CDC
  // so `Serial` keeps working) and the transmit lock. Call it once from
  // setup(), before anything sends.
  void begin();

  // Reads and dispatches one incoming USB-MIDI packet; false when the
  // endpoint has nothing (which is what ends seqPumpMidi()'s bounded loop).
  bool read();

  // ------------------------------------------------------------- out ----
  void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
  void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
  void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);
  void sendClock();                     // 0xF8
  void sendStart();                     // 0xFA
  void sendStop();                      // 0xFC (Teensy's sendStop sends 0xFB)
  void sendRealTime(uint8_t type);      // any raw 0xF8..0xFF
  // hasTerm == false adds the F0/F7 delimiters, exactly like the Teensy
  // core's usb_midi_send_sysex_add_term_bytes()
  void sendSysEx(uint32_t length, const uint8_t *data, bool hasTerm = false);

  // -------------------------------------------------------------- in ----
  // Signatures and 1-based channels match usb_midi.h; a Note-On with
  // velocity 0 reaches the Note-Off handler, as on the Teensy.
  void setHandleNoteOn(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity));
  void setHandleNoteOff(void (*fptr)(uint8_t channel, uint8_t note, uint8_t velocity));
  void setHandleControlChange(void (*fptr)(uint8_t channel, uint8_t control, uint8_t value));
  void setHandleClock(void (*fptr)(void));
  void setHandleStart(void (*fptr)(void));
  void setHandleStop(void (*fptr)(void));
  void setHandleContinue(void (*fptr)(void));
  // Reassembled across packets; data carries the F0/F7 delimiters, and
  // complete is false for the chunks of a message longer than the buffer.
  void setHandleSysEx(void (*fptr)(const uint8_t *data, uint16_t length, bool complete));

private:
  void emit(uint8_t cin, uint8_t b1, uint8_t b2, uint8_t b3);
  void sendRealTimePacket(uint8_t type);
  void sysexByte(uint8_t b);            // RX assembly (Teensy sysex_byte)
  void sysexEnd();                      // deliver the assembled message

  void (*hNoteOn)(uint8_t, uint8_t, uint8_t) = nullptr;
  void (*hNoteOff)(uint8_t, uint8_t, uint8_t) = nullptr;
  void (*hControlChange)(uint8_t, uint8_t, uint8_t) = nullptr;
  void (*hClock)(void) = nullptr;
  void (*hStart)(void) = nullptr;
  void (*hStop)(void) = nullptr;
  void (*hContinue)(void) = nullptr;
  void (*hSysEx)(const uint8_t *, uint16_t, bool) = nullptr;

  // Same size as the Teensy core's USB_MIDI_SYSEX_MAX: a longer message is
  // handed over in complete=false chunks and reassembled by the handler.
  uint8_t  sysexBuf[290];
  uint16_t sysexLen = 0;
};

extern TeensyUsbMidi usbMIDI;

#endif  // use_SC01Plus
