<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](README.md) · [Cheat sheet](cheatsheet.md) · [Glossary](glossary.md)_

# Extending / Customizing

- **Add a scale**: Edit `kScaleName`, `kScaleMood`, `kScalePat` in `Chords.cpp`
- **Add a chord quality**: Extend `kQualityRow`, `kQualityName`, `kTriad` in `Chords.cpp`
- **Change KEYMODE4 genres/tables**: regenerate `genre_data.h` from `tools/genre_data/` (method + sources documented in `GENRE_DATA.md`); `Key4.cpp` is its only consumer
- **New POLY engine/page**: copy the shape of `Drift.h/.cpp` (per-track RAM settings, an
  `M2` page in `Events.cpp`, a bake/write path through the step writers, a mark-and-poll for a
  feeder change) and give it a free value in the shared `monoType` byte (`SeqClock.h`) — 4 is
  reserved, 7 is the next free one
- **New control binding**: Add `EncSet`/`CtrlSet`/`StepSet`/`KeySet` enums, extend binding struct
