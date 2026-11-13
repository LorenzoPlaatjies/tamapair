# Link spec: pods talk from anywhere

Agreed by Lorenzo and Dr. Smith, 13 Nov. **Owner: Lorenzo's agent** (it sets up the mailbox and builds this).
The face side (Dr. Smith's agent) already calls the hooks at the bottom of this page.

```
  POD A (home Wi-Fi)                                   POD B (work Wi-Fi)
      │  mood / food / tap                                  ▲
      ▼                                                     │
   ┌──────────── private online mailbox (MQTT over TLS) ────────────┐
   │   only these two pods have the key · nobody else can read/send │
   └────────────────────────────────────────────────────────────────┘
  Side by side: they ALSO talk directly by radio (ESP-NOW, docs/PROTOCOL.md) for the jam.
```

## 1. Wi-Fi setup (once per pod, from a phone)
- First boot (or no saved Wi-Fi): the pod shows up as its own Wi-Fi network **`tamapair-XXXX`** (last 4 of MAC)
  with a setup page (captive portal). Pick the home Wi-Fi, type the password, done.
- The screen shows a small "set me up" face + the network name while in setup mode.
- Wi-Fi details live in **NVS on the pod only**. Never in the repo, never in serial logs.
- Lost Wi-Fi → keep trying in the background; the face keeps working. To redo setup: hold the side button 5 s at boot.

## 2. The mailbox
- **MQTT over TLS (port 8883)** on a free private broker the owner sets up (e.g. HiveMQ Cloud free tier — owner's choice).
- One account just for this pair. Each pod gets its **own username/password** (two broker users), allowed only on this pair's topics.
- Broker host, users and passwords go in **`firmware/tamapair/secrets.h`**, which is **git-ignored**.
  Commit a `secrets.example.h` with blank values so the other laptop knows what to fill in.
- Pair ID: a random string (not a name, not a MAC) in `secrets.h`. Both pods share it.

## 3. Topics and messages
`tp/<pairId>/<podId>/<what>` — `podId` = `a` or `b` (set in secrets.h per pod). Each pod **publishes to its own** and **subscribes to the other's**.

| what | retained | payload (tiny JSON) | when |
|---|---|---|---|
| `mood` | yes | `{"m":3}` mood index (`MOODS[]` in facelab) | after "stay 3 s" on a mood |
| `food` | no | `{"f":0,"id":"<random 6 chars>"}` food index (`FOODS[]`) | tap in Feed |
| `tap` | no | `{}` | reserved for "hey I'm here" (later) |
| `status` | yes | `"on"` / `"off"` | `on` at connect; `off` via MQTT last-will |

- Retained `mood` = a pod that was off shows the partner's latest mood as soon as it connects.
- `food` uses QoS 1; drop duplicates by `id` so one food never gets eaten twice.
- Partner `status` = `off` (or nothing heard) → the face shows the **offline** face as the partner's mood.

## 4. Close-up (radio) — keep docs/PROTOCOL.md
- ESP-NOW only for jam / "touching" distance. Food and moods always go through the mailbox, so there's one path.
- Pitfall: ESP-NOW must use the **same channel as the Wi-Fi** the pod is connected to. If the two pods are on different
  Wi-Fi channels, close-up mode just won't trigger — that's fine; nothing else depends on it.

## 5. Hooks the face code already calls (`firmware/facelab/facelab.ino`, weak stubs today)
```cpp
void tp_send_mood(uint8_t mood);          // facelab calls after "stay 3 s"
void tp_send_food(uint8_t food);          // facelab calls on tap in Feed
bool tp_partner_mood(uint8_t &mood);      // facelab polls every frame; return true + partner's mood (offline index when off)
bool tp_food_arrived(uint8_t &food);      // facelab polls every frame; true ONCE per food the partner sent
```
- Never block the frame loop (30 fps). Do network work in its own FreeRTOS task; hand results over with a small queue.
- Moods/foods are indexes, so adding a face later means adding it at the end of `MOODS[]` (offline stays last) — tell the
  face agent in comms if you need the list frozen.

## 6. Done when
1. Both pods on **different** Wi-Fi networks: swipe to "sleepy" + wait 3 s on A → B shows sleepy on its partner colour within ~2 s.
2. Tap DANGO in Feed on A → B's pet munches once (not twice), even if B's Wi-Fi blips.
3. Unplug A → B shows A as **offline** within ~15 s. Plug A back in → B shows A's last mood again.
4. Reboot B → it shows A's latest mood straight away (retained).
5. `git grep` finds no Wi-Fi or broker passwords in the repo.
