# Drive the knobs as a CC controller

**Goal** — eight knobs mapped to CCs, two pages of four, remembered per MIDI channel.

**Before you start** — nothing but a MIDI host.

## Steps

1. Tap **MODE** to `ENCODER 1`.
2. **ENC1–4** send CC **17–20**; **ENC5–8** send the armed F-bank: **F1 = CC 1–4**,
   **F2 = 5–8**, **F3 = 9–12**, **F4 = 13–16**.
3. Press **M3** (`PAGE`) to reach the second page group: **F1 = CC 21–24**, then 25–28,
   29–32, 33–36. The F row changes colour with the group (dim purple in group 1, dim sea
   green in group 2).
4. Turn a knob and watch the DAW move. Feedback on CC 1–36 moves the matching knob back, so
   automation and your hand stay in sync.
5. To pick the encoder channel without holding anything, go to `ENCODER 4`: the 16 step
   pads are the 16 MIDI channels (dim yellow, selected bright yellow). A press sets the
   channel and repaints the dials.
6. Save: in `ENCODER 4`, press **REC**. All 36 × 16 knob values and the channel go to
   EEPROM.

## What you should hear

Nothing — this mode sends no notes. What you should *see*: 270° dials drawn as single-pixel
arcs, the value inside the ring, the CC number under it in the caption font.

## Variations

- `PLAY` in `ENCODER 3` toggles clock **INT/EXT** and never starts the transport — it lives
  here so `SEQUENCER 4` does not have to carry it.
- Knob positions are stored **per CC number × channel** (36 × 16): switching channel loads
  that channel's values instead of inheriting the previous one, so nothing jumps.
- The F bank and page group are remembered per encoder channel and restored when you
  re-enter ENCMODE.

## Traps

- Values are 0–127 and **clamped, no wrap**.
- ENCMODE 1/2/4 draw **no control-button legend** — the dials take the whole band.
- `ENC 3` keeps the plain label/value screen, because its pads are the channel overlay, not
  the knobs.
- An older `ENC 4 + REC` block is ignored rather than partially loaded.

See [Encoder mode](../modes/encmode.md) and [Display](../reference/display.md).
