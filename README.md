# C Celeste Clone

A game cloning (Celeste) project using [cakez yt playlist](https://www.youtube.com/watch?v=FrOkcJH9hGc&list=PLFAIgTeqcARmowCzcOMil78OxcPNsac70) as guide

## Features

- **Cross-platform support**: Windows, Linux, and macOS
### Prerequisites

- **Clang/LLVM** with C23 support
- **Make** build system
- **OpenGL** drivers/development libraries
- Platform-specific dependencies:
  - **Windows**: DirectSound (dsound), GDI32, User32 (included with Windows SDK)
  - **Linux**: X11, Xext, PulseAudio development libraries (`libx11-dev`, `libxext-dev`, `libpulse-dev`)
  - **macOS**: Cocoa, AudioToolbox frameworks (included with Xcode command line tools)

### Build Commands

```bash
# Debug build (default)
make

# Release build
make release
  - **Linux**: X11, OpenGL, PulseAudio development libraries
  - **macOS**: Xcode command line tools

### Build Commands

```bash
# Debug build (default)
make

# Release build
make release

# Build and run
make run

# Clean build artifacts
make clean

# Show help
make help
```

## Project Structure

- `src/` - Source code (main.c, game.c)
- `include/` - Header files
- `platform/` - Platform-specific implementations
- `external/` - Third-party dependencies
- `assets/` - Game assets (sounds, sprites)
- `build/` - Build output directory

## Development

The game uses a hot-reload system where the main executable loads game logic from a dynamic library. This allows you to modify game code and see changes instantly without restarting the application.

Game logic is implemented in `src/game.c` and gets compiled into a separate DLL/dylib/so file that's automatically reloaded when changed.
