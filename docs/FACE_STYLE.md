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
- Mood swap: my mood on black for SWAP EVERY (3/5/8 s, default 5) ↔ partner's mood for 3 s on a faint tint of THEIR COLOUR.
  Soft change, not the squash: ordered-dither dissolve (old face crumbles 4f with blue sparkles at the edge, new one fills in 4–5f),
  background tint fades in/out over ~15 frames. Same mood on both = face stays, only the tint breathes in.

## Touch
- **Tap = poke, never a mood change.** Frames 0–1: crackle across the whole screen and ~30% of rows jolt ±2 px sideways.
  A ring of TV static grows from the finger (11 frames, radius = age×2.4, band 2.4), rows within ±7 of the finger get shoved
  5→1 px outward, and the face gets **pushed away from the finger** (impulse 4.4 x / 2.6 y), then springs back. One at a time.
- **Taps and swipes never mix.** The poke waits until the touch proves it's a tap: released having moved ≤12 px, or held
  still for 120 ms. Moving >12 px cancels it, so a swipe shows only the swipe tear.
- **Swipe = sideways tear** (its own transition, not the squash): old face slides out in the swipe direction (2→7→15→28 px)
  while rows rip sideways leaving blue streaks, 1 glitch frame, new face tears in from the other side (−30→−14→−4→+2→0).
- Dropped: laugh-on-tap (looked like a chosen mood), big full-screen ripple (too much, froze on rapid taps).

## Settings (button → menu → SETTINGS card). Tap a row to change it; all saved on the board. Swipe = back to the menu.
| Row | Choices |
|---|---|
| BLUSH | on / off |
| SWAP EVERY | 3S / 5S / 8S / OFF (how often the partner's mood shows) |
| THEIR COLOUR | pink / peach / blue / lilac / mint swatch (tint behind the partner's mood; the screen previews it 1.5 s) |
| TINT | FAINT / SOFT / BOLD |
| BRIGHTNESS | 3 bars (backlight PWM) |
| status line | NO WIFI - TAP FOR QR · CONNECTING · ONLINE - PARTNER OFF/ON |
- Rows slide in one after another; a changed row gets a pink tick and its value nudges.
- Wi-Fi QR screen: a short button press hides it (play offline); the status line in settings brings it back.

## Menu + feed feel
- Menu cards are always in colour (white face / pink dango / blue gear), pop in one after another (2 frames apart),
  the chosen one bobs and a pink marker slides under it. Finger down = card squishes; tap = it grows while the others shrink, then opens.
- Feed carousel slides (eases) and follows the finger while dragging; side foods smaller; page dots; tap the middle = send
  (flies off with two hearts, "SENT", then the next one pops back in).
