# NUNO Player

A modern reimagining of the iPod mini, built with premium components and focused on exceptional audio quality. NUNO combines the iconic Click Wheel interface with audiophile-grade hardware to create a dedicated music player that brings joy back to music listening.

## Project Status: Early Development 🚧

Currently in the early stages of development, focusing on core embedded systems and hardware integration. The project uses the STM32H743 microcontroller for smooth performance with low power consumption and the ESS ES9038Q2M DAC to deliver exceptional audio quality.

### Current Focus Areas
- Audio playback pipeline implementation
- Basic UI state machine development
- Hardware abstraction layer
- Core platform drivers (I2C, GPIO, DMA)

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
