# Brickbreaker

A feature-rich brickbreaker clone built in C++ with [SFML 3.x](https://www.sfml-dev.org/).

## Features

- **Main menu** – Play, Settings, Exit
- **Score tracker** with persistent high score
- **Paddle = health bar** – the paddle physically shrinks when hit and grows back on a 5-hit streak
- **Per-brick bullet types**
  - 🔴 Red – thick, slow bullet
  - 🟠 Orange – two bullets in a 45° V spread
  - 🟢 Green – aimed shot toward the paddle
  - 🔵 Blue – fast bullet
  - 🟣 Purple – baseline speed
- **Shop system** – earn gold each stage, browse 3 random powerups per visit
  - +1 max health, slower ball, bigger ball
  - Interest cap expansion (+10 gold cap, up to 5 purchases)
  - Lucky brick multiball (1% per purchase, up to 5%)
  - Bullet Clear consumable (press **E** to wipe all bullets; buy up to 3 charges)
- **Configurable stages** – set brick rows and allowed colors per stage via `StageBrickDef`
- **Gold interest** – idle gold earns 10% interest, capped at 50 gold (expandable)
- **Boss fight (Stage 7)**
  - Boss sits above the bricks in all phases
  - Animated indestructible shield moves back and forth
  - 3 HP with white flash on hit; ball resets to paddle on each hit
  - Phase 2 – random bricks gain a damage shield (+1 HP) with glow animation
  - Phase 3 – top brick row fuses into one mega-brick (20 HP) with merge animation
- **F1 god-mode** – instantly clears all bricks (for testing)

## Requirements

- C++17 compiler (MSVC 2022, GCC 11+, or Clang 13+)
- SFML 3.x (tested with 3.1)

## Building

### Windows (Visual Studio 2022)

1. Download [SFML 3.x for Visual Studio](https://www.sfml-dev.org/download.php) and unzip.
2. Create a new **Empty C++ Project** in VS 2022.
3. Add `main.cpp` to the project.
4. In **Project → Properties**:
   - **C/C++ → General → Additional Include Directories** → `<sfml>\include`
   - **Linker → General → Additional Library Directories** → `<sfml>\lib`
   - **Linker → Input → Additional Dependencies** → `sfml-graphics.lib;sfml-window.lib;sfml-system.lib`
   - **C/C++ → Language → C++ Language Standard** → C++17
5. Copy all SFML `.dll` files from `<sfml>\bin` into the same folder as your `.exe`.
6. Build and run.

### Linux / macOS

```bash
# Install SFML 3.x via your package manager or build from source
sudo apt install libsfml-dev   # Ubuntu (check version — needs 3.x)

# Compile
g++ -std=c++17 -O2 main.cpp -o brickbreaker \
    -lsfml-graphics -lsfml-window -lsfml-system

./brickbreaker
```

## Controls

| Key | Action |
|-----|--------|
| Arrow Left / Right | Move paddle |
| Space | Launch ball |
| P | Pause |
| E | Use Bullet Clear charge |
| F1 | God mode (clears all bricks) |

## License

MIT
