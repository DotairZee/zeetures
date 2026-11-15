![openc2e logo](https://raw.githubusercontent.com/ligfx/openc2e/master/Openc2e-logo2008.png)

# Zeetures

> **Zeetures: a fan fork of openc2e, focused on Creatures 2** (forked from openc2e's commit [dfcd46f2](https://github.com/openc2e/openc2e/commit/dfcd46f2), 7 October 2026). It adds an original creature mind (`--mind homeostat`), a C2-style interface with the kits, world saving, scents and many fixes; see the `creature-mind` branch. Nightly builds are under [Releases](../../releases/tag/nightly) (they contain no game data: you need your own copy of Creatures 2). A noncommercial project, not affiliated with Gameware Europe or the openc2e developers. Everything after the Zeetures section is upstream openc2e's README.

## What you need

* **Your own copy of Creatures 2.** Zeetures contains no part of the game: no sprites, sounds, genomes, scripts or worlds. It reads them from your installed copy. Creatures 2 comes with *Creatures: The Albian Years* on Steam, and is also sold on GOG. Creatures, Norns, Grendels, Ettins and Albia belong to Gameware Europe.
* A 64-bit Linux, Windows or macOS computer.

## Download and play

Ready-to-run builds for each system are made automatically every night there are changes: see the [nightly release](../../releases/tag/nightly).

| System | Download |
| --- | --- |
| Linux (x86-64) | `zeetures-nightly-linux-x86_64.tar.gz` |
| Windows (64-bit) | `zeetures-nightly-windows-x64.zip` |
| macOS (Apple Silicon) | `zeetures-nightly-macos-arm64.zip` |

Unpack it and start the program (still called `openc2e`) with the path to your Creatures 2 folder, the one that holds its `Images`, `Sounds` and `Bootstrap` folders:

* **Linux:** `./openc2e -d "$HOME/.local/share/Steam/steamapps/common/Creatures The Albian Years/Creatures 2" --mind homeostat`
* **Windows:** `openc2e.exe -d "C:\Program Files (x86)\Steam\steamapps\common\Creatures The Albian Years\Creatures 2" --mind homeostat`
* **macOS:** `./openc2e -d "/path/to/Creatures 2" --mind homeostat`. The Steam release is for Windows, so copy the `Creatures 2` folder over from a Windows or Linux install. The download is not signed by Apple: the first time, right-click `openc2e` and choose Open, or run `xattr -dr com.apple.quarantine .` in its folder.

`--mind homeostat` gives the creatures Zeetures's own mind; without it they use openc2e's unfinished copy of the original brain and do very little. Your worlds, saves and settings are kept in openc2e's user folder (`~/.openc2e` on Linux), never in the game's folder.

Linux is where Zeetures is played and tested every day; the Windows and macOS builds are new and less tried.

## Building it yourself

You need CMake (3.16 or newer), Python 3, Git and a C++20 compiler. SDL3, SDL3_mixer, zlib (on Windows), fmt, imgui and the rest come with the source, in `externals`.

```bash
git clone https://github.com/DotairZee/zeetures.git
cd zeetures
```

### Linux (Debian, Ubuntu and relatives)

```bash
sudo apt-get install build-essential cmake python3 git zlib1g-dev libpng-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxfixes-dev \
  libwayland-dev libxkbcommon-dev libegl1-mesa-dev libgl1-mesa-dev libdecor-0-dev wayland-protocols \
  libasound2-dev libpulse-dev libpipewire-0.3-dev libdbus-1-dev libudev-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENC2E_ASAN=OFF -DOPENC2E_BUNDLED_SDL=ON
cmake --build build -j"$(nproc)"
./build/openc2e -d "/path/to/Creatures 2" --mind homeostat
```

`OPENC2E_BUNDLED_SDL=ON` builds the SDL3 in `externals` (the packages above are its headers). If your system has SDL3 development packages (`libsdl3-dev`), you can leave it off. `OPENC2E_ASAN=OFF` turns off AddressSanitizer, which is on by default and makes the game several times slower.

### Windows

Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with the "Desktop development with C++" workload (it includes CMake), and [Python 3](https://www.python.org/). In a Developer PowerShell or Command Prompt:

```bat
cmake -S . -B build -DOPENC2E_ASAN=OFF
cmake --build build --config RelWithDebInfo
build\RelWithDebInfo\openc2e.exe -d "C:\path\to\Creatures 2" --mind homeostat
```

### macOS

Install the Xcode command line tools (`xcode-select --install`) and CMake and Python 3 (for example with [Homebrew](https://brew.sh/): `brew install cmake python`). Then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENC2E_ASAN=OFF
cmake --build build -j"$(sysctl -n hw.ncpu)"
./build/openc2e -d "/path/to/Creatures 2" --mind homeostat
```

# openc2e

openc2e is a free and open-source game engine for the [_Creatures_](https://creatures.wiki) artificial life games.

The goal is to allow you to play games such as Creatures, Creatures 2, Creatures 3, Docking Station and more, on many different platforms.

Want to know more? Come visit us on the [Caos Coding Cave Discord](https://discord.gg/rWFC3b3).

# Games

openc2e is intended to support:

* Creatures (1996)
* Creatures 2 (1998)
* Creatures 3 (1999)
* Docking Station (2001)
* Creatures Playground (1999)
* Creatures Adventures (2000)

# Status

## Working

* Agents (COBs) work
* Imperfect physics
* Creatures biochemistry
* Sound effects

## Todo

* Actual creatures (C3+ creatures work somewhat, C1 and C2 ones don't work at all)
* Physics improvements (Especially C3+)
* Network ability (the Docking Station warp)
* Serialization (world saving, creature exporting, ...)

There are lots of [open issues](https://github.com/openc2e/openc2e/issues) that still need solving.

# Building

## Dependencies

openc2e depends on:

* SDL2
  * SDL2_mixer
* Python
* CMake
* Boost.Serialization (optional)

### Linux

#### Ubuntu

On Ubuntu 18.10 and up, you'll need these packages:

```bash
sudo apt-get install \
  build-essential \
  libsdl2-dev \
  libsdl2-mixer-dev \
  cmake
```

### macOS

Install dependencies using [Homebrew](https://brew.sh):

```bash
brew install cmake
```

### Windows

Install [Python](https://www.microsoft.com/en-us/p/python-38/9mssztt1n39l#activetab=pivot:overviewtab), and Visual Studio's [C++ CMake Tools for Windows](https://docs.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=vs-2019#installation).

Open the folder in Visual Studio and it will automatically run CMake and set up the build system.

## Compiling

Create a new build directory and compile:

```bash
cmake -B build .
make -C build openc2e -j4
```

## Running

You should provide a path to a game's data files with the `-d` or `--data-path` flag:

```bash
./build/openc2e -d /path/to/Creatures2
```

The engine will try to guess the game based on files in the directory. If it can't decide, it will default to C3/DS.

## Advanced Usage

To see an overview of all available options, use `-h` / `--help`:
```bash
./build/openc2e --help
```
