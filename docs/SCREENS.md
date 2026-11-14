# Screens (Dr. Smith's idea, 13 Nov)

The faces *are* the messages. No typing needed to say "I'm sad" or "I'm sleepy".

## 1. Home — the mood swap
```
 ┌──────────────┐        ┌──────────────┐
 │ ·        2h🍙│        │ ·        2h🍙│   🍙 2h = when MY pet last ate
 │              │  few   │   (pink bg)  │   · = settings dot
 │   (•‿•)      │  secs  │   (-_-) zz   │
 │   my mood    │  ───▶  │  YOUR mood   │
 └──────────────┘  ◀───  └──────────────┘
   my colour             partner's colour
```
- The face slowly swaps between **my mood** and **your mood**, every few seconds, forever.
- The **background colour** tells you whose mood it is (each person has their own colour).
- Both the same mood → it just stays on that face. ("We're both happy.")
- **Swipe left/right** → scroll through the faces. Stop on one = that's my mood now. It gets sent to the other pet.

## Controls (decided 13 Nov, both humans)
```
 HOME (face)            press button          MENU: 3 cards
 swipe  = pick mood   ───────────────▶  ┌──────┐┌──────┐┌────────┐
 stay 3s = send mood                     │ HOME ││ FEED ││SETTINGS│
 tap/hold = play                         └──────┘└──────┘└────────┘
                                         swipe ← → between cards, tap = open
 FEED: 🍡 [🍙] 🍓 swipe foods, tap the big one = send → partner's pet munches
 SETTINGS: blush, swap time, their colour, tint, brightness, link status (saved) — see FACE_STYLE.md
 Button from anywhere = open the menu; from the menu = back home.
```
- **Moods:** the face you land on becomes yours after ~3 s and is sent; a little heart flies off = "sent".
- Touching the face is only for play: wink, tongue, boop, cheek, tickle (hold), watch-my-finger (slow drag), dizzy. Table in docs/FACE_STYLE.md "Play".

## 2. (old idea) Double-tap → pick a mode
```
 ┌──────────────┐
 │ ┌────┐┌────┐ │
 │ │MOOD││FEED│ │
 │ └────┘└────┘ │
 └──────────────┘
```
Two cards: **Mood** (back home) or **Feed**.

## 3. Feed — food carousel
```
 ┌──────────────┐
 │  🍡  🍙  🍓  │   neighbours peek in small,
 │     (🍙)     │   the middle one is big
 └──────────────┘
```
- Swipe through the foods. **Tap** the big one → it gets sent.
- On the **other** screen, their pet munches it (eating animation).
- Resets that pet's "last ate" timer on the home screen.

## Still to decide
- Screen layout from the humans' sketches (coming as photos).
- The two people's colours.
- Settings dot: what's in it.
- Screen: **320 × 170 landscape** (Waveshare ESP32-C6-LCD-1.9). See docs/BOARD.md.
