# Changelog

User-visible changes, newest first. The firmware has **no tagged releases** yet, so the
de-facto version marker is the size of the saved-settings block (see
[Compatibility](#compatibility)).

## Unreleased

### SC01 panel: black lane pads, and a strip that breathes

- **The step lane's squares are the page's black.** An empty square was `#080808` and a step that
  carried notes `#212121`, which drew the strip above the header as a grey band on the black page.
  Both fills are `#000000` now, so the row is sixteen pads on the page itself; a step with notes
  is told apart by its cyan edge, and the lit channel square (solid cyan) and the playhead are
  unchanged.
- **The lane keeps 3 px of air above and below its chips** instead of 2: the band is 28 px around
  the same 22 px squares (37–64), and every band under it moved down two pixels — the knob card
  66–195, the help row 197–234, the legend 236–313, with 6 px of black left under it.

### SC01 panel: a tighter lane, a larger status line, and no cut labels

- **The step lane is a strip again.** Each of the sixteen chips is 25 × 22 instead of 25 × 26, so
  the band is 37–62 and the knob card starts at 64; the number still sits inside its chip. The
  row reads as a strip along the top of the page instead of a second key row above the card.
- **The header's right slot is set in the 12 px face**, not the 10 px one: the temporary message
  and `TRACK 02 - STEP 08/16` were the smallest type on the panel and were hard to read from
  where the panel actually sits. The slot starts a pixel higher so it stays centred against the
  16 px page name beside it.
- **A turning encoder no longer cuts its label.** Repainting one dial clears the 29 px box around
  it, which has to reach the pointer's lowest point — in the top rows of the CC caption's ink.
  The caption is now drawn back after the dial, so `CC17` stays whole while the knob moves
  instead of losing its top half.

### SC01 panel: the pre-redesign palette is back

- **White values on black, the palette the panel used before the mockup restyle.** The layout is
  unchanged; only the colour roles move. Cell values, bars, the header's page name and the help
  row's name are white `#FFFFFF`, the page and the knob grid are black `#000000`, cards and chips
  are the old dark grey `#1C1C1C`, captions are a light grey `#C6C6C6`, the lane's dim numbers
  the old `#8C8C8C`, hairlines `#292929` and chip edges `#4A4A4A`. Cyan `#00FFFF` is back to
  meaning *live* only: the accented chip, the lit lane square, the playhead, the dial pointer,
  the curves and the transient message. The redesign had made cyan the value colour as well,
  which flattened the hierarchy and read harder at 16 px.
- **An unlit lane square is almost black.** The step lane's squares were drawn in the chip grey
  `#3C3C3C`, which made the row brighter than the page above it and competed with the one lit
  square. An empty square is `#080808` now and one that carries notes `#212121` — still a shade
  off the page, still edged in cyan, but the row reads as sixteen empty pads and the mode's
  channel is again the only thing that draws the eye.
- **The help row is black too.** Its card was the same dark grey as the knob card, which made the
  information strip a third grey band between two black ones. It is filled in the page's black
  with only its hairline edge kept, so the pictogram, the page's name and its one-line
  explanation now sit directly on the page.
- **The legend's chips are black as well**, so the bottom band matches the strip above it: an
  unarmed chip is the page's black inside its `#4A4A4A` outline, an armed one stays solid cyan.
  The `#1C1C1C` fill now survives only on the level bars (the dial track, the bar cell and the
  envelope readouts), where it is a track rather than a button.

### SC01 panel: the strip names the channel, and the numbers moved inside

- **The step lane is sixteen numbered squares**, 25 × 26 with each pad's number drawn inside it
  instead of under a 13 px bar — so the numbers sit where the hand already is and the band is the
  squares plus 2 px of air above and below.
- **The square of the channel the mode is on is lit**: `SEQMODE` lights the edited track's square,
  `KEYMODE` the keyboard's channel, `ENCMODE` the encoder bank's CC channel (`panelStripChannel()`
  in `Display.cpp`). Editing channel 1 lights the first square, controlling the keyboard on
  channel 8 lights the eighth, whatever the pattern length — a silent track still shows which
  channel its mode is on. `scrStepLane()` takes that channel in place of the old window's first
  step; a square that carries notes is filled in grey, and the playhead is a bar inside its
  own square.

### SC01 panel: black grid, and a channel switch repaints the track

- **The knob grid is black.** The big card's ground and every cell fill are `#000000` rather than
  the card's `#0B1D28`, so the grid reads as a black working area with steel hairlines in it. The
  page and the help card keep the palette; the card's rounded corners are filled once per full
  repaint (`scrUpdate`'s `mainDirty` branch), which the cell repaints never reached before.
- **Changing the edited channel now repaints the whole page.** `Display.cpp` tracks the channel
  once per loop (`channelFollow()`) and, when it moves, clears the model and refills it for the
  new track. Before, only the paths that refreshed by hand did anything - and even then the
  per-cell string comparison could see no change (two tracks of the same type on the same knob
  values) and leave the old track's grid up. Every way the focus moves is covered now: the
  channel pads, the KEYMODE3 channel link, an undo or a paste that lands on another track, and a
  restored session.

### SC01 panel: the mockup redesign

- **The panel is drawn in the reference mockup's language**: a navy page, the knob grid as one
  rounded card with hairline dividers, chips in steel blue, cyan for anything live and a pale
  blue-grey for text that is only read. Nothing about the content moved — the same eight cells,
  nine legend slots and header text — it is the drawing layer that changed.
- **Captions are 10 px over 16 px values** (was 12 px for both). That hierarchy is what gives the
  screen its look: `OCTAVE` small over `0` big, `PROBABILITY` small over `100%` big. The three
  faces were already embedded, so nothing new is stored.
- **A step lane under the header** (37..66) shows the edited track's own pads: one chip per step of
  the edit window, lit for a step that carries notes, the playhead in cyan, the step numbers under
  them. It is the pads' picture, read from the same functions (`seqStepViewBase`, `seqStepUsed`,
  `seqStepPlayPad`), and a step repaints two chips rather than the row. New panel-only call:
  `scrStepLane()` (a no-op on the 128x64 strip, where the pads already are the picture).
- **The header's right slot is a 10 px status slot**: `TRACK 02 - STEP 08/16` (or `- SILENT`),
  with the temporary message taking the slot while it is up — so the announcement no longer needs
  a line of its own and the name is never disturbed. `scrStatus()` feeds it from `Display.cpp`,
  which rebuilds it only when the track, length, window or playhead changes.
- **A help row** (199..236): a pictogram, the page's name and one line saying what the page is for —
  `GENERATOR` / `Writes a pattern into this track`, `PROBABILITY` / `Chance the step plays`,
  `TIE` / `Holds the step into the next one`. The line is looked up from the header's own label in
  a table in `ScreenPanel.cpp`, so it follows whatever page the screen is on; a page the table does
  not know gets its name only.
- **The legend is 3 + 3 + 4** — the mockup's split: 150 px chips nine apart in the first two rows,
  111 px eight apart in the last — and an empty slot keeps its column standing as an outline
  instead of pulling the row left.
- **The mode row is gone**: the family is named in the header (as it always was on the 128x64
  strip) and the three mode pads switch it, so its 32 px went to the lane, the help row and the
  taller chips. `scrModeRow()` is still called — the help row falls back on the family when the
  header shows a page name instead.

### SC01 panel: the temporary message has a line of its own

- **The header is two lines**: the 16 px label / status line on top, and the temporary
  announcement (2.5 s) under it on **a line of its own in the 10 px Nasalization cut**
  (`kFontNasa10`, already embedded), starting at the panel's left edge instead of a right-aligned
  tail that shrank to the grid size or squeezed the label when it was long. The band is 58 px
  and the accent rule sits at its bottom.
- **The knob rows paid for it**: a cell is 67 px instead of 75 (caption 10..22, value 29..41,
  its wrapped second line 42..54 - still a dozen pixels of air at the bottom), and the on-screen
  buttons are 24 px instead of 26 on 32 px mode and legend rows. Dial rings are 48 px across
  (was 52) with the caption still in the notch, and the ENVELOPE / LFO row moved up with the
  cell (curve 7..38, bars 43..48, readouts 51..63). The ENCMODE4 row preview follows the new
  cell height.

### SC01 panel: the grid face is 12 px

- **The mode row, every cell caption and value and every legend button are drawn two pixels
  taller**: the grid face moved from the 10 px Nasalization cut to the 12 px one
  (`kFontNasa12`, embedded from `fonts/nasa12.bin`), and the cell's lines moved up with the
  shorter line height (13 px - caption at 10..22, value at 29..41, its wrapped second line at
  42..54).
- **12 px is the largest cut the fixed furniture takes**, measured on the face: the widest
  caption (`INPUT CHANNEL`) is 104 px inside the 108 px text box, the widest legend
  (`PROBABILITY`) 83 px in the 104 px button, and the mode labels fit with room
  (`SEQUENCER` is 79 px).
  A 14 px cut would put `INPUT CHANNEL` at 122 px, so captions would lose their tails. The
  16 px header band is untouched. `ScreenPanel.cpp`'s type comments now carry the measured
  numbers instead of the old 8 px-per-character estimate, and the ENCMODE4 row preview draws
  its captions at 12 px to match the panel.

### `DRIFT`: a second progression from an existing one

- **New `POLY` track type `DRIFT`** (type value 6, after `SKANK`): it reads the `FOLLOW` feeder's
  **stored chords** as a progression, cuts them into blocks (one chord plus how many steps it lasts),
  fits the blocks onto its own pattern length and writes a related progression into its own steps.
  `M3` derives and writes (one undo snapshot, the track's `FILL`/`ALGO` rolls the rhythm), `M3` hold
  undoes. Editing the feeder's notes or length re-derives only the steps the track already uses;
  the feeder's speed never matters, because the map counts steps, not time.
- **Three answers** (`M2` → `F1 BASE`, `ENC3 = HOW`): `SAME` repeats the feeder chord through a
  block, `WALK` lands on it then leads into the next one (the dominant on the last step of a block
  with room), `REPLY` replaces it with a related chord drawn from the genre grammar while `ANCHOR`
  keeps the original. `F3 FIT` sets `SPAN` (`AUTO` keeps the feeder's rhythm of chord changes, 1–8
  forces a fixed block length) and `READ` (`CHRD` reads the whole chord, `ROOT` takes the lowest note
  as the root). `F2 VOICE` carries this engine's own `SHAPE` / `SIZE` / `VOICE` / `INV`.
- **`RHYTHM` decides how a block fills its steps** (`M2` → `F3 FIT`, `ENC3`): `BLOCK` puts the chord
  on a block's first step only and leaves the rest empty, so one block is one chord with air after
  it — `POLY` holds a chord only for its gate and cannot sustain across steps, so this is what makes
  the derived progression read as harmony instead of a pile of repeated stabs. `FILL` keeps the old
  behaviour (the track's `FILL`/`ALGO` roll, the HARM bake's rhythm) and `STAB` puts a chord on every
  step. `FILL` and `ALGO` only matter under `FILL`. Changing `RHYTHM` re-lays the pattern at once and
  takes the undo snapshot (the other page knobs keep re-deriving in place), so `M3` hold restores the
  previous rhythm.
- **`F4 = APPLY` fits chords to a rhythm you already wrote** (`M2` → DRIFT page): the pass a feeder
  edit runs, on demand — every step the track already uses gets the chord the engine places there,
  while its activation and rhythm stay exactly as they are (no `FILL` roll, no step switched on or
  off, a `BLOCK` track keeps its block interiors). It keeps the current `REPLY` answer and takes the
  undo snapshot, so `M3` hold restores the previous chords; the header reports `Fitted N Steps`.
- Chains work because the result is stored: `A → DRIFT B → DRIFT C`. `FOLLOW` off makes a DRIFT
  track read its own chords instead. The page settings and the written steps are RAM only, like the
  other generators; an older build reading type 6 shows the track as `STEP`.

### `SEQ` / `DRUM`: picking another voice lands back on step edit

- **A drum-voice pick returns the pads to step edit**: with an attribute target armed (say
  `PROB`, lines on the snare), choosing another part — the `M1` part-select overlay or a keyboard
  pad — now comes back on the **`STEPS`** target with the edit radio still on, so the next step tap
  places the new voice instead of editing the previous voice's attribute. Picking the voice you are
  already on changes nothing. The reset lives in `stepEditVoiceLand()` (`Events.cpp`) and is called
  from `drumPartSet()`, the single funnel every voice pick goes through, so it only ever fires in
  SEQMODE on a DRUM track.

### `HARM`: the chords follow their own track's octave

- **Turning the keyboard octave on one track no longer transposes every `HARM` track**: the
  harmonizer built its chords in whichever octave the keyboard happened to be on, so a turn on any
  track moved every HARM track's chords — baked, auto re-baked and live-followed alike. The register
  is now the **HARM track's own channel octave** (its per-track KEYMODE octave, default 3) plus the
  **feeder sequence's playback octave shift** (ENC1 on the bass/arp track), so shifting the bassline
  up carries the harmony with it while an octave turned on an unrelated track leaves the HARM track
  alone. One new reader, `keyOctaveOf(ch)` in `Keyboard.cpp`, exposes a track's octave without the
  keyboard sitting on it.

### `ENVELOPE` / `LFO` filter: framed in its own rectangle

- **The filter graph now sits in a rectangle the size of the `ENC1` cell**: the curve is inset four
  pixels inside a 116 x 71 frame that fills the container, instead of floating in the row's upper
  half, so it reads as its own little instrument panel. The plot is taller as a result (69 rows
  against 48), so the passband, the peak and the fall all gain resolution; the curve may reach the
  frame's top and right sides, so only the left keeps its air. The dB window is now symmetric
  (+32 / -32 dB) instead of +24 / -32, which puts the passband — the flat run the curve starts
  with on the left — on the frame's **middle row**; it used to sit four rows above centre.
  The resonance's height and the curve's right-hand end are the two points below.
- **The curve is the module's own LP24 response, drawn tight**: a 4-pole Butterworth
  (`|H|^2 = 1 + u^8`) on the log-frequency axis — flat at 0 dB up to the corner, exactly −3 dB (0.707) at
  it — with the slope fixed at 24 dB/oct, because the slope belongs to the filter and not to the knob.
  At this width the fall is fifteen of the curve's 111 columns: a tight knee and a wall-like drop that is
  still a curve, never a corner or a vertical line. Earlier revisions varied the order with the cutoff
  (1, 2, 4, 8, 16 poles) chasing the look, which made the shape either a hill or a cut; the knee's radius
  is what reads as "curvy", and 24 dB/oct is where that radius and the steepness both land. With the
  resonance at 0 there is no bump at all.
- **A maxed resonance touches the top side of the frame**: the bell is a stated 32.5 dB of amplitude over a
  response that is already a decibel or two down at the corner, i.e. the 31.9 dB that puts the spike's tip
  one pixel under the frame's top line, with no clipping in the 128 x 128 cutoff/resonance sweep. It was a
  14.5 dB bump before. The curve uses the frame's whole interior (rows 3–71, still symmetric about the
  passband's row 37), so the tip has the top line to reach.
- **The fall ends on the bottom row, and the floor is not drawn on**: the line closes where the response
  leaves the dB window and stops there, instead of running along the bottom to the frame edge, where it
  read as part of the border. The cutoff's top value puts the corner at 2.6 of the three decades — the far
  right — so a wide-open filter is a horizontal line across the frame whose knee and fall land in the
  bottom-right corner.

### `LFO` row: readouts flush right, and the filter peak keeps its headroom

- **The `LFO` row's four readouts are right-aligned**: the last value ends 4 px from the panel's
  right edge and `DEPTH` / `RATE` / `SHAPE` / `SYNC` run left from there, instead of starting under
  `ENC2` and sliding about whenever a value changed width. Each pair is still `label: value` on one
  line.
- **The filter curve sits a little lower, so a maxed resonance is a spike and not a flat top**: the
  band's headroom above the passband is 24 dB instead of 16, and the peak is a stated 20 dB
  (`FLT_DB_PEAK`) instead of a multiple of the headroom. At resonance 127 the peak now lands about
  seven of the band's 48 rows below the top on the panel (and clear of the top row on the OLED)
  rather than one row under it, where it read as truncated; the passband moved down with it, from
  row 17 to row 22.

### `ENVELOPE` / `LFO` row: the filter curve actually falls

- **The filter is drawn in dB, so the roll-off is a fall instead of a flat line at the end**: an
  amplitude plot of a low-pass cannot look right on this half — the tail plunges and then
  approaches zero asymptotically, which drew as a short horizontal run along the bottom of the
  band. The curve is now a passband at 0 dB above the middle of the band, a resonance peak using
  the headroom above it, and a fall below it; the passband itself leaves the band 32 dB down,
  so the curve **ends on the bottom row** where the response runs out rather than trailing along
  it. The cutoff's corner was also pulled in (`0.18 .. 1.78` of the three decades) so the fall has
  the same readable width at every cutoff instead of becoming a wall at the top of the dial and
  ending a quarter of the way across at the bottom. Same maths on the OLED strip and in
  `tools/env_row_preview.html`.
- **…and the passband no longer meets the fall at an angle**: the roll-off was `max(0, f - corner)`,
  which is flat right up to the corner and then straight — a drawn angle, not a curve. It is now
  easiest in with a soft knee (`FLT_DB_KNEE`, 0.16 decades), so the passband bends progressively
  into the fall and reaches the full slope only past the corner: no step between two columns is
  more than 3 of the band's 48 rows, and the peak is stated in dB so it keeps its height whatever
  the headroom is set to.

### `FLAVOR` per track type — and it finally survives a reboot

- **The row flavor is now kept per TRACK TYPE as well as per F bank**: DRUM, MONO and POLY
  each remember their own four flavors (twelve values), and the set that applies is the **edit
  channel's TRACK** — the same track whose CCs the row shows. A drum page can draw `LEVEL`
  bars while the poly page on the same channel keeps its `ENVELOPE`, and turning `TRACK`
  brings that type's own choices back instead of overwriting them. The `F1–F4` legend labels
  follow the edit track too.
- **The flavors were being overwritten by the brightness setting**: both the FLAVOR block and
  the device-settings record (LED % / screen %) lived at EEPROM address **2184**, so the first
  LED or screen brightness change replaced the flavor magic and every boot fell back to
  `DIALS`. The setting moved to **2200**, the flavor block is now 14 bytes at **2184..2197**
  (`magic 0xD0 | 3 × 4 flavors | checksum`, with a new magic because the old 6-byte layout has
  a different length), and `CFG_EE_BYTES` grew to 2208 to cover both.
- **`ENCMODE4 + REC` now saves the flavors with the CC values**, instead of CCs only — the
  REC is the one save gesture on that page, so it stores everything the page owns. The
  release-time write stays, `encFlavorHold(false, …)` also flushes when a mode change drops the
  menu, and `loop()` calls `encFlavorFlush()` so a pending edit is committed as soon as no F pad
  is held (still one write per edit, never mid-turn).

### `ENVELOPE` row: a readable resonance peak and a real envelope knee

- **The filter curve now has a visible resonance peak and a rounded corner**: the passband was a
  flat line that met a brick wall at a right angle, with the bump buried in the wall. The shelf
  now sits at half the band with the top half left as headroom, the resonance lifts a broad hump
  out of it at the corner (a wider shoulder below the corner than above it), and the 24 dB/oct
  drop is a quartic knee instead of `1/sqrt(1 + x^48)` — it still reaches the floor a third of a
  decade past the corner, but the bend is a curve, not a corner.
- **`ATTACK` at 0 is now an instant jump**: the attack segment had a 5 % floor, so even a zero
  attack drew a 12 px ramp and the stage always kept an angle. It is one pixel now — the note
  starts at full level — and the knob stretches it from there. The rest of the shape is
  unchanged: straight rise to the peak, bending decay to the sustain level, flat hold, bending
  release back to zero, with the axes as they were (time left to right, level bottom to top).

### The F-bank `FLAVOR`s survive a power cycle

- **Each bank's row flavor (DIALS / BARS / ENVELOPE / LFO) is now remembered at boot**: it was
  RAM-only by design, so a reboot always drew dials and every bank had to be set again. The four
  bytes live in their own EEPROM block after the CC values — `magic | 4 flavors | checksum` at
  **2184..2189** — and are written **when the F pad being edited is released**, not on every
  detent: one NVS commit per edit instead of one per click, and no knob turn ever waits on the
  store. `encFlavorLoad()` runs at boot next to `encCcLoad()`, before the first draw. A blank or
  corrupt block (a board that never stored one, or an older firmware) leaves the dials exactly
  as before, and the block is separate from the CC block so neither can invalidate the other's
  checksum.

### `PLAY` works in KEYMODE and ENCMODE

- **The transport's `PLAY` button now starts and stops the sequence in `KEYMODE` (all four
  variants) and in `ENCMODE` 1/2/4**, not only in SEQMODE and KEYMODE4: the sequencer keeps
  playing across a mode change, so the button that started it has to travel with you instead of
  going dead on the page you switched to. Same Start / Stop-while-INT behavior as SEQMODE.
  **ENCMODE3** keeps its documented exception — there `PLAY` toggles the clock source
  (`INT` / `EXT`) and never touches the transport.

### Panel knobs: the list knobs step at the shared rate

- **Every list / enum knob now steps through `encStep()`, not a hard-wired 4 counts**: on this
  panel the knobs are **detentless AS5600s driven at 96 counts a turn**, so the old `4 counts =
  1 value` blocks meant **24 values per revolution**. SEQMODE3 `SPEED` / `TRACK` / `TYPE`,
  SEQMODE4 `EXT INPUT` / `SOURCE`, the ARP page's `SPAN` / `MODE` / `INST` / `ORDER` / `STEP` /
  `PIVOT` (and every other non-% ARP slot), the BASS `TONES` / `RANGE` / `LOW` / `ALGO` and the
  LEAD `CONTOUR` / `GD` knobs, `FOLLOW`, the three live-record knobs, `PAD`, `MAP`, swing style,
  `INPUT` and the per-step `TYPE` / `RATCHET` arms now move **one value per 12 counts (8 a
  turn)** on the panel and keep one value per detent on the Teensy's quadrature knobs. The BASS
  options were the worst of them: they ran through `encDrain` (1 count = 1 unit), so a four-value
  list passed 24 times in one turn.
- **Wide stepped ranges keep their old rate on purpose**: `PAT LEN` (0–64), `VELOCITY` and
  `ACCENT VEL` (1–127), the generator's per-step `OCT` / `BRW` / `PROB` and the `SHIFT` rotation
  stay at 4 counts per value — 8 a turn would make a 127-value sweep 16 turns of the knob.

### `FLAVOR` row: slower knob, taller filter, tight readouts

- **The `FLAVOR` knob now steps at the shared list-knob rate**: the hold-F knob advanced one
  state for every raw encoder edge, so a flick walked DIALS → BARS → ENVELOPE → LFO in one
  movement. It goes through **`encStep()`** now — the helper GENRE / SCALE / PHRASE use — which
  is one detent (4 counts) on the Teensy's quadrature knobs but a wider **12 counts** on this
  panel's detentless AS5600 knobs: 8 states a turn instead of 24, i.e. a real quarter turn per
  flavor, was a rate this list was never tuned against. The residue is kept per F bank, so
  changing pads cannot spend a turn in progress. Still no wrap at either end.
- **The filter is drawn in the `ENC1` column only, and taller**: it used to span a cell and a
  half at the ADSR's 36 px; the width it gave up went back as height (48 px), so the LPF24
  shelf, resonant bump and wall read as a real curve instead of a flat strip. Same maths, and
  the right half (ADSR or LFO) is untouched.
- **The `F1–F4` buttons now name their bank's flavor**: the control legend read a bare
  `F1 F2 F3 F4`, so which bank drew what was only visible by arming it. Each button now
  carries that bank's row flavor — **`DIAL` / `LEVEL` / `ENV` / `LFO`** (the spelled-out
  `ENVELOPE` / `BARS` would not fit the OLED's 25 px slot) — on ENCMODE 1/2/4, with the armed
  one still filled. The NEXT Live overlay (where the pads are clip actions) and ENCMODE3's
  channel picker keep the plain `F1..F4`.
- **The LFO readouts are `label: value` on one line** (`DEPTH: 75%`, `RATE: 1.2HZ`,
  `SHAPE: RANDOM`, `SYNC: BEAT`): the value used to be right-aligned at the end of a 90 px
  slot, which read as four loose numbers. The pairs are measured and laid out from the `ENC2`
  column, leaning left over the free space beside the filter if they need it. The 128 px OLED
  keeps them stacked, but the value now sits left-aligned directly under its caption.

### `FSCALE` on KEYMODE's ENC4

- **KEY 1's ENC4 now dials `FSCALE` (force-to-scale)**: the slot idled at `--`, but the
  switch it controls was never KEY 2-only — with it **ON** the sequencer snaps every stored
  step into the current scale **as it plays it** (the stored notes stay raw), so turning
  `ENC2` (scale) there recolors recorded progressions live; **OFF** plays a recording
  exactly as stored. It is the *same* global state as KEY 2's `ENC4` and KEY 4's `PREV` row
  / TONALITY bank 1 (`chordFScale()`), so the knob that was turned last decides, and the
  value line reads `ON` / `OFF` like the other two.
- **Nothing else moved**: KEY 3 keeps `ENC4` = `KBOARD` (its channel-link switch) and
  KEY 4's main row keeps `ENC4` = `MOTION` (`FSCALE` stays on its `PREV` row). No LED pass
  on the turn — FScale changes what sounds, never which pads are lit — and no settings
  format change (the switch is RAM-only, as it always was).
- Docs follow the labels: cheat sheet, bindings reference, keyboard-modes page and the
  status table.

### `GATE` / `VEL` on every track type

- **`GATE` (ENC5) and `VEL` (ENC6) now shape DRUM and POLY playback too, not only MONO**: the
  row has always *labelled* those two slots `GATE` / `VEL` on every track, and with the
  **step-edit radio armed** the knobs now turn on a DRUM or POLY track as well — they edit the
  **edit channel's** own values and the track **hears** them: notes leave at the track's `VEL`
  and release at the `GATE` % of the step, exactly the shaping MONO always had (accented hits
  still use the accent velocity).
- **Compatibility — DRUM/POLY note velocity changed**: those tracks were sent at a hard-wired
  `KEY_VEL` **100** and now use the track's `VEL`, which **boots at 90** (the value MONO has
  always used). `VEL` has been saved per channel in the SEQ block all along — it simply was
  not read on playback — so a channel you once dialled as MONO keeps the value you set and an
  untouched one now plays at **90**. `GATE` boots at **100 = hold to the step boundary**, so
  gate length is unchanged until you turn it. A side benefit: the accent velocity (boot
  **101**) now sits clearly above the default instead of one count above it.
- The knobs keep the **radio gating** (they only turn with the M1 edit radio on) and still
  **yield the slot to whatever owns it**: the DRUM `RATCHET` target keeps `ENC6` for its armed
  count, POLY's `EMB` target keeps `ENC5` / `ENC6` as `SUB` / `INV`, and an open M2 page keeps
  its own bindings. A **ratcheted, strummed or COMP-stabbed** step keeps its own release
  timing — those queues own the voice, so the gate timer stays unarmed on them (firing would
  cut the repeats short, the same reason MONO's TIE/GLIDE steps skip it).

### The BASS page drops the GNR8 name

- **The MONO/BASS M2 page is now called `BASS` everywhere**: the control legend reads `BASS`
  (it said `GNR8`), and the docs and comments use the name too (`docs/generators/gnr8.md` is
  now `bass.md`). GNR8 was the shorthand from when this was the *only* generator — with LEAD,
  HARM, SKANK, ARP and the drum styles around it no longer meant anything, while the track
  type has said BASS all along. No behavior change: the header still reads `GENERATOR`, M3
  still generates/undoes, and saved settings are untouched (`SEQ_EE_LEN_GNR8` keeps the
  historical constant name).

### ENCMODE: `PREV` opens a CC CONFIG page

- **A `PREV` click in ENCMODE now opens a `CC CONFIG` page**: the eight knobs listed as
  `ENC1 CC17` … `ENC8 CC4` — the numbers the dials can only fit as a 4×6 caption in the
  ring's bottom notch. The page reads the live `encCcNum()`, so `F1–F4` and the `M3` page
  group move what it prints, and the header names the encoder channel it sends on. It is
  informational: every knob keeps sending while it is up. A second click returns to the
  dials, and the page closes with the family like every other page; `PREV`'s control label
  reads `CONFIG` while the family is up.
- **The map behind those numbers is now a file you can edit**: `CcConfig.h` holds the four
  fixed CCs and the 4×4 block per page group, and `encCcNum()` only looks numbers up there —
  a remap therefore moves the MIDI send, the dial captions and the `CC CONFIG` page at once,
  with no other code touched. Entries are clamped to **CC 1–36**, the range the
  per-CC-per-channel value store (RAM + the 579-byte EEPROM block) can hold.

### ENCMODE4: a `FLAVOR` for each F bank's second row

- **The `ENC5–8` row can now be drawn four ways in ENCMODE4** — `DIALS` (as everywhere),
  `BARS` (a level bar per CC, which reads better for a volume or expression number) and
  `ENVELOPE`, which splits the row with a blank band down the middle: the left half (under
  `ENC1`/`ENC2`) draws a **24 dB/oct low-pass** — a shelf at about half the band's height, a
  resonant bump at the corner (cutoff moves it, resonance lifts it), then the steep drop — with
  no readout under it, since those two numbers are the dials directly above; the right half the
  **ADSR** of `ENC5` attack, `ENC6` decay, `ENC7` sustain, `ENC8` release at half the row's
  width, with a straight attack, bending decay/release, and a tight bar plus **A / D / S / R**
  under each stage. The envelope is laid out left to right, so the release's end moves with the
  release knob and stops short of the row's right edge. The filter's slope is drawn as a wall
  (the plot is deliberately steeper than the 24 dB/oct it describes), so the end of the picture
  reads as a low-pass rather than as a diagonal. Each F bank keeps its own flavor, and the row
  draws the armed bank's: the F hold (and the FLAVOR knob itself) arms the bank being edited, so
  the presentation you pick stays on screen and comes back with that bank after leaving and
  re-entering ENCMODE instead of showing another bank's `DIALS`. Holding an F pad to set the flavor arms that bank, so the row keeps the
  flavor you chose instead of falling back to the previously armed bank's dials.
- **A third flavor, `LFO`, splits the row like the envelope's and draws a still waveform on
  the right**: the filter (same LPF24, same width) stays on the left under `ENC1`/`ENC2`, and
  the right half carries the waveform plus its four values. `ENC5` = depth
  (`0–100 %`, a flat line at 0), `ENC6` = rate (`0.01–10 Hz`, also the number of cycles
  squeezed across the row), `ENC7` = shape (`SINE` / `TRI` / `RISE` / `FALL` / `SQUARE` /
  `RANDOM`, the last a stepped sample-and-hold), `ENC8` = sync (`HZ` free-running, `BEAT`
  whole cycles with a rule at each boundary, `TRIG` one shot: a single cycle as wide as one
  cycle at the current rate, then the flat line it leaves behind, with a rule where the shot
  ended — so `ENC6` still changes the picture with `TRIG` selected instead of every rate
  drawing the same full-width cycle). Nothing moves — the knobs only change the picture — and
  each readout prints what its value means (`DEPTH 75%`, `RATE 2.53HZ`, `SHAPE RANDOM`,
  `SYNC BEAT`) instead of the raw 0–127 the dial above it already carries, caption and value
  on **one line** in four 90 px slots that start at the `ENC2` column and use the empty band
  under the filter's tail (the curve ends above it) — `SHAPE RANDOM` does not fit the ADSR's
  52 px column, and the whole line would have to be two lines there. The mapping lives in `Screen.h`, so the panel, the OLED and the
  readout text cannot disagree.
- **The flavor follows the bank across the family**: it is drawn on **ENCMODE 1, 2 and 4**, not
  only on 4 — clicking from ENCMODE4 back to plain ENCMODE used to repaint the second row as
  dials (the draw was gated on `modeVariant == 4`), which read as the flavor resetting even
  though the bank still held it. Only ENCMODE3, whose pads are the channel overlay, keeps its
  label/value screen; the `FLAVOR` menu itself is still the variant-4 F hold.
- **Hold an F pad and the row previews that bank with `ENC1` = `FLAVOR`**: one state per
  turn, no wrap, every other knob idle while the hold lasts; releasing puts the row back on
  the armed bank. One flavor per F bank, RAM only — and it is a drawing choice, so the CC
  numbers, the stored values and the MIDI send are exactly what they were.

### Leaving a page (or moving TRACK / TYPE) lands back on step edit

- **Closing an M2 page with its own `M2` press — and turning the `TRACK` or `TYPE` knob — now
  lands back on step edit**: the edit radio comes back on with **M1 (STEPS)**, exactly like
  choosing a channel already did. Previously a DRUM pattern detour (or ARP / BASS / LEAD /
  HARM / COMP / SKANK) left the radio off after the page closed, and a `TRACK` / `TYPE` change
  dismissed the pages it had to without ever touching the radio — either way the player had to
  hunt for `M1` to get the step pads editing again. A radio that is already on keeps its
  target, so the knob that moves a track never steals a live edit session
  ([ADR 0004](docs/adr/0004-one-radio-at-a-time.md)).

### M1 closes an open M2 page — every track type

- **Pressing `M1` while an M2 page is open now closes the page on every track type**: a quick
  tap on a DRUM track with the **DRUM pattern page** up (or HARM / COMP / SKANK on POLY) drops
  the page and arms the edit radio in the same press, and arming an **F1–F4** target does the
  same. The one-radio rule ([ADR 0004](docs/adr/0004-one-radio-at-a-time.md), already promised
  by the cheat sheet) only ever ran on the MONO branch and only while the radio was off, so the
  DRUM/POLY paths could leave a page sitting under an armed radio — from there only the
  page's own `M2` press would leave it.

### SEQMODE3: KEY button releases the committed keyboard row

- **Touching the `KEYBOARD` mode button now immediately releases the SEQMODE3 bar/verb
  copy/paste latch** (`seq3KbRelease()`), so the keyboard row shows the keys again instead of
  staying on the overlay until a channel happens to be picked on KEYMODE3. LEDs refresh with
  it. Previously the latch only let go by selecting the keyboard's own track on `KEYBOARD 3`.
  Holding SEQ into SEQMODE3 itself is unaffected (the row is up for as long as the hold lasts).

### SEQMODE3: `PAD` knob on DRUM tracks (ENC3)

- **`ENC3` on a DRUM track in SEQMODE3 is now `PAD` instead of `FOLLOW`** — a drum has no
  follower job, so the slot picks what a keyboard-row press does while the keyboard sits on
  this track: **`LIVE`** (boot default = the original behavior: the press picks the drum voice
  *and* sounds it) or **`SELECT`** (the press only moves the voice cursor — no note leaves the
  port). Two-state, 4 counts = 1 state, clamped — no wrap; the header announces
  `Pad LIVE` / `Pad SELECT`. Per track, saved with SEQMODE4+REC — see
  [Compatibility](#compatibility) (the SEQ block grew 1380 → 1398 bytes; older saves load with
  PAD = `LIVE`).

### DRUM step RATCHET (F2) + accent on ENC7

- **A DRUM track in SEQMODE gains a per-voice RATCHET attribute**: **F2** switches the step-edit
  radio to the `RATCHET` target (F2 again hands the pads back to the steps; the header and the
  control legend read `RATCHET`). **ENC6** picks the armed count **2 · 3 · 4** (4 counts = 1
  step, clamped — no wrap); tapping an active step **stamps** that count onto the step's
  **selected drum part**, tapping the same part again **clears** it (header `C1 Ratchet 3`,
  then `C1 Ratchet off`). Every voice keeps its own count: only the stamped part re-triggers
  (hit 1 lands with the step, repeats spaced `step ms / count`), the kick and snare beside it
  play straight. RAM only like every step field — it rides SHIFT, copy/paste, the undo slot
  and dies at power-off; steps where the selected part rolls get the yellow attribute pulse
  and F2 blinks while the playhead reads one. M2 still opens the DRUM pattern page (F2 is the
  only new owner of that key on DRUM).
- **Accent velocity moved to `ENC7` (`ACC`) in SEQMODE 1/2/4, for every track type** — the old
  F3-hold that borrowed ENC6 is retired (one knob, one job; ENC6 keeps `VEL`). ENC7 yields to
  the POLY step-edit `FX`/`TYPE`/`EMB` targets while they are engaged and to the M2 pages that
  bind it (**BASS `TIE`, ARP `INST`, LEAD `DENS`, SKANK `TES%`**), and the DRUM pattern page /
  SEQ overlays keep their rows while open. Legend rows for SEQ 1/2/4 now read `ACC` instead of
  `--`.

### SC01 panel type: Nasalization

- **The 480x320 panel draws in Nasalization** — the OTF cannot be rendered by the panel, so it
  ships converted to LovyanGFX's compact bitmap font (1 bpp, ASCII 0x20-0x7f) and embedded in
  flash (`fonts/`), with no filesystem or SD card needed: 16 px for the header band, 10 px for
  the mode row, every cell caption and value and every legend button. The 10 px grid size is
  what the fixed furniture allows — `PROBABILITY` is 85 px at 10 px against a 104 px button.
  The built-in bitmap faces stay as the fallback if the font ever fails to load, so a bad font
  cannot blank the panel. The `.c`/`.fcfg` files in `WipidiMSite/nasalization/` are LVGL output
  and are not used here.

### `PREV` / `NEXT` LEDs

- **`PREV` and `NEXT` now light a dim yellow-green in every mode** instead of sitting dark, so the
  two page keys can be found by position without reading the screen. KEYMODE2 keeps the dim
  green `NEXT` it already had from the chord row (`reference/leds.md`).

### SEQMODE3 channel picker

- **Only `LENGTH` and `SPEED` take the step pads back from the channel selector** — those two
  change what the step row shows, so the pads return to the sequencer as before. Dialling
  `OCT`, `FOLLOW`, `INPUT CHANNEL`, `TRACK`, `TYPE` or `TEMPO` now leaves the yellow picker up,
  so several channels can be picked in one hold (`modes/seqmode.md`).

### Ableton Live bridge

- **A clip press with no link now says `Link Down Check MIDI`** instead of repeating
  `Live Link Off`, which read like the overlay closing; the overlay's own strings are
  `Live Mode On` / `Live Mode Off`. `modes/link.md` now separates the two pairs and names the
  surface's missing **Output** port as the usual reason a loaded script never answers.

- **`ENCABLETONMODE` — tap `NEXT` in ENCMODE and the instrument becomes a Live controller** —
  a latched overlay (like an ARP page, not a fourth mode family) with a companion remote
  script in `Remote Scripts/Teensy2048/`. The pads become the selected track's 16 **clip
  slots** (bright green playing · blinking green triggered · dim green has a clip · dark blue
  empty · dim red armed): press an empty one to **create and fill** it from that channel's
  Teensy pattern, a filled one to **launch**, a playing one to **stop**, double-press to
  **re-fill**. A fill is `patLen × 0.25` beats with notes at the stored pitch **+ 12 × octave
  + transpose**, duration `0.25 × gate%`, TIE ringing through the following empty steps,
  GLIDE +0.02 beat, velocity accent-aware and PROB as the note's probability. **PREV** (hold)
  turns the pads into the 16 **tracks**, **M3** swaps **F1–F4** to `FILL · SCENE · STOP ·
  ALL`, **F1/F2** page the parameter bank and **F3/F4** step the device. The knobs auto-map to
  the selected device's eight parameters on the **new CC block 64–71** — ENCMODE's CC 17–20 /
  CC 1–36 map, its dials and its per-CC-per-channel memory stay **byte-identical**, so every
  MIDI-learn map keeps working; a track with no device shows its mixer instead. Traffic is
  SysEx only (`F0 7D 37 32 34 38 … F7`, ≤ 32 bytes, chunked pattern dumps, HELLO retried
  every 200 ms for the first 5 s and then every second, with **any** frame back — not just the
  `ACK` — counting as the link being up), so a port that also carries Ableton's clock is
  unaffected; state-only, never clock. The companion script ships as-is and is never edited
  here: the phone app loads the same files, so bridge behaviour is fixed in firmware.
  `Link.h/.cpp` (new), `Keyboard`, `Bindings`, `Modes`, `Events`, `Display`, `SeqClock`,
  `QESSEQ16.ino`, `Remote Scripts/Teensy2048/` (new), docs (`modes/link.md` new); patterns
  and overlay state are RAM only — **no EEPROM block change**.

### Sequencer

- **Layer panel: `MUTE` never has to be re-armed, and its key breathes** - the hold-`M2`
  panel opened on `MUTE` already, but tapping `C#` disarmed it and an erase cleared the arm,
  so a layer-by-layer pass kept dropping back to the report. `MUTE` is the **resting
  selection** now: `C#` only repeats `Mute layer?`, a white key mutes/unmutes and the arm
  stays, and `ERASE` hands the row back to `MUTE` after its confirm (a second `D#` also
  goes back to mute instead of nothing). The selected `MUTE` key is a **dim orange that
  breathes** - the panel was painted once when it opened, so the colour never moved; a
  30 fps repaint now runs while the row is up - and selected `ERASE` stays a static **dim
  dark red**. Muted layers themselves read dim red as before.

- **Overdub layers + the hold-`M2` layer panel** — every note a record pass writes now
  remembers **which pass wrote it**: up to **4 layers per track** (RAM only, like the
  patterns; DRUM steps stay untagged). A pass writes the **first layer that holds no notes
  anywhere**, so pre-existing content counts as layer 1, the first pass over a written
  pattern lands in layer 2, and a full set **stacks into layer 4**. **Holding `M2`** in
  SEQMODE 1/2/4 (non-DRUM edit track) opens a keyboard-row panel in the **M2 button's
  hue**: white **C D E F = layers 1–4** (bright = holds notes, dim = empty, dim red =
  muted), the black keys carry **C# = mute layer · D# = erase layer · F# = undo · G# =
  redo · A# = reset layers** — `MUTE` is armed whenever the panel opens and a white key
  confirms on that layer, `D#` arms erase for one confirm (see the note above),
  undo/redo/reset fire at once.
  **Mute** gates playback per layer (an all-muted step rests; a follower filters with the
  feeder's mask), **erase** deletes the layer's notes from every step (the rest compact,
  emptied steps lose their attributes, mirrors/HARM re-derive), **reset** flattens all
  layers back into one. Undo/redo walk **layer commands only** on an **8-deep** stack per
  direction and switch the edit channel to whatever they restore. The panel swallows its
  keys (press/release/click) so nothing leaks to the instrument, and closes on every M2
  release even across a mid-hold mode switch. `Keyboard` (`stepLay`/`layMute`/`recLay`,
  playback filter, overlay + stacks), `Events`, `Modes`; docs (`modes/seqmode.md`); no
  EEPROM change.

- **`OVERDUB` now works while `FOLLOW` is ON** - a follower re-resolves every step's
  pitches from the feeder, so notes stacked by a record pass were thrown away on the
  next lap and the knob only shaped what you heard with FOLLOW off. A step the pass
  stacked while `OVERDUB` = `ON` now **claims** itself: it plays its **own** stored
  notes exactly as it would with FOLLOW off (engine, FX, FScale, the PLAYBACK knob and
  the per-step `OCT`/`TRANS` all still apply), while every other step of the track keeps
  following the feeder. A plain record gesture (`OVERDUB` = `OFF`) never claims - it
  still gates the rhythm and the feeder still supplies its pitches, as before - and
  FREEZE plus the live HARM / SKANK refreshes lose to a claim. The flag rides in
  `stepAtt`, so a rotate, copy/paste and the undo snapshot carry it while a fresh
  gesture, a generator re-roll or erasing the step clears it; it is masked out of the
  armed-attribute LED pulse so a claimed step cannot light an FX target it does not
  carry. Patterns are RAM only, so no EEPROM change.

- **Live record gains `LISTEN` / `PLAY` / `OVERDUB` on the M1 step-edit row** (SEQMODE 1
  and 2, ENC2/3/4, per track, saved with SEQMODE4 + REC). `LISTEN` picks what the pass
  stores: `CHORD` (the gesture, as before) or `NOTE`, which writes the **live arp's own
  output** onto the step it sounds on — one note per step — instead of a single chord on
  the step where the gesture started; on a MONO track the gesture's key edges stop
  recording while `NOTE` is set. `PLAY` picks how stored steps sound: `GATE` (through the
  engine, as before) or `RECORDED`, which plays the stored notes **verbatim, all at once**
  with the engine bypassed (octave and per-step `OCT`/`TRANS` still apply; step FX, FScale,
  strum/HAND and COMP shaping are skipped). `OVERDUB` picks what a fresh gesture does:
  `OFF` replaces the step (as before), `ON` stacks notes onto it — and a MONO step that
  then holds several notes plays **all of them at once**. All three are clamped (4 counts =
  1 state, no wrap); a DRUM track shows `--`, and only the gesture you play feeds the
  recorder, never the track's own playback.

- **ENCMODE: the step pads are the channel picker in plain ENCMODE, not ENCMODE4** -
  the locked picker moved off the double-click variant, so entering ENCMODE now hands the
  16 pads to the encoder's MIDI channels (left header `CHANNEL SELECT`, bright yellow =
  the selected one, a press sets the channel and repaints the dials). **ENCMODE4 keeps the
  sequencer on the pads** - playhead, step states and triggers - so the CC dials can be
  worked while the pattern runs. Variant 3 is still the momentary hold picker, and
  ENCMODE4 + REC still saves the 36x16 CC values and channel.

- **ARP: the MODE knob now carries all nine walk orders** - `OUTIN` / `INOUT` /
  `CONV` / `DIV` existed only on the F1 `ORDER` knob, so with `ORDER = MODE` (the
  default) they were unreachable and the MODE knob stopped at `RANDOM2`. The enum
  grew past `RANDOM2` (`SEQ_ARP_OUTIN` .. `SEQ_ARP_DIV`), the knob and the value
  column follow it, and an older save's mode 0..4 keeps its exact sound. The ORDER
  knob's list is unchanged, so `RANDOM` / `RANDOM2` stay on MODE and `PLAY` still
  follows MODE for its shape.

- **SEQ 3: hold a bar pad for that bar's `OCT` / `TRANS`** - the keyboard's C D E F row
  already picks the 16-step edit window (and loops it on a second press); now the pad
  also arms an encoder row for **that bar**: `ENC1` = `OCT` (-2..+2), `ENC2` = `TRANS`
  (-12..+12 st), header `Bar 3 Transpose +5`. The stored chords are untouched - the
  shift folds into the track's own octave/`TRANS` at playback, so a `KLOCK` track
  snaps the transposed bar back into scale instead of stepping outside it. DRUM tracks
  refuse both (`Bar 2 Drum Track`): their steps are drum-map ids, not pitches. One bar
  at a time (the last pad pressed); the row drops when the last C-F pad lets go.

### Keyboard

- **KEYMODE4: the TONALITY page is four banks of eight** - `M2` opened a single
  four-knob row (OCT/SCALE/KEY/FSCALE), which is a shortcut, not tonality
  control. It now has four banks and `F1`-`F4` pick one while the page is up
  (the open bank's pad burns white, the other three drift through a slow dim
  hue cycle). Bank 1 TONAL CENTER (`KEY SCALE GRAV MOD MODTGT MODLEN FSCALE
  CADSTR`), 2 CHROMATICISM (`BORROW PREP SD MIX DIAT UNRES SURP STRICT`),
  3 VOICE LEADING (`PARAL BASS LEAP DOUBL CMNTN TOP LEAD SPREAD`), 4 STRUCTURE
  (`RATE ELIDE CADWIN CELL VAR ANTE MOTIF PHRASE`) - 26 new generator controls
  plus the six mirrors (KEY/SCALE/FSCALE/SURP/VOICE-LEAD/PHRASE), every one
  defaulting to a neutral value so the page never changes the sound until a
  knob is turned. Tonal gravity, modulation plan/target/span, cadence
  strictness, borrow budget, preparation, secondary-dominant density, modal
  mixture, diatonic bias, unresolved cost, functional strictness, parallel
  strictness, bass continuity, leap limit, doubling rule, common-tone weight,
  top-voice weight, register spread, harmonic rhythm, phrase overlap, cadence
  window, cell reuse, the variation operator, antecedent relation and motif
  length all reach the scoring, the apply pass or the variation pass.
  `Key4.cpp/.h`, `Bindings.cpp/.h` (four new sets), `Events.cpp`, `Display.cpp`,
  `Modes.cpp`.
- **KEYMODE4: M2 is a TONALITY menu, auto-advance moved to F4** - the M2 button
  carried the auto-advance toggle, which blocked the encoder-page slot the other
  modes use for their menus. `F4` **tap** now toggles auto-advance (a long press
  is still the audition-only hold), and `M2` toggles a sticky **TONALITY** page:
  `OCT / SCALE / KEY / FSCALE` on ENC1-4, the four knobs the PREV overlay otherwise
  hides behind a hold, `--` on the rest, `TONALITY` in the header's left slot. The
  M2 pad is a **slow hue cycle** - four times slower than the M3 APPLY marker, dim
  while the page is closed and brighter while it is open (KEYMODE4 now repaints at
  30 fps so both markers actually move; the M3 cycle was being sampled once per
  repaint before). The page dies with the mode, like the EMB row. `Bindings.cpp/.h`
  (`ENC_KEY4_MENU`), `Events.cpp`, `Display.cpp`, `Key4.cpp/.h`, `Modes.cpp`.

- **The keyboard on a `DRUM` track is a drum pad row** - when the keyboard channel
  points at a DRUM sequence the twelve pads stop being pitches: the scale filter, the
  KEY/SCALE colors and the `KEY 2` / `KEY 4` chord engines all stay out of it, and the
  row sits in one **dim OrangeRed**, blinking **dim green** on the pad whose note the
  playhead is sounding and reading **dim orange** on a voice muted in the ENCMODE drum
  overlay (mute wins over the blink; touch still flashes white). The keyboard **octave
  snaps to 1** on landing there - the octave the kit's own notes sit on (`C1` = 36
  through `B1` = 47) -and the octave is **per track**: every track keeps its own KEYMODE octave (default 3, a DRUM row defaulting to 1), so the snap and any knob turn on one track never move another, while the octave knob still transposes the drum row. Pressing a
  pad also **picks that drum voice** for the step edit, the direct form of hold `M1` +
  step N (`C` = part 1, `C#` = part 2 … `D#2` = part 16).

- **KEY 4 phrase templates are per-bar, so `AABB` / `ABAB` / `AABA` differ** - the
  restating templates used to copy the first half verbatim into the second, which made
  all three produce the same `AA` pattern and left only `ABAC` distinguishable. They
  are a four-letter map over bars now: a bar whose letter has already played replays
  the first bar that carried it, so `AABB` runs one cell for bars 1-2 and a repeated
  second cell for 3-4, `ABAB` alternates, `AABA` returns to the opening cell for its
  last bar, and `ABAC` repeats just bar 3 from bar 1. The replay goes through the
  normal write path, so the chain advances with it and the next new bar answers the
  repeated material; each bar's planned landing and any locked step are never
  overwritten, which keeps the cadence arc intact.

- **KEY 4: `PHRASE` promoted to the encoder row, `ERA` moved to the EMB page** - the phrase
  template decides whether the pattern has an arc at all, so it no longer sits two gestures
  deep (`NEXT` tap, then `ENC3`): it takes `ENC7`, the slot `ERA` used, and reads out by name
  (`FREE`/`AABB`/...). `ERA` keeps its value and now lives at the EMB page's `ENC3`, where a
  turn regenerates the unlocked steps immediately instead of waiting for the next row turn.
- **KEY 4 phrase templates** - `FREE` / `AABB` / `ABAB` / `ABAC` / `AABA` (EMB page
  `ENC3`) give the progression generator a **goal**: the cadence the knob selected is
  planned for the last bar, every earlier bar end is derived **backward** from it, and
  the forward pass scores those landings x8. `FREE` (the boot default) keeps the old
  causal chain. After M3/F1 the header says which layer decided the phrase end
  (`Plan V-vi PLAN|CAD|GENR`) - arc-level only, the scoring terms are not attributed.
- **Named readouts for two KEY 4 knobs** - `MOOD` reads DARK/COOL/MIXED/WARM/LIGHT and
  `CAD` its cadence pair (`V-I`/`IV-I`/`V-vi`/`ii-V`). The `CADENCE` knob is now a
  **four-band selector** (HALF `ii-V` under 25, PLAGAL `IV-I` under 50, AUTHENTIC `V-I`
  under 75, DECEPTIVE `V-vi` above): a seeded weighted roll left the type unchanged
  while the knob moved, which read as a dead knob. Its continuous value still scales the
  cadence terms.

- **KEY 3 `ENCOD` / `KBOARD` boot `locked`** — the encoder channel and the keyboard
  channel follow the SEQ edit channel (the hub) from power-up, instead of each keeping
  its own channel until you turn the link on. Turn the knob CCW to go back to `free`.
  A save written before this change holds no explicit choice, so it boots locked too;
  the first turn of the knob records yours.

### Documentation

- `README.md` split into a one-page landing page and a `docs/` tree — the old page was
  103,568 characters on 599 lines with no table of contents. Prose was moved verbatim; see
  [ADR 0008](docs/adr/0008-docs-split.md).
- New pages: [start here](docs/start-here.md), [glossary](docs/glossary.md),
  [cheat sheet](docs/cheatsheet.md), [panel map](docs/panel-map.md),
  [concepts](docs/concepts.md), [feature status](docs/status.md),
  [recipes](docs/recipes/index.md), [FAQ](docs/faq.md),
  [limitations](docs/limitations.md), [style guide](docs/style-guide.md),
  [bindings reference](docs/reference/bindings.md) and eight [ADRs](docs/adr/README.md).
- `AGENTS.md` now points at `docs/` as the reference instead of `README.md`.

No firmware behaviour changed in this entry.

## Feature timeline

The first commit whose diff mentions each surface, with the commit subject quoted as it was
written. Subjects often describe the broad change rather than the feature, so read the date
as "this is when it appeared", not as a release note.

| Date | Commit subject | Surface |
|---|---|---|
| 2026-09-25 | `first up` | 48-pad MPR121 scan, OLED, sequencer skeleton, USB-MIDI |
| 2026-09-28 | `scales update` | scale table and the chord engine (`Chords.cpp`) |
| 2026-11-01 | `Swing, pattern edit` | per-track swing style and amount |
| 2026-09-30 | `mono channel implementation` | per-channel step edit; the channel variants (`SEQ 3`/`4`, `ENC 3`/`4`) |
| 2026-10-03 | `64 steps` | 64-step patterns, bar loop, follow |
| 2026-10-06 | `chord progressions` | KEY 4 progression engine, 18 genre tables |
| 2026-10-07 | `Strumming` | strummed chords and the HAND strum zone |
| 2026-10-09 | `bassline generator` | GNR8 page (MONO/BASS) |
| 2026-10-20 | `minor bugs` | HARM harmonizer |
| 2026-10-21 | `MONO / HARM engine` | LEAD generator page |
| 2026-10-22 | `skank track engine` | SKANK walking voicings and COMP stabs |
| 2026-10-25 | `drum patterns` | 18 preset DRUM styles |
| 2026-10-26 | `more arp options` | ARP sub-pages; encoder dial grid; 21-character header lint |
| 2026-11-01 | `live input` | `EXT IN = LIVE`, `SRC = DIN/BOTH` |

## Compatibility

Saved settings are **backward-tolerant**: a block written by an older build loads, with the
fields it did not have left at their defaults. Fields are appended, never inserted. The
block size tells you which build wrote it.

| SEQ block | Added |
|---|---|
| 260 | earliest saved layout |
| 388 | GNR8 page parameters |
| 404 | LEAD contour |
| 420 | COMP `HITS` per track |
| 436 | TRANS offset |
| 532 | SKANK knobs `TES`…`SUB` |
| 564 | SKANK randomizers `TES%` / `NOT%` |
| 567 | KEY 3 `ENCOD` / `KBOARD` link flags |
| 616 | per-channel mode memory (KEY variant, F bank + page group) |
| 649 | LEAD CT blend, GD depth |
| 665 | per-track `Klock` (7-rung speed ladder) |
| 666 | 9-rung speed ladder marker |
| 923 | DRUM page state: genre, VAR, lane lines, per-lane FILL/MOT |
| 940 | DRUM kit map |
| 1262 | ARP sub-page parameters |
| 1296 | per-track swing style and amount |
| 1330 | per-track `EXT IN` and `SRC` |
| 1380 | per-track live-record options (`LISTEN` / `PLAYBACK` / `OVERDUB`) |
| 1398 | per-track PAD mode (SEQMODE3 `ENC3` on a DRUM track: `LIVE` / `SELECT`) |

`ENC 4 + REC` is the exception: an older block is **ignored** and defaults stay, because a
half-loaded CC map is worse than a clean one.

## Release-note template

```markdown
## <date> — <what a player can now do>

### Play
- The three or four things that are new or different at the panel, in player language.

### Compatibility
- SEQ block: <old> → <new> bytes. An older save loads with <defaults>.
- Any gesture that changed, and what it was before.

### Known issues
- Open items from TODO.md that a player can hit, with the workaround.
```

Lead with the behaviour, not the files. Never claim a fix that was only compiled.
