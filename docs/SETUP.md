# Put your pod on Wi-Fi (1 minute, phone only)

1. **Plug the pod in.** If it doesn't know any Wi-Fi yet, the screen shows a **QR code** and "SCAN ME".
2. **Point your phone camera at the QR code** and tap "Join network" (the pod's own network, e.g. `tamapair-F114`).
3. **A cute pink page pops up by itself:** "Hi! Let's get me online". Tap your Wi-Fi name, type its password, tap **Connect 💗**.
   (No page? Open a browser and go to `192.168.4.1`.)
4. The page says "Yay, I'm online!", the QR code disappears and your pet's face comes back. Done — it remembers this Wi-Fi.
   Typo? The page says "Double-check the password?" — just fix it and tap Connect again.

**New place / new Wi-Fi?** Hold the side button for **5 seconds** → the QR code comes back → repeat.

Your Wi-Fi password is stored only on the pod. It is never in the code or on GitHub.

---
For the agents: build/flash with `tools/flash.sh` (needs `firmware/facelab/secrets.h`, same file on both laptops,
shared directly between the humans — never via git). The phone page is `firmware/facelab/tp_portal.h`;
preview it without a board: `python3 tools/portal-preview/serve.py` → http://localhost:8766/. Details: docs/LINK_SPEC.md, docs/BOARD.md.
