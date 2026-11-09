# Genre Data Tables

Generated artefacts for KEYMODE chord/track helpers. Everything below is
produced by `tools/genre_data/` (Python 3 stdlib only); re-run the
pipeline to regenerate:

```
python3 fetch.py && python3 process.py && python3 generate.py && python3 verify.py
```

## What is in `genre_data.h`

`GenreData genreTable[18]` in PROGMEM-style const storage, per genre:

| Field | Size | Meaning |
|---|---|---|
| `trans[7][7]` | 49 B | next-chord probabilities, rows = from-degree, row sum 100 |
| `qual[7][4]` | 49 B | quality buckets (triad / 7th / 9th+ / sus-alt) per degree, row sum 100 |
| `inv[7][4]` | 49 B | inversion buckets (root / 1st / 2nd / 3rd) per degree, row sum 100 |
| `rhythmW[8]` | 8 B | weights over `rhythmMasks`, sum 100 |
| `microVar` | 1 B | humanization amount 0-255 |
| `flags` | 1 B | `0x01` estimated, `0x02/0x04/0x08` row fallback used |

Struct size: 157 B x 18 genres = 2826 B.

Rhythm templates (16th-grid attack masks, one bar = 16 sixteenths):

| # | Name | Mask | Attacks |
|---|---|---|---|
| 0 | WHOLE | `0x0001` | 0 |
| 1 | HALF | `0x0101` | 0, 8 |
| 2 | QUART | `0x1111` | 0, 4, 8, 12 |
| 3 | EIGHTH | `0x5555` | 0, 2, 4, 6, 8, 10, 12, 14 |
| 4 | BACKBEAT | `0x1010` | 4, 12 |
| 5 | OFFBEAT | `0x4444` | 2, 6, 10, 14 |
| 6 | PUSH | `0x4001` | 0, 14 |
| 7 | SYNCOP | `0x0909` | 0, 3, 8, 11 |

## Method

1. Parse chord + key annotations per corpus (Harte, jazz, ABC, Humdrum).
2. Key/scale detection: Krumhansl-Schmuckler correlation against
   major/minor key profiles; drop threshold `KS_MIN_CORR = 0.55`
   (weimar/TAVERN ship explicit keys; others fall back to K-S).
3. Map each chord to a scale degree 0-6 (I ii iii IV V vi vii);
   chromatic chords snap to the nearest in-scale degree.
4. Count transitions / quality buckets / inversion buckets per corpus.
   Inversions follow the Harte default: an absent bass annotation
   means root position; explicit non-chord-tone basses (`/9` etc.)
   are not counted as inversions. Corpora that only annotate
   non-root basses (Isophonics, TAVERN, Nottingham) therefore get
   implicit roots; the Weimar jazz DB rarely carries bass info, so
   its inv rows are root-dominated rather than measured.
5. Normalise every row with largest remainder to integer sum 100.
   Rows with < 20 samples fall back to the pooled empirical row, then
   to uniform, and set a fallback flag.

Empirical genres use their corpus counts as-is. Hybrid genres
(COUNTRY, BLUES, RNB, FUNK, METAL, HIPHOP, ELECTRONIC, REGGAE) multiply
a pooled POP+ROCK+FOLK prior by style multipliers from genre literature.
Estimated genres (GOSPEL, LATIN, CINEMATIC, MODAL) use the same
machinery and are flagged `GENRE_F_ESTIMATED`.
Hand-authored genres (DUBTECHNO) ship full rows written straight from
genre theory in `literature.py` (`HAND`) - no corpus, no prior - and
are flagged `GENRE_F_ESTIMATED` too.

Rhythm weights and `microVar` are hand-authored judgements for all
genres (no rhythm corpus); see `tools/genre_data/literature.py`.

## Corpora

| Source | Genre | License | Pieces | Chord events | Chromatic |
|---|---|---|---|---|---|
| POP_billboard | POP | CC0-1.0 | 860 | 115656 | 4.2% |
| POP_isophonics | POP | no-formal-license (free research download) | 6 | 657 | 2.9% |
| ROCK_isophonics | ROCK | no-formal-license (free research download) | 200 | 16534 | 8.6% |
| JAZZ_weimar | JAZZ | ODbL-1.0 (db_info table; contents under DbCL) | 436 | 28088 | 13.7% |
| CLASSICAL_tavern | CLASSICAL | CC-BY-SA-4.0 | 1057 | 11789 | 1.1% |
| FOLK_nottingham | FOLK | GPL-3.0 (cleaning); original Nottingham Music Database is free/community | 1022 | 27587 | 1.3% |

Excluded sources: chordonomicon (CC-BY-NC-4.0) — NonCommercial license; excluded by project decision.; casd (CC-BY-NC-SA-4.0) — NonCommercial license.; dilemmadata (CC-BY-NC-SA-4.0) — NonCommercial license (per its .zenodo.json).; dcmlab_corpora (CC-BY-NC-SA-4.0) — NonCommercial license (DCML standard corpus license).; when_in_rome (no license) — No repository license; it republishes third-party corpora of mixed licensing.; augmentednet (MIT) — MIT, but checked-in data is only 199 MuseScore correction zips; its corpora are unlicensed/NC submodules. Dropped as data source (TAVERN covers classical)..

## Per-genre output

| Genre | Tier | Events | Chromatic | Flags | rhythmW | microVar |
|---|---|---|---|---|---|---|
| POP | empirical | 116313 | 4.2% | 0x00 | [10, 15, 35, 10, 5, 10, 10, 5] | 50 |
| ROCK | empirical | 16534 | 8.6% | 0x00 | [5, 10, 25, 25, 5, 10, 15, 5] | 60 |
| JAZZ | empirical | 28088 | 13.7% | 0x00 | [5, 10, 15, 10, 5, 25, 10, 20] | 175 |
| CLASSICAL | empirical | 11789 | 1.1% | 0x00 | [40, 30, 15, 5, 0, 5, 3, 2] | 90 |
| FOLK | empirical | 27587 | 1.3% | 0x00 | [15, 25, 35, 10, 2, 5, 5, 3] | 70 |
| COUNTRY | hybrid | — | — | 0x01 | [8, 18, 35, 15, 4, 8, 9, 3] | 65 |
| BLUES | hybrid | — | — | 0x01 | [5, 15, 30, 20, 3, 12, 10, 5] | 120 |
| RNB | hybrid | — | — | 0x01 | [8, 14, 22, 8, 8, 18, 12, 10] | 110 |
| FUNK | hybrid | — | — | 0x01 | [3, 10, 15, 15, 10, 25, 10, 12] | 140 |
| METAL | hybrid | — | — | 0x01 | [2, 8, 22, 30, 8, 8, 14, 8] | 20 |
| HIPHOP | hybrid | — | — | 0x01 | [12, 18, 18, 6, 10, 14, 14, 8] | 155 |
| ELECTRONIC | hybrid | — | — | 0x01 | [5, 15, 30, 15, 8, 15, 7, 5] | 15 |
| REGGAE | hybrid | — | — | 0x01 | [4, 10, 14, 6, 12, 38, 10, 6] | 100 |
| DUBTECHNO | hand | — | — | 0x01 | [30, 20, 10, 5, 5, 20, 5, 5] | 30 |
| GOSPEL | estimated | — | — | 0x01 | [8, 15, 25, 10, 12, 15, 8, 7] | 105 |
| LATIN | estimated | — | — | 0x01 | [4, 12, 22, 12, 8, 20, 10, 12] | 130 |
| CINEMATIC | estimated | — | — | 0x01 | [55, 25, 10, 3, 1, 3, 2, 1] | 85 |
| MODAL | estimated | — | — | 0x01 | [30, 25, 20, 8, 4, 6, 4, 3] | 95 |

## Attribution

- McGill Billboard Dataset — CC0 (Miranda et al., mirror via mirdata).
- Isophonics annotations (Beatles, Queen, Carole King) — free research
  distribution, no formal license.
- Weimar Jazz Database — ODbL-1.0 + DbCL (Weiss & Goebl).
- TAVERN classical encodings — CC BY-SA 4.0 (Seyfarth et al.).
- Nottingham Database — original NMD free; cleaning repo GPL-3.0
  (jukedeck/nottingham-dataset).
- Only aggregate integer counts are redistributed; raw data stays in
  `tools/genre_data/cache/` (gitignored).
