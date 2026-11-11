#pragma once

/*****************************************************************************
 Hardware selection
 ------------------
 Uncomment EXACTLY ONE of the two lines below, then build.

   use_Teensy    - the original target: Teensy 4.1, SSD1306 128x64 header
                   display, quadrature encoders, IRQ-driven MPR121 touch,
                   42 WS2812 LEDs on pin 6
   use_SC01Plus  - the SC01 Plus panel: ESP32-S3, ST7796 480x320 (the 128x64
                   layout is scaled 3.75x and letterboxed), AS5600 magnetic
                   knobs behind a TCA9548A mux, polled MPR121 touch, 50 WS2812
                   LEDs on pin 12 (42 behind the buttons, the last eight
                   behind the knobs carrying the active mode family's colour)

Everything except the hardware layer (display, encoders, touch, LEDs, and the
Wire/panel bring-up in QESSEQ16.ino) is shared, unmodified code.
*****************************************************************************/

//#define use_Teensy
#define use_SC01Plus

#if defined(use_Teensy) && defined(use_SC01Plus)
#error "Config.h: uncomment only ONE of use_Teensy / use_SC01Plus"
#endif

#if !defined(use_Teensy) && !defined(use_SC01Plus)
#error "Config.h: uncomment use_Teensy or use_SC01Plus to select the hardware"
#endif
