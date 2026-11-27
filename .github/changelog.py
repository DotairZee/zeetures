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
    ("0.18.2", "3ee6c7a9",
     "A new program icon, drawn from scratch: an egg with a heartbeat, a pale pink core in a mauve shell "
     "with a plum outline. It is the window icon and the icon of the Windows, macOS and Linux packages."),
    ("0.18.3", "5ad16c0e",
     "Each creature ages at its own pace, 0.9 to 1.1 times its genome's lifespan, drawn at hatching: creatures "
     "of one genome no longer all die at the same age, and a hatching boom no longer becomes a wave of old-age deaths."),
    ("0.18.4", "c7ce359a",
     "Creatures no longer fall through the upper-east deck over the lift's shaft, or out of the lift's cage at its "
     "top stop: a floor line running along a room's bottom now holds creatures and objects too big for it."),
    ("0.19.0", "4e950b64",
     "Old Creatures 2 COBs work as they did in the original game: speech commands (SAY$, SAY#), scenery objects, "
     "scripts whose blocks do not nest, objects compared with numbers, and more; 115 of 116 objects from a large "
     "archive of fan COBs now work. A COB that cannot be added says why instead of closing the game. Custom worlds "
     "(not made from Eden) no longer get the additions written for Eden's map."),
    ("0.19.1", "565e665d",
     "Creatures explore more, and more wisely: a place stays interesting until visited a few times and becomes "
     "interesting again after a long while away, and the young explore most."),
    ("0.20.0", "3fbb1b63",
     "Genes shape the mind: part of each creature's temperament comes from its own genes' chemistry (bred and "
     "custom lines can be bolder, calmer or quicker to anger), and eight mind settings are inherited: how fast it "
     "learns, how decisive and patient it is, how much it wanders, and how readily it learns about danger, food, "
     "water and company. Shown in the Owner's Kit; genes.csv tracks the population's averages over generations."),
    ("0.20.1", "0eb8ea82",
     "A fruit bush grows in the west airlock's corridor, among the hungriest places with no food."),
    ("0.20.2", "b213d294",
     "The far west workshop's fruit bush is no longer hidden behind the dark rock: it stands in front, pot "
     "and all."),
    ("0.20.3", "fdec90f4",
     "The submarine no longer drifts off through the world after a game is loaded: saved mid-trip, it now waits "
     "at its last dock. The far west workshop's bush moved a little right, and the bush by the big gold pot under "
     "the domes stands in a planter."),
    ("0.20.4", "2bc272b1",
     "An object that came to rest half inside the rock is set back where it fits."),
    ("0.21.0", "db545f0c",
     "Creatures learn by watching each other, from their own family most and when young most: what things are "
     "good for, where food is, and which places and drops hurt. They name the things they use, often for a young "
     "one of their family, and family pick up each other's words faster."),
    ("0.21.1", "3a233b58",
     "The submarine no longer falls off the screen when a game is loaded. Creatures can no longer push it while it "
     "travels: that restarted its trip part-way, and over many trips it lost track of its own settings."),
    ("0.21.2", "8b899577",
     "A creature that starves is recorded as starved, not as \"organ failure\", even when it fainted before its "
     "hunger had risen."),
    ("0.22.0", "0e4cbfc1",
     "Bacteria and disease, as in the original game: germs live in dark caves and rotting food and are carried by "
     "grendels; creatures catch them, sneeze, cough and run a fever, spread them by sneezing, and get better as "
     "their antibodies build up. Others look after the ill as they do the young. The Health Kit shows a creature's "
     "illness, and the Ecology Kit's map shows where the germs are. Each kit can now turn its own notices on the "
     "status bar on or off."),
    ("0.23.0", "aa0130fc",
     "Air pressure moves between rooms and makes the wind, as in the original game: storms raise it, the weather "
     "machines push it up or down, and the wind blows from high pressure to low, so the clouds drift with the "
     "weather instead of always meeting the same wind. The Ecology Kit's pressure and wind maps show it change."),
    ("0.24.0", "1cd1e5b2",
     "Radiation spreads out from the volcano and the sky as in the original game, following doors as they open "
     "and close. The fruit of the volcano's ember-red bushes now carries EDTA, the only cure for the heavy metals "
     "that radiation builds up in a creature."),
    ("0.25.0", "2da71d25",
     "Heavy metals now matter: a creature carrying them feels a dull pain that grows with the amount, and they slowly "
     "wear down one of its organs until, left for days, they can kill it; the volcano's fruit cures them. "
     "Radiation also lowers the chance of conceiving, and a baby exposed to it in its mother or its egg hatches "
     "with extra mutations, which its own children inherit."),
    ("0.25.1", "6b5845be",
     "Creatures in lasting pain now go back to places where it eased, and the volcano's fruit soothes the pain of "
     "heavy metals as soon as it is eaten, so poisoned creatures find their cure: in a twelve-day test their heavy "
     "metals fell almost twice as far, with less pain."),
    ("0.26.0", "0e6273fe",
     "Creatures poisoned by heavy metals are now cared for like the sick and the young, and creatures learn the "
     "volcano's fruit as a medicine of its own: those who know it eases pain bring it to sufferers and go looking "
     "for it when they hurt. The Health Kit's Doctor shelf has the volcanic fruit, for heavy metals from radiation "
     "poisoning. Swarm bees stuck against walls now join their hive."),
    ("0.26.1", "1e0c2adb",
     "The Breeder's Kit's Breeds page shows each breed as a whole creature facing you, male and female, instead of "
     "just their faces, and its list of breeds grows to match."),
    ("0.26.2", "e98cf280",
     "The program icon's egg is now tipped as if rolling, its heartbeat still level, to match the new Zeetures logo."),
    ("0.27.0", "a204085e",
     "Lighter and safer: the creature mind no longer writes a running commentary to the log (about 70 MB an hour "
     "in a world of a hundred creatures; now a few kilobytes), and each sound is decoded once instead of at every "
     "play. (An earlier version of this note also claimed about 100 MB less memory; measured again, the difference "
     "was the moment it was measured, not the change.) A save the game cannot read is kept aside rather than "
     "overwritten. The downloads carry only the game: no developer tools, and 2.7 MB of unused pictures gone."),
    ("0.28.0", "9a94d3fc",
     "Faster, and rotten fruit. A world loads in seconds (a crowded one took over a minute at every start in a "
     "player's folder), play takes about a third less of the processor, and players' downloads leave out the "
     "developer switches; Linux's are fully optimized. Our bushes' fruit now rots where it lies, feeding the soil "
     "and the germs of rotting food; eating it hurts and may bring illness, and creatures learn it apart from good "
     "fruit. A saved game that cannot be read is now shown on screen, with a way to put it back. Windows builds "
     "again (they had failed since 0.19.0)."),
    ("0.28.1", "2f6ff0cc",
     "The Breeder's Kit's Breeds page is as wide as its pictures, and the Mind Kit's Places page no longer grows as "
     "wide as the screen."),
    ("0.28.2", "030e599f",
     "File > Exit saved an emptied world: quitting now saves it whole, an empty world is never saved over a good "
     "one, and a world already stripped gets its objects back. File > Exit also works while paused. Players' builds "
     "open no listening port, so no firewall question at the first start."),
    ("0.28.3", "31d17d7e",
     "The first players' reports fixed: display scaling on Windows, speaking after naming a world, creatures that "
     "grew up unable to breed, sleep broken off within seconds, a crash at some births, and clearer wording of what "
     "a creature is doing."),
    ("0.28.4", "5e88e479",
     "Spring and summer fruit that feeds: hatchlings no longer starve on a full stomach in a new world."),
    ("0.28.5", "7dada256",
     "Z's over creatures for as long as they sleep, and grendel eggs that hatch without the Boney Grendels pack."),
    ("0.29.0", "c292a7f3",
     "Creatures move smoothly from pose to pose, with pictures made between the game's own by turning each part at "
     "its joint; Camera > Smooth Movement turns it off."),
    ("0.29.1", "07727e4b",
     "The hidden passage west of the incubator's pond can be walked both ways, and a world whose breed files are "
     "missing no longer crashes: its save is kept safe."),
    ("0.29.2", "1a1f7ddd",
     "Smooth walking without pauses or sudden turns, a world opened at every launch if you like, and creatures put "
     "down on the volcano's catwalk."),
    ("0.30.0", "6a7941e0",
     "Creatures that answer when spoken to: they look at you, try your words, look at what you name, and weigh the "
     "verbs they know; and Ctrl+S says your last words again."),
    ("0.31.0", "7b2eab6f",
     "Names of your own: teach creatures what to call your hand and things, and they use them and teach one another; "
     "and following goes with the creature selected."),
    ("0.32.0", "6f395b70",
     "Help in depth: 43 topics with a search and links, one for every kit, opened with F1."),
    ("0.32.1", "4ecc0052",
     "Favourite places saved; Save World As copies photos and genes; the Syringe brings back the newly dead; "
     "the arrow keys and dragging stop following."),
    ("0.33.0", "0504ceaf",
     "Zeetures updates itself from inside the game, with switches for checking and for installing automatically; "
     "Backspace no longer leaves a block in typed words."),
    ("0.33.1", "fb8699f5",
     "Creatures that drop off where they stand sleep properly, instead of dozing on and on."),
    ("0.33.2", "726e7673",
     "Creatures eat before bed, and hunger no longer cuts a sleep short; the horizontal scroll bar goes round with the world."),
    ("0.33.3", "52b45fdc",
     "The incubator's waterfall calms instead of lulling creatures to sleep; eyes shut while a creature is out cold; a larger icon."),
    ("0.33.4", "34984e9e",
     "Raindrops stuck against rocks are cleared away."),
    ("0.33.5", "69811014",
     "The laptop opens and shuts with a click, and can be picked up whether open or shut."),
    ("0.34.0", "ee3cf3ed",
     "View > Creature Frame Rate: choose how smoothly creatures move, from the original 5 per second to 60."),
    ("0.34.1", "8c12ec45",
     "Room heat is saved, so the boiler room no longer starts each session hotter than the volcano; the incubator's cave is lit, so newborns there get less sleepy."),
    ("0.35.0", "7e0b3908",
     "Preferences gathers every setting in one window; the words creatures learn can be changed per world or for all, with the game's six languages, Nornish and 19 draft translations; speech bubbles and the computers use Fusion Pixel, with Greek, Cyrillic, Japanese, Chinese, Korean, Hebrew, Arabic and Thai; the Creature's View can be resized and its blinking turned off; the status bar's parts can each be hidden; a landing in the lab lift's shaft."),
    ("0.36.0", "ecb1bd2a",
     "The music follows the creatures in view: their contentment and the danger they face shape it, it falls silent with none in view, and births, deaths and creatures close to death have their own pieces; CAOS MUSC."),
    ("0.36.1", "98253461",
     "The grendel egg pile hatches one grendel at a time: eggs that waited while the world was full no longer all hatch together."),
    ("0.36.2", "20c074ae",
     "Instincts and brain genes are read for any C2 brain (Nova, Canny, Akamai and their hybrids): an instinct keeps the conditions it can read, lobes are found by what reads them, and hybrids missing added lobes keep their object instincts."),
    ("0.36.3", "9e00d30d",
     "C2 genomes are read whole, whatever their genes: a damaged or imported gene no longer refuses the genome; chemistry, stimulus, gait and pose genes are read within the game's ranges. (0.36.2 had no installers; its change is in this one.)"),
    ("0.36.4", "b02c3a2e",
     "Damaged pose genes are read as the game reads them: a stray character becomes the pose the game would show."),
    ("0.37.0", "66a5014d",
     "Old objects' CAOS works: objects show speech and thought bubbles (BBLE: Holo Doc, Zora Castle, the Borg Doozer); eggs made by scripts get an \"Egg laid\" notice and objects can clear theirs (EVNT, RMEV); rooms' links can be read and dropped (RMNO, RMN#, RMNR, RMND, DELN: Volcano Garden, Plato Project); DRV! gives a creature's strongest drive, OBSV the door value ahead, and TELE lets a vehicle's riders out elsewhere."),
    ("0.37.1", "fb44a41d",
     "The airlock stays open to the sea, flooded and shut to land while the submarine comes to it, so creatures in the chamber no longer drain it in the submarine's way; it works as before once the submarine has docked."),
    ("0.38.0", "b2fc3a84",
     "Kits in their own windows (experimental; File > Preferences > Display): a kit, the Creature's View or Preferences dragged out of the game becomes a window of its own, on another monitor if you like, and comes back when dragged in; a bamboo bridge from the flying fox's landing up to the ledge before the stone arches."),
    ("0.39.0", "3e2e6f14",
     "Words from the C2 manual: typing \"yes\" or \"no\" praises or blames a creature that knows the word, \"what\" and \"why\" have it say what it is doing and the need behind it, and \"look\" turns it to the thing by the hand and has it name it; creatures say how strongly they need something, from \"very\" to \"extremely\"; the Creature's View can show the creature itself, its bubbles and (by default) its name in the window's title."),
    ("0.40.0", "a100cfb6",
     "Learning to talk as children do (experimental, Preferences > Creatures > Language): words mean what was in view when heard, phrases grow with the words known, speech aimed at a creature teaches most, words fade unless heard again after a pause or slept on, creatures make up names for things and the colony settles on one, and the young stretch words, babble and run words together; explained in Help > Learning to talk (experimental); the status bar keeps one height whatever parts it shows."),
    ("0.40.1", "631c3cad",
     "Crowded creatures walk out of a crowd toward the emptier side, so the crowd that gathered under the incubator, fed by the endless cave fungus, thins out; rooms Zeetures adds start with no wind instead of garbage values."),
    ("0.40.2", "b49c71ec",
     "For freezes on Windows: when the game stops responding for 8 seconds its log now says what it was doing and where (as on Linux), and the last session's log is kept as zeetures-log-previous.txt so it survives closing the game; settings are no longer read from disk every frame; a walk through the rooms that could loop for ever now gives up; the log says how long each save took."),
    ("0.40.3", "95b035b7",
     "Creatures stop gathering in crowds: they expect to feel crowded from walking up to, or staying in, a group of four or more, and no longer walk out of one crowd into the next; at the population limit a willing pair mates without an egg instead of not at all, and after any mating desire rests for about ten minutes, so would-be mates no longer cluster with their desire at its height. In copies of a full world they explored more, felt crowded and went hungry less."),
    ("0.40.4", "8c77ac47",
     "Toys and other things lying buried in the ground under the dome's garden, or in the pit beside the incubator, are set back where creatures can reach them; the tree house's bamboo lift is drawn in front of the tube's glass at its walkway stop; creatures can step between the arch-ledge bridge and the ledge."),
    ("0.41.0", "ebd8932a",
     "File > Export Current Creature saves a creature whole (body, chemistry, mind, words, friends and foes, temperament, life story, photos) to a Zeetures export file and takes it out of the world; File > Import Creature brings it back as it was, here or in another world; the cave fungus is no longer an endless meal (only a whole one feeds, and a bite leaves it bitten for a minute); foods smell of their own eating script only."),
    ("0.42.0", "bc91ce9c",
     "Three new choices in Preferences > Creatures: babies say their first words (a baby that has learned the word for what it wants says it); words come out a little wrong while being learned (\"han-nah-do\", then \"wetht\" or \"hanb\", then right); and creatures' heads follow the hand, angling up and down, as in Creatures 2. The first two are off unless ticked."),
    ("0.43.0", "44aac64a",
     "Lightning strikes creatures: a jolt of pain, as Creatures 2 meant but never managed; with Lightning has strange effects ticked (Preferences > World), a strike may now and then change a creature, give it a gift, or more, for you to discover; the bamboo lift east of the big hut is drawn in front of the tube's glass; every world has a fruit-bearing kelp clump on the sea floor."),
    ("0.43.1", "2c9f888c",
     "Two choices now on from the start, unless you untick them: words come out a little wrong while being learned (Preferences > Creatures), and lightning has strange effects (Preferences > World)."),
    ("0.43.2", "9ce48cb0",
     "The brain is now an organ that needs feeding: it takes a little of a creature's energy, never so much that it starves one, and a creature short of food grows dazed (slower to learn, less sure, slow on its feet) until fed; lamp-lit places, the incubator's cave among them, keep their light at night instead of going dark under the night sky."),
    ("0.44.0", "d4d2998a",
     "Each creature's brain now grows, holds up, and ages: it grows most while young, from conversation (talk with it, and let it answer), care, and sleep; a well-raised brain learns faster all its life. With age the signs show unless the brain kept busy (new words, places, friends) and the creature kept moving: slower walking, less sure choices, forgotten places, then words. Stress that soon passes makes the young hardier; stress that goes on does harm. See Help, How the brain grows."),
    ("0.44.1", "5b3d2f81",
     "Lamp-lit places across Albia get the light their pictures show, and keep it at night: lamp caves, the lab, the garden domes, the glowing lava, and the cave over the walkway east of the incubator's lift, among 40 rooms."),
    ("0.44.2", "aef1bccc",
     "Fixes: a save that fails part way is reported instead of ending the game; creatures no longer get stuck past the world's west edge; speech too long for the widest bubble is shortened instead of not shown; in Vietnamese (and any language whose words share a first syllable), saying a longer word is no longer also heard as the shorter one, so \"buồn ngủ\" (sleepy) is not also \"buồn\" (sad)."),    ("0.45.0", "29a18a90",
     "Grendels and ettins each have a role: bulbous grendels prowl by night and take eggs to their lair, Boney grendels roam in packs, scavenge and steal food, and ettins hoard gadgets, carry eggs to the incubator and food to the hungry; norns that fall ill near a grendel learn to keep away. Grendels and ettins breed while a mate of their kind lives, Preferences sets how many the game hatches, and they now come before norns' eggs at the population limit. C1 grendels arrive: RudeDog's C1-in-C2 Grendel, fetched as a third breed pack (its female's body data mended), on a genome made from your own game's grendel: gluttons and thieves, deaf to the hand, fearless loners that keep ettins' company, clean of germs, and mostly barren, now and then laying a lone egg. Things fall, bounce and sink as in Creatures 2; illness can harm organs (an option); worlds played in Creatures 2 open with their creatures, words and families; a census of hatchings, eggs and deaths; children talk themselves through what they do; things are no longer carried below the feet."),
    ("0.45.1", "13dfed06",
     "Grendels and ettins now shrink a little in old age, as norns do: senile ones are about seven-eighths of their grown size."),
    ("0.45.2", "e6f437f6",
     "The C1 grendels have a name: grouchy grendels."),
    ("0.46.0", "7b9358fa",
     "Every world keeps its own preferences, layout included: a new world starts with the defaults, and worlds from before keep the settings they had. A new tick box in Preferences, World, \"Grendel and ettin personalities\", on unless you untick it: unticked, grendels and ettins are as the games made them, and C1 grendels hatch with Creatures 1's own grendel genome (from your Creatures 1, when it is installed beside Creatures 2) in a Creatures 2 body."),
    ("0.46.1", "cafd4863",
     "Help has a topic of its own for the grendel and ettin personalities: what each kind does, how they breed, and what changes when they are switched off."),
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
