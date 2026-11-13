# Plan

## The idea
Two pets. Each lives on its own screen. When the boards get close, both screens react at once.

| Distance | What happens on BOTH screens |
|---|---|
| far | pet idles, gets lonely over time |
| nearby | pet perks up, looks toward the friend |
| touching | **jam session** — bobbing in sync, music notes |

Tap your screen while close → **send food** flying to the other pet.

## Faces & touch
- **Always a face on screen** — even during proximity moments the face stays; effects pop up around it.
- Face style: https://themochi.huykhong.com/ (expressive mochi faces — match this look).
- **Tap** the pet → it reacts (giggle, blink, wiggle).
- **Swipe** → food menu. Pick a food; if the friend is near, it flies over to them.
- Room for **secrets**: hidden reactions/easter eggs (not just faces) — e.g. special combos when both pets do something at once.

## Works from anywhere (big idea)
Feeding is **not** a proximity thing — you can feed each other from anywhere (home, work…).
The food just **drops into the other person's screen**.

Also from anywhere, only between our two chips (a private pair, no one else, no blocking 😄):
- **Feed** — pick food from the swipe menu → it lands on the friend's screen.
- **Notes** — draw with your finger (e.g. "love you", a heart) → the doodle appears on theirs.
- **Tap / "hey I'm here"** — tap a button → their pet jumps + screen flashes (later: buzz/vibrate with a small motor).
- Later: mic / voice ping.

How: each board joins Wi-Fi and checks a tiny private online "mailbox" just for the pair.
Close together still uses the direct radio (for jamming — instant, no Wi-Fi needed).

| | Close (radio) | Anywhere (Wi-Fi mailbox) |
|---|---|---|
| Jam session | ✅ | — |
| Feed | ✅ | ✅ |
| Doodle notes | ✅ | ✅ |
| Tap / buzz | ✅ | ✅ |

Open question: which online mailbox to use (needs a free account; keys stay out of the public repo).

## How proximity works
Boards shout a tiny "hi, I'm here + my mood" over radio (ESP-NOW) ~5×/sec.
Signal strength = distance. Strong signal → close. No new protocol hardware needed.

## Milestones
1. ✅ Repo + toolchain
2. Hello face on each screen (proves screen + touch)
3. Boards see each other — show signal bars on screen
4. Near / far / touching states → face reacts on both
5. Jam animation in sync
6. Send food by tapping
7. Personalities (two different pets), hunger/mood over time
8. Extras: light sensor → sleeps in the dark; shake → dizzy (if board has a motion sensor)

## Split
- Lorenzo's side: radio, distance, food sending, pet stats (3, 4, 6, 7-stats)
- Dr. Smith's side: faces, expressions, jam + food animations, personality look (2-art, 5, 7-look)
