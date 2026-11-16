ZEETURES: NIGHTLY BUILD
=======================

Zeetures is a labor of love by an old fan: a noncommercial fan fork of
openc2e, the open-source engine for the Creatures games
(https://github.com/openc2e/openc2e), forked from openc2e's commit
dfcd46f2 of 7 October 2026, and focused on Creatures 2. It adds a new,
original creature mind (the "homeostat" mind), a C2-style interface with
kits, world saving, scents, and many fixes. Nightly builds are made
automatically from whatever was committed that day, so expect rough
edges.

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
    ./openc2e -d "/path/to/Creatures 2"

  The path is the folder that holds Creatures 2's Images, Sounds and
  Bootstrap folders. With Steam it is usually under
  ~/.local/share/Steam/steamapps/common/Creatures The Albian Years/Creatures 2

Windows:
    openc2e.exe -d "C:\path\to\Creatures 2"

macOS (Apple Silicon, M1 or later; this build does not run on Intel Macs):
    ./openc2e -d "/path/to/Creatures 2"

  The Steam release is for Windows: copy the Creatures 2 folder over from
  a Windows or Linux install. The download is not signed by Apple: the
  first time, right-click openc2e and choose Open, or run
  xattr -dr com.apple.quarantine .  in its folder.

  (The Windows and macOS builds are new and less tried than Linux.)

The creatures use Zeetures's own mind (--mind original gives them
openc2e's unfinished copy of the original brain, which does very little).

Two breed packs are fetched on your machine the first time a Creatures 2
world runs (with curl, from the Eem Foo archive, which already hosts
them) and imported: Boney Grendels (Creature Labs) and Frog Norns V3 (by
Alex of Norns of the Round Table, from Nornenmeister's V2), which takes
the place of the game's own frog norn. Zeetures does not carry them. To
skip this, put the line "breed-packs off" in your user folder's
settings.txt.

Zeetures's world additions are built in and applied to every Creatures 2
world. For now the 17 that draw new pictures (walkways, new lifts,
airlocks, seasonal bushes, the memorial stone and others) are skipped,
because their art is not shipped yet.

Your worlds, saves and settings are kept in the user folder, never in
the game's own folder:
    Linux:    ~/.openc2e/Creatures 2
    Windows:  Documents\My Games\Creatures 2
    macOS:    ~/Documents/openc2e Data/Creatures 2

License
-------
openc2e is free software under the GNU Lesser General Public License,
version 2.1 (see COPYING). Zeetures's changes are under the same
license. The source: https://github.com/DotairZee/zeetures
