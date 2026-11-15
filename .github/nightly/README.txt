ZEETURES: NIGHTLY BUILD
=======================

Zeetures is a noncommercial fan fork of openc2e, the open-source engine
for the Creatures games (https://github.com/openc2e/openc2e), forked from
openc2e's commit dfcd46f2 of 7 October 2026, and focused on Creatures 2. It
adds a new, original creature mind (the "homeostat" mind), a C2-style
interface with kits, world saving, scents, and many fixes. Nightly
builds are made automatically from whatever was committed that day, so
expect rough edges.

You need your own copy of Creatures 2
-------------------------------------
This download contains no part of the game: no sprites, sounds, genomes,
scripts or world files. The engine reads them from your own installed
copy of Creatures 2, for example Steam's "Creatures: The Albian Years"
or the GOG release. Creatures, Norns, Grendels, Ettins and Albia belong
to Gameware Europe. This project is not affiliated with or endorsed by
Gameware.

Running it
----------
(The program is still called openc2e.)

Linux:
    ./openc2e -d "/path/to/Creatures 2" --mind homeostat

  The path is the folder that holds Creatures 2's Images, Sounds and
  Bootstrap folders. With Steam it is usually under
  ~/.local/share/Steam/steamapps/common/Creatures The Albian Years/Creatures 2

Windows:
    openc2e.exe -d "C:\path\to\Creatures 2" --mind homeostat

macOS:
    ./openc2e -d "/path/to/Creatures 2" --mind homeostat

  The Steam release is for Windows: copy the Creatures 2 folder over from
  a Windows or Linux install. The download is not signed by Apple: the
  first time, right-click openc2e and choose Open, or run
  xattr -dr com.apple.quarantine .  in its folder.

  (The Windows and macOS builds are new and less tried than Linux.)

Without --mind homeostat the creatures use openc2e's unfinished copy of
the original brain, and do very little.

Your worlds, saves and settings are kept in openc2e's user folder
(~/.openc2e on Linux), never in the game's own folder.

License
-------
openc2e is free software under the GNU Lesser General Public License,
version 2.1 (see COPYING). Zeetures's changes are under the same
license. The source: https://github.com/DotairZee/zeetures
