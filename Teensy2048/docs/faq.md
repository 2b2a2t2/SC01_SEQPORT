# FAQ and troubleshooting

Symptom first. If your question is "why is it designed like that", it belongs in
[Concepts](concepts.md) or the [ADRs](adr/README.md).

## No sound

| Symptom | Cause | Fix |
|---|---|---|
| Pads light but nothing sounds | The track is muted, or its sequence channel is not the one the host listens to | Unmute; check the track's channel in [MIDI I/O](reference/midi-io.md) |
| A step is lit but silent | Step is outside the pattern length, or its `PROB` gate missed | Shorten/lengthen with `LEN`; check `PROB` |
| A follower track makes no sound | It reads an empty feeder, or `FOLLOW` is off | Fill the feeder, or set `SEQ 3 → FOLLOW` on |
| External notes do nothing | `EXT IN` is `OFF` for that track, or `SRC` does not match the port | [Timing](modes/timing.md) |
| The keyboard plays on the wrong channel | Keyboard channel is ch2 by default (ch1 is reserved for DRUM) | `KEY 3` pads pick the channel |

## Pads and touch

| Symptom | Cause | Fix |
|---|---|---|
| A column of pads is dead | An MPR121 chip failed to calibrate or is not answering | `diag` in the serial monitor; check the IRQ and address pins in [Hardware](hardware.md) |
| Pads trigger without touch | Baseline drifted, or the threshold is too low | `recal`; the boot print shows each electrode's baseline |
| A pad never lights | Pads 42–47 have no LED | [Panel map](panel-map.md) |
| Scale pads stay dark | In-scale-only lighting: out-of-scale pads are deliberately unlit | [Keyboard modes](modes/keymode.md) |

## Encoders

| Symptom | Cause | Fix |
|---|---|---|
| A knob does nothing | No idle binding in this mode/variant — `--` in the label table | [Bindings](reference/bindings.md) |
| A knob jumps several values per click | Fast spin is worth up to 4× a detent; wide dials move 1 unit per count | Turn slower; see *Encoder feel* in [Sequencer](modes/seqmode.md) |
| A knob will not go past an end | Dials are clamped, never wrapped, by design | — |
| The row looks wrong after closing a page | An overlay was left armed | Re-enter the page and release it, or change variant |

## Timing and sync

| Symptom | Cause | Fix |
|---|---|---|
| A DAW will not follow | `SYNC = INT` means the device is master and sends clock; the host must be set to external | [Transport](reference/midi-io.md) |
| The playhead stutters | The MIDI pump was starved by a long display or serial write | Paged OLED write already limits this; avoid blocking serial in the loop |
| A DIN keyboard cannot start playback | Clock and transport are USB only, by design | Start from the device or a USB host |
| Swing does nothing | The knob belongs to the **edit track**, and the page must be closed | [Recipe: swing one track](recipes/swing-one-track.md) |

## Saving

| Symptom | Cause | Fix |
|---|---|---|
| Settings are gone after power cycle | There is no autosave; `SEQ 4 + REC` is the save gesture | Save before switching off |
| Patterns disappeared | Patterns are RAM only, by design | [Limitations](limitations.md) |
| CC values reset | `ENC 4 + REC` is required, and an older block is ignored entirely | Re-save in `ENC 4` |
| A new feature reads as default after loading an old save | Backward-tolerant layout: absent fields take their defaults | Expected; see [Compatibility](status.md) |

## Display and LEDs

| Symptom | Cause | Fix |
|---|---|---|
| A header message runs off the right | Messages wider than 21 characters are pinned left | Shorten the message; `tools/header_lint.py` catches it |
| The legend is missing | ENCMODE 1/2/4 draw no control legend — the dials take the band | [Display](reference/display.md) |
| A page pad is dim | Dim means the page is closed; breathing means it is open | Press `M2` |
| Two track hues look alike | They are the closest pair in CIELAB (HARM aqua vs SKANK spring green, 14.4 ΔE) | Check the header |

## Build and flash

| Symptom | Cause | Fix |
|---|---|---|
| No serial port appears | USB Type is **MIDI**; the board only shows as a Teensy port | [Build & flash](build.md) |
| `arduino-cli board list` finds nothing | No board on this machine's USB (cloud/remote sessions have no passthrough) | Flash from the local checkout |
| A library is "not found" | The five libraries live in the sketchbook, not the sketch | `tools/fw.sh build` collects the right roots |
| An upload "succeeds" but nothing changes | The `.hex` came from a different commit | Rebuild, then flash |

## Build-time symptoms (from the original README)

These are the engineering-level failures, kept because they are the ones that look like
mysteries:

| Symptom | Cause | Fix |
|---|---|---|
| Green playhead freezes | MIDI pump starved (long `displayUpdate`/`Serial` block) | Keep `pollSeqClock()` frequent; paged OLED write already helps |
| Stuck notes on mode change | Channel change without note-off | `keySetChannel()` / `chordCut()` / `chordRestore()` handle this |
| MPR121 not found | I²C address conflict / wiring | Check ADDR pins (0x5A–0x5D), 4.7 kΩ pull-ups on SDA/SCL |
| Encoder jitter | Mechanical bounce / no debounce | Encoder library handles quadrature; add an RC filter if needed |
| Loop doesn't start immediately | Old code waited for the playhead to reach the bar | Updated: the loop teleports on activation (`seqStepAdvance`) |
| Wrong bar after loop disable | Playhead didn't track the global position | `chStepShadow` advances silently during the loop |

## Still stuck?

Reproduce it on the panel, then check [Feature status](status.md) — if the area is **Beta**,
the behaviour may be an open tuning item in `TODO.md` rather than a fault.
