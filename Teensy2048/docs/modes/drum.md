<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# DRUM Part Sequencing
- 16 parts = MIDI notes 36 (C1) … 51 (D#2) — **this kit's map** (device note naming, MIDI 24 = C0):
  **C1** bass drum · **C#1** hand clap · **D1** closed hat · **D#1** open hat · **E1** snare · **F1** low conga · **F#1** mid conga · **G1** hi conga · **G#1** low tom · **A1** mid tom · **A#1** hi tom · **B1** (unused) · **C2** cow bell · **C#2** claves · **D2** maracas · **D#2** cymbal
  - **Two maps**: `CUSTOM` (above, the default) and `GM` (the General MIDI percussion map) — a DRUM track picks one on **SEQMODE ENC5 = `MAP`** (variants 1/2/4); the drum page's lanes are voiced from the selected map, so a preset pattern plays the right sounds on either kit. Per track, saved with SEQMODE4+REC
- **Hold M1** (SEQMODE, not variant 3, DRUM track) → orange part-select overlay on steps
- **Press step in overlay** → selects part
- **Normal step view** → toggles selected part on/off at that step
- **M2** on the same track opens the [DRUM pattern page](../generators/drum-styles.md) — 18 preset styles with live FILL/MOT and per-lane line selectors
