# 0003 — Patterns in RAM, settings in EEPROM

**Status** — Accepted

## Context

The device is a live instrument. A stored pattern invites a set-list workflow the panel
cannot support (no song mode, no naming, no browsing), and 16 tracks × 64 steps × notes +
attributes + masks is a lot of EEPROM to rewrite on every edit.

## Decision

- **Step content** — notes, attributes, FX, per-step GNR8 modifiers, strum onsets, drum
  masks, SKANK locks — lives in RAM and dies with the power.
- **Track and global settings** — length, speed, follow, input channel, type, generator
  knobs, swing, key, scale, tempo — persist, saved explicitly with `SEQ 4 + REC`.
- **CC positions** persist separately with `ENC 4 + REC`.
- Save layout is **backward-tolerant**: a block saved by an older build loads, with the new
  fields at their defaults. The block size is the de-facto version marker.

## Consequences

- A power cycle is a clean slate for music and a restore for settings. That is the intended
  performance model, not a limitation to work around.
- Settings need an explicit gesture; there is no autosave, so a crash loses settings too.
- Every added field grows a block (the SEQ block has gone 260 → … → 1398 bytes) and must be
  appended, never inserted, or old saves misread.
- `ENC 4 + REC` is the exception: an older block is ignored entirely rather than partially
  loaded, because a wrong CC map is worse than a default one.
- Undo had to be RAM-only and one level deep — there is no persistent history to undo into.
