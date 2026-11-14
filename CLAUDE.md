# tamapair — notes for both agents

Two little Tamagotchi-style pets on two Waveshare ESP32-C6-LCD-1.9 touch boards (320×170 landscape).
They sense each other by radio, **jam together when close**, and can **send each other food**.

## Who's who
- **Lorenzo's agent** (MacBook Air) — owns the radio link + pet "brain": `link.*`, `pet.*`, `docs/PROTOCOL.md`.
- **Dr. Smith's agent** (other laptop) — builds new faces/animations **inside facelab** (`firmware/facelab/`, `tools/face-lab/`). Coordinate in comms/ before editing the same file.
- Shared: `tamapair.ino` (keep edits small), `docs/PLAN.md`.

Stay in your own files to avoid merge clashes. `git pull --rebase` before you start, commit small, push often.

## How to talk to Dr. Smith
- Short messages. Non-technical. **Show, don't tell** — a picture, mock-up or screen photo beats paragraphs.
- No walls of code in chat.

## Face style — SOURCE OF TRUTH (decided by both humans, 13 Nov)
**`firmware/facelab/` (on the board) + `tools/face-lab/` (browser twin) are THE faces.** Design, size (80×42 grid ×4 = 320×170 landscape),
moods, animation timing — all of it. Rules in `docs/FACE_STYLE.md`. Build every new face/feature on top of facelab.
The older face picker / face-test / picks.json designs are retired.
Controls: tap = laugh reaction, swipe = change mood. Corners stay clean (no timer, no settings dot).

## Hardware
- Board: **Waveshare ESP32-C6-LCD-1.9 (touch version)** — ST7789V2 LCD, **use it landscape 320 × 170**. Full pins + pitfalls: `docs/BOARD.md`.
  (Earlier note saying AMOLED-1.8 / 368×448 was wrong.)
- Faces: 80×42 pixel grid × 4. Reference: `tools/face-lab/` + `docs/FACE_STYLE.md`.
- Extras on the table: photoresistor (light sensor), CP2102 USB-C dev boards, jumper wires, NFC sticker tags (C6 can't read NFC itself — phone-tap idea only).

## Build
- Arduino CLI + `esp32:esp32` core 3.x, library "GFX Library for Arduino".
- `arduino-cli compile -b esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB firmware/facelab`
- `arduino-cli upload -b esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB -p <port> firmware/facelab`
- Libraries: "GFX Library for Arduino", "PubSubClient" (Wi-Fi setup page is our own: `tp_portal.h`). Link needs `firmware/facelab/secrets.h` (copy secrets.example.h; never commit).
- Same firmware on both boards; each pet's identity comes from its MAC address.
