# Radio protocol (ESP-NOW broadcast, channel 1)

Every packet starts with magic `'T','P'` and a type byte.

| type | name  | sent | payload |
|---|---|---|---|
| 1 | HELLO | every 200 ms | `uint8 mood, uint8 hunger, uint8 jamming` |
| 2 | FOOD  | on tap while near | `uint8 food_id` |
| 3 | ACK   | on FOOD received | `uint8 food_id` |

Proximity from smoothed RSSI of HELLO (thresholds calibrated on real boards):
- `FAR`      : none heard for 2 s, or RSSI < -75
- `NEAR`     : -75 .. -50
- `TOUCHING` : > -50

The face code only needs `link_state()` → FAR/NEAR/TOUCHING and events (food arrived, friend mood).

## In facelab today (tp_near.cpp, 14 Nov)
Same idea, one packet: type `4` HELLO on the Wi-Fi's current channel (not fixed channel 1), every 200 ms and at once when something changes.
`'T','P', 4, uint16 pair_tag, uint8 flags (bit0 = being tickled), uint8 heart_counter, uint32 shared_clock_ms`.
`pair_tag` is a hash of the pair id, so only the two pods of a pair react. Food and moods still go through the mailbox only.

