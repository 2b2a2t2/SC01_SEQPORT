# Live arpeggio from your DAW

**Goal** — hold a chord in the DAW and hear the device arpeggiate it, with nothing recorded.

**Before you start** — a MONO track with **TYPE = ARP**, and a DAW sending notes on a known
channel.

## Steps

1. `SEQUENCER 3`: **ENC6 = TRACK** to the ARP track, **ENC7 = TYPE** → `MONO`, transform
   → `ARP`. **ENC5 = INCH** to the channel the DAW sends on.
2. `SEQUENCER 4`: **ENC3 = EXT IN** → `LIVE`, **ENC4 = SRC** → `USB`.
3. Leave the ARP track's steps **empty** — LIVE only fills steps that have no stored
   content, so a programmed step keeps its own chord.
4. Start the transport. Either press **PLAY** (`SYNC = INT`, the device is master) or send
   clock from the DAW (`SYNC = EXT`).
5. Hold a chord in the DAW. The ARP walks it, gated by the pattern's rhythm.

## What you should hear

An arpeggio that exists only while you hold the chord. Release, and those steps rest again.

## Variations

- **THRU** instead of LIVE forwards the notes out on the track's output channel — a per-track
  MIDI thru box.
- `ON` listens and feeds the pending chord for **recording**: same input path, but you
  capture it into steps with **REC** instead of playing it live.
- To **keep the arpeggio you heard** rather than the held chord, arm **REC** with the
  step-edit row's **`LISTEN`** → `NOTE`: each arp note is written to the step it sounds
  on, one note per step, and the track's key edges stay out of the way. `PLAY` →
  `RECORDED` then plays that line back exactly as it was captured — see
  [Live recording](../modes/seqmode.md#live-recording).
- Swap the transform to `BASS` or `LEAD` and the same held chord becomes a different part.

## Traps

- The gesture must be **held**. Pending notes survive a release for the record workflow, but
  LIVE playback stops when the chord is let go.
- `SRC` must match the port the notes arrive on. A note-off only reaches tracks whose SRC
  allows that port — otherwise the track keeps its note.
- DRUM tracks ignore LIVE.
- Clock and transport are **USB only**; a 5-pin source cannot drive the transport.

See [Swing & external input](../modes/timing.md) and [ARP](../generators/arp.md).
