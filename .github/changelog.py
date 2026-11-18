#!/usr/bin/env python3
"""Write CHANGELOG.md: every change Zeetures has made to openc2e, from the
commit history (each commit's full message), grouped by version and day,
newest first.

    python3 .github/changelog.py           # print it
    python3 .github/changelog.py --write   # write CHANGELOG.md
    python3 .github/changelog.py --check   # exit 1 if CHANGELOG.md is stale

Run from anywhere in the repository. Commits that change only CHANGELOG.md
are left out, so the commit that updates the file does not make it stale.

When the version is raised (OPENC2E_FORK_VERSION in
src/openc2e/openc2eimgui/C2ToolBar.cpp), add the version to VERSIONS below:
its last commit (the bump, or the last commit folded into it) and a short
summary. Commits after the last entry are listed as "Unreleased".
"""
import re
import subprocess
import sys
from collections import OrderedDict

REPO_URL = "https://github.com/DotairZee/zeetures"
FORK_POINT = "dfcd46f2"  # openc2e's commit of 7 October 2026
OUT = "CHANGELOG.md"

# (version, last commit in it, summary); oldest first
VERSIONS = [
    ("0.1.0", "99b5f845",
     "The first five days of the fork: the pluggable creature mind interface and the homeostat mind; "
     "C1/C2 creatures interacting with objects; breeding from courtship to hatching; world saving; "
     "the C2-style menus, toolbars and kits; speech and Nornish; temperament and relationships; "
     "map fixes and engine crash, deadlock and hang fixes. The version number first shown in Help > About."),
    ("0.3.0", "35cfb114",
     "Renumbered past openc2e's last release (v0.2, June 2020), since the base already ran 825 commits past it. "
     "Hand, toolbar and menu work, sealed-pocket rescue, speech and walking fixes."),
    ("0.4.0", "82320cdf",
     "Food that cannot be eaten no longer counts as food; stalled meals end; starving creatures leave a "
     "foodless place by a known way out; lift links only from real rides; bodies float away again; the "
     "game speed control on the toolbar; script-moved creatures glide between ticks; vivid babies."),
    ("0.5.0", "60b22c3f",
     "Every object in the world saved with it (stage 3), C2's announcement windows, starving creatures "
     "walking to a remembered teleporter, and a repeated device press judged as one try."),
    ("0.6.0", "ac33de2b",
     "Signature scents; the hand takes things from creatures and keeps its mode; fertility switches; "
     "sunken and stranded creatures and objects kept out of the rock; the speed spin box; the portability "
     "audit for Windows and macOS; shut doors stop thrown creatures; a light old-age gray for breeds without "
     "old-age pictures; grown creatures tend the young and the senile; scents on the Ecology Kit's map; the "
     "\"+N\" notice list; pinch zoom; the hand pets on the head, spanks at the hips and pushes on the body."),
    ("0.6.1", "2c1d95d0",
     "The first version published as Zeetures, with nightly builds for Linux, Windows and macOS. "
     "Notices kept through restarts, the hand's hot spot at the fingertip, scent dose bands and habituation, "
     "a narrower notice list, 32-bit Windows and Linux builds, and the README brought up to date."),
    ("0.7.0", "e97a386e",
     "The memorial stone, an Easter egg for the four who grew old together, and the \"Together\" notice when "
     "four or more that hatched together die of old age together; the world's additions and art recipes in the "
     "repository; the waterwheel and seabed airlock rides a touch less smooth; this changelog."),
    ("0.8.0", "f63adf35",
     "The same default world on any machine: the world additions built into the program, their art shipped or "
     "made in the engine from the player's own game, Home's settings as defaults, and the Boney Grendels and Frog "
     "Norns V3 packs fetched and imported; an Intel Mac build; all sounds on to start. Seasons that change "
     "creatures' biology (eat more in autumn, save in winter), how strongly inherited from the parents. The west "
     "sea airlock lets creatures out into the pool east of it; mind fixes that freed creatures pinned at walls "
     "and waiting on food out of reach; real crowdedness, radiation and slope senses; the population limit counts "
     "every creature; more fruit and bushes; stranded objects set back inside the rooms; a census of hunger and "
     "food every 5 minutes; the relationship map's zoom and drag; where each creature died kept with the world."),
    ("0.9.0", "db095114",
     "Seasons that shape breeding, bonds, rest and play, weighted by life stage (a spring breeding season, "
     "company as warmth and stronger loneliness in winter, more rest in winter and play in summer); curiosity "
     "that follows learning progress; eating ahead of a long time without food; food less pleasant the fuller "
     "a creature is; the research behind Zeetures listed in Help > About, with newer work beside each older "
     "source; about half the CPU use (a room lookup grid, cached record and kinship lookups, lighter status-bar "
     "and drawing work) and a built-in sampling profiler."),
    ("0.10.0", "6e872d9a",
     "Temperament partly passed on from the parents. Old custom genomes work: eggs with an author's description "
     "read and hatchable (and Import Genome takes eggs); the genomes' sense, drive, decision and attention cells "
     "set from what the creature does, so their emitters work (the norns' kin pheromones among them); instincts "
     "naming senses or no drive become inborn nudges judged by the genome's reward and punishment chemicals; lobe "
     "7 read as attention (Canny Norns' eating instincts); brain profiles giving added lobes roles in the mind "
     "(Nova Subterra and Canny Norns known, others read from their wiring), shown in the Breeder's Kit."),
    ("0.11.0", "f5e5ae6f",
     "Temperament that shifts with experience while young (hurt, comfort, fights, exploring), tapering as a "
     "creature grows up; the soil's nutrient cycle (breakdown, seeping downhill, the dead feeding it, bushes "
     "growing by it), shown in the Ecology Kit with food smell as a scent; outdoor light following day and night; "
     "walking the way the creature faces (a cause of drownings); brain profiles reading search and chemical-fed "
     "lobes from wiring; Import Genome takes exported creatures; Creature's View without the viewer's own bubbles, "
     "seen through the creature's own eye (pupil by light and fear, blinking and drooping lids, iris tinted by its "
     "pigment genes, sight doubled when hurt and rippling underwater), centered on what it looks at, the world "
     "showing through a checkered screen."),
    ("0.12.0", "46e2d6ae",
     "Ways out of the seas: the submarine picks up swimmers along its path and lets them out dry at its next "
     "dock, the west sea's airlock draws in every creature down in the sea near it, and the three jump pads in "
     "the seas throw creatures in an arc onto the shore instead of into the sky; creatures fall to far floors "
     "instead of jumping there in one step (at the desert lift's shaft and the treehouse platform's end); no "
     "fruit bushes on the water's surface."),
    ("0.12.1", "44dd6505",
     "Fixes: creatures no longer fall down the cage lift's shaft stepping into the tunnel at its middle stop; "
     "wide creatures by the pipe machine stay in the room map; a creature stuck on the seabed tries the jump pad "
     "beside it instead of pacing (the way out of water ignores floors it cannot step up to); hatchlings stand "
     "where their egg lay instead of in the floor; the far-east lift balcony's railing keeps creatures in; a "
     "developer aid prints creatures' fuel chemicals."),
]

MONTHS = ["January", "February", "March", "April", "May", "June", "July",
          "August", "September", "October", "November", "December"]


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True, text=True).stdout


def commits():
    """Oldest first: (full hash, date, subject, body)."""
    out = git("log", "--reverse", "--date=short", "--format=%H%x1f%ad%x1f%B%x1e", f"{FORK_POINT}..HEAD")
    result = []
    for rec in out.split("\x1e"):
        rec = rec.strip("\n")
        if not rec:
            continue
        h, date, msg = rec.split("\x1f", 2)
        files = [f for f in git("show", "--format=", "--name-only", h).split("\n") if f]
        if files and all(f == OUT for f in files):
            continue
        lines = [l for l in msg.strip("\n").split("\n") if not l.startswith("Co-Authored-By:")]
        subject = lines[0].strip() if lines else "(no message)"
        body = "\n".join(lines[1:]).strip("\n")
        result.append((h, date, subject, body))
    return result


def escape_inline(s):
    # keep the text as written, without markdown or HTML reading it as
    # markup; `code spans` are left as they are
    parts = re.split(r"(`[^`]+`)", s)
    return "".join(p if p.startswith("`") and p.endswith("`") and len(p) > 1 else escape_plain(p) for p in parts)


def escape_plain(s):
    s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    s = re.sub(r"(?<!\\)\*", r"\\*", s)
    s = re.sub(r"(?<![A-Za-z0-9])_", r"\\_", s)
    s = s.replace("[", "\\[").replace("]", "\\]")
    return s


def escape_body(body):
    out = []
    for line in body.split("\n"):
        if line.startswith("    ") or line.startswith("\t"):
            out.append(line)  # an indented code example: shown as code
            continue
        bullet = ""
        m = re.match(r"^(\s*)([-*] )", line)
        if m:
            bullet, line = m.group(1) + "- ", line[m.end():]
        e = escape_inline(line)
        if not bullet:
            e = re.sub(r"^(\s*)(#|\+ |\d+\. |=+$|-+$)", lambda m: m.group(1) + "\\" + m.group(2), e)
        out.append(bullet + e)
    return "\n".join(out)


def nice_date(d):
    y, m, dd = d.split("-")
    return f"{int(dd)} {MONTHS[int(m) - 1]} {y}"


def build():
    cs = commits()
    index = {h: i for i, (h, *_rest) in enumerate(cs)}
    groups = []  # (version, summary, [commits]) oldest first
    start = 0
    for version, last, summary in VERSIONS:
        full = git("rev-parse", last).strip()
        if full not in index:
            sys.exit(f"changelog.py: {version}'s last commit {last} is not in {FORK_POINT}..HEAD")
        end = index[full] + 1
        groups.append((version, summary, cs[start:end]))
        start = end
    if start < len(cs):
        groups.append(("Unreleased", "Changes since the last version.", cs[start:]))

    total = len(cs)
    w = []
    w.append("# Zeetures changelog\n")
    w.append(
        f"Every change Zeetures has made to [openc2e](https://github.com/openc2e/openc2e) since it was forked "
        f"from openc2e's commit [{FORK_POINT}](https://github.com/openc2e/openc2e/commit/{FORK_POINT}) "
        f"(7 October 2026): {total} commits, newest first, each with its full description. "
        "It is generated from the commit history by `.github/changelog.py`, so it lists everything, "
        "down to test aids and build fixes.\n")
    w.append(
        "Not listed here: the world additions (the scripts and art recipes run from the player's Additions "
        "folder, such as the walkways, airlocks, lifts and seasonal bushes), which are kept outside this "
        "repository for now. Commits that changed the engine for them are listed.\n")
    w.append("## Versions\n")
    for version, summary, group in reversed(groups):
        if not group:
            continue
        anchor = "unreleased" if version == "Unreleased" else "version-" + version.replace(".", "")
        days = f"{nice_date(group[0][1])}" if group[0][1] == group[-1][1] else \
            f"{nice_date(group[0][1])} to {nice_date(group[-1][1])}"
        w.append(f"- [{version}](#{anchor}): {len(group)} commits, {days}")
    w.append("")
    for version, summary, group in reversed(groups):
        if not group:
            continue
        title = "Unreleased" if version == "Unreleased" else f"Version {version}"
        w.append(f"## {title}\n")
        w.append(summary + "\n")
        days = OrderedDict()
        for c in reversed(group):
            days.setdefault(c[1], []).append(c)
        for day, items in days.items():
            w.append(f"### {nice_date(day)}\n")
            for h, _date, subject, body in items:
                w.append(f"#### {escape_inline(subject)}\n")
                w.append(f"[`{h[:8]}`]({REPO_URL}/commit/{h})\n")
                if body:
                    w.append(escape_body(body) + "\n")
    return "\n".join(w).rstrip("\n") + "\n"


def main():
    top = git("rev-parse", "--show-toplevel").strip()
    import os
    os.chdir(top)
    text = build()
    if "--write" in sys.argv:
        open(OUT, "w", newline="\n").write(text)
    elif "--check" in sys.argv:
        try:
            have = open(OUT).read()
        except FileNotFoundError:
            have = ""
        if have != text:
            print(f"{OUT} is out of date: run  python3 .github/changelog.py --write  and commit it.")
            sys.exit(1)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
