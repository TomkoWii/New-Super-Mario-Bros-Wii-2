# New Super Mario Bros. Wii 2 — prototype

This is an original, homebrew Wii platform-game prototype. It does not include Nintendo game code, graphics, music, or other extracted assets.

## Build
Use a devkitPro/devkitPPC environment with the Wii development libraries installed:

```sh
make
```

The output is `boot.dol`.

## Run in Dolphin
1. Build the project (locally or with the included GitHub Actions workflow).
2. Download the `nsmbwii2-dolphin-wii` artifact from the Actions run.
3. Extract `boot.dol`.
4. In Dolphin, choose **File → Open** and select `boot.dol`.

## Controls
- Wii Remote left/right: move
- A or 2: jump
- HOME: return to the Wii Menu (on hardware) / emulator menu behavior
- Reach the flag at the far right to clear the level
- Collect the gold coins

## Current prototype
World 1-1: Green Meadow. This is a first playable foundation; character selection, Yoshi, more levels, enemies, sound, and the full 15-world plan are future additions.
