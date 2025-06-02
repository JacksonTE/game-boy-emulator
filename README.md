# Emulate Game Boy

A cycle accurate emulator of the Nintendo Game Boy (DMG) written in C++. Game ROMs are not provided with the project and must be acquired elsewhere.

## Gameplay
| ![Image](https://github.com/user-attachments/assets/b55f6f5c-91e9-4e4b-9c8f-3b80293b4ecd) | ![Image](https://github.com/user-attachments/assets/bde64b63-2a41-4dfc-9a43-236feb244979) | ![Image](https://github.com/user-attachments/assets/0300acab-d50c-4fe6-9c85-31351e4e9793) | 
|---------------------------------|---------------------------------|---------------------------------|

## Screenshots
| <img width="399" alt="Image" src="https://github.com/user-attachments/assets/9dbadadd-039c-4363-a377-5190fed293b7" /> | <img width="398" alt="Image" src="https://github.com/user-attachments/assets/57bcda3e-c0d3-4004-b749-548d11c65bae" /> | <img width="395" alt="Image" src="https://github.com/user-attachments/assets/77706150-6746-4ac2-b9af-740071f5e7ee" /> | 
|---------------------------------|---------------------------------|---------------------------------|

## Features
- A complete Central Processing Unit, Memory Management Unit, Pixel Processing Unit, and Internal Timer (the emulated internals of the original Game Boy).
- Interactable user interface for loading ROMs and adjusting emulation options made with ```Dear ImGui```.
- Rendering frame data produced during GameBoyCore emulation using ```SDL3```.
- Support for 32KiB ROM-only games and games with original cartridges that used the MBC1 memory bank controller.
- Three preset colour palettes including the light green featured in the above screenshots, greyscale black and white, and a darker green similar to the original hardware.

## Planned Features
- Implement the Audio Processing Unit (APU) to recreate sound from Game Boy ROMs.
- Add support for memory bank controllers MBC2, MBC3, and MBC5 for compatability with most games.
- Add support for Game Boy Color (CGB) games. 

## Compilation
To build this project on Windows, the following are required:
- CMake version 3.30.5 or higher.
- A C++ compiler supporting C++20.

Here are several methods to compile and run the project:

**Visual Studio CMake Project**
1. Clone the ```emulate-game-boy``` repository.
2. In Visual Studio, click ```Open a local folder``` and select the ```emulate-game-boy``` folder.
3. Two debug targets should populate at the top - select ```emulate-game-boy-app.exe```.
4. Run by clicking the play button or CTRL+F5.

**Command Line**
1. Clone the ```emulate-game-boy``` repository.
2. Navigate into the ```emulate-game-boy``` folder using in the terminal.
3. Execute ```cmake -S . -B build -DCMAKE_BUILD_TYPE=Release```.
4. Execute ```cmake --build build```.
5. This will create an ```emulate-game-boy-app.exe``` executable that can be run.

## Usage Instructions
1. Acquire a Game Boy game ROM file (not provided with the project).
2. Run the project and in the top menu click ```File```->```Load Game ROM```.
3. Select the ROM file to run.
4. If the game is currently supported, it should now be running, and should be interactable via the key mappings below.

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
