# Survival Arena

A top-down arena shooter made with C++ and SFML. Survive as long as possible while enemy waves grow, collect ammo, health, and weapon pickups, and score points by staying alive and defeating enemies.

## Controls

- `WASD` or arrow keys: move
- Mouse: aim
- Left mouse button or `Space`: shoot
- `P`: pause
- `R`: restart after game over
- `Esc`: quit

## Build

```bash
cmake -S . -B build
cmake --build build
./build/SurvivalArena
```

Requires SFML 2.5 or newer.
