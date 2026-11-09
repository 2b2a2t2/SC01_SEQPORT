# Accept notes from a 5-pin controller

**Goal** — a hardware keyboard on the DIN input playing one track, without touching the
others.

**Before you start** — a 5-pin MIDI cable into the DIN **input** (Serial2, pin 7 RX /
pin 8 TX). The DIN port is input only for notes: it never carries clock or transport.

## Steps

1. `SEQUENCER 3`: **ENC5 = INCH** to the channel your controller sends on.
2. `SEQUENCER 4`: **ENC4 = SRC** → `DIN` for that track. (`USB` is the default and the
   behaviour every earlier build had; `BOTH` accepts either port.)
3. **ENC3 = EXT IN** → `ON` to listen, or `THRU` to also forward the notes to the track's
   output channel over USB.
4. Play the controller. The notes feed the track's pending chord — play into a step to
   record, or use `EXT IN = LIVE` to hear the chord through the track's engine while the
   transport runs.
5. Start the clock from USB (`PLAY` with `SYNC = INT`, or `SYNC = EXT` and let the DAW
   drive).

## What you should hear

Only that track responding to the DIN keyboard; the other 15 ignore it because their SRC is
`USB`.

## Variations

- `THRU` is a per-track MIDI thru box: DIN in → USB out on the track's channel, still
  filtered by the track's input channel, so a controller can reach the DAW through the
  device.
- `BOTH` is the setting for a setup where either port may be the source — useful when the
  DAW and the keyboard both send.

## Traps

- A note-off only releases tracks whose SRC allows the port it arrived on. Changing the mode
  or source releases anything the track had forwarded, so nothing hangs at the host.
- Clock/transport over DIN is a **non-goal**: two clock sources must never both advance the
  sequencer.
- Pins and the chip select lines are in [Hardware](../hardware.md).

See [Swing & external input](../modes/timing.md) and [MIDI I/O](../reference/midi-io.md).
