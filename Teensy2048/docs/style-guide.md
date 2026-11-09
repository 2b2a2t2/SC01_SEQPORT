# Documentation style guide

Rules for writing a page in `docs/`. They exist so the tree stays readable as it grows —
the failure mode being replaced is one 103,000-character page with no way in.

## Shape of a page

1. **H1** — the thing, not the category: `GNR8 Generator (MONO, TYPE = BASS)`.
2. **One orientation sentence** — what this page covers, who it is for.
3. **Body** — explanation, then tables for anything with values.
4. **`Where to go next`** — two or three links, never a wall of them.

Hard limits:

- One H1 per page. Depth stops at H3; if you need H4, the page is doing two jobs.
- No page longer than roughly 200 lines. Split it and link.
- A table's first column is the thing being chosen (parameter, mode, symptom), not prose.

## Voice

- **Second person, present tense**: "Tap M2 to open the page", not "the user may open".
- **Behaviour first**: say what you hear or see, then why. Implementation notes belong in
  [Architecture](architecture.md) or [Extending](extending.md).
- **Name the gesture, not the abstraction**: "hold M3", not "invoke undo".
- **Say what does not happen**: the useful half of a parameter is often its limit
  ("nothing wraps", "stops at both ends", "dies with the page").
- **No marketing adjectives.** "Powerful", "seamless", "simply" — delete.

## Terminology

Fixed by the [glossary](glossary.md). The ones most often got wrong:

| Use | Not |
|---|---|
| mode family (`SEQ`), variant (`SEQ 3`) | "mode 3", "submode" |
| page (opened by M2) | "menu", "screen" |
| radio (one armed at a time) | "toggle group" |
| feeder / follower | "master / slave", "source / target" |
| stored vs derived notes | "real / virtual notes" |
| step content vs track settings | "pattern data" for both |
| announce (header message) | "toast", "popup" |
| encoder row | "knob row" |

Capitalise mode families and pad names (`MODE`, `M1`, `M2`, `M3`, `PREV`, `REC`, `PLAY`) and
`ENC1`–`ENC8` exactly as the OLED prints them. Write option values in backticks: `FOLLOW`,
`near`, `EUCLID`.

## OLED header text

The header band is 128 px / **21 characters**, and it must read as words:

- Name the parameter and its value: `Pattern Length 16`, `Accent Velocity 90`, `Kit Map GM`.
- Never the 4-character main-screen code (`LEN`, `ACC`, `PROB`) and never the shorthand the
  strip has retired (`mot`, `klock`, `GNR8` as a message).
- A scale change announces its **mood** only (`Scale Happy`), because the value line already
  shows the scale.
- Run `python3 tools/header_lint.py` after touching any message. Exit status 1 = findings:
  over-width messages, retired shorthand, or an `snprintf` worst case that would overflow.

## Cross-references

- **Link, do not repeat.** A binding is defined once, in
  [Bindings](reference/bindings.md); every other page points at it.
- **Relative links only**, no absolute GitHub URLs — the tree must work in a local checkout
  and in a printed PDF export.
- **Anchor links are checked**: a heading rename is a link break. Grep before renaming.
- Every page opens with the same nav line (`Docs home · Cheat sheet · Glossary`) so a reader
  who lands deep can get out.

## Status claims

Only four words: **Stable**, **Beta**, **Experimental**, **Absent by design**, defined in
[Feature status](status.md). A feature with an open item in `TODO.md` is Beta, whatever it
feels like. Never write "should work" — either it does, or it is Beta with a note.

## Recipes and ADRs

**Recipe** (`docs/recipes/`): goal → prerequisites → numbered steps with the exact pads and
encoders → what you should hear → variations → links. Ten to forty lines. One musical
outcome per recipe.

**ADR** (`docs/adr/`): `Status`, `Context`, `Decision`, `Consequences` — including the
consequences you do not like. ADRs are immutable: to change a decision, add a new one that
supersedes it and update the old one's `Status` line.

**Release note**: lead with what a player can now do, then compatibility (EEPROM block size
and what an older save does), then known issues. Never lead with file names.

## When behaviour changes

1. Update the page that describes the behaviour.
2. Update [Feature status](status.md) if a status or date moves.
3. Add a `CHANGELOG.md` entry under *Unreleased*.
4. Append the item to `TODO.md` if it is still open; tick and move it to *Done* when it is not.
5. `README.md`'s landing page changes only if the map changed.
