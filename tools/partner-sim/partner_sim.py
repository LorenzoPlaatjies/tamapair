#!/usr/bin/env python3
"""Pretend to be the partner pod from a laptop, to test the link without a second board.

Reads TP_PAIR_ID / host / port / user / pass from firmware/facelab/secrets.h (never printed).
Usage (from the repo root):
  python3 tools/partner-sim/partner_sim.py listen            # print everything the real pod publishes
  python3 tools/partner-sim/partner_sim.py mood 5            # partner's mood = MOODS[5] (retained), status on
  python3 tools/partner-sim/partner_sim.py food 2            # send FOODS[2]
  python3 tools/partner-sim/partner_sim.py off               # partner status = off (pod shows the offline face)
Needs: pip install paho-mqtt
"""
import json, os, random, re, ssl, string, sys, time
import paho.mqtt.client as mqtt

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
src = open(os.path.join(ROOT, "firmware/facelab/secrets.h")).read()
def define(name, default=""):
    m = re.search(r'#define\s+%s\s+("?)(.*?)\1\s*(//.*)?$' % name, src, re.M)
    return m.group(2) if m else default
PAIR, HOST, PORT = define("TP_PAIR_ID"), define("TP_MQTT_HOST"), int(define("TP_MQTT_PORT", "8883"))
USER, PASS = define("TP_MQTT_USER") or None, define("TP_MQTT_PASS") or None
ME = "sim000000000"  # the simulated partner's pod id

def client(as_pod=True):
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="tp-sim-%04x" % random.getrandbits(16))
    c.tls_set(cert_reqs=ssl.CERT_REQUIRED if os.environ.get("TP_VERIFY") else ssl.CERT_NONE)
    if not os.environ.get("TP_VERIFY"): c.tls_insecure_set(True)
    if USER: c.username_pw_set(USER, PASS)
    if as_pod:  # only when pretending to be the partner; listen/clear must leave no trace in the mailbox
        c.will_set(f"tp/{PAIR}/{ME}/status", "off", qos=1, retain=True)
    c.connect(HOST, PORT, keepalive=10)
    return c

def pub(c, what, payload, retain):
    c.publish(f"tp/{PAIR}/{ME}/{what}", payload, qos=1, retain=retain).wait_for_publish(5)

cmd = sys.argv[1] if len(sys.argv) > 1 else "listen"
c = client(as_pod=cmd not in ("listen", "clear")); c.loop_start(); time.sleep(1)
if cmd == "listen":
    c.on_message = lambda cl, u, m: print(time.strftime("%H:%M:%S"), m.topic.split("/", 2)[2], m.payload.decode(), "(retained)" if m.retain else "")
    c.subscribe(f"tp/{PAIR}/+/#", qos=1)
    try:
        while True: time.sleep(1)
    except KeyboardInterrupt: pass
elif cmd == "mood":
    pub(c, "status", "on", True); pub(c, "mood", json.dumps({"m": int(sys.argv[2])}, separators=(",", ":")), True); print("sent mood", sys.argv[2])
elif cmd == "food":
    fid = "".join(random.choice(string.ascii_lowercase + string.digits) for _ in range(6))
    pub(c, "status", "on", True); pub(c, "food", json.dumps({"f": int(sys.argv[2]), "id": fid}, separators=(",", ":")), False); print("sent food", sys.argv[2])
elif cmd == "off":
    pub(c, "status", "off", True); print("partner off")
elif cmd == "clear":  # remove the simulator's retained messages
    for w in ("status", "mood"): c.publish(f"tp/{PAIR}/{ME}/{w}", b"", qos=1, retain=True).wait_for_publish(5)
    print("cleared")
c.loop_stop(); c.disconnect()
