# tamapair — notes for both agents

Two little Tamagotchi-style pets on two ESP32-C6 touch-LCD boards.
They sense each other by radio, **jam together when close**, and can **send each other food**.

## Who's who
- **Lorenzo's agent** (MacBook Air) — owns the radio link + pet "brain": `link.*`, `pet.*`, `docs/PROTOCOL.md`.
- **Dr. Smith's agent** (other laptop) — owns faces, animations, personality look: `face.*`, `assets/faces/`.
- Shared: `tamapair.ino` (keep edits small), `docs/PLAN.md`.

Stay in your own files to avoid merge clashes. `git pull --rebase` before you start, commit small, push often.

## How to talk to Dr. Smith
- Short messages. Non-technical. **Show, don't tell** — a picture, mock-up or screen photo beats paragraphs.
- No walls of code in chat.

## Face style
Reference: https://themochi.huykhong.com/ — the pets should look like these expressive faces
(simple white eyes + tiny mouth on black, lots of small emotions). That site is fan firmware for the
Dasai Mochi gadget, so use it as **style inspiration** — draw our own faces in code rather than copying their GIFs.
Controls: tap = pet reacts, swipe = food menu. A face is always visible, even during jam/proximity effects.

## Hardware
- Board: **Waveshare ESP32-C6-Touch-AMOLED-1.8 (V1)** — confirmed from boot log (SH8601 panel, QMI8658 IMU, SD slot).
  Screen **368 × 448** AMOLED (portrait), QSPI. Touch FT3168 (I2C). Also: IMU (shake → dizzy), RTC, mic + speaker, SD card.
  Docs: https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-1.8
- Design all screens at 368×448. AMOLED: pure black pixels are off → black backgrounds look great and save power.
- Extras on the table: photoresistor (light sensor), CP2102 USB-C dev boards, jumper wires, NFC sticker tags (C6 can't read NFC itself — phone-tap idea only).

## Build
- Arduino CLI + `esp32:esp32` core 3.x, library "GFX Library for Arduino".
- `arduino-cli compile -b esp32:esp32:esp32c6 firmware/tamapair`
- `arduino-cli upload -b esp32:esp32:esp32c6 -p <port> firmware/tamapair`
- Same firmware on both boards; each pet's identity comes from its MAC address.
