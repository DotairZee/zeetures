#!/usr/bin/env python3
"""Check that saved worlds still load in a new build (owner, 2026-11-21: old
worlds must survive every new version). Run it before a release.

Each world under WORLDS that has a save is copied into a throwaway home
folder (the originals are never touched), loaded by the engine without
autosave, and its "Restored saved world" line is compared with the creatures
and eggs its save file lists. Needs the player's own Creatures 2 folder.

    scripts/check_saves.py ENGINE C2_FOLDER WORLDS_FOLDER [WORLD ...]

For example:
    scripts/check_saves.py build-fast/openc2e ~/.steam/.../Creatures\\ 2 ~/.openc2e/Creatures\\ 2/Worlds

Exit status 0 when every world loads with every creature and egg.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading


def counts(state):
    """The save's creatures, eggs and kept objects (blocks of state.txt)."""
    n = {"creature": 0, "egg": 0, "object": 0}
    with open(state, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line in n:
                n[line] += 1
    return n["creature"], n["egg"], n["object"]


def check(engine, c2, world_dir):
    name = os.path.basename(world_dir.rstrip("/"))
    state = os.path.join(world_dir, "save", "state.txt")
    if not os.path.exists(state):
        return None
    want = counts(state)
    with tempfile.TemporaryDirectory(prefix="zeetures-savecheck-") as home:
        worlds = os.path.join(home, ".openc2e", "Creatures 2", "Worlds")
        os.makedirs(worlds)
        shutil.copytree(world_dir, os.path.join(worlds, name))
        env = dict(os.environ, HOME=home, SDL_VIDEO_DRIVER="offscreen", SDL_AUDIO_DRIVER="dummy", OPENC2E_NO_GENOME_TIDY="1")
        # (read until the load is reported, then stop the engine: it would
        # otherwise run on; CHECK_SAVES_TIMEOUT seconds at most)
        limit = float(os.environ.get("CHECK_SAVES_TIMEOUT", "300"))
        p = subprocess.Popen([engine, "-d", c2, "--world", name, "--no-world-chooser", "--no-autosave"],
                             env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        out = ""
        timer = threading.Timer(limit, p.kill)
        timer.start()
        for line in p.stdout:
            out += line
            if "Restored saved world" in line or "could not be read" in line:
                break
        timer.cancel()
        p.kill()
        p.wait()
    m = re.search(r"Restored saved world: (\d+) creatures, (\d+) eggs, (\d+) kept", out)
    if not m:
        why = "could not be read" if "could not be read" in out else "no 'Restored saved world' line"
        return f"FAIL {name}: {why}"
    got = tuple(int(g) for g in m.groups())
    same = "ok  " if got == want else "FAIL"
    return f"{same} {name}: " + ", ".join(f"{g}/{w} {what}" for g, w, what in zip(got, want, ("creatures", "eggs", "kept objects")))


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    engine, c2, worlds = os.path.abspath(sys.argv[1]), sys.argv[2], sys.argv[3]
    names = sys.argv[4:] or sorted(os.listdir(worlds))
    failed = 0
    for n in names:
        res = check(engine, c2, os.path.join(worlds, n))
        if res:
            print(res, flush=True)
            failed += res.startswith("FAIL")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
