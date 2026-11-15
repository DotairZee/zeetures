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
