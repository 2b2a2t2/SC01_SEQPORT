# 0007 — Per-track length and speed (polimeter, polyrhythm)

**Status** — Accepted

## Context

A single global grid is simpler, and most grooveboxes ship one. But the musical results the
instrument is for — a 3-against-4 hat pattern, a bassline that phrases over two bars while
the kick stays in one — need tracks that do not share a bar line.

## Decision

**Length** (`LEN`, 0–64 steps) and **speed** (1/64 … 4, default 1) are **per track**, and
the playhead is tracked per track (`chStep[16]`), with a global shadow position
(`chStepShadow[16]`) that keeps advancing even while a bar is looped.

## Consequences

- Polimeter (different lengths) and polyrhythm (different rates) are the natural result of
  the data model, not a feature bolted on.
- There is no single "current bar" — the bar buttons select a 16-step **edit window**, and
  bar follow exists so the window can chase a track's own playhead.
- Bar loop must **teleport** on activation and resume at the shadow position on release, or
  a loop would silently drop the rest of the pattern's time.
- A slow track skips master ticks until its accumulator fills; gate time scales with the
  track's own step length, which is why swing has to be expressed as a ratio rather than a
  fixed millisecond offset.
- Length changes have to move content with them: doubling fills the new steps from the old
  pattern, and shortening hides steps rather than deleting them. Steps past the length are
  dark and inert, so stale content cannot be edited by accident.
