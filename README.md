# Survival Arena

A top-down arena shooter made with C++ and SFML. Survive as long as possible while enemy waves grow, collect ammo, health, and weapon pickups, and score points by staying alive and defeating enemies.

## What You Need

This game is built from source code. That means your computer needs a few development tools before it can create the game program.

On Debian, Ubuntu, Linux Mint, or a similar Linux system, open the Terminal app and install the required tools with:

```bash
sudo apt update
sudo apt install build-essential cmake libsfml-dev
```

If the terminal asks for your password, type your normal computer password and press Enter. The password may not visibly appear while you type it.

## Controls

- `WASD` or arrow keys: move
- Mouse: aim
- Left mouse button or `Space`: shoot
- `P`: pause
- `R`: restart after game over
- `Esc`: quit

## Build and Run

1. Open the Terminal app.

2. Go to the project folder. If the project is in `~/Develop/C++/Survival_0.1`, run:

```bash
cd ~/Develop/C++/Survival_0.1
```

3. Ask CMake to prepare a build folder:

```bash
cmake -S . -B build
```

4. Compile the game:

```bash
cmake --build build
```

5. Start the game:

```bash
./build/SurvivalArena
```

## Rebuilding After Changes

After editing the code, you usually only need to run:

```bash
cmake --build build
./build/SurvivalArena
```

## If Something Goes Wrong

If CMake says the `build` folder was created from a different project folder, delete and recreate it:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
./build/SurvivalArena
```

If the game opens but images are missing, make sure you run it from this project folder using `./build/SurvivalArena`. The build copies the `assets` folder next to the game automatically.

Requires SFML 2.5 or newer. The `libsfml-dev` package installs SFML on Debian/Ubuntu systems.
