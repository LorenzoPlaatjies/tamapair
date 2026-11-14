#!/usr/bin/env python3
"""Preview the pod's Wi-Fi setup page (firmware/facelab/tp_portal.h) in a browser, no board needed.

    python3 tools/portal-preview/serve.py        -> http://localhost:8766/
Fake networks; password "wrong" fails, Wi-Fi name "nope" isn't found, anything else succeeds.
"""
import http.server, json, pathlib, time, urllib.parse

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "firmware/facelab/tp_portal.h"
NETS = ["Home Sweet Home", "Lorenzo's iPhone", "NETGEAR-5G", "xfinitywifi"]
state = {"st": "idle", "at": 0}


def page():
    s = SRC.read_text()
    return s[s.index('R"TPHTML(') + 9: s.index(')TPHTML"')].encode()


class H(http.server.BaseHTTPRequestHandler):
    def send(self, code, body, ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body if isinstance(body, bytes) else body.encode())

    def do_GET(self):
        path = urllib.parse.urlparse(self.path).path
        if path == "/":
            self.send(200, page(), "text/html; charset=utf-8")
        elif path == "/nets":
            self.send(200, json.dumps({"busy": False, "saved": "", "n": NETS}))
        elif path == "/status":
            if state["st"].startswith("trying") and time.time() - state["at"] > 3:
                state["st"] = state["st"].split(":")[1]
            self.send(200, json.dumps({"st": state["st"].split(":")[0]}))
        else:
            self.send(404, "{}")

    def do_POST(self):
        form = urllib.parse.parse_qs(self.rfile.read(int(self.headers["Content-Length"])).decode())
        s, p = form.get("s", [""])[0], form.get("p", [""])[0]
        result = "nonet" if s == "nope" else "fail" if p == "wrong" else "ok"
        state.update(st="trying:" + result, at=time.time())
        self.send(200, '{"st":"trying"}')

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    print("setup page preview: http://localhost:8766/")
    http.server.ThreadingHTTPServer(("", 8766), H).serve_forever()
