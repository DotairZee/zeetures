#!/usr/bin/env python3
"""Write CHANGELOG.md: every change Zeetures has made to openc2e, from the
commit history (each commit's full message), grouped by version and day,
newest first.

    python3 .github/changelog.py           # print it
    python3 .github/changelog.py --write   # write CHANGELOG.md
    python3 .github/changelog.py --check   # exit 1 if CHANGELOG.md is stale

Run from anywhere in the repository. Commits that change only CHANGELOG.md
and this script are left out, so the commit that updates the file (with a
version's entry here) does not make it stale.

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
    ("0.13.0", "ee4c076a",
     "Lifts, boats, the submarine and call buttons are saved with the world, with their riders still aboard; "
     "five more fruit plants in compact and slender bush forms, potted away from the earth, and the upper-east "
     "deck's bush moved to the cave under the east dock; brain relay cells pass on their whole state, so every "
     "creature keeps a full energy reserve; the metabolism restart is off unless asked for; the sea jump pads "
     "land creatures by their size; pocket rescues fixed: the incubator's glass terrarium is a room behind a "
     "door, creatures at the gene lab's west wall step back onto its floor, and a rescued creature goes back "
     "to the side it came from; developer aids for the energy chain and for cutting a creature's air."),
    ("0.14.0", "2947236d",
     "Creatures look after their friends: food a creature carries over and drops counts as its gift, a well-fed "
     "creature brings food to a hungry friend, and creatures may go after one that just hurt a friend; foragers "
     "no longer row straight back to a shore they just searched; railings close the treehouse platform's and the "
     "scaffold's open ends; creatures no longer step off into the cage lift's shaft when a nearer floor is beside them; a fall that starts just under a floor line begins on it; lifts rest half as long "
     "between rides; no fruit bushes in lift shafts."),
    ("0.14.1", "58cd1380",
     "Fixes: the gene splicer works: two creatures in its pods give an egg crossed from both (one gives a "
     "clone), and the Owner's Kit says the baby was spliced from them; eggs being hatched can no longer be "
     "cut off by a creature pushing, pulling or hitting them, which left grendel, ettin and norn eggs that "
     "never hatched, and eggs stuck that way start again at load."),
    ("0.15.0", "cb77e0c8",
     "The Life Kit's Metamorph Device stands in every world, in the gene splicer room: a creature put in its "
     "cabin is replaced by a newly hatched copy of the other sex, and a switch on the device (a lightning bolt "
     "or a skull) chooses whether the creature put in is ended at once or poisoned, kept with the world; only "
     "the hand runs it. Fix: a click on hotspots that objects' scripts set up ran the wrong action."),
    ("0.16.0", "a1c18476",
     "The glass tube between the domed lab and the gene splicer room is walked: its car is gone, the tube "
     "slopes down into the gene lab, which is widened west behind a new rock face, with stalactites under the "
     "cave arch and the teleporter moved beside the tube's end; the cage lift by the huts no longer stops at "
     "the old deck, which now holds its pulley on bamboo props. The tube's pictures are made from the "
     "player's own game files, in the engine, the same on every machine."),
    ("0.16.1", "630f3c58",
     "Fix: the cage lift by the huts goes down to the lower deck again, with its caller there, in saved "
     "worlds too."),
    ("0.16.2", "7e8c9bf8",
     "Fixes: creatures pressed against the gene lab's east wall no longer bob up and down in the rock; the "
     "gene lab's console stands behind creatures walking past."),
    ("0.16.3", "784a4df6",
     "Fix: the domed lab's glass lift is behind creatures walking past it, and still in front of the "
     "creatures riding in it."),
    ("0.16.4", "3a71370b",
     "Fix: the glass tube's checkered glass runs on around the pulley by the huts, so creatures inside "
     "show through it there too."),
    ("0.16.5", "88f0214c",
     "The Metamorph Device stands 20 px further west in the gene splicer room, its switch with it."),
    ("0.16.6", "09400119",
     "Fix: creatures no longer walk in mid-air across the domed lab's left side and through its glass "
     "lift's shaft; a wall stands at the shaft's left side."),
    ("0.16.7", "cc218617",
     "The domed lab's power is switched on and off by the button on the dock's wall (where the pod used to be "
     "called), no longer by the generator on the desk; the gene splicer knows its power after a load, and its "
     "\"no power\" window no longer opens."),
    ("0.16.8", "585ae575",
     "The submarine has a caller at each of its three stops (the sea cave's dock, the airlock under the seabed "
     "and the east dock): a press brings it from wherever it is docked; a submarine saved mid-trip gets its "
     "buttons back at load."),
    ("0.16.9", "67a78941",
     "Fix: creatures at the east end of the sandy tunnel under the west sea now pass behind the tunnel's "
     "bronze collar, not in front of it."),
    ("0.16.10", "5f417596",
     "Fix: creatures share food with friends who are really hungry; a friend's hunger was read from a "
     "chemical that never changes in Creatures 2."),
    ("0.16.11", "7589bb9f",
     "A second skinny fruit bush in the gene splicing room, by the teleporter where the glass tube comes in. "
     "Fix: riders stay aboard the submarine (and other vehicles) for the whole trip; they were let out and "
     "taken back many times a second."),
    ("0.16.12", "2ccfcbd5",
     "The submarine is drawn in front of creatures and its callers behind them, beneath the submarine; the "
     "east dock's caller moves out from behind the rock onto the berth's back wall. The gene splicing room's "
     "west half has its floor on the drawn floor slab, 10 pixels higher."),
    ("0.16.13", "9772325e",
     "Fix: the egg machine under the gene lab lays one egg at a time; while the world was at its population "
     "limit it kept laying ettin eggs that could not hatch, and the pile is cleared at load."),
    ("0.16.14", "b71b325f",
     "The domed lab's power button is green while the power is on and red while it is off."),
    ("0.17.0", "59623f5f",
     "The Breeder's Kit has an Insemination tab for female creatures: choose any creature of her kind, male "
     "or female, living, departed or a genome file, see the two side by side with their temperaments and "
     "kinship, and she conceives from its genes. Creatures' genes are kept when they die so they can be "
     "chosen later."),
    ("0.17.1", "e4a93669",
     "Objects resting on open water sink (a toy, a tin and others lay on the east sea's surface). The "
     "Chronicles Kit's History shows a detail's full text on hovering, its Details column half as wide, and "
     "the kit fits each page, so Statistics is no wider than it needs. The changelog check no longer fails "
     "on every push."),
    ("0.17.2", "a572debe",
     "Fix: the Metamorph Device takes only a creature inside its cabin, no longer one standing beside it."),
    ("0.17.3", "de5f38a9",
     "Fix: the white line in the sky over the west sea is gone (a stray row of pixels in one frame of the "
     "game's own picture of the big tree, now made without it). The Insemination page keeps one width "
     "whichever donor or source is picked, and looks for genome files each time it is opened."),
    ("0.17.4", "ad766b2b",
     "The Owner's Kit's Relationships list shows seven rows and scrolls, so the kit is less tall."),
    ("0.17.5", "e26c1a31",
     "The Genetics folder is tidied when a world loads: babies' genomes that no world needs any more go "
     "to the system's Trash, so the folder no longer grows by hundreds of files a day."),
    ("0.18.0", "2d1670d4",
     "Install and play with no command line: a Windows installer, a Mac app in a disk image (Apple Silicon "
     "and Intel) and Linux AppImages. Zeetures finds Creatures 2 in the usual Steam and GOG folders, or asks "
     "for its folder once and remembers it (File > Creatures 2 Folder changes it). The nightly keeps a "
     "system's download from the night before when its build fails, so macOS no longer drops out."),
    ("0.18.1", "7cf503c5",
     "A starving creature with no food in sight and no known way out tries a lift: it rides one that is "
     "there, calls one, or walks to one it can see (the west airlock's corridor, whose only ways out are lifts)."),
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
        # (the changelog's own upkeep: the file, and a version's entry in
        # this script, committed together after the version bump; counting
        # that commit made the file it carries stale on every push)
        if files and all(f in (OUT, ".github/changelog.py") for f in files):
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
