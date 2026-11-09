<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# Encoder / CC Mode (ENCMODE)
- **ENC1–ENC4** send **CC 17–20** on `encChannel` (default ch1, set by the locked step-pad picker of plain ENCMODE, or momentarily in variant 3; a KEYMODE3 **ENCOD lock** links it to the SEQ edit channel — see Keyboard Playing)
- **ENC5–ENC8** send the current **F-bank** block inside the active **M3 page group**: group 1 (M3 off) = **F1 = CC 1–4**, **F2 = CC 5–8**, **F3 = CC 9–12**, **F4 = CC 13–16**; group 2 (M3 on) = **F1 = CC 21–24**, **F2 = CC 25–28**, **F3 = CC 29–32**, **F4 = CC 33–36**
- **F1–F4** = CC-bank radio (bright white = armed; always one armed — **remembered per encoder channel and persisted with SEQMODE4+REC**: switching the encoder channel, or re-entering ENCMODE, recalls that channel's last bank + page group). Idle color follows the page group: dim purple in group 1, dim medium sea green in group 2. In ENCMODE the F row leaves the inherited KEYMODE2 chord-extension job
- **M3 (PAGE)** = page-group toggle: dim medium sea green = group 1 (CC 1–16), purple = group 2 (CC 21–36); off on first entry (also leaves the inherited KEYMODE2 chord-quality job)
- Values persist across mode changes (0–127, clamped, no wrap); DAW feedback on CC 1–36 syncs the matching knobs
- Knob positions are stored **per CC number × channel** (36×16): switching the encoder channel loads that channel's own values instead of inheriting the previous channel's (no jump)
- **Screen (ENCMODE 1/2/4)**: the eight knobs are drawn as **270° dials** in a 4×4 grid — **dial, CC number, dial, CC number** — so the value sits *inside* its ring and the CC number underneath it. A dial is a **radius-9 ring drawn as a single-pixel arc** (`u8g2.drawArc`, 90° gap at 6 o'clock: 0 at the lower-left end, 127 at the lower-right) with a **4-px-long, 2-px-wide pointer that reaches one pixel outside the rim** at the current value (a *second* Bresenham pass one pixel across — on the upper side, so it can never grow down into the CC caption in the ring's bottom notch), and the 0–127 value in the middle. The CC number under each dial is set in the **4×6 caption font** (`u8g2_font_micro_tr` — 15×5 px, between the 3×3 that was unreadable and the 5×7 that shouted), centred under its ring, and its ink falls in the ring's open bottom notch: 1 px below the arc's ends, 2 px in from the ring's sides. The second row's CC numbers follow the **F bank + M3 page group** (`CC1`–`CC16` / `CC21`–`CC36`), which is what the old bottom legend used to hint at: **the control-button legend is gone in this mode** so the second dial row gets the room. **ENCMODE3 keeps the plain label/value screen** (its pads are the channel overlay, not the knobs)
- **Step pads = channel picker (locked) — plain ENCMODE**: the 16 pads are the encoder's 16 MIDI channels (dim yellow, the selected one bright yellow, left header `CHANNEL SELECT`), and a press sets the encoder channel, shows `Channel N` and repaints the dials, so the CC bank can be addressed without holding ENC into ENCMODE3. The M1 (drum-voice mute) and M2 (channel mute) overlays still take the row while held and give it back on release
- **ENCMODE4 leaves the pads on the sequencer** — playhead, step states and the M1/M2 overlays behave as on any other screen, so the knobs can be worked while the pattern runs and the steps stay visible. Channel changes there come from the momentary variant 3 (hold ENC)
- **ENCMODE4 + REC** saves all 36×16 knob values + channel to EEPROM (an older save is ignored, defaults stay)

## ENCABLETONMODE — the Live bridge (NEXT)
- **NEXT** taps a latched overlay on top of ENCMODE: the pads become Ableton Live's **clip
  slots**, the knob captions become **Live's parameter names**, the values become **Live's
  display values**. Tap NEXT again and ENCMODE is back exactly as it was — this is an
  overlay, not a fourth mode family, and its state is RAM only
- the knobs keep sending CC — but in this overlay they send the **new automap block
  CC 64–71**, never 17–20/1–36: every ENCMODE dial, its per-CC-per-channel memory and every
  MIDI-learn map you made there are untouched
- **PREV** (hold) = the 16 **tracks**; **M3** = swaps the F row to the clip actions
  (fill track / fill scene / stop track / stop all); **F1/F2** = parameter bank,
  **F3/F4** = device
- it needs the companion remote script in `Remote Scripts/Teensy2048/`; without it the
  overlay says `Link Down Check MIDI` and the knobs still send CC 64–71
- clip content, LED meanings, the protocol and the limits:
  [ENCABLETONMODE](link.md)
