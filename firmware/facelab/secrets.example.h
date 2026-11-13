// Copy to secrets.h (git-ignored) and fill in. Never commit secrets.h. See docs/LINK_SPEC.md.
#pragma once

// Shared by BOTH pods: a random string (not a name). Both laptops must use the same value.
#define TP_PAIR_ID     ""

// MQTT broker (TLS). Dev default works with a public test broker; swap for the private one when it exists.
#define TP_MQTT_HOST   "broker.emqx.io"
#define TP_MQTT_PORT   8883
#define TP_MQTT_USER   ""          // per-pod user on the private broker (blank on the public test broker)
#define TP_MQTT_PASS   ""
// PEM of the broker's root CA. Blank = encrypted but certificate NOT checked (dev only; serial prints a warning).
#define TP_MQTT_CA     ""
