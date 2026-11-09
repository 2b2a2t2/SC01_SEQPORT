# Architecture decision records

One file per decision that is expensive to reverse. Format: **Status**, **Context**,
**Decision**, **Consequences** — including the consequences we do not like.

ADRs are immutable. To change a decision, add a new record that supersedes the old one and
update the old one's status line.

| # | Decision | Status |
|---|---|---|
| [0001](0001-three-families-four-variants.md) | Three mode families × four variants on one pad | Accepted |
| [0002](0002-bindings-as-tables.md) | Bindings are tables, with one overlay slot per group | Accepted |
| [0003](0003-patterns-ram-settings-eeprom.md) | Patterns in RAM, settings in EEPROM | Accepted |
| [0004](0004-one-radio-at-a-time.md) | One radio at a time: edit radio vs page | Accepted |
| [0005](0005-stored-vs-derived-notes.md) | Stored notes vs notes derived at send time | Accepted |
| [0006](0006-usb-only-clock.md) | Clock and transport on USB only; DIN carries notes | Accepted |
| [0007](0007-per-track-length-and-speed.md) | Per-track length and speed (polimeter, polyrhythm) | Accepted |
| [0008](0008-docs-split.md) | Split the documentation into a landing page and `docs/` | Accepted |

## Writing a new one

```markdown
**Status** — Proposed | Accepted | Superseded by NNNN

## Context

What forced a decision: the constraint, the failure, the options considered.

## Decision

The rule, in the present tense, specific enough to check code against.

## Consequences

What becomes easy, what becomes impossible, and what now has to be maintained.
```

Number sequentially, never reuse a number, and link the ADR from the page it explains.
