# C Celeste Clone

A game cloning (Celeste) project using [cakez yt playlist](https://www.youtube.com/watch?v=FrOkcJH9hGc&list=PLFAIgTeqcARmowCzcOMil78OxcPNsac70) as guide

## Features

- **Cross-platform support**: Windows, Linux, and macOS
- **Hot-reloadable game logic**: Modify game code without restarting
- **Modern C**: Uses C23 standard with clean, organized code
- **Custom engine**: Built-in renderer, audio system, and platform abstraction
- **Memory management**: Arena-based allocators for efficient memory usage

## Building

### Prerequisites

- **Clang compiler** with C23 support
- **Make** build system
- Platform-specific dependencies:
  - **Windows**: Visual Studio Build Tools or similar
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
