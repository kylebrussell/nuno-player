## NUNO Player — Embedded Build Buy List (2026 refresh)

This buy list is for running the **embedded** target (not the SDL simulator). It reflects the current prototype assumptions in `README.md` and `include/nuno/board_config.h` (STM32H7 Nucleo + WM8960 over I2S + IQS550 trackpad over I2C).

### Required (minimum viable embedded prototype)

| Item | Qty | Example buy link(s) | Notes |
|---|---:|---|---|
| **STM32H7 Nucleo-144 dev board (recommended)** | 1 | [Digi-Key: `NUCLEO-H753ZI`](https://www.digikey.com/en/products/detail/stmicroelectronics/NUCLEO-H753ZI/21348937) | `NUCLEO-H743ZI2` is marked obsolete/no longer manufactured at some distributors; `H753ZI` is the closest “keep moving” substitute. |
| **STM32H7 Nucleo-144 dev board (original plan / fallback)** | 1 | [Digi-Key: `NUCLEO-H743ZI2`](https://www.digikey.com/en/products/detail/stmicroelectronics/NUCLEO-H743ZI2/10130892) | If you already have one (or can still buy one), it matches the repo docs. Availability is the main downside. |
| **USB A → Micro‑B data cable** | 1 | [Adafruit: Micro‑USB cable (2m)](https://www.adafruit.com/product/2185) | Used for ST-LINK power/debug on most Nucleo boards. |
| **WM8960 I2S codec module (audio out)** | 1 | [Waveshare: WM8960 Audio HAT](https://www.waveshare.com/wm8960-audio-hat.htm) | Firmware currently configures WM8960 via I2C (`NUNO_CODEC_I2C_ADDR = 0x1A`) and streams PCM via I2S2. Any WM8960 I2S module should work if it supports 3.3V logic. |
| **Azoteq IQS550-based I2C trackpad (input)** | 1 | [Azoteq: I2C Trackpad Modules](https://www.azoteq.com/product/i2c-trackpad-modules/) | Firmware expects default IQS550 address `0x74` (`NUNO_TRACKPAD_I2C_ADDR`). Choose a module that’s already configured for I2C + reports X/Y. |
| **Momentary switch for “CENTER click” (optional on Nucleo)** | 0–1 | [Adafruit: 6mm tact switches (20 pack)](https://www.adafruit.com/product/367) | On Nucleo, `PC13` is commonly the built-in USER button; the firmware maps click to `PC13` (`NUNO_TRACKPAD_CLICK_PIN`). If you want an external click switch, wire it to `PC13` (active-low, pull-up). |
| **Female–female jumper wires (Dupont)** | 1 | [Adafruit: 40× female-female (6")](https://www.adafruit.com/product/266) | Easiest way to wire Nucleo headers ↔ modules. |
| **Solderless breadboard (or equivalent wiring method)** | 1 | [Adafruit: half-size breadboard](https://www.adafruit.com/product/64) | Optional if you’re wiring point-to-point / using a proto board instead. |
| **2.54mm (0.1") breakaway headers** | 1 | [Adafruit: 0.1" header strip (36-pin)](https://www.adafruit.com/product/4151) | Useful for making modules breadboard/jumper friendly. |

### Storage (recommended if you want “real” audio playback soon)

The simulator uses host files, but embedded playback will eventually need removable storage (plan calls out FAT32 + SD).

| Item | Qty | Example buy link(s) | Notes |
|---|---:|---|---|
| **microSD breakout (3.3V, SPI/SDIO)** | 1 | [Adafruit: MicroSD SPI/SDIO breakout (3V only)](https://www.adafruit.com/product/4682) | Good match for 3.3V MCUs; SDIO is faster if you wire it that way. |
| **microSD card** | 1 | (any reputable brand) | Size per your library needs; FAT32 is a likely target. |

### Display (pick one path)

The repo currently defines `DISPLAY_WIDTH=160` / `DISPLAY_HEIGHT=128` (`include/nuno/display.h`), and the embedded display driver isn’t wired up yet. Choose a display based on whether you want **speed-to-first-pixels** or to prototype the **planned monochrome** experience.

| Path | Item | Qty | Example buy link(s) | Notes |
|---|---|---:|---|---|
| **Fastest bring-up (matches 160×128)** | SPI TFT breakout (ST7735R) | 1 | [Adafruit: 1.8" TFT (ST7735R) breakout + microSD](https://www.adafruit.com/product/358) | Easiest to source + matches current geometry, but it’s color/backlit (not the final vibe). |
| **Planned monochrome display** | Sharp Memory LCD breakout | 1 | [Kuzyatech: Sharp Memory LCD Breakout A2](https://kuzyatech.com/sharp-memory-lcd-breakout-a2) | Typically used with `LS013B7DH03` (128×128); would require updating display geometry and implementing the SPI/VCOM drive requirements. |
| **Planned monochrome display** | Sharp Memory LCD panel | 1 | [Digi-Key: `LS013B7DH03`](https://www.digikey.com/en/products/detail/sharp-microelectronics/LS013B7DH03/5300387) | Distributor stock varies; if this is OOS, buy the panel from an alternate vendor and keep the breakout the same. |

### Optional but very helpful (debug & bring-up)

| Item | Qty | Example buy link(s) | Notes |
|---|---:|---|---|
| **Logic analyzer (I2C/I2S/SPI)** | 0–1 | (Saleae or similar) | Not required, but it turns “why is this not working?” into minutes instead of days. |
| **Bench 3.3V/5V supply (or USB power meter)** | 0–1 | (any reputable brand) | Helpful once you stop powering everything from USB. |

### “Final hardware” parts mentioned in the long-term plan (optional; availability varies)

These are **not required** for the current 2026 prototype stack, but are referenced in `NUNO Player plan` as end-state components.

| Item | Qty | Example buy link(s) | Availability note |
|---|---:|---|---|
| **ES9038Q2M DAC module (I2S)** | 1 | [Audiophonics: I‑Sabre ES9038Q2M](https://www.audiophonics.fr/en/dac-and-interface-modules/audiophonics-dac-i-sabre-es9038q2m-raspberry-pi-i2s-spdif-pcm-dsd-usb-c-power-supply-p-12795.html) | Use as a “real DAC” target once the I2S/audio chain is stable. |
| **Low phase-noise audio clocks (22.5792 MHz)** | 1 | [Digi-Key: CCHD-575-25-22.5792](https://www.digikey.com/en/products/detail/crystek-corporation/CCHD-575-25-22-5792/5423298) | Frequently out-of-stock; plan should allow substitutes. |
| **Low phase-noise audio clocks (24.5760 MHz)** | 1 | [Digi-Key: CCHD-575-25-24.576](https://www.digikey.com/en/products/detail/crystek-corporation/CCHD-575-25-24-576/5423299) | Often easier to buy than 22.5792. |
| **Battery charger / power-path eval** | 1 | [TI: BQ25616EVM](https://www.ti.com/tool/BQ25616EVM) | TI EVM stock fluctuates; plan already says “or similar”, which is the right approach. |

### Wiring reference (what the firmware expects)

- **I2C1**: `PB8=SCL`, `PB9=SDA` (trackpad + codec control)
- **I2S2 (SPI2 in I2S mode)**: `PB12=WS/LRCK`, `PB13=BCLK`, `PB15=SD`, `PC6=MCLK`
- **Trackpad I2C address**: `0x74`
- **WM8960 I2C address**: `0x1A`
- **Click switch**: `PC13` (pull-up, active low)

