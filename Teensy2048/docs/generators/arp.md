<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# ARP Options (MONO, TYPE = ARP)
- **M2** becomes an **ARP** key on such a track: click toggles the ARP encoder page (the left slot reads `ARP` while it is open — `ARP ORDER` … `ARP OPTIONS` once a sub-page is armed — and the control legend reads `ARP`)
  - **Auto-off**: changing channel, TRACK away from MONO, TYPE away from ARP, or leaving SEQMODE always closes the page
- **LED**: M2 burns **pink** whenever the edit track is a MONO arp — dim while the page is closed, bright while it is open (touch still lights it white)
- **Encoder page** (the four knobs the page owns; everything else keeps its binding):

| Encoder | Name | Values |
|---------|------|--------|
| ENC1 | OCT | playback octave shift of the sequence (unchanged) |
| ENC2 | **SPAN** | 1–4 octaves, default **1** |
| ENC3 | **SKIP** | ON/OFF, default **OFF** |
| ENC4 | **MODE** | **UP / DOWN / UPDOWN / RANDOM / RANDOM2 / OUTIN / INOUT / CONV / DIV**, default **UP** |
| ENC5 | GATE | step gate % (unchanged) |
| ENC6 | VEL | default velocity (unchanged) |
| ENC7 | **INST** | **OFF / HI / LOW / 3F1B / 4F2B**, default **OFF** |
| ENC8 | PROB | probability of the edit track, 0–100% (default 100), per track and live |

- **SPAN**: the chord is extended over that many octaves (up to 3 more on top of it, octave 1 = the chord itself), notes folded back into the MIDI range when they run high
- **MODE**: the walk order over the spanned chord
  - **UP** / **DOWN**: bottom→top or top→bottom of the whole span
  - **UPDOWN**: up through every octave and back down without repeating either turning point (period 2×span−2)
  - **RANDOM**: the chord order is shuffled once per octave and stays that way until the chord changes
  - **RANDOM2**: same, but re-shuffled every full pass of the sequence
  - **OUTIN** / **INOUT**: start at the top and bottom of the span and walk inward, or the reverse
  - **CONV** / **DIV**: converge on / diverge from the **PIVOT** tone by distance (the F1 page's PIVOT and WEIGHT shape it)
- **MODE carries all nine orders**, so `ORDER = MODE` on the F1 page reaches every shape and no sub-page is needed to pick one. The ORDER knob's own list is `MODE`, `UP`, `DOWN`, `UPDN`, `OUTIN`, `INOUT`, `CONV`, `DIV`, `PLAY`: `RANDOM` / `RANDOM2` stay on the MODE knob (they are the two stochastic walks, not fixed orders), and `PLAY` keeps the chord in the order the keys arrived while still taking its shape from MODE
- **INST**: repeats notes inside the sequence (extends its length)
  - **OFF**: none · **HI**: the highest note goes in front of every other note · **LOW**: the lowest does the same · **3F1B**: 3 steps forward, 1 back · **4F2B**: 4 steps forward, 2 back
- **SKIP**: on a rest (empty step) **or** a probability miss the arp position **advances anyway** instead of pausing, so the sequence keeps walking over dead steps
- Both the **playhead track** and the **live keyboard voice** (KEYMODE MONO+ARP) use these settings; the keyboard channel's track owns the live one
- Saved/restored with SEQMODE4+REC

**F1–F4 = sub-pages** (legend `ORD · MOT · GEN · OPT`): arming one turns the **whole encoder row** into that page's knobs (header `ARP ORDER`, `ARP MOTION`, `ARP GENERATIVE`, `ARP OPTIONS` — the full name of the page, not a code; the armed F burns bright). Slots a page does not own keep the plain binding above, so GATE/VEL/PROB (and OCT on F3, INST on F1/F2/F4) stay reachable from inside every page — and GATE/VEL/PROB stay **live** there, because the page always turns the M1 edit radio off and so carries its own path for those three encoders. Clicking the armed F again goes back to the plain row; the armed page dies with the page (M2, channel, TRACK, TYPE or mode change). While the ARP page owns the F row the SEQMODE step-edit targets and the **F3-hold ACCENT VEL** overlay stay closed — close the page first, the same trade GNR8 makes. **Every knob stops at both ends — no wrap** (an enum that wrapped would drop a shape you just dialled in on one click), and the percentage knobs (WEIGHT, MUTATE, MORPH, RATE, DEPTH, MEMORY, the OPT amounts) move 4% per detent like every other 0–100% dial (see *Encoder feel*).

| Page | ENC1–4 | ENC5–8 |
|------|--------|--------|
| **F1 ORD** | **ORDER** (`MODE` = follow the MODE knob, else UP/DOWN/UPDN/OUTIN/INOUT/CONV/DIV/PLAY) · **STEP** (1/1·2/1·3/1·2/2·3/2·4/2 skip pattern over that walk) · **PIVOT** (ROOT/5TH/TOP/LOW/CTR) · **WEIGHT** (0–100% pull onto the pivot tone) | GATE · VEL · INST · PROB |
| **F2 MOT** | **MUTATE** (0–100% chance a step becomes another chord tone in register) · **INSERT** (`FOLLOW` = the INST knob, else OFF/HI/LOW/3F1B/4F2B/RAND) · **CYCLE** (1/2/4/8/16 bars between RANDOM2 reshuffles) · **MORPH** (0–100% of steps played in the plain walk) | GATE · VEL · INST · PROB |
| **F3 GEN** | OCT · **ENGINE** (OFF/MARKOV/WALK) · **RATE** · **DEPTH** · **MEMORY** | **BIAS** (NONE/HIGH/LOW/ROOT/PIVOT/SPARSE/DENSE) · **SEED** (0–63, turning re-rolls) · PROB |
| **F4 OPT** | the armed engine's own four, labelled per engine: MARKOV = **JUMP/SMOOTH/STAY/REST**, WALK = **STEP/REFLECT/DRIFT/REST** | **TARGET** (ORDER/PITCH/BOTH) · **CLOCK** (STEP/PASS/BAR/PHRASE) |

- **ORDER**: `MODE` keeps the MODE knob's walk; **OUTIN** starts at the top and bottom and walks inward, **INOUT** the reverse, **CONV**/**DIV** converge on / diverge from **PIVOT** by distance (WEIGHT biases the pick toward the pivot tone), **PLAY** keeps the chord in the order the keys arrived instead of sorted
- **ENGINE**: the walk is regenerated from (chord, params, SEED, cycle) on every call, so the playhead and the live voice always agree and nothing has to be stored or undone. **TARGET** chooses what it rewrites — the walk order, the note's octave, or both; **CLOCK** picks the reroll point (per step / pass / bar / phrase) and **RATE** stretches it, **DEPTH** bounds the leap, **MEMORY** holds position (100 = frozen), **SEED** re-rolls. **MARKOV** favours small steps (SMOOTH damps them, STAY repeats, JUMP widens the leap range), **WALK** moves a fixed step (REFLECT turns at the edges, DRIFT steers); **BIAS** pushes the result high/low, toward the chord root or PIVOT, or sparser/denser
- All 20 F-page values are per track and live (a turn is audible on the next step); they persist in the extended SEQ EEPROM block, and an older save loads with the plain-page defaults (ORDER `MODE`, INSERT `FOLLOW`, ENGINE `OFF`, RATE 50, DEPTH 30, MEMORY 50, TARGET `ORDER`, CLOCK `PASS`) so the track sounds exactly as before until you turn something
