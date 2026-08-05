> **English** · [日本語](led_drive_test.ja.md)

# M5AtomS3R LED Drive Validation (V2 Hardware Configuration)

The V2 board (FPC-isolation-sphere/kiban/) has already been ordered, but before it arrives,
this is test firmware for bench-validating whether an **M5AtomS3R standalone unit + on-hand WS2812 LED strips**
can drive a "5 strips × 160 LEDs = 800 LEDs" configuration.

- Source: `src/led_drive_test/led_drive_test_main.cpp`
- Build environment: `pio run -e led_drive_test -t upload` (independent of the main `env:atoms3r`)

## Points to Validate

| # | Point | Background |
|---|---|---|
| 1 | 5th data output | The ESP32-S3's RMT TX has **4 channels**. Measure how FastLED handles a 5th line (expected to be sent sequentially via multiplexing → show() time roughly doubles) |
| 2 | show() time | 160 LEDs × 30µs ≈ 4.8ms/strip. With 4 channels in parallel + 1 line sequential ≈ 9.6ms → expected to comfortably fit within 30fps (33ms period). Confirm by measurement |
| 3 | Chain order | In CHASE mode, visually confirm the DIN→DOUT flow and the count of 160 |
| 4 | Current and brightness cap | Full white @255 would draw about 48A across 800 LEDs, which is out of the question. The brightness cap of 64/255 is enforced in code (a rehearsal of the operational practice for the pogo-pin RTLECS 1.5A/pin × 2-branch constraint) |

## Pin Assignment (to be confirmed)

The core-M5atom-FPC board nets `GPIO01`–`GPIO05` are the 5 LED data lines.
The physical correspondence with the AtomS3R bottom sockets (J3 "M5atom-L" / J4 "M5atomS3-R")
is set to default values based on **the following hypothesis**. **Confirmation by the board designer is required**:

| Board net | Hypothesized ESP32-S3 pin | Build flag |
|---|---|---|
| GPIO01 | G5 | `LED_TEST_PIN0` (default 5) |
| GPIO02 | G6 | `LED_TEST_PIN1` (default 6) |
| GPIO03 | G7 | `LED_TEST_PIN2` (default 7) |
| GPIO04 | G8 | `LED_TEST_PIN3` (default 8) |
| GPIO05 | G38 | `LED_TEST_PIN4` (default 38) |
| GPIO06 (spare?) | G39 | — |

If it turns out to be different, override it by adding something like
`-D LED_TEST_PIN4=39` to `env:led_drive_test` in `platformio.ini`.

Reference: the current main firmware (`BoardConfig.h`) uses 4 strips on G5/G6/G7/G8.
I2C (BNO055) uses the Grove port G2(SDA)/G1(SCL) — via the board's J9.

## Test Modes (switched with the main unit button)

| Mode | Display | What it checks |
|---|---|---|
| STRIP_ID | Solid color per strip (R/G/B/Y/M) | Connectivity of all 5 outputs, mapping between wiring and strip number |
| CHASE | A single white pixel runs + an identifier color at the head | Chain order, LED count (160) |
| RAINBOW | Rainbow scroll | Rendering smoothness, flicker |
| WHITE_64 | Full white at brightness 64 | Current measurement (clamp meter / power supply readout) |
| FPS_BENCH | Fastest loop | show() average time and maximum FPS |

The LCD and serial (115200) display FPS / show() average time / pin assignment every second.

## Expected Results (pass criteria)

- All 5 strips light up with correct colors
- In FPS_BENCH, show() ≈ 10ms or less → effectively ~100fps class, ample headroom over the 30fps production target
- No flicker or color corruption in 30fps mode
- Current in WHITE_64 is within the assumed ratings of the power supply and pogo pins

Alternative if show() is significantly slow (>20ms):
switch to FastLED's ESP32-S3 I2S/LCD parallel driver (`FASTLED_USES_ESP32S3_I2S`)
for fully parallel output of up to 16 strips.
