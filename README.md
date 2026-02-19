# Neon Dodge — SDL3 Game Prototype

A simple arcade dodge-the-asteroids game built with **SDL3** in pure C.  
No external assets needed — all rendering is procedural.

## Features
- Smooth 60 FPS gameplay with particle effects
- Parallax starfield background
- Increasing difficulty over time
- Score & high-score tracking (per session)
- WASD / Arrow key controls

## Quick Start (Ubuntu)

```bash
chmod +x build.sh
./build.sh run
```

The script will:
1. Install build deps (`build-essential`, `cmake`, X11/Wayland libs, etc.)
2. Clone and build SDL3 from source (if not already installed)
3. Compile and launch the game

## Manual Build

If you already have SDL3 installed:

```bash
make        # compile
./neon_dodge # run
```

## Controls

| Key               | Action                  |
|-------------------|-------------------------|
| Arrow keys / WASD | Move ship               |
| Space             | Restart after game over |
| Escape / Q        | Quit                    |

## Requirements
- Ubuntu 22.04+ (or similar Linux distro)
- GCC, CMake, Git, pkg-config
- X11 or Wayland dev libraries
