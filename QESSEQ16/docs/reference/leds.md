<!-- section moved from the old README; edit freely -->
_Docs home: [Teensy2048](../README.md) · [Cheat sheet](../cheatsheet.md) · [Glossary](../glossary.md)_

# LED Feedback

- **Mode LEDs** (32/33/34): Purple/Blue/Green dim; selected = white/pulse/blink per variant
- **Step pads**: Green playhead; dim green = active step; dim blue = empty; yellow channel overlay (variant 3, and the locked plain ENCMODE); orange drum part overlay; KEYMODE4 adds white = focus in AUDITION / red = focus in WRITE, amber = locked step (bright = both)
- **Keyboard pads**: Dark blue (KEY1/3), blue-violet (KEY2 chord mode), degree map in KEYMODE4 (hue-mapped C = blue … B = red, rank 0 hottest, dimming down the rank, orange = borrowed, dark = not scored); in-scale only lit (KEY1/2/3); out-of-scale = dark; sounding chord keys tint cyan; playback on keyboard channel blinks dim green; **HAND strum zone** (POLY TYPE = HAND): white keys = pink → seagreen string gradient (hue 247 → 107, silent strings almost dark), black keys = option toggles (violet on / dim dark violet off); **overdub-layer panel** (hold `M2`, SEQMODE 1/2/4): the row wears the **M2 pad's own hue** — white C D E F = layers 1–4 (bright = holds notes, dim = empty, red = muted), the five black keys = commands (dim in the hue, the **selected** one in its own colour — `MUTE` a dim orange that breathes, `ERASE` a static dim dark red), G A B dark, gap LEDs black (see [Overdub layers](../modes/seqmode.md#overdub-layers--hold-m2))
- **`PREV` / `NEXT`**: dim yellow-green in every mode, so the two page keys are findable without reading the screen (KEYMODE2 is the exception: its `NEXT` belongs to the chord row and keeps that row's dim green)
- **Touch**: White while touched (wins over mode colors)

## Encoder page pad colors

SEQMODE **M2** opens an encoder page on a track that has one, and the pad carries that **track type's own hue** — dim while the page is closed, breathing while it is open — so the pad says which page it holds without the header. The hues are spread deliberately:

| Track type | M2 pad | Page behind it |
|---|---|---|
| **DRUM** | indigo `120,0,255` | [DRUM pattern page](../generators/drum-styles.md) |
| **MONO + ARP** | pink `255,0,170` | [ARP options](../generators/arp.md) |
| **MONO + BASS** | blue `0,70,200` | [GNR8 generator](../generators/gnr8.md) |
| **MONO + LEAD** | sea green `46,120,80` | [LEAD generator](../generators/lead.md) |
| **POLY + STRUM/HAND** | lavender `205,135,255` | the strum ADDS row |
| **POLY + HARM** | aqua `0,215,215` | [HARM page](../generators/harm.md) |
| **POLY + STEP** | pale sky `135,195,255` | [COMP page](../generators/comp.md) — only lit while the page is open |
| **POLY + SKANK** | spring green `0,205,125` | [SKANK page](../generators/skank.md) |
| MONO + BASS/LEAD, POLY + HARM | **M3** slow hue cycle | the generator button (tap = generate, hold = undo) |

**Red, orange and yellow are reserved** — REC, the drum / encoder MUTE overlays, the variant-3 channel selector and the armed-attribute step pulse already use them — and the step-edit attribute row keeps its **blue** (TIE / GLIDE / ACCENT / PROB). Every track hue therefore lives in the cool arc from spring green through cyan and blue to indigo and pink; two **pale tints** (STRUM lavender, COMP pale sky) buy their separation by lightness instead of hue.

When an M2 page is open, **its F row wears the same hue as the pad** that opened it (ARP sub-pages, GNR8 option radios, the strum ADDS row, the drum knob sets) — bright for the armed entry, dim for the rest. F pads that a page does not own keep their step-edit blue.

Idle brightness is 64/255 (breathing 64→200); the closest pair in CIELAB at that level is **14.4 ΔE** (HARM aqua vs SKANK spring green), against 0.0 for the duplicates and 8.9 for indigo vs blue before the split.
