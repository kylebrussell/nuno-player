# NUNO Player

A modern reimagining of the iPod mini, built with premium components and focused on exceptional audio quality. NUNO combines the iconic Click Wheel interface with audiophile-grade hardware to create a dedicated music player that brings joy back to music listening.

## Project Status: Early Development 🚧

Currently in the early stages of development, focusing on core embedded systems and hardware integration. The project uses the STM32H743 microcontroller for smooth performance with low power consumption and the ESS ES9038Q2M DAC to deliver exceptional audio quality.

### Current Focus Areas
- Audio playback pipeline implementation
- Basic UI state machine development
- Square trackpad input integration (tap zones + scroll)
- Hardware abstraction layer
- Core platform drivers (I2C, GPIO, DMA, I2S)

### Prototype Hardware (2026 refresh)
- MCU dev board: STM32 NUCLEO-H753ZI (preferred) or NUCLEO-H743ZI2 (original target; often listed as obsolete)
- Audio: WM8960 I2S codec module (I2C control)
- Input: Azoteq IQS550-based I2C trackpad module + single click switch

## Simulator Quick Start

You can exercise the UI and menu flow locally without the embedded toolchain by building the SDL2 simulator target:

1. Ensure dependencies are installed (macOS examples shown):
   - `brew install cmake`
   - `brew install sdl2`
2. Configure and build the simulator:
   ```bash
   cmake -S . -B build -DBUILD_TESTS=OFF
   cmake --build build --target nuno-sim
   ```
3. Launch the simulator:
   ```bash
   ./build/nuno-sim                 # default device (iPod mini)
   ./build/nuno-sim --device ipod-5g
   ./build/nuno-sim --list          # list available device skins
   ```

### Device Skins (multiple iPod generations)

The simulator renders the same playback/UI engine through a runtime
**device profile** describing each generation's screen (size + colour model),
click wheel (scroll / touch / touch+buttons / click), chassis and theme.
Profiles live in `src/platform/sim/device_profiles.c`; adding a generation is a
one-line append.

- `--device <id>` – start on a specific generation (`--list` to see ids)
- `[` and `]` – cycle to the previous / next generation **live**, without
  restarting; playback and menu position carry over
- `--shot <file.bmp>` – render one frame of the current device and exit
  (headless preview capture)

Every body is laid out from the device's **real millimetre dimensions** through
one global pixels-per-mm constant, so the generations come out at their true
relative sizes: a nano is genuinely narrower and smaller than a classic, the
nano 3G is short & wide, and the nano 4G/5G are tall & narrow with **portrait**
screens. The screen footprint is the panel's real physical size, so it also
serves as the UI's logical resolution and occupies its authentic fraction of
the face (the early iPods wore big bezels; the 5G/classic and nano panels fill
much more of the body).

Current lineup:

- Full-size: `ipod-1g`, `ipod-2g`, `ipod-3g`, `ipod-4g`, `ipod-photo`,
  `ipod-5g`, `ipod-classic`
- iPod mini: `ipod-mini`, plus the anodized colour variants `ipod-mini-blue`,
  `ipod-mini-pink`, `ipod-mini-green`
- iPod nano: `ipod-nano` (1G), `ipod-nano2g`, `ipod-nano3g` (short & wide),
  `ipod-nano4g`, `ipod-nano5g` (portrait screens)

Each profile carries its own palette and main-menu feature set, so the same
engine renders each generation authentically:

- **Themes** – monochrome panels (1G–4G, mini) keep the original inverted
  selection bar; colour panels (photo, 5G, classic, every nano) get a glossy
  blue selection **gradient** and a subtly shaded title bar.
- **Per-generation menus** – `Music`, `Settings` and `Now Playing` are
  universal; `Photos` appears on the colour models and `Videos` on the
  video-capable generations (5G, classic, nano 3G/4G/5G), matching the real
  lineup. Feature flags live in `DeviceProfile.features`.
- **Faceplate hook** – a profile may set `chassis.faceplateImage` to a bitmap
  path; the sim then blits that image (loaded once and cached) scaled to the
  canvas instead of the procedural body. All shipped profiles leave it unset and
  use the procedural chassis.

### Simulator Controls
- Arrow keys or `j`/`k` – scroll through menu items
- Mouse wheel – scroll through menu items
- Click and drag around the click wheel ring – rotate the wheel (clockwise to move down)
- Click the wheel segments (`MENU`, `<<`, `>>`, `PLAY/PAUSE`) – trigger the corresponding buttons
- Click the center button – select menu item
- `Enter` – select menu item
- `Backspace` or `Esc` – go back
- `Space` – toggle play/pause state indicator
- In **Now Playing**, the wheel/scroll adjusts volume (like a real iPod); the
  audio engine applies the gain in software. Track changes are gapless.

### Simulator Trackpad Mode (experimental)
- Press `t` to toggle square trackpad mode on/off
- Drag inside the square (wheel bounding box) – scroll
- Tap zones: top=MENU, left=PREV, right=NEXT, bottom=PLAY
- Right click – CENTER/Select

## Sample Music Library

The repository bundles a small public-domain library so you can exercise the audio stack without any setup. The tracks live under `assets/music/bach/open-goldberg-variations/` and come from Kimiko Ishizaka’s CC0 recording of J.S. Bach’s *Goldberg Variations*. In the simulator, navigate to `Music → Songs` to browse the bundled playlist and press the centre button to drill into `Now Playing`. Drop additional audio files anywhere beneath `assets/music/` and update `src/core/audio/music_catalog.c` if you want them to appear in the queue.

## Features (Planned)

### Audio
- High-end ESS ES9038Q2M DAC
- Support for multiple formats (MP3, AAC, ALAC, FLAC, WAV)
- Gapless playback

### Interface
- Classic Click Wheel navigation
- High-contrast monochrome display
- Smooth scrolling and responsive UI

### Storage
- Multiple capacity options (planned: 512GB - 4TB)
- Fast music database to support large libraries

### Power Management
- Intelligent battery management
- USB-C PD quick charging support

## Dependencies

### Audio Codecs
- **libFLAC** - Free Lossless Audio Codec for FLAC file support
  - Version: 1.4.3
  - License: BSD-like
  - Source: https://github.com/xiph/flac
