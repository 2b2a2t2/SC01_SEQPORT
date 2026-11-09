# 0004 — One radio at a time: edit radio vs page

**Status** — Accepted

## Context

Two features want the same hardware at the same time. The **step-edit radio** (M1 + TIE /
GLIDE / ACCENT / PROB) needs the step pads to edit attributes and the encoder row for gate
and velocity. A **track page** (M2: ARP, GNR8, LEAD, COMP, HARM, SKANK, DRUM) needs the same
pads to pick a step and the same row for its knobs.

Both could be "open" simultaneously only by splitting the row, which the 8-knob panel cannot
do without hiding the thing the player is editing.

## Decision

They are mutually exclusive. Opening a page turns the M1 radio **off**; pressing M1 while a
page is open **closes the page**. The same rule repeats inside pages: arming one F takes the
entire encoder row, and the armed sub-page dies when its page closes.

## Consequences

- The rule is predictable and visible (the pad and the header both show which one is live),
  at the cost of one extra gesture for workflows that need both.
- A page must carry its own path to GATE / VEL / PROB, because turning the radio off took
  the row's normal path away.
- The TIE span on a bass track requires closing GNR8 first — the most-reported trap in the
  instrument, and documented as such in [Cheat sheet](../cheatsheet.md).
- A page cannot be "half armed": the picked step, the armed F and the page state all reset
  together, so there is no stale selection to reason about.
