# Emulate Game Boy

A cross-platform, cycle accurate emulator of the Nintendo Game Boy (DMG) written in C++. Game ROMs are not provided with the project but can be found easily online.

## Gameplay
| ![Image](https://github.com/user-attachments/assets/b55f6f5c-91e9-4e4b-9c8f-3b80293b4ecd) | ![Image](https://github.com/user-attachments/assets/bde64b63-2a41-4dfc-9a43-236feb244979) | ![Image](https://github.com/user-attachments/assets/0300acab-d50c-4fe6-9c85-31351e4e9793) | 
|---------------------------------|---------------------------------|---------------------------------|

## Screenshots
| <img width="399" alt="Image" src="https://github.com/user-attachments/assets/9dbadadd-039c-4363-a377-5190fed293b7" /> | <img width="398" alt="Image" src="https://github.com/user-attachments/assets/57bcda3e-c0d3-4004-b749-548d11c65bae" /> | <img width="395" alt="Image" src="https://github.com/user-attachments/assets/77706150-6746-4ac2-b9af-740071f5e7ee" /> | 
|---------------------------------|---------------------------------|---------------------------------|

## Features
- Interactable user interface for loading game ROMs and adjusting emulation options made with ```Dear ImGui```.
- Frame data rendering using ```SDL3```.
- Support for a majority of available Game Boy and backwards compatible Game Boy Color games.
- Three preset colour palettes and a custom colour palette with selectable colours.

## Planned Features
- Implement the Audio Processing Unit (APU) to recreate sound from Game Boy ROMs.
- Implement save state exporting and cartridge ram exporting.
- Port to browser with Emscripten.
- Add support for Game Boy Color (CGB) games. 

## Compilation
**Windows**
1. Clone the ```emulate-game-boy``` repository.
2. Install ```Visual Studio Community 2022```.
3. Select the ```Desktop development with C++``` workload to download and install.
4. In Visual Studio, click ```Open a local folder``` and select the cloned ```emulate-game-boy``` folder.
5. Two debug targets should populate at the top - select ```emulate-game-boy-app.exe``` for the main SDL application.
6. Run by clicking the green play button at the top or by pressing ```CTRL+F5```.

**Linux**
1. Clone the ```emulate-game-boy``` repository.
2. Install ```build-essential```, ```cmake```, ```pkg-config```, and ```libgtk-3-dev```.
3. In a terminal, in the ```emulate-game-boy``` directory, run ```mkdir build```, ```cmake -S . -B build```, ```cmake --build build```.
4. This creates an executable that can be run with ```./build/bin/emulate-game-boy-app```.
5. For subsequent builds, only ```cmake --build build``` is required from step 3.

## Usage Instructions
1. Acquire a Game Boy game ROM file (not provided with the project but found online easily).
2. Run the project and in the top menu click ```File```->```Load Game ROM```.
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
