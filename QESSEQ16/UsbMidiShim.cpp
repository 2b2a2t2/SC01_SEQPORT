/*************************************************************
 usbMIDI facade for the SC01 Plus (ESP32-S3) build - implementation
 ------------------------------------------------------------
 Translates the Teensy-shaped API of UsbMidiShim.h into the ESP32 core's
 TinyUSB class USBMIDI (libraries/USB/src/USBMIDI.h):

   sendXxx()      -> USBMIDI::noteOn/controlChange/... or a hand-built
                     CIN packet (clock, transport, SysEx)
   read()         -> USBMIDI::readPacket(), decoded like the Teensy core's
                     usb_midi_read(): CIN -> handler, Note-On velocity 0
                     becomes Note-Off, SysEx reassembled across packets

 Nothing here compiles on the Teensy (guarded by use_SC01Plus), where the
 core already provides usbMIDI.
*************************************************************/

#include "Config.h"

#ifdef use_SC01Plus

#include "UsbMidiShim.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "USB.h"
#include "USBMIDI.h"

// The MIDI class only works with the USB-OTG (TinyUSB) peripheral; in the
// "Hardware CDC and JTAG" mode the core's examples bail out the same way.
#if ARDUINO_USB_MODE
#error "USB MIDI needs the board option 'USB Mode = USB-OTG (TinyUSB)' - build with FQBN esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc"
#endif

// One core object: its constructor registers the MIDI interface with the
// descriptor loader, so it has to be a global (same as the core examples).
static USBMIDI sCoreMidi;

// uClock dispatches its sync callback from its own FreeRTOS task while
// loop() sends from the Arduino task, so the packet writes share a lock.
// (Both run in task context - never in an ISR - so a mutex is legal here.)
static SemaphoreHandle_t sTxLock = nullptr;

// USB-MIDI Code Index Numbers (USB-MIDI 1.0 table 4-1)
enum {
  CIN_SYSCOM_2BYTE  = 0x02,
  CIN_SYSCOM_3BYTE  = 0x03,
  CIN_SYSEX_START   = 0x04,   // SysEx starts or continues (3 bytes)
  CIN_SYSEX_END_1   = 0x05,   // SysEx ends with 1 byte
  CIN_SYSEX_END_2   = 0x06,   // SysEx ends with 2 bytes
  CIN_SYSEX_END_3   = 0x07,   // SysEx ends with 3 bytes
  CIN_NOTE_OFF      = 0x08,
  CIN_NOTE_ON       = 0x09,
  CIN_CONTROL_CHANGE= 0x0B,
  CIN_1BYTE_DATA    = 0x0F    // system realtime / single byte
};

// ------------------------------------------------------------------ out --
void TeensyUsbMidi::emit(uint8_t cin, uint8_t b1, uint8_t b2, uint8_t b3) {
  midiEventPacket_t packet = {cin, b1, b2, b3};
  if (sTxLock) xSemaphoreTake(sTxLock, portMAX_DELAY);
  sCoreMidi.writePacket(&packet);
  if (sTxLock) xSemaphoreGive(sTxLock);
}

void TeensyUsbMidi::sendRealTimePacket(uint8_t type) {
  emit(CIN_1BYTE_DATA, type, 0, 0);
}

void TeensyUsbMidi::begin() {
  if (!sTxLock) sTxLock = xSemaphoreCreateMutex();
  sCoreMidi.begin();          // no-op in this core, kept for parity
  USB.begin();                // starts TinyUSB: CDC (Serial) + the MIDI
                              // interface registered by sCoreMidi's ctor
}

void TeensyUsbMidi::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
  if (channel < 1) channel = 1; else if (channel > 16) channel = 16;
  if (note > 127) note = 127;
  if (velocity > 127) velocity = 127;
  emit(CIN_NOTE_ON, (uint8_t)(0x90 | (channel - 1)), note, velocity);
}

void TeensyUsbMidi::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
  if (channel < 1) channel = 1; else if (channel > 16) channel = 16;
  if (note > 127) note = 127;
  if (velocity > 127) velocity = 127;
  emit(CIN_NOTE_OFF, (uint8_t)(0x80 | (channel - 1)), note, velocity);
}

void TeensyUsbMidi::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
  if (channel < 1) channel = 1; else if (channel > 16) channel = 16;
  if (control > 127) control = 127;
  if (value > 127) value = 127;
  emit(CIN_CONTROL_CHANGE, (uint8_t)(0xB0 | (channel - 1)), control, value);
}

void TeensyUsbMidi::sendClock()    { sendRealTimePacket(0xF8); }
void TeensyUsbMidi::sendStart()    { sendRealTimePacket(0xFA); }
void TeensyUsbMidi::sendStop()     { sendRealTimePacket(0xFC); }

void TeensyUsbMidi::sendRealTime(uint8_t type) {
  sendRealTimePacket(type);
}

void TeensyUsbMidi::sendSysEx(uint32_t length, const uint8_t *data, bool hasTerm) {
  if (hasTerm) {
    // usb_midi_send_sysex_buffer_has_term(): data already carries F0/F7, so
    // it goes out as 3-byte CIN 4 chunks with a closing CIN 5/6/7
    while (length > 3) {
      emit(CIN_SYSEX_START, data[0], data[1], data[2]);
      data += 3;
      length -= 3;
    }
    if (length == 3)      emit(CIN_SYSEX_END_3, data[0], data[1], data[2]);
    else if (length == 2) emit(CIN_SYSEX_END_2, data[0], data[1], 0);
    else if (length == 1) emit(CIN_SYSEX_END_1, data[0], 0, 0);
    return;
  }

  // usb_midi_send_sysex_add_term_bytes(): prepend F0, append F7
  if (length == 0) {
    emit(CIN_SYSEX_END_2, 0xF0, 0xF7, 0);
    return;
  }
  if (length == 1) {
    emit(CIN_SYSEX_END_3, 0xF0, data[0], 0xF7);
    return;
  }
  emit(CIN_SYSEX_START, 0xF0, data[0], data[1]);
  data += 2;
  length -= 2;
  while (length >= 3) {
    emit(CIN_SYSEX_START, data[0], data[1], data[2]);
    data += 3;
    length -= 3;
  }
  if (length == 2)      emit(CIN_SYSEX_END_3, data[0], data[1], 0xF7);
  else if (length == 1) emit(CIN_SYSEX_END_2, data[0], 0xF7, 0);
  else                  emit(CIN_SYSEX_END_1, 0xF7, 0, 0);
}

// ------------------------------------------------------------------- in --
void TeensyUsbMidi::sysexByte(uint8_t b) {
  // same overflow behaviour as the Teensy core: flush a full buffer to the
  // handler as a partial (complete == false) chunk, then start a new one
  if (hSysEx && sysexLen >= sizeof(sysexBuf)) {
    hSysEx(sysexBuf, sysexLen, false);
    sysexLen = 0;
  }
  if (sysexLen < sizeof(sysexBuf)) sysexBuf[sysexLen++] = b;
}

void TeensyUsbMidi::sysexEnd() {
  uint16_t len = sysexLen;
  sysexLen = 0;
  if (hSysEx) hSysEx(sysexBuf, len, true);
}

bool TeensyUsbMidi::read() {
  midiEventPacket_t packet;
  if (!sCoreMidi.readPacket(&packet)) return false;

  const uint8_t cin = (uint8_t)(packet.header & 0x0F);
  const uint8_t ch  = (uint8_t)((packet.byte1 & 0x0F) + 1);

  switch (cin) {
    case CIN_SYSEX_START:                     // 3 bytes, message continues
      sysexByte(packet.byte1);
      sysexByte(packet.byte2);
      sysexByte(packet.byte3);
      break;

    case CIN_SYSEX_END_1:                     // last byte of the message
      sysexByte(packet.byte1);
      sysexEnd();
      break;

    case CIN_SYSEX_END_2:
      sysexByte(packet.byte1);
      sysexByte(packet.byte2);
      sysexEnd();
      break;

    case CIN_SYSEX_END_3:
      sysexByte(packet.byte1);
      sysexByte(packet.byte2);
      sysexByte(packet.byte3);
      sysexEnd();
      break;

    case CIN_NOTE_OFF:
      if (hNoteOff) hNoteOff(ch, packet.byte2, packet.byte3);
      break;

    case CIN_NOTE_ON:
      // the Teensy core routes Note-On velocity 0 to the Note-Off handler
      if (packet.byte3) {
        if (hNoteOn) hNoteOn(ch, packet.byte2, packet.byte3);
      } else if (hNoteOff) {
        hNoteOff(ch, packet.byte2, 0);
      }
      break;

    case CIN_CONTROL_CHANGE:
      if (hControlChange) hControlChange(ch, packet.byte2, packet.byte3);
      break;

    case CIN_SYSCOM_2BYTE:                     // F1/F3: no handler here
    case CIN_SYSCOM_3BYTE:                     // F2: no handler here
      break;

    case CIN_1BYTE_DATA:
      if (packet.byte1 >= 0xF8) {              // system realtime
        switch (packet.byte1) {
          case 0xF8: if (hClock)     hClock();     break;
          case 0xFA: if (hStart)     hStart();     break;
          case 0xFB: if (hContinue)  hContinue();  break;
          case 0xFC: if (hStop)      hStop();      break;
          default:   break;                    // FE/FF: unused by this sketch
        }
      } else if (packet.byte1 == 0xF0 || sysexLen > 0) {
        sysexByte(packet.byte1);               // stray byte inside a SysEx
      }
      break;

    default:                                   // cable events, other channels
      break;
  }
  return true;
}

// ------------------------------------------------------------ handlers --
void TeensyUsbMidi::setHandleNoteOn(void (*fptr)(uint8_t, uint8_t, uint8_t)) {
  hNoteOn = fptr;
}
void TeensyUsbMidi::setHandleNoteOff(void (*fptr)(uint8_t, uint8_t, uint8_t)) {
  hNoteOff = fptr;
}
void TeensyUsbMidi::setHandleControlChange(void (*fptr)(uint8_t, uint8_t, uint8_t)) {
  hControlChange = fptr;
}
void TeensyUsbMidi::setHandleClock(void (*fptr)(void))     { hClock = fptr; }
void TeensyUsbMidi::setHandleStart(void (*fptr)(void))     { hStart = fptr; }
void TeensyUsbMidi::setHandleStop(void (*fptr)(void))      { hStop = fptr; }
void TeensyUsbMidi::setHandleContinue(void (*fptr)(void))  { hContinue = fptr; }
void TeensyUsbMidi::setHandleSysEx(void (*fptr)(const uint8_t *, uint16_t, bool)) {
  hSysEx = fptr;
}

TeensyUsbMidi usbMIDI;

#endif  // use_SC01Plus
