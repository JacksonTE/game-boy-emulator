# Game Boy Emulator
A cross-platform, cycle-accurate Nintendo Game Boy emulator written in C++. Game ROMs are not provided with the project but can be found easily online.

## Gameplay
| ![Tetris - Gameplay](https://github.com/user-attachments/assets/23dae5d8-91a8-4843-bf50-9f1a194e72ac)                                  | ![Kirby's Dream Land - Gameplay](https://github.com/user-attachments/assets/6bd456a0-53ad-4fef-9dcf-ecd0b2504eca) | ![Super Mario Land - Gameplay](https://github.com/user-attachments/assets/86acf98d-a079-49a3-92b6-7070b0591d3f)                      |
|----------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| ![Pokémon Yellow: Special Pikachu Edition - Gameplay](https://github.com/user-attachments/assets/a6dce68c-cbb2-4688-8d13-b7fac0789bbb) | ![Donkey Kong Land - Gameplay](https://github.com/user-attachments/assets/634cb879-4189-4196-b395-6e6e06999298)   | ![The Legend of Zelda: Link's Awakening - Gameplay](https://github.com/user-attachments/assets/fc67733c-b722-44d2-9a22-70717476da85) |

## Usage Instructions
1. Download the latest available version for your operating system from the `Releases` tab.
2. Unzip the folder and run `emulator-gui.exe`.
3. Acquire a Game Boy game ROM file (not provided but found online easily).
4. In the top menu click `File`->`Load Game ROM`.
5. Select the ROM file to run from the file selector.
6. If the game is currently supported, it will now be running and will be interactable via the default key mappings below.

## Default Controls
<table>
<tr>
<td>

| Game Boy | Key                    |
| -------- | ---------------------- |
| A        | <kbd>X          </kbd> |
| B        | <kbd>Z          </kbd> |
| Up       | <kbd>Up Arrow   </kbd> |
| Down     | <kbd>Down Arrow </kbd> |
| Left     | <kbd>Left Arrow </kbd> |
| Right    | <kbd>Right Arrow</kbd> |
| Start    | <kbd>Enter      </kbd> |
| Select   | <kbd>Shift      </kbd> |

</td>
<td valign="top">

| Emulator      | Key               |
| ------------- | ----------------- |
| Load Game ROM | <kbd>O     </kbd> |
| Quit          | <kbd>Alt+F4</kbd> |
| Fast-Forward  | <kbd>Space </kbd> |
| Pause         | <kbd>Escape</kbd> |
| Reset         | <kbd>R     </kbd> |

</td>
</tr>
</table>

## Features
- Interactable GUI for loading game ROMs and adjusting emulation options made with [Dear ImGui](https://github.com/ocornut/imgui).
- Frame data rendering using [SDL3](https://github.com/libsdl-org/SDL).
- Support for a majority of Game Boy and backwards compatible Game Boy Color games.
- Rebindable button mappings with customizable controls for both Game Boy and emulator functions.
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
5. Two debug targets should populate at the top - select `emulator-gui.exe` for the main [SDL](https://github.com/libsdl-org/SDL)/[ImGui](https://github.com/ocornut/imgui) application.
6. Run by clicking the green play button at the top or by pressing `CTRL+F5`.

**Linux**
1. Clone the `game-boy-emulator` repository.
2. Install `build-essential`, `cmake`, `pkg-config`, and `libgtk-3-dev`.
3. In a terminal, in the `game-boy-emulator` directory, run `mkdir build && cmake -S . -B build && cmake --build build --parallel --target emulator-gui`.
4. This creates an executable that can be run with `./build/bin/emulator-gui`.
