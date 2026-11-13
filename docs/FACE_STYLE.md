# Face style + animation rules

Measured frame-by-frame from a reference video of a retro pixel desk pet (30 fps).
We copy the **feel** (timing, proportions, punchiness), never the actual faces.
Live demo: `tools/face-lab/index.html` (serve `tools/` and open `/face-lab/`).

## Look
- White pixels on pure black (AMOLED black = off). Accent colours only for blush/tongue (pink `#FF6F9F`) and tears (blue `#6FD3FF`).
- Draw on a **92 × 112 grid**, blow up **4×** → 368 × 448. Face drawn then scaled 1.4× around the eye line.
- Eyes: rounded rectangles, **w 16 × h 24** grid px (tall, ~1:1.5), corner radius ~6.
  Centres at x = 25 and 67 (gap between inner edges ≈ 1.6 eye-widths), y = 46.
- Mouth: tiny, ~10 px wide, 2 px stroke, sits between the eyes just below eye-bottom (y ≈ 59).
- Expressions change the **eye shape** first (^ ^, > <, lines, rings, hearts); mouth is the small accent.

## Motion grammar (the "explosive" part)
| Beat | Frames @30fps | What happens |
|---|---|---|
| Squash out | 4 | eyes flatten vertically to a line (sy 1 → 0.08), slight widen |
| Glitch | 1 | horizontal slice shifts + ~5% noise pixels. Also where the bg colour swaps |
| Pop in | 4 (zoom: 5) | new face at sy 0.15 → 0.7 → **1.25** → 1 (overshoot). Zoom moods: scale 0.35 → 0.8 → 1.3 → 1.12 → 1 |
| Hold | 1–3 s | details **wobble every 2nd frame** (tongue, shake, sparkle) — never perfectly still |
| Blink | 4 | sy 1 → .45 → .08 → .08 → .45, every 2–5 s (random) |
| Glance | 8 / 25 / 2 | ease out to the side over 8, hold 25, **snap** back in 2 |

Rule of thumb: slow in, **snap** out. Big moments are 1–2 frame cuts, not smooth fades.

## Moods in the lab
idle · happy · laugh (tap reaction) · yum · love · sleepy · sad · grumpy · surprised · dizzy · munch (eating) · hype

## Home screen bits (from SCREENS.md)
- Settings dot: top-left, 3×3 grey.
- Last-ate: top-right, tiny rice ball + "2h" in a 3×5 pixel font.
- Mood swap: my mood on black ↔ partner's mood on partner's dark tint, swapped on the glitch frame, every ~4 s.
