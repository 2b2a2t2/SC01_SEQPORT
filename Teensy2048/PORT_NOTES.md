# SC01 Plus port — what changed and what is left

This folder is a copy of `~/Desktop/Teensy2048` (sources, `docs/`, README,
CHANGELOG, LICENSE — verbatim, minus `.git`, `tools/`, `Remote Scripts/`,
`AGENTS.md`, `TODO.md`) plus a hardware-selection switch and an SC01 hardware
layer. The original checkout was not modified.

## Selecting the hardware

`Config.h` — uncomment **exactly one** line (shipped with the SC01 selected):

```c
//#define use_Teensy
#define use_SC01Plus
```

Both defined, or neither, stops the build with an `#error`.

## Files added

| File | Purpose |
| --- | --- |
| `Config.h` | the switch + `#error` guards |
| `PanelSC01.h` | ST7796 480x320 / FT5x06 / PWM backlight panel description (copied from `../PanelSC01.h`) |
| `Screen.h` | the drawing layer both panels implement: header, 8 knob cells, 9 legend slots, dials |
| `ScreenOled.cpp` | Teensy 4.1 backend: the 128x64 SSD1306, drawn exactly as before |
| `ScreenPanel.cpp` | SC01 backend: the 480x320 layout from `SC01_SEQPORT/Display.cpp` (header band, mode row, 4x2 cells of 120x94, legend band, CC dials) |
| `UsbMidiShim.h/.cpp` | a Teensy-shaped `usbMIDI` object on top of the ESP32 core's TinyUSB `USBMIDI` (empty on the Teensy) |
| `PORT_NOTES.md` | this file |

## Files guarded (`#ifdef use_Teensy` / `use_SC01Plus`)

| File | What the SC01 branch does |
| --- | --- |
| `Teensy2048.ino` | moves `Wire` to GPIO 10/11 (the core's default 8/9 are two of the panel's data lines) |
| `Display.cpp` | still owns every label and value, but draws through `Screen.h` instead of u8g2 directly — the SC01 build shows the 480x320 design, not a scaled 128x64 picture |
| `Encoders.cpp` | TCA9548A (0x70..0x77 probe, this board answers at 0x71) + AS5600 angle, converted to 96 counts/rev so 1 detent = 4 counts like the Teensy |
| `TouchChips.h/.cpp` | polled MPR121 every 20 ms instead of IRQ-driven; every address is tried, a missing chip is reported instead of aborting the loop |
| `Buttons.h/.cpp` | `NUM_LEDS` 42→50, `DATA_PIN` 6→12, current cap `setMaxPowerInVoltsAndMilliamps(5,500)`, and the panel's pad → function order (see below) |
| `Modes.cpp`, `SeqClock.cpp` | `FastLED.show()` → `ledsShow()` (identity on the Teensy) |
| `UsbMidiShim.h/.cpp` | the whole file — nothing to dispatch to on the Teensy, so it compiles to nothing there |
| `Chords.cpp`, `Events.cpp`, `Keyboard.cpp`, `Link.cpp`, `SeqClock.cpp` | one line each: `#include "UsbMidiShim.h"` — no other change |

### Pad order on the SC01

`ledForPad[pad]` holds a **function** index (0..41, `-1` for the six pads with
no function), so `buttonNames`, `group*` and every existing caller keep working
unchanged. What the first port got wrong is only how that table is built: it
used the inverse of `padToLedList`, which describes the *Teensy's* pad wiring.
This panel's pads are wired differently, and `kLedForPad[48]` (copied from
`../Leds.cpp`, where button N owns strip LED N) is the panel's own pad ->
physical-button calibration. So the function behind a pad is simply
`kLedForPad[pad]`, and slots 42..47 are the six spare buttons at the far end.

The symptom was a scrambled keyboard page: the first step button reported
"D#", the second "C#", the third "Spare", and the 150 ms ENC hold timeout was
handed to whichever pad `padToLedList[32]` named (physical slot 46) instead of
the ENC button. `btnUpdate()` now asks `ledForPad[p] == 32`, so the timeout
follows the function rather than a Teensy pad number.

`leds[]` is therefore the strip in function order, which on this board is also
wire order: `ledsShow()` is a guarded `FastLED.show()`, with no copy onto a
second buffer. Quick check with the mode colours: the first button is Step 1
and owns strip LED 0, and the six pads that map to slots 42..47 (pads 6..11,
one chip's upper half) are the spares.

Those six pads have no function in the original layout, so strip LEDs **42..47**
stay dark, along with the two spare strip ends 48/49. Giving them a function is
application work, not hardware work.

## The clock tick on the SC01 (uClock 2.3.0 + arduino-esp32 3.x)

Symptom: PLAY moved the playhead once, the step's note went on, and nothing
followed - no note-off, no step 3 - until STOP released the notes.

uClock 2.3.0's ESP32 backend (`platforms/esp32.h`) puts its microsecond value in
both timer slots: `timerBegin(init_clock)` for the timer and
`timerAlarm(_uclockTimer, init_clock)` (plus `setTimer(us_interval)` on every
`setTempo()`) for the alarm. On arduino-esp32 3.x `timerBegin()` takes a
frequency in Hz and `timerAlarm()` a count of those ticks, so the pair collapses
to **one tick per second** at 120 BPM - 192x slower than the 5.208 ms tick the
clock needs. A step is 24 ticks (`mod_step_ref = output_ppqn / 4`), so the next
step was 24 s away, and the note-off that belongs to it never came.

`SeqClock.cpp` now replaces uClock's timer handle with a 1 MHz one
(`uClockUseMicrosecondTimer()`, SC01 build only). Every `timerAlarm(us)` the
library issues then means exactly microseconds, at every tempo and through its
own `setTempo()`, so tempo changes keep working without touching them. The ISR,
clock task and mutex are untouched. It reaches `_uclockTimer` and `handlerISR`
from the library's backend by `extern`, so it is pinned to uClock 2.3.0: an
update that fixes the backend itself has to be checked against this shim.

## Build status

* **Teensy 4.1** (`teensy:avr:teensy41:usb=midi,speed=600`): builds clean
  with `use_Teensy` selected. FLASH code 250956 (data 37080, headers 8920),
  RAM1 variables 184224 with 77920 free for locals, RAM2 517984 free. Only the
  pre-existing `-Wformat-zero-length` warnings from `Display.cpp` plus the
  library warnings the original also emits.
* **ESP32-S3** (`esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc`): builds
  clean, exit 0, no errors and no warnings from this sketch — 691311 bytes of
  flash (52 %) and 203644 bytes of RAM (62 %), i.e. ~124 KB of dynamic memory
  left once the static block is reserved. Before the shim it stopped on
  31 `'usbMIDI' was not declared in this scope` errors in exactly the five
  files that own USB-MIDI sites: `Chords.cpp` (4), `Events.cpp` (2),
  `Keyboard.cpp` (18), `Link.cpp` (2), `SeqClock.cpp` (5).

## USB MIDI

The Teensy core hands the sketch a global `usbMIDI`; the ESP32-S3 core has
the same functionality as the TinyUSB class `USBMIDI`
(`libraries/USB/src/USBMIDI.h`) with a different API and no global object —
there is **no library to install**. `UsbMidiShim.h/.cpp` supplies an object
with the Teensy's name, method names, 1-based channels, SysEx framing
(`F0`/`F7` added when `hasTerm` is false, chunked exactly like the Teensy's
`usb_midi_send_sysex_*`) and handler signatures, and decodes incoming
CIN packets the way `cores/teensy4/usb_midi.c` does — Note-On velocity 0
reaches the Note-Off handler, SysEx is reassembled across packets and handed
over in `complete = false` chunks when it outgrows the 290-byte buffer.

Two things worth knowing:

* The shim is called `UsbMidiShim`, not `UsbMidi`: macOS's case-insensitive
  filesystem made `#include "USBMIDI.h"` inside it resolve to its own
  `UsbMidi.h`, and hiding the core header is also what stopped arduino-cli
  from putting the `USB` library on the include path. Do not rename it back.
* The board option must be **`USB Mode = USB-OTG (TinyUSB)`**. With the
  default `Hardware CDC and JTAG` the MIDI class is not usable and the shim
  stops the build with an `#error` naming the fix:

```
arduino-cli compile --fqbn "esp32:esp32:esp32s3:USBMode=default,CDCOnBoot=cdc" \
  --libraries "$HOME/Documents/Arduino/libraries,$HOME/Documents/Arduino/Arduino maison/libraries" \
  Teensy2048
```

`CDCOnBoot=cdc` keeps `Serial` on the same USB connector (it becomes the
TinyUSB CDC instead of the JTAG serial). If esptool then refuses to reset
into the bootloader, add `UploadMode=cdc` as well (1200 bps touch + wait for
the port).

`usbMIDI.begin()` is the first call in `setup()`; it starts TinyUSB — the
MIDI interface registered by the shim's global constructor plus CDC. The
uClock sync callback runs on uClock's FreeRTOS task while `loop()` sends
from the Arduino task, so every packet write in the shim goes through one
lock (task context only, never an ISR).

Status: **compiles and links, not yet run on hardware.** MIDI out (notes,
CC, clock/transport, Link SysEx) and MIDI in still need a check against a
host.

## Boot diagnosis and safe mode (SC01)

The first hardware boot after the port put the panel and the strip up and then
reset, with nothing on screen naming the step it died in — and the old path
could not report it either: a missing MPR121 hung in `while (1)`, which looks
exactly like the fault it was meant to describe. `Teensy2048.ino` now carries
the mechanism `SC01_SEQPORT.ino` was built around:

* the bring-up stage reached is written to **NVS before each step runs**, so
  the value the next boot reads is the step that never returned (NVS, not RTC
  memory: a brownout clears the RTC domain, which is how a rebooting board
  used to reset its own failure count)
* the boot page — `displayShowBoot()`, drawn straight through `Screen.h` before
  any mode state exists — shows RESET / LAST STAGE / FAILED BOOTS / FREE HEAP /
  MPR121 PADS, and hands the panel to the normal UI after 3 s
* a boot only settles after `loop()` has run for 5 s, so a board that gets the
  panel up and still resets is counted too; "setup done" as the stage says the
  fault is in `loop()`, not in bring-up
* two failed boots in a row start **SAFE MODE**: the strip, the knobs and the
  pads stay off (`ledsShow()` is a no-op until the driver exists), so a fault
  in any of them cannot hide the panel
* bring-up order is display first, then strip, knobs, modes, clock, link, pads
* a missing MPR121 is reported on the boot page instead of hanging

The first report off the board was **`panic` / last stage `clock`**. The cause
is in the clock bring-up, not in the hardware: uClock's ESP32 backend creates
its hardware timer *and* its mutex inside `initTimer()`, which only `init()`
calls, while every setter (`setOutputPPQN`, `setInputPPQN`, `setClockMode`,
`setTempo`) runs its work inside `ATOMIC()` = `xSemaphoreTake(_mutex, …)`.
`initSeqClock()` configured the clock before `uClock.init()`, so the first
setter took a null semaphore, FreeRTOS asserted, and the chip panicked with the
panel and the strip already up - the reboot loop. `initSeqClock()` now calls
`uClock.init()` first; the Teensy was never affected because `ATOMIC()` is a
no-op on that platform, which is why the original order looked fine.

The Teensy build keeps its original order and its `while (1)` on a missing
chip: nothing there was being changed.

## Deferred (next steps)

1. **Display follow-up** — the panel sits on an 8-bit parallel bus at 20 MHz
   (`PanelSC01.h`), so a full-band repaint is ~30 ms of bus time while a knob
   cell is a fraction of that. `refreshMainScreen()` used to start with
   `clearMainScreen()` = `scrClear()`, which blanked the whole model and marked
   the band dirty: every value change therefore repainted all eight cells and
   the legend band, flashing on screen and blocking the loop long enough to
   delay the sequencer's notes. It now calls `scrFrameBegin()`, a buffer clear
   on the OLED backend and a no-op on the panel, so the per-cell/per-dial
   comparison in the setters decides: a knob turn repaints one dial, a value
   change its own cell. Row-level diffing inside a cell is still the next step
   if a long text ever needs it. The FT5x06 is initialised by LovyanGFX but
   nothing in this firmware reads it — pads are the only input.

## Observation about the original (not introduced here)

`Events.cpp` writes the ENCMODE CC block at address 0 with `CC_EE_LEN = 579`
(bytes 0..578) while `Keyboard.cpp` writes the SEQ block at `SEQ_EE_ADDR = 160`
(bytes 160..1539) — the two overlap by 419 bytes. When the CC block was
introduced it was 131 bytes (commit `abc6849`, 2026-09-30), which fitted
below 160; commit `9311e0a` "more CCs" (2026-10-18) grew it to 579 without
moving the SEQ block. Each save therefore invalidates the other block's
checksum, and the other block reloads its defaults. The port fixes it: the CC
block moved to `CC_EE_ADDR = 1600` (bytes 1600..2178), after the SEQ block,
so the two can no longer reach each other.

## Settings persistence on the SC01

Two faults made `CC Saved` / `Sequence Saved` a lie on this board, and both
are fixed in the store rather than in the saves:

1. **The region was never opened.** The ESP32 `EEPROM` is NVS emulation: with
   no `EEPROM.begin()` the 2 KB RAM area does not exist, so every `put()` and
   `get()` silently does nothing, and a `put()` that never gets a `commit()`
   only edits that RAM copy. `cfgStoreBegin()` (Events.cpp, called once in
   `setupSc01()` before the two loaders) opens it — `CFG_EE_BYTES = 2200`, the
   160-byte pad + the 1380-byte SEQ block + the 579-byte CC block — and
   `cfgStoreCommit()` ends both saves. Both are `#ifdef use_SC01Plus`: the
   Teensy 4.1 writes its emulated EEPROM on `put()` and has neither call.
2. **The blocks overlapped** (the section above): the CC save wrote its magic
   and checksum across the SEQ block and vice versa, so even with a working
   store only the last press would have survived.

The SD slot is not involved: this firmware has no card code at all (no
`SD.h`, no `SD.begin`), on either target — settings live in the internal
store, as they did on the Teensy, and the card is unnecessary. Cards still
fit the slot; nothing reads them.

Verified builds only: an actual save → reboot → load needs the board.
