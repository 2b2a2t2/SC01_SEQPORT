<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# Display

**Header** (y 0–15): the mode in whole words on the left (`ENCODER 1`, `SEQUENCER 3`, `KEYBOARD 2`; variant 3 reads `CHANNEL SELECT` while its pad overlay is up, and plain ENCMODE always does) and one **temporary message** on the right (2.5 s). Every message names the parameter that just moved and its value in full words — `Pattern Length 16`, `Accent Velocity 90`, `Kit Map GM` — never the 4-character main-screen code (`LEN`, `ACC`, `PROB`). A **scale change** announces only its mood (`Scale Happy`, `Scale Sad`), because the knob's own value line already shows which scale is selected. An open page takes the left slot (`ARP ORDER`, `GENERATOR`, `DRUM PATTERN`, `STRUM ADDS`, `PROBABILITY`, `CC CONFIG`). The band is 128 px (21 characters); a message too long for it is pinned to the left edge so its head stays readable instead of running off-screen. KEYMODE4 also keeps a **persistent left header** (the progression `V vi > I IV`) that transients overlay and then uncover again  
**Main screen** (y 16–63):
- Encoder grid 4×2: labels (top) + values (bottom) — **ENCMODE 1/2/4 replace both text rows** with the dial grid above (value inside the ring, CC number under it)
- Control button labels: row1 PREV NEXT M1 M2 M3, row2 M1 M2 M3 M4 (KEYMODE4 labels NEXT as **EMB** — the sticky EMB menu; ENCMODE 1/2/4 label row2 with each CC bank's row-flavor — **DIAL / LEVEL / ENV / LFO** — and the armed bank is filled; ENCMODE3 keeps the plain **F1–F4** channel-picker row, and ENC5–8 values show the live CC numbers per bank). **ENCMODE 1/2/4 draw no legend at all** — the dials take the whole band

Paged I2C write (1 page per `displayUpdate()` call, ~4 ms @ 400 kHz) to avoid blocking MIDI pump.

## SC01 panel (480 × 320)

The same content, laid out for the panel and with two things the 128 px strip has no room for.
There is **no mode row**: the family is named in the header (as on the strip) and the three mode
pads switch it.

| Band | y | |
|---|---|---|
| header | 0–35 | the page's name at 16 px; the right slot at 12 px carries the temporary message while one is up and `TRACK 02 - STEP 08/16` when none is |
| step lane | 37–64 | sixteen numbered chips, 25 × 22 with the pad number inside, 3 px of air above and below: the channel the current mode is on (edited track, keyboard, encoder bank) filled in cyan, a square edged in cyan for a step that carries notes, the playhead marked inside its square |
| cells | 66–195 | one rounded black card: 4 × 117 by two rows of 65, a 10 px caption over a 16 px value |
| help row | 197–234 | pictogram, the page's name and one line saying what the page is for |
| legend | 236–313 | three rows of chips, 3 + 3 + 4 columns |

The panel keeps the pre-redesign theme: **white values on black**, dark grey cards and chips
(`#1C1C1C`), crisp hairlines, light grey captions, and cyan reserved for what is live — the
accented chip, the lit lane square, the playhead, the dial pointer, the curves and the temporary
message in the header.

The step lane is the exception to the chip grey: a square is the page's black inside its outline —
nothing is filled for a step that carries notes, only its edge turns cyan — so the row reads as
sixteen pads on the page with the mode's own channel the only square that stands out. The help row's information strip is black as well, drawn
as its hairline outline only, so the pictogram and the page description sit on the page rather
than on a second grey card. The legend's chips follow it: an unarmed chip is black inside its
`#4A4A4A` outline, so the grey `#1C1C1C` is left only to the level-bar tracks.

A step costs two lane chips and the status slot, not the screen: `scrStepLane()` and `scrStatus()`
compare against what is already drawn, and both are no-ops on the 128x64 strip.
