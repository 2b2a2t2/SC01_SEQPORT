# 0001 — Three mode families × four variants on one pad

**Status** — Accepted

## Context

The panel is fixed: 48 pads, 8 encoders, one 128×64 OLED, 9 control buttons, and no room
for a screen-with-menus interaction. The instrument has to be three different products at
once — a keyboard, a sequencer and a CC surface — without a boot menu, and a player has to
be able to see where they are in one glance.

A flat list of twelve screens does not fit the panel: twelve mode buttons would consume the
whole control row.

## Decision

Mode is the product of two small numbers: a **family** (`ENC`, `SEQ`, `KEY`) and a
**variant** (1–4). The family is chosen by the three mode pads — a family returned to recalls its last
variant. Inside a family the lit pad toggles 1 ↔ 2 on a tap, jumps to 4 on a double-tap,
and enters 3 while held. Twelve combinations come from three pads.

Variant 3 is **momentary**: it is entered by holding the mode pad and left by releasing it,
which restores the previous family and variant.

## Consequences

- The header must always name family and variant in words (`SEQUENCER 3`) — it is the only
  place the state is legible.
- Release-restores-previous is correct for channel selection and track config, and is
  surprising the first time. It is documented as a sharp edge in
  [Limitations](../limitations.md), not smoothed over.
- `ENC 4` needed to be an exception: its job is to stay open while 16 channels are
  addressed, so it is a locked picker rather than a momentary one. One family therefore
  breaks the variant-3 rule.
- Per-channel memory became necessary: a player switching channels expects that channel's
  variant, not a global one.
- Adding a screen means adding a variant, which is cheap; adding a fourth family would not
  be.
