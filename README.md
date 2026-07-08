# Most Wanted Collision Viewer

An experiment for Need for Speed: Most Wanted that shows collision meshes

<img width="1920" height="1080" alt="collview" src="https://github.com/user-attachments/assets/349a5f94-38aa-44b0-a5f4-01f7efcaaff3" />

## Installation

- Make sure you have v1.3 of the game, as this is the only version this plugin is compatible with. (exe size of 6029312 bytes)
- Plop the files into your game folder.
- Start the game and press F5 to open the mod's menu and tweak the settings.
- Enjoy, nya~ :3

## Building

Building is done on an Arch Linux system with CLion being used for the build process. 

Before you begin, clone [nya-common](https://github.com/gaycoderprincess/nya-common), [nya-common-nfsmw](https://github.com/gaycoderprincess/nya-common-nfsmw), and [CwoeeMenuLib](https://github.com/gaycoderprincess/CwoeeMenuLib)to folders next to this one, so they can be found.

Required packages: `mingw-w64-gcc vcpkg`

To install all dependencies, use:
```console
vcpkg install tomlplusplus:x86-mingw-static
```

To install the BASS audio library:

Download the Win32 version from [here](https://www.un4seen.com/bass.html) and extract it somewhere

Once that's done, copy `bass.lib` from the `c` folder into `nya-common/lib32`

You should be able to build the project now in CLion.
