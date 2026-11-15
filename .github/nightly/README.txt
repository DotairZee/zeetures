NIGHTLY BUILD OF A FORK OF OPENC2E
==================================

This is a nightly build of a noncommercial fan fork of openc2e, the
open-source engine for the Creatures games, focused on Creatures 2. It
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
Linux:
    ./openc2e -d "/path/to/Creatures 2" --mind homeostat

  The path is the folder that holds Creatures 2's Images, Sounds and
  Bootstrap folders. With Steam it is usually under
  ~/.local/share/Steam/steamapps/common/Creatures The Albian Years/Creatures 2

Windows:
    openc2e.exe -d "C:\path\to\Creatures 2" --mind homeostat

  (Windows builds are new and untested.)

Without --mind homeostat the creatures use openc2e's unfinished copy of
the original brain, and do very little.

Your worlds, saves and settings are kept in openc2e's user folder
(~/.openc2e on Linux), never in the game's own folder.

License
-------
openc2e is free software under the GNU Lesser General Public License,
version 2.1 (see COPYING). This fork's changes are under the same
license. The source is on GitHub, in the fork this download came from.
