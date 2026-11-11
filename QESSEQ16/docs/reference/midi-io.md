<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# Transport
- **PLAY** button (SEQMODE): Start (or Stop if INT transport running)
- **PLAY** button (**ENCMODE3**): toggle clock **INT/EXT** — it never starts the transport there (frees SEQMODE3 ENC4)
- **SYNC=INT**: Teensy generates MIDI Clock (0xF8) + Start (0xFA) → DAW follows (**default**)
- **SYNC=EXT**: Follows incoming USB-MIDI Clock/Start/Stop/Continue

## MIDI I/O
- **USB-MIDI Device**: Notes, CC, Clock, Start/Stop/Continue
- **5-pin DIN MIDI input** (Serial2, pin 7 RX / pin 8 TX): **notes only** — clock/transport deliberately stays USB-only so two clock sources can never double-advance the sequencer; per track gated by SEQMODE4 **EXT IN / SRC** (see [Swing + External In](../modes/timing.md))
- **Keyboard output channel**: Default ch2 (ch1 reserved for DRUM)
- **Sequencer output**: Each sequence plays on its own MIDI channel (1–16)
- **External MIDI In**: `extNoteOn/Off` feeds pending chords of sequences whose input channel matches **and whose SRC (USB/DIN/BOTH) allows the port the note arrived on**; a THRU track forwards them to its output channel

## Live bridge (SysEx)

`ENCABLETONMODE` ([page](../modes/link.md)) talks to a remote script over SysEx on the same
USB-MIDI port:

```
F0 7D 37 32 34 38 <cmd> [7-bit args] F7      manufacturer 0x7D, tag "7248"
```

- One `0x7D`/`7248` frame identifies the protocol, so it cannot collide with another
  device's numeric SysEx IDs. **≤ 32 bytes a message** (USB-MIDI packet granularity), so a
  64-step poly pattern is sent in **chunks** the script buffers by `(channel, firstStep)`.
- Device → host: `HELLO` (retried every 200 ms until answered), `TRACK_SEL`, `CLIP_ACT`,
  `PAT_STEP`, `KNOB`, and the bank/device/stop requests. Host → device: `ACK` (handshake +
  liveness), `TRACK`, `CLIP_STATE`, `PARAM`, `PARAM_SHORT`, `PAGE`, `BANK`.
- **Liveness**: host traffic within 2 s = link alive; a dead link leaves the overlay
  visible but greyed and the sequencer untouched.
- Outgoing frames go through one queue drained **one message per loop**, plus a pattern
  chunk per pass — the bridge never blocks the sequencer's step clock.
- Clock is unaffected: this is SysEx only, so a port that also carries `0xF8`/`0xFA` keeps
  running both ways. Nothing here is sample-accurate, which is why the bridge carries state
  (tracks, clip LEDs, parameters) and never clock.
