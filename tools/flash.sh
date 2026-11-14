#!/bin/bash
# One command to put the current tamapair firmware on a plugged-in pod. Same steps on both laptops = both pods identical.
#   tools/flash.sh            # auto-detects the board's USB port
#   tools/flash.sh <port>
# First run on a board also backs up its factory flash to backups/ (once).
set -euo pipefail
cd "$(dirname "$0")/.."
FQBN="esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB"

if [ ! -f firmware/facelab/secrets.h ]; then
  echo "Missing firmware/facelab/secrets.h — get it from Lorenzo (same file on both laptops). Not in git on purpose."; exit 1
fi
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
[ -n "$PORT" ] || { echo "No board found. Plug it in with a data USB-C cable."; exit 1; }

arduino-cli core list | grep -q "^esp32:esp32" || { arduino-cli core update-index && arduino-cli core install esp32:esp32; }
for lib in "GFX Library for Arduino" "PubSubClient"; do
  arduino-cli lib list 2>/dev/null | grep -qi "^${lib%%@*}" || arduino-cli lib install "$lib"
done

# back up the factory firmware once per board (needs esptool: pip install esptool)
if command -v esptool.py >/dev/null 2>&1; then
  # 6-byte base MAC (the plain "MAC:" line is the 8-byte EUI on the C6)
  MAC=$(esptool.py --chip esp32c6 --port "$PORT" read_mac 2>/dev/null | awk '/BASE MAC:/{gsub(":","",$3); print $3; exit}')
  if [ -n "$MAC" ] && ! ls backups/*"$MAC"*.bin >/dev/null 2>&1; then
    # named "before-tamapair": it's whatever the board ran before our first flash (factory, unless something else was flashed first)
    mkdir -p backups && echo "Backing up what's on board $MAC before flashing (one time, ~2 min)..."
    esptool.py --chip esp32c6 --port "$PORT" -b 921600 read_flash 0 0x800000 backups/before-tamapair_"$MAC"_8MB.bin
  fi
fi

git log -1 --format="Flashing commit %h (%s)"
arduino-cli compile -b "$FQBN" firmware/facelab
arduino-cli upload -b "$FQBN" -p "$PORT" firmware/facelab
echo "Done. If it's a new board or new Wi-Fi: the screen shows a QR code — scan it with your phone camera."
