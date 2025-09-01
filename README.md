# Game Boy Emulator
A cross-platform, cycle-accurate Nintendo Game Boy emulator written in C++. Game ROMs are not provided with the project but can be found easily online.

## Gameplay
| ![Tetris Game Boy Gameplay](https://github.com/user-attachments/assets/23dae5d8-91a8-4843-bf50-9f1a194e72ac) | ![Kirby Gameplay](https://github.com/user-attachments/assets/6bd456a0-53ad-4fef-9dcf-ecd0b2504eca) | ![Super Mario Land Gameplay](https://github.com/user-attachments/assets/86acf98d-a079-49a3-92b6-7070b0591d3f) |
|--------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------|

| ![Pokemon Yellow Gameplay](https://github.com/user-attachments/assets/a6dce68c-cbb2-4688-8d13-b7fac0789bbb) | ![Donkey Kong Gameplay](https://github.com/user-attachments/assets/634cb879-4189-4196-b395-6e6e06999298) | ![Zelda Gameplay](https://github.com/user-attachments/assets/fc67733c-b722-44d2-9a22-70717476da85) | 
|-------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------|

## Features
- Interactable GUI for loading game ROMs and adjusting emulation options made with `Dear ImGui`.
- Frame data rendering using `SDL3`.
- Support for a majority of available Game Boy and backwards compatible Game Boy Color games.
- Three preset colour palettes and a custom palette with selectable colours.
- Pausing emulation and emulating at up to 4x speed, provided that the target hardware can produce frame data fast enough.

## Future Additions
- Implement the Audio Processing Unit to recreate sound from Game Boy ROMs.
- Implement save state exporting and cartridge ram exporting.
- Port to browser with Emscripten.
- Add support for Game Boy Color games. 

## Compilation
**Windows**
1. Clone the `game-boy-emulator` repository.
2. Install `Visual Studio Community 2022`.
3. Select the `Desktop development with C++` workload to download and install.
4. In Visual Studio, click `Open a local folder` and select the cloned `game-boy-emulator` folder.
5. Two debug targets should populate at the top - select `emulator-gui.exe` for the main SDL/ImGui application.
6. Run by clicking the green play button at the top or by pressing `CTRL+F5`.

**Linux**
1. Clone the `game-boy-emulator` repository.
2. Install `build-essential`, `cmake`, `pkg-config`, and `libgtk-3-dev`.
3. In a terminal, in the `game-boy-emulator` directory, run `mkdir build && cmake -S . -B build && cmake --build build --parallel --target emulator-gui`.
4. This creates an executable that can be run with `./build/bin/emulator-gui`.

## Usage Instructions
1. Acquire a Game Boy game ROM file (not provided with the project but found online easily).
2. Run the project and in the top menu click `File`->`Load Game ROM`.
3. Select the ROM file to run.
4. If the game is currently supported, it will now be running, and will be interactable via the key mappings below.

## Controls
| Game Boy | Key |
| --- | --- |
| A | <kbd>'</kbd> |
| B | <kbd>.</kbd> |
| Up | <kbd>W</kbd> |
| Down | <kbd>S</kbd> |
| Left | <kbd>A</kbd> |
| Right | <kbd>D</kbd> |
| Start | <kbd>Enter</kbd> |
| Select | <kbd>Shift</kbd> |

| Emulator | Key |
| --- | --- |
| Load Game ROM | <kbd>O</kbd> |
| Quit | <kbd>Alt+F4</kbd> |
| Fast-Forward | <kbd>Space</kbd> |
| Pause | <kbd>Escape</kbd> |
| Reset | <kbd>R</kbd> |
