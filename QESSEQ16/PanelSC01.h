// SC01 Plus panel description for LovyanGFX.
//
// 3.5" 480x320 ST7796 driven over an 8-bit parallel bus, FT5x06 capacitive
// touch on I2C (SDA GPIO 6 / SCL GPIO 5), PWM backlight. This is the
// hardware layer only - drawing and layout live in Display.cpp.
//
// GPIO 10/11 are deliberately NOT used here: the MPR121 pad controllers own
// that second bus (see TouchChips.h).

#pragma once

#define LGFX_USE_V1

#include <LovyanGFX.hpp>

class PanelSC01 : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;  // 8-bit parallel bus (ESP32-S3 only)
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_FT5x06 _touch_instance;  // FT5206/FT5306/FT5406/FT6206/FT6236/FT6336/FT6436

 public:
  PanelSC01(void) {
    // Bus settings
    {
      auto cfg = _bus_instance.config();
      cfg.freq_write = 20000000;  // up to 20 MHz (80 MHz divided by an integer)
      cfg.pin_wr = 47;            // WR
      cfg.pin_rd = -1;            // RD
      cfg.pin_rs = 0;             // RS (D/C)
      cfg.pin_d0 = 9;             // D0
      cfg.pin_d1 = 46;            // D1
      cfg.pin_d2 = 3;             // D2
      cfg.pin_d3 = 8;             // D3
      cfg.pin_d4 = 18;            // D4
      cfg.pin_d5 = 17;            // D5
      cfg.pin_d6 = 16;            // D6
      cfg.pin_d7 = 15;            // D7
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // Panel settings
    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs = -1;    // CS
      cfg.pin_rst = 4;    // RST
      cfg.pin_busy = -1;  // BUSY

      cfg.panel_width = 320;     // native (portrait) width
      cfg.panel_height = 480;    // native (portrait) height
      cfg.offset_x = 0;          // panel offset in X
      cfg.offset_y = 0;          // panel offset in Y
      cfg.offset_rotation = 90;  // rotation offset 0..7 (4..7 = upside down)
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = true;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;

      _panel_instance.config(cfg);
    }

    // Backlight
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 45;      // backlight pin
      cfg.invert = false;   // true to invert brightness
      cfg.freq = 44100;     // PWM frequency
      cfg.pwm_channel = 7;  // PWM channel
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    // Touch
    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;            // minimum raw X
      cfg.x_max = 319;          // maximum raw X
      cfg.y_min = 0;            // minimum raw Y
      cfg.y_max = 479;          // maximum raw Y
      cfg.pin_int = 7;          // INT
      cfg.bus_shared = true;    // shared with the screen bus
      cfg.offset_rotation = 0;  // 0..7 if touch and display orientations differ
      // I2C connection
      cfg.i2c_port = 1;    // I2C port 0 or 1 (bus 1 = Wire1, the pad bus)
      cfg.i2c_addr = 0x38; // device address
      cfg.pin_sda = 6;     // SDA
      cfg.pin_scl = 5;     // SCL
      cfg.freq = 400000;   // I2C clock
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

// Single panel instance, owned by the sketch.
extern PanelSC01 tft;
