<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# Display

**Header** (y 0–15): the mode in whole words on the left (`ENCODER 1`, `SEQUENCER 3`, `KEYBOARD 2`; variant 3 reads `CHANNEL SELECT` while its pad overlay is up, and plain ENCMODE always does) and one **temporary message** on the right (2.5 s). Every message names the parameter that just moved and its value in full words — `Pattern Length 16`, `Accent Velocity 90`, `Kit Map GM` — never the 4-character main-screen code (`LEN`, `ACC`, `PROB`). A **scale change** announces only its mood (`Scale Happy`, `Scale Sad`), because the knob's own value line already shows which scale is selected. An open page takes the left slot (`ARP ORDER`, `GENERATOR`, `DRUM PATTERN`, `STRUM ADDS`, `PROBABILITY`). The band is 128 px (21 characters); a message too long for it is pinned to the left edge so its head stays readable instead of running off-screen. KEYMODE4 also keeps a **persistent left header** (the progression `V vi > I IV`) that transients overlay and then uncover again  
**Main screen** (y 16–63):
- Encoder grid 4×2: labels (top) + values (bottom) — **ENCMODE 1/2/4 replace both text rows** with the dial grid above (value inside the ring, CC number under it)
- Control button labels: row1 PREV NEXT M1 M2 M3, row2 M1 M2 M3 M4 (KEYMODE4 labels NEXT as **EMB** — the sticky EMB menu; ENCMODE3 labels row2 **F1–F4** — the CC-bank radio, and ENC5–8 values show the live CC numbers per bank). **ENCMODE 1/2/4 draw no legend at all** — the dials take the whole band

Paged I2C write (1 page per `displayUpdate()` call, ~4 ms @ 400 kHz) to avoid blocking MIDI pump.
