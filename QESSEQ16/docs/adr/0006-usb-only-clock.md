# 0006 — Clock and transport on USB only; DIN carries notes

**Status** — Accepted

## Context

The 5-pin DIN socket is wired to Serial2 (pin 7 RX / pin 8 TX) and is attractive as a
second clock source — a hardware sequencer or drum machine on DIN, a DAW on USB. Two clock
sources arriving at once would both advance the playhead, and merging them is not a solved
problem on a microcontroller with a 1 ms-ish loop budget.

## Decision

Clock (0xF8) and transport (Start / Stop / Continue) are accepted and generated over **USB
only**. The DIN port is a **note** input. Per track, `SRC` picks the accepted port
(`USB` / `DIN` / `BOTH`) and `EXT IN` decides what happens to the notes (`OFF` / `ON` /
`THRU` / `LIVE`).

## Consequences

- A DIN-only setup cannot drive the transport: something must start the clock (the device,
  or a DAW on USB). Documented as a non-goal, not a bug.
- Two sources can never double-advance the sequencer, which was the actual failure being
  prevented.
- Note-off matching had to become port-aware: a note-off only releases tracks whose `SRC`
  allows the port it arrived on, and changing mode or source releases forwarded notes so
  nothing hangs at the host.
- `THRU` gives the DIN port a second job — a per-track MIDI thru box, still filtered by the
  track's input channel.
- The board exposes no serial tty in this configuration (USB Type MIDI), which changes how
  diagnostics and flashing are done; see [Build & flash](../build.md).
