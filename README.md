# Zeetures

> **Zeetures: a fan fork of openc2e, focused on Creatures 2** (forked from openc2e's commit [dfcd46f2](https://github.com/openc2e/openc2e/commit/dfcd46f2), 7 October 2026). It adds an original creature mind (`--mind homeostat`), a C2-style interface with the kits, world saving, scents and many fixes; see the `creature-mind` branch. Nightly builds are under [Releases](../../releases/tag/nightly) (they contain no game data: you need your own copy of Creatures 2). A noncommercial labor of love by an old fan, not affiliated with Gameware Europe or the openc2e developers. Every change is in the [changelog](CHANGELOG.md). See [Built on openc2e](#built-on-openc2e) below.

## What you need

* **Your own copy of Creatures 2.** Zeetures contains no part of the game: no sprites, sounds, genomes, scripts or worlds. It reads them from your installed copy. Creatures 2 comes with *Creatures: The Albian Years* on Steam, and is also sold on GOG. Creatures, Norns, Grendels, Ettins and Albia belong to Gameware Europe.
* A Linux, Windows or macOS computer. There are 64-bit builds for all three, and 32-bit (x86) builds for Linux and Windows.

## Download and play

Ready-to-run builds for each system are made automatically every night there are changes: see the [nightly release](../../releases/tag/nightly).

| System | Download |
| --- | --- |
| Linux (x86-64) | `zeetures-nightly-linux-x86_64.tar.gz` |
| Windows (64-bit) | `zeetures-nightly-windows-x64.zip` |
| macOS (Apple Silicon) | `zeetures-nightly-macos-arm64.zip` |
| Linux (32-bit x86) | `zeetures-nightly-linux-i386.tar.gz` |
| Windows (32-bit) | `zeetures-nightly-windows-x86.zip` |

Unpack it and start the program (still called `openc2e`) with the path to your Creatures 2 folder, the one that holds its `Images`, `Sounds` and `Bootstrap` folders:

* **Linux:** `./openc2e -d "$HOME/.local/share/Steam/steamapps/common/Creatures The Albian Years/Creatures 2" --mind homeostat`
* **Windows:** `openc2e.exe -d "C:\Program Files (x86)\Steam\steamapps\common\Creatures The Albian Years\Creatures 2" --mind homeostat`
* **macOS:** `./openc2e -d "/path/to/Creatures 2" --mind homeostat`. The Steam release is for Windows, so copy the `Creatures 2` folder over from a Windows or Linux install. The download is not signed by Apple: the first time, right-click `openc2e` and choose Open, or run `xattr -dr com.apple.quarantine .` in its folder.

`--mind homeostat` gives the creatures Zeetures's own mind; without it they use openc2e's unfinished copy of the original brain and do very little. Your worlds, saves and settings are kept in openc2e's user folder (`~/.openc2e` on Linux), never in the game's folder.

Linux is where Zeetures is played and tested every day; the Windows and macOS builds are new and less tried, and the 32-bit builds are compiled every night but have not yet been tried on a 32-bit machine.

## Status

Zeetures is at version 0.7.0 (shown in Help > About). It is a work in progress, but Creatures 2 is playable from start to finish: creatures hatch, eat, learn, talk, breed, grow old and die, and worlds are saved.

### What works

* **A new creature mind.** In stock openc2e, C2 creatures hatch but do almost nothing, because the original brain was never finished. Zeetures adds a small "creature mind" interface to the engine and an entirely new mind behind it (`--mind homeostat`): an original design, not a recreation of the C2 brain. Creatures learn what each action does to their needs, remember where food is, follow smells, learn a map of the world (lifts, boats, teleporters), hold their breath underwater, avoid dangerous drops and show their thoughts in picture bubbles.
* **Personality and language.** Seven inborn temperament traits; relationships with friends, enemies and mates; creatures born knowing no words, who learn them from the learning machines, the hand, each other and the Stone of Knowledge, and babble in "Nornish" until they do.
* **The life cycle.** Courtship, genetic crossover with mutation, pregnancy, eggs and hatching; aging, deaths with real causes, and grown creatures who tend babies, children and the senile. Breed import from zips, COBs, old installers and .exp files.
* **Saving.** Multiple worlds with a chooser and autosave. Creatures, their minds, eggs, family records and every object in the world are saved.
* **The interface.** Every C2 kit rebuilt inside the game window (Health, Science, Breeder's, Owner's, Observation, Mind, Graveyard, Chronicles, Ecology, the Agent Injector and the Hatchery), status-bar notices, a speed control, zoom from 50% to 300%, pinch to zoom, scroll bars and full screen.
* **The world.** Dozens of map fixes, new routes and objects (seasonal fruit bushes, airlocks, walkways, a lab lift), scents that drift between rooms, day and night, and seasons.
* **Stability.** Fixes for an audio deadlock, several crashes and hangs, and a game tick about seven times faster in a crowded world.

### Not done yet

* Lifts, boats and the hand start each session as the world file has them.
* Inherited mind traits and evolution.
* Hunger in very crowded worlds (about 150 creatures in a world built for 16).
* Windows and macOS are built every night but played far less than Linux.
* Creatures 2 is the focus. Creatures 1, Creatures 3 and Docking Station run as they do in upstream openc2e, and have not been tested in Zeetures.

Every change, in technical detail, is in the [changelog](CHANGELOG.md).

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

### 32-bit builds

On Windows, add `-A Win32` to the first `cmake` line to build for 32-bit x86. On Debian or Ubuntu, a 32-bit build on a 64-bit system needs the multilib compilers and the `:i386` development packages:

```bash
sudo dpkg --add-architecture i386 && sudo apt-get update
sudo apt-get install gcc-multilib g++-multilib zlib1g-dev:i386 libpng-dev:i386 libx11-dev:i386 libxext-dev:i386 \
  libxcursor-dev:i386 libxrandr-dev:i386 libxi-dev:i386 libxss-dev:i386 libxtst-dev:i386 libxfixes-dev:i386 \
  libxkbcommon-dev:i386 libgl1-mesa-dev:i386 libegl1-mesa-dev:i386 libasound2-dev:i386 libpulse-dev:i386
PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig cmake -S . -B build32 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_LIBRARY_ARCHITECTURE=i386-linux-gnu \
  -DOPENC2E_BUNDLED_SDL=ON -DOPENC2E_ASAN=OFF -DSDL_WAYLAND=OFF
cmake --build build32 -j"$(nproc)"
```

(On a 32-bit system itself, the plain Linux steps above work as they are.)

## Command-line options

`openc2e --help` lists them all. The ones Zeetures adds or uses most:

* `-d PATH`: the Creatures 2 folder (needed).
* `--mind homeostat`: give the creatures Zeetures's mind.
* `--world NAME`: open that world from the Worlds folder, skipping the world chooser.
* `--no-autosave`: don't save the world on quit or when switching worlds.
* `--no-world-chooser`: load the game's own starting world, as the original game does.
* `-s`: no sound.

# Built on openc2e

![openc2e logo](https://raw.githubusercontent.com/ligfx/openc2e/master/Openc2e-logo2008.png)

Zeetures is a fork of [openc2e](https://github.com/openc2e/openc2e), the free and open-source engine for the [_Creatures_](https://creatures.wiki) artificial life games (Creatures, Creatures 2, Creatures 3, Docking Station and more), by Alyssa Milburn, Michael Maltese, Bryan Donlan, Jeremy Apthorp, Eric Goodwin and others. It was forked from openc2e's commit [dfcd46f2](https://github.com/openc2e/openc2e/commit/dfcd46f2) of 7 October 2026. For openc2e itself, its other games and its own status, see [its repository](https://github.com/openc2e/openc2e); its developers and players gather in the [Caos Coding Cave Discord](https://discord.gg/rWFC3b3). Zeetures is not affiliated with the openc2e developers.

openc2e is free software under the GNU Lesser General Public License, version 2.1 (see `COPYING`), and Zeetures's changes are under the same license.
