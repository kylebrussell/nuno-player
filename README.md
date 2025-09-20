# NUNO Player

A modern reimagining of the iPod mini, built with premium components and focused on exceptional audio quality. NUNO combines the iconic Click Wheel interface with audiophile-grade hardware to create a dedicated music player that brings joy back to music listening.

## Project Status: Early Development 🚧

Currently in the early stages of development, focusing on core embedded systems and hardware integration. The project uses the STM32H743 microcontroller for smooth performance with low power consumption and the ESS ES9038Q2M DAC to deliver exceptional audio quality.

### Current Focus Areas
- Audio playback pipeline implementation
- Basic UI state machine development
- Hardware abstraction layer
- Core platform drivers (I2C, GPIO, DMA)

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
   ./build/nuno-sim
   ```

### Simulator Controls
- Arrow keys or `j`/`k` – scroll through menu items
- Mouse wheel – scroll through menu items
- Click and drag around the click wheel ring – rotate the wheel (clockwise to move down)
- Click the wheel segments (`MENU`, `<<`, `>>`, `PLAY/PAUSE`) – trigger the corresponding buttons
- Click the center button – select menu item
- `Enter` – select menu item
- `Backspace` or `Esc` – go back
- `Space` – toggle play/pause state indicator

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
