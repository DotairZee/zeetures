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

  The path is the folder that holds Creatures 2's Images and Sounds
  folders and Eden.sfc. With Steam it is usually under
  ~/.local/share/Steam/steamapps/common/Creatures The Albian Years/Creatures 2

Windows:
    openc2e.exe -d "C:\path\to\Creatures 2"

macOS (the arm64 download for Apple Silicon, M1 or later; x86_64 for
Intel Macs, macOS 10.15 or later):
    ./openc2e -d "$HOME/Games/Creatures The Albian Years/Creatures 2"

  Zeetures runs natively: no Wine, CrossOver or Steam Play is needed,
  only the game's files. Steam for Mac won't install the game (it is
  listed as Windows-only), but downloads its files through its console:
  run  open "steam://open/console"  in Terminal, then type
      download_depot 1818340 1818341
  in Steam's Console tab. The files land in
  ~/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/
  MacOS/steamapps/content/app_1818340/depot_1818341 ; copy that folder
  somewhere permanent, for example ~/Games/Creatures The Albian Years
  (ditto <that folder> ~/Games/"Creatures The Albian Years"), and use its
  Creatures 2 folder. The download is not signed by Apple: the first
  time, right-click openc2e and choose Open, or run
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
world, with the pictures Zeetures draws itself in the zeetures-art
folder (keep it beside the program). For now 7 of the 39 are skipped,
because their pictures are cut from the game's own and are not yet made
on your machine (the waterwheel stream, the west sea airlock, the
volcano passage and trunk, the treehouse lift, the waterfall path and a
pillar's foot).

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
