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
The friendliest downloads are Zeetures-Setup.exe (Windows),
Zeetures-macOS.dmg (Mac) and Zeetures-x86_64.AppImage (Linux), on the
same release page; the one-time steps for each are under "Download and
play" at https://github.com/DotairZee/zeetures#download-and-play

This archive is the same program unpacked. Start Zeetures.exe (Windows)
or openc2e (Linux) from this folder: no command line is needed. The first
time, it looks for Creatures 2 in the usual Steam and GOG folders; if it
finds none, a window asks for the folder (the one that holds
Creatures2.exe, Eden.sfc and the Images folder, or the Creatures The
Albian Years folder that holds it) and remembers it. To change it later:
File > Creatures 2 Folder.

From a terminal, -d still gives the folder directly:
    ./openc2e -d "/path/to/Creatures 2"
    Zeetures.exe -d "C:\path\to\Creatures 2"

  (The Windows builds are newer and less tried than Linux.)

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
world: all 42 of them. The pictures Zeetures draws itself are in the
zeetures-art folder (keep it beside the program); the ones cut from the
game's own pictures (the waterwheel stream, the west sea airlock's rock,
the volcano passage and trunk, the treehouse lift, the waterfall path, a
pillar's foot) are made by Zeetures from your own copy of the game when
it starts.

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
