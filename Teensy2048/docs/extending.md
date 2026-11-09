<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](README.md) · [Cheat sheet](cheatsheet.md) · [Glossary](glossary.md)_

# Extending / Customizing

- **Add a scale**: Edit `kScaleName`, `kScaleMood`, `kScalePat` in `Chords.cpp`
- **Add a chord quality**: Extend `kQualityRow`, `kQualityName`, `kTriad` in `Chords.cpp`
- **Change KEYMODE4 genres/tables**: regenerate `genre_data.h` from `tools/genre_data/` (method + sources documented in `GENRE_DATA.md`); `Key4.cpp` is its only consumer
- **New mode variant**: Add row to `table` in `Bindings.cpp`, define encoder labels, update `Events.cpp` dispatch
- **New control binding**: Add `EncSet`/`CtrlSet`/`StepSet`/`KeySet` enums, extend binding struct
