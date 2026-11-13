# Face style + animation rules

Measured frame-by-frame from a reference video of a retro pixel desk pet (30 fps).
We copy the **feel** (timing, proportions, punchiness), never the actual faces.
Live demo: `tools/face-lab/index.html` (serve `tools/` and open `/face-lab/`).

## Look
- White pixels on pure black. Accent colours only for blush/tongue (pink `#FF6F9F`) and tears (blue `#6FD3FF`).
- Draw on an **80 × 42 grid**, blow up **4×** → 320 × 168 on the 320 × 170 landscape screen.
- Eyes: rounded rectangles, **w 13 × h 19** grid px (tall, ~1:1.5, ~45% of screen height), corner radius 4.
  Centres at x = 21 and 59, y = 19.
- Mouth: tiny, ~8 px wide, 2 px stroke, between the eyes at eye-bottom level (y ≈ 28).
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
idle · happy · laugh (tap reaction) · yum · love · sleepy · sad · grumpy · surprised · dizzy · munch (eating) · hype · offline

## Home screen bits
- Corners stay clean: humans removed the settings dot and the last-ate timer.
- Offline (partner board off/unplugged): old-TV switch-off (line shrinks 5f → dot 6f → face), grey hollow eyes, unplugged cable wiggle.
- Mood swap: my mood on black ↔ partner's mood on partner's dark tint, swapped on the glitch frame, every ~4 s.
