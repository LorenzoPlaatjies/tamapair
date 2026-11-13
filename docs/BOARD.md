# Our board: Waveshare ESP32-C6-LCD-1.9 (touch version)

**Correction:** it is NOT the ESP32-C6-Touch-AMOLED-1.8 (Lorenzo's agent guessed wrong from the
"sh8601" log line). Proof:
- Chip ESP32-C6**FH8** = 8MB flash built in. The AMOLED-1.8 uses 16MB external flash.
- Factory app is called `ST7789_Demo`, does a BLE scan (`GATTC_DEMO`), SD on SPI — all exactly
  Waveshare's `08_FactoryProgram` for the LCD-1.9. That program borrows the `esp_lcd_sh8601` driver
  file name but sends **ST7789** init commands, which is why the log says sh8601.
- I2C scan (Dr. Smith's agent): SDA 18 / SCL 8, 0x15 touch, 0x6B QMI8658, 0x20 TCA9554 — matches.

Official code: https://github.com/waveshareteam/ESP32-C6-LCD-1.9 (`02_Example/Arduino/07_Hello_World_GFX`, `08_LVGL_Test/lcd_config.h`)

## Pins (from Waveshare's lcd_config.h + Hello World)
| What | Pin |
|---|---|
| LCD MOSI | GPIO 4 |
| LCD SCLK | GPIO 5 |
| LCD DC | GPIO 6 |
| LCD CS | GPIO 7 |
| LCD RST | GPIO 14 |
| LCD backlight | GPIO 15 (**active LOW**) |
| SD MISO / SD CS | GPIO 19 / GPIO 20 (shares the LCD SPI bus) |
| I2C SDA / SCL | GPIO 18 / GPIO 8 |
| Touch | CST816 family @ 0x15 |
| IMU | QMI8658 @ 0x6B |
| IO expander | TCA9554 @ 0x20 |

## Screen
- ST7789V2, **170 × 320** native (portrait). We use it **landscape: 320 × 170** (rotation 1).
- Our faces: 80 × 42 pixel grid × 4 = 320 × 168 (+1 px margin top/bottom). See `tools/face-lab/`.

## Working Arduino_GFX setup (straight from Waveshare's example)
```cpp
Arduino_DataBus *bus = new Arduino_HWSPI(6 /*DC*/, 7 /*CS*/, 5 /*SCK*/, 4 /*MOSI*/);
Arduino_GFX *gfx = new Arduino_ST7789(bus, 14 /*RST*/, 1 /*rotation: landscape*/, true /*IPS — false = inverted colours*/,
                                      170, 320, 35, 0, 35, 0); // col offset 35 both sides
gfx->begin();
pinMode(15, OUTPUT); digitalWrite(15, LOW);   // LOW = backlight ON
```

## Pitfalls (why a screen stays black / looks wrong)
1. **Wrong board profile.** Any AMOLED / SH8601 / CO5300 / QSPI code will never light this panel. Use ST7789 over plain SPI.
2. **Backlight is active-LOW** on GPIO 15. `HIGH` = dark screen. For PWM brightness, invert the duty (Waveshare uses `0xff - level`).
3. **Column offset 35.** The 170-wide panel sits inside a 240-wide controller. Without offsets 35/0/35/0 you get a garbage stripe and a shifted image.
4. **Width/height order.** Pass the native `170, 320` to the constructor and let `rotation=1` turn it landscape. Passing 320×170 with rotation 0 breaks the offsets.
5. **IPS flag must be `true`** on our boards. Waveshare's example says `false`, but that gives inverted colours (white background instead of black). Confirmed on Lorenzo's board.
6. **Touch is portrait-native.** CST816 reports x 0–169, y 0–319. In landscape (rotation 1): `sx = ty`, `sy = 169 - tx` (verify with a tap test; may need the mirror).
7. **Touch needs a wake write.** Waveshare writes `0x00` to register `0x00` at 0x15 (up to 10 tries, 100 ms apart) before reading. Read 7 bytes from reg 0x00: count = b[2], x = (b[3]&0x0F)<<8 | b[4], y = (b[5]&0x0F)<<8 | b[6].
8. **SD shares the SPI bus.** If you don't use SD, drive GPIO 20 HIGH so the card stays off the bus.
9. **Serial over USB.** C6 uses USB-Serial/JTAG: compile with `CDCOnBoot=cdc` or `Serial` prints go nowhere.
10. **Flash size 8MB**, not 16MB: `arduino-cli compile -b esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M`.
11. **Stuck in a crash loop / won't upload:** hold BOOT, tap RESET, release BOOT, then upload.
12. **Stack overflow after panel start** (seen in the factory demo on one board): give big drawing work its own task with ≥8 KB stack, or draw from `loop()` and keep frame buffers global, not on the stack.
13. **A full-screen push costs ~30 ms, whatever you do.** Measured: the LCD's SCK/MOSI (GPIO 5/4) aren't the C6's fast IOMUX SPI pins, so the clock is capped (~40 MHz) and 109 KB takes ~29 ms with HWSPI *or* ESP32SPI, at any `begin()` speed. At 30 fps (33 ms budget) that leaves nothing. **Only push the rectangle that changed** (diff the 80×42 grid against last frame): idle frames drop to ~8 ms.
14. **No FPU on the C6.** Floats are software-emulated: per-pixel float maths over 3360 grid cells cost ~20 ms/frame. Precompute per-row/per-column (`mapX[]/mapY[]`) instead.
15. **Measure, don't guess:** facelab prints frame times over serial — send `s` (stats), `p` (poke), `l`/`r` (swipe), `d` (dump grid).
