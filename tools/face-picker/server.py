"""Tiny shared face picker. Run: python3 tools/face-picker/server.py
Both laptops open the printed link; picks are saved to assets/faces/picks.json."""
import json, re, socket, threading
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
PICKS = REPO / "assets" / "faces" / "picks.json"
PORT = 8777
PEOPLE = ("Lorenzo", "Dr. Smith")
EYES = {"#ffffff", "#9ff3e1", "#ffc2dc", "#ffe08a"}
lock = threading.Lock()


def load():
    try:
        return json.loads(PICKS.read_text())
    except Exception:
        return {"people": {p: {"faces": [], "blush": "on", "eye": "#ffffff"} for p in PEOPLE}}


class H(BaseHTTPRequestHandler):
    def _send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else body.encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            return self._send(200, (HERE / "index.html").read_bytes(), "text/html; charset=utf-8")
        if self.path == "/api/state":
            with lock:
                return self._send(200, json.dumps(load()))
        self._send(404, "{}")

    def do_POST(self):
        if self.path != "/api/update":
            return self._send(404, "{}")
        try:
            n = min(int(self.headers.get("Content-Length", 0)), 2000)
            req = json.loads(self.rfile.read(n) or b"{}")
        except Exception:
            return self._send(400, "{}")
        who = req.get("who")
        if who not in PEOPLE:
            return self._send(400, "{}")
        with lock:
            st = load()
            me = st["people"][who]
            face = req.get("toggle")
            if isinstance(face, str) and re.fullmatch(r"[a-z]{1,20}", face):
                me["faces"] = [f for f in me["faces"] if f != face] if face in me["faces"] else me["faces"] + [face]
            if req.get("blush") in ("on", "off"):
                me["blush"] = req["blush"]
            if req.get("eye") in EYES:
                me["eye"] = req["eye"]
            PICKS.parent.mkdir(parents=True, exist_ok=True)
            PICKS.write_text(json.dumps(st, indent=2) + "\n")
            self._send(200, json.dumps(st))

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.connect(("10.255.255.255", 1)); ip = s.getsockname()[0]
    except Exception:
        ip = "localhost"
    print(f"Face picker:  this laptop  → http://localhost:{PORT}")
    print(f"              other laptop → http://{ip}:{PORT}  (same Wi-Fi)")
    ThreadingHTTPServer(("0.0.0.0", PORT), H).serve_forever()
