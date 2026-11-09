# 0002 — Bindings are tables, with one overlay slot per group

**Status** — Accepted

## Context

Twelve mode/variant combinations, plus held-key overlays (PREV, a held step), plus per-track
pages (ARP, GNR8, LEAD, COMP, HARM, SKANK, DRUM), plus CC banks. Handling that with
conditionals inside the event handlers produced a dispatch that was impossible to reason
about: every new page added a branch to every handler.

## Decision

Resolution is data. A mode/variant row in `table[3][4]` names four sets — `EncSet`,
`CtrlSet`, `StepSet`, `KeySet` — and each set is a label table plus a handler. An
**overlay** replaces one group's set for as long as it is active.

Each group has **exactly one** overlay slot (`overlayOn`/`overlaySet`), so overlays cannot
stack.

## Consequences

- Adding a screen is a table row plus a handler, not an edit to every branch. The procedure
  is in [Extending](../extending.md).
- "One radio at a time" falls out of the single slot: an overlay cannot be pushed over an
  overlay, so a page and a held row cannot both own the encoders. This is why pages close
  each other.
- Overlay lifetime has to be explicit. Forgetting to clear one leaves the whole row wrong,
  so every overlay is tied to a visible state (a held key, an armed F, an open page).
- The idle labels are part of the data, which keeps the OLED legend and the handler in
  sync — the `--` placeholders are the honest statement that a knob has no idle job.
