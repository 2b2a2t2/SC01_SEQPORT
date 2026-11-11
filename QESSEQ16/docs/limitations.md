# Limitations and non-goals

Things this firmware does not do, and things it will not do. The second list matters more
than the first: several "missing" features are load-bearing decisions.

## Deliberate non-goals

| Not supported | Why |
|---|---|
| Saving patterns to EEPROM | Patterns are RAM by design. The instrument is a live performance tool: what you program is for this session, and a power cycle gives you a clean slate instead of a stale set you have to clear. Settings persist; music does not. |
| A song mode / pattern chaining | There is no arrangement layer. Bar loop and follow chains cover the live cases without introducing a second timeline. |
| Quantised recording of external MIDI into a pattern | Live recording captures what you play, not a repaired version of it. |
| MIDI clock over the 5-pin DIN | Clock and transport are USB only, so two clock sources can never both advance the sequencer. DIN carries **notes only**. |
| Persisting the `KEY 4` progression | The progression is a function of (genre, mood, tension, motion, cadence, complexity, era, surprise). Storing the output as well as the inputs would let the two disagree. |
| Arbitrary per-step automation lanes | Step content is a fixed small struct; adding a lane per parameter would multiply the RAM cost of 16 tracks × 64 steps. |
| A serial console over USB | USB Type is **MIDI**, so the board exposes no tty. Diagnostics use the two serial commands on a build that has serial, plus the OLED. |

## Hard limits

| Limit | Value | Consequence |
|---|---|---|
| Pads | 48 (4× MPR121, 12 each) | — |
| LEDs | 42 | Pads 42–47 have no LED |
| Encoders | 8 | Some pages use `---` placeholders rather than a ninth knob |
| Tracks | 16, one per MIDI channel | A track cannot be moved to a second channel |
| Steps per track | 16 base, up to 64 (4 bars) | Longer phrases require bar loop |
| Notes per chord | 8 | `CHORD_MAX` |
| OLED header | 21 characters | Longer messages are pinned left, not truncated mid-word |
| Scale table | 54 entries | — |
| CC storage | 36 CC × 16 channels | Knob positions are per channel, not global |
| Key range | octave −2 … +8, default 3 (C3 = 60) | — |

## Behavioural sharp edges

These are documented because they surprise people, not because they are bugs.

- **Variant 3 restores the previous variant.** Releasing the mode pad is not "stay here".
- **A page and the edit radio cannot coexist.** Opening one closes the other, and the armed
  sub-page dies with its page.
- **Some per-step values are RAM only** even though the page knobs beside them persist:
  GNR8's OCT / BRW / PROB and SKANK's locks. Every step field behaves this way.
- **Regenerating clears step content** for the steps it writes. M3-hold restores one level.
- **An armed attribute needs its prerequisites.** A TIE span on a bass track only works
  with the GNR8 page **closed** and M1 + TIE armed.
- **`FOLLOW OFF` ignores MAP and BRW.** The line plays as generated; only the chord changes
  make a difference.
- **Mute gates the output, not the engine.** A muted track still plays internally and still
  feeds followers.
- **An EEPROM save from an older build loads with defaults for the new fields**, except
  `ENC 4 + REC`, where an older block is ignored entirely.
- **ROOT / ARP take OCT but not MAP/BRW**; STRUM's rake takes neither.

## Tooling gaps

- **No automated test suite.** Verification is compiling (`./tools/fw.sh build`) plus
  playing the instrument. The one executable check is `tools/header_lint.py`, which guards
  header width and forbidden shorthand.
- **No release tags.** The de-facto version marker is the EEPROM block size; see
  [Feature status](status.md).
- **No hardware-in-the-loop CI.** Flashing requires the board on a USB port of the machine
  running the command.
