# Zeetures changelog

Every change Zeetures has made to [openc2e](https://github.com/openc2e/openc2e) since it was forked from openc2e's commit [dfcd46f2](https://github.com/openc2e/openc2e/commit/dfcd46f2) (7 October 2026): 453 commits, newest first, each with its full description. It is generated from the commit history by `.github/changelog.py`, so it lists everything, down to test aids and build fixes.

Not listed here: the world additions (the scripts and art recipes run from the player's Additions folder, such as the walkways, airlocks, lifts and seasonal bushes), which are kept outside this repository for now. Commits that changed the engine for them are listed.

## Versions

- [0.12.0](#version-0120): 7 commits, 18 November 2026
- [0.11.0](#version-0110): 14 commits, 17 November 2026 to 18 November 2026
- [0.10.0](#version-0100): 7 commits, 16 November 2026 to 17 November 2026
- [0.9.0](#version-090): 14 commits, 16 November 2026
- [0.8.0](#version-080): 47 commits, 15 November 2026 to 16 November 2026
- [0.7.0](#version-070): 4 commits, 15 November 2026
- [0.6.1](#version-061): 14 commits, 15 November 2026
- [0.6.0](#version-060): 30 commits, 14 November 2026 to 15 November 2026
- [0.5.0](#version-050): 6 commits, 14 November 2026
- [0.4.0](#version-040): 66 commits, 12 November 2026 to 14 November 2026
- [0.3.0](#version-030): 38 commits, 12 November 2026
- [0.1.0](#version-010): 206 commits, 7 November 2026 to 12 November 2026

## Version 0.12.0

Ways out of the seas: the submarine picks up swimmers along its path and lets them out dry at its next dock, the west sea's airlock draws in every creature down in the sea near it, and the three jump pads in the seas throw creatures in an arc onto the shore instead of into the sky; creatures fall to far floors instead of jumping there in one step (at the desert lift's shaft and the treehouse platform's end); no fruit bushes on the water's surface.

### 18 November 2026

#### Version 0.12.0

[`46e2d6ae`](https://github.com/DotairZee/zeetures/commit/46e2d6aec7bb310cbdbea40eda26fff1a79d5467)

Help &gt; About and the README's Status now say 0.12.0 (owner, 2026-11-18,
after the sea jump pads). The README mentions the ways out of the seas
(the submarine, the west airlock, the sea jump pads), and both READMEs
now count 42 world additions; a fresh install (empty user folder) runs
all 42, with no script errors.

#### Sea jump pads throw creatures onto a shore

[`1fa574e6`](https://github.com/DotairZee/zeetures/commit/1fa574e650ac933d04f2cdee3fe43db5b376fd1e)

The three jump pads in the seas launched creatures with a velocity
openc2e moved in one tick: to the top of the world and straight back
into the sea. A patch to the pad's activate script now hands each
creature it launches (modes 1 and 2) to a carrier that throws it in an
arc onto the dry shore the pad points at: the east dock planks, the
west sea's east shore sand and its west beach. OV95 marks a creature in
the air; the airlock, shelf current, submarine and boat catcher leave
it alone.

#### Creatures fall to far floors instead of jumping there

[`a4177aea`](https://github.com/DotairZee/zeetures/commit/a4177aeaa3900d56edc7b8cf1757dc4fdec6acf5)

Found from BIG MOVE logs (the TODO's engine-moved creatures): resolving
each engine move's stack in a 19-minute copy of Home showed
SkeletalCreature::snapDownFoot setting creatures on a floor far from
their foot in one step, two ways:

- Up 243-301 px at the desert lift's shaft (room 12, x 5735-5870, a
  passable floor line near its top): a foot placed afresh (let out, or
  moved by a script) below the floor line skipped the walk-beneath rule,
  which needed a previous foot room, and was set on the line above. The
  rule now applies to a fresh foot too (OPENC2E_OLD_SHAFT_PLACE for the
  old way).
- Down 300-620 px where a tall room stays the foot's room while the foot
  is past its edge (the east end of the treehouse's upper platform, room
  51 ending at x 3917 with room 52 running down to the cave floor; the
  open lift shaft at x 4183-4300 under its upper stop): no guard turned
  this into a fall, as one does for a new room or a fresh foot. A drop
  of more than 20 px in the same room is now a fall
  (OPENC2E_OLD_SAME_ROOM_SNAP for the old way), so it is seen, hurts by
  height and is learned like any other drop.

Tests: a norn placed in the desert shaft now stays on its floor (875)
instead of rising to 580; one placed at the platform's end falls to the
cave floor over several ticks instead of in one step. Two matched pairs
of Home copies (5 and 8 min, new vs old): engine snaps 0/0 vs 1/3,
sealed-pocket rescues 1/3 vs 3/15, hungry 12/10 vs 10/10, dead 0/2 vs
1/1, meals 2445/3278 vs 2349/3608 (mixed, within copy noise); falls of
200 px or more 240/139 vs 150/132, the snaps that are falls now.

The other engine moves seen were the sea launchers (2 13 1), left for
the owner to choose (TODO).

#### Submarine scoops up swimmers along its path

[`4e5890be`](https://github.com/DotairZee/zeetures/commit/4e5890be4f1638813ca12ae2a96d94cdb079245f)

The owner (2026-11-17) asked that while the submarine is moving along its
path it scoop up any creature under water near it and deliver it safely
out of the water. A new addition, sub-rescue.cos: a watcher (2 1 162,
every 2 ticks) finds the submarine (3 1 2) while it has any speed, and
draws every live creature wholly under the waterline (top below 710, so
not one in a boat or on a dock) in a water room, with its middle within
200 px of the submarine's box, toward the submarine at 16-24 px a step;
once its middle is inside the box, GPAS takes it aboard and the engine
puts it in the cabin. It rides to the next dock, where the submarine
lets everyone out onto dry floor as before. Never across the seabed:
east of the shaft (x 6391) the sand at y 1175 parts the open sea from
the tunnel the submarine runs along to its dock below.

Route traced: east dock (7150, 820) down to 935, west to 6065, up to the
west dock (790); then down the shaft and east through the tunnel (1255)
to the airlock dock at 6465; and back. Tests (Home copies): norns on the
seabed and mid-water were taken in within about a second and let out dry
at the west dock (twice); one 300 px down the shaft, and one in the
tunnel under the sand while the submarine passed above, were left alone.
Pictures: submarine-rescue.png in the project folder.

#### West sea airlock: draws in everyone down in the sea

[`422627db`](https://github.com/DotairZee/zeetures/commit/422627db4820ba5e217b7c79a2d5bdcdce00b9b6)

The owner (2026-11-17) asked that when the west sea's seabed airlock is
triggered it pull all creatures near it under the sea through with it,
not only the one that set it off. When the sea door opens, every live
creature in the west sea's water rooms (x 164-975) below y 1000 (clear
of the band under the surface where swimmers climb out), not in the pool
east of the rock and not already being carried by the shelf current or
just let out, is marked (OV91) and moved to the pickup point at 16-24 px
a tick, across at its own height and then down (the seabed falls toward
the hatch, so the path stays in open water). The door waits for the last
of them (up to about 9 s) before the car takes them all in together.

Test (Home copy, four norns placed in the sea, one at the hatch): the
three away from the shelf went through together into the pool and
walked out east; the one on the shelf was carried down by the shelf
current and went through on the next cycle. Pictures:
west-airlock-pull.png in the project folder.

#### Fruit bushes: none on the water's surface

[`a1028535`](https://github.com/DotairZee/zeetures/commit/a1028535c8d919a7a093815f30c8e9bf39906bf6)

The owner (2026-11-17) found a seasonal fruit bush standing on the sea
by the boats. A fruit that rots where a sea room's top is near its base
(from 45 px above to 12 below; the sea's rooms start about 30 px above
the waterline drawn) no longer sprouts: floating at the surface, or on
a boat that later sails off, which is the likeliest way the first one
got there (fruit dropped into the sea sinks to the floor and becomes
kelp, as before). At load, any bush that is not potted standing at the
surface is removed; the potted bush on the dock planks stays.

Test (fruit held at the surface, 24 per copy, two copies each): bushes
on the surface afterwards 0 and 0 with the rule, 2 and 2 without (the
old bush and a kelp at the surface).

#### Changelog

[`d5c33dda`](https://github.com/DotairZee/zeetures/commit/d5c33ddaf0a71a9aeeac6f9b868efaf8e6232789)

## Version 0.11.0

Temperament that shifts with experience while young (hurt, comfort, fights, exploring), tapering as a creature grows up; the soil's nutrient cycle (breakdown, seeping downhill, the dead feeding it, bushes growing by it), shown in the Ecology Kit with food smell as a scent; outdoor light following day and night; walking the way the creature faces (a cause of drownings); brain profiles reading search and chemical-fed lobes from wiring; Import Genome takes exported creatures; Creature's View without the viewer's own bubbles, seen through the creature's own eye (pupil by light and fear, blinking and drooping lids, iris tinted by its pigment genes, sight doubled when hurt and rippling underwater), centered on what it looks at, the world showing through a checkered screen.

### 18 November 2026

#### Version 0.11.0

[`f5e5ae6f`](https://github.com/DotairZee/zeetures/commit/f5e5ae6f30a7c14bb63ba66d3d41f6f539ea3b88)

Help &gt; About and the README's Status now say 0.11.0 (owner, 2026-11-18,
after the Creature's View eyes). The README also mentions temperament
shifting with experience while young, the soil's nutrient cycle, outdoor
light following day and night, and Creature's View through the
creature's own eye.

#### Creature's View: an ettin's pupil is clear in the middle

[`e79994eb`](https://github.com/DotairZee/zeetures/commit/e79994ebaca002224ac9b5ed5fbee04b0a1101b5)

The owner (2026-11-18) wanted to still see clearly what an ettin is
focused on. Its dotted screen now covers only the outer fifth of the
pupil, thinning toward the middle, which is clear.

#### Creature's View: less eye, more checkered see-through

[`159722fc`](https://github.com/DotairZee/zeetures/commit/159722fc08ca1587fd9fa5335366e8ab739303bd)

The owner (2026-11-18) asked for more of C2's checkered transparency
and for the eye to take up less of the view. The opening now reaches
past the view's sides (lids only take the corners and the top and
bottom edges), the pupil is at least 72% of the iris, a grendel's slit
is wider, and the white, the inner iris and a 40 px band along the
lids' edge are a 50% checkerboard the world shows through. Shut lids
stay solid, so sleep is still dark.

#### Creature's View: seen through a living eye, centred on what it looks at

[`e90e46e4`](https://github.com/DotairZee/zeetures/commit/e90e46e4c4403f10e5726a229e1843ca935d258f)

Owner, 2026-11-18, after seeing real C2's Creature's View (its eye masks,
bubb.s16: a norn's round pupil in a blue iris, a grendel's slit, an
ettin's dotted sight; not used, our own art): the view through an eye,
centred on what the creature attends to, and the eye alive.

- The view glides to centre the object the creature attends to; the
  corner brackets are gone. With nothing in mind it shows where the
  creature is (itself left out).
- Over it, an eye drawn here each frame (no game art): an almond
  opening between lids, the white, an iris with fibres darkening to its
  rim, a glint, and the pupil the view shows through. Norns have a round
  pupil, grendels a vertical slit, ettins a round pupil seen through a
  coarse dotted screen, denser toward its rim. Iris colours: a norn's
  blue, a grendel's amber, an ettin's deep red, each tinted by the
  creature's own pigment genes (red, green, blue, 128 neutral); lids in
  each kind's skin tone.
- The pupil narrows in bright rooms (room light, which now follows day
  and night) and widens in the dark and with fear, easing between; a
  round pupil is never under half the iris, to see by.
- The lids blink every few seconds, droop as Tiredness or Sleepiness
  passes half, and close in sleep and death.
- Hurt or ill (Pain or Injury past 0.4): the view doubles and reddens,
  more the worse. With its head under water: the view ripples, tinted.

OPENC2E_OPEN_EYE_VIEW=2 or 3 opens it on a grendel or an ettin.

### 17 November 2026

#### Hatchery: Import Genome takes exported creatures (.exp)

[`79522a71`](https://github.com/DotairZee/zeetures/commit/79522a7188538ff4450f811730aca24039b6adaf)

Packs such as Canny Norns v1.2 and the Akamai Canny pack ship their
genomes inside exported creatures (.exp). The Breeder's Kit read those;
the Hatchery's Import Genome took only .gen and .egg. The export reader
moves to Breeds.h (GenomeFromExport: the genome after "CGenome", as a
"dna2" genome file) and Import Genome uses it, so an .exp makes an egg
of that creature's genome. The file chooser offers .gen, .egg and .exp.

#### Creature's View: not the viewer's own speech or thought bubbles

[`aeccba9b`](https://github.com/DotairZee/zeetures/commit/aeccba9b9dd13502f240c3103895bbbbaa7a0514)

Owner, 2026-11-17: in Creature's View, don't show the selected
creature's word or thought bubbles. The view already left the creature
itself out (it does not see itself); now World::drawWorld also leaves
out speech bubbles floating on it (Bubble, floatingagent), and
Thoughts::draw takes a creature to skip, the one whose eyes the view
looks through. Other creatures' bubbles still show, and the main view
is unchanged.

#### Daylight: outdoor rooms' light follows day and night

[`d2314507`](https://github.com/DotairZee/zeetures/commit/d2314507f8ba91059c597ef1b7058474f6c4aa47)

Every norn, grendel and ettin has a gene that raises Sleepiness in the
dark (an emitter on the light sense, inverted). Rooms kept the world
file's light day and night, so a cave always made a creature sleepy and
the open air never did. Owner's choices (2026-11-17, from mockups of the
Ecology Kit's Light map: light-mockup-which-rooms.png,
light-mockup-how-dark.png): outdoors as the sky, night at a quarter of
daylight.

World::tickDaylight, every 50 ticks: each room's light is its own (the
world file's, or what a script last set) times a daylight factor in
proportion to how far outdoors it is: full by day, 0.7 at dawn, 0.6 at
dusk, a quarter at night, fading over about 30 seconds as the sky tint
does. How far outdoors a room is comes from SkyTint's outdoors map,
traced from the painted sky (new SkyTint::outdoors: 0 caves and indoors
to 1 under the sky; -1 until the background has been drawn once). Caves
and indoor rooms keep their light, and so do rooms with a light source
(lamps). OPENC2E_NO_DAYLIGHT turns it off (A/B testing).

The Ecology Kit's Light choice says it changes with day and night
outdoors.

Three matched 9-minute copies of Home (each about one game day): total
room light fell from 28,283 by day to 14,637 at night and came back;
hungry 27.7 / 29.9 / 28.6 vs 24.4 / 30.1 / 28.9, starving 16.2 / 17.3 /
17.1 vs 14.3 / 16.6 / 17.6, sleep choices 6.1-6.2% vs 5.9-7.0%; no
starvation deaths.

#### Ecology Kit: food smell as a scent; nutrient texts brought up to date

[`5f9d4fab`](https://github.com/DotairZee/zeetures/commit/5f9d4fab6c4aa7c2a0b242ac36281be873fa3020)

The Ecology Kit's Map already showed each pheromones.txt scent and could
tint the world by one; food smell (World::tickFoodScent), the smell
hungry creatures forage by, was not among them. It is now the first
scent under Show (one past the last pheromone channel), on the map, in
a room's hover readings, and as the world tint (green), and
OPENC2E_ECO_SCENT="Food smell" picks it for snapshots.

With the nutrient cycle running, the organic and inorganic nutrient
choices say what moves them (rotting food and the dead, breaking down
faster where warm, seeping downhill; bushes growing faster on minerals
and drawing them up), and their legend says they change, no longer
"stays as the world began".

#### The nutrient cycle: breakdown, seeping downhill, the dead feeding the soil

[`cabefec1`](https://github.com/DotairZee/zeetures/commit/cabefec1deb3d1c8d14fcdf32a417c820842a4ba)

C2's rooms hold organic and inorganic nutrients (ONTR, INTR). The game's
own scripts add organic nutrient where things rot and let some plants
and fungi feed on it; some plants live only within a range of inorganic
nutrient. Nothing broke organic matter down, nothing moved, nothing was
saved: organic piled up slowly, inorganic stayed as the world file set
it, and every load reset both. Owner's choices (2026-11-17): a full
cycle, the dead feeding the soil, nutrients seeping downhill with water,
a modest effect on our fruit bushes.

World::tickNutrients, every 50 ticks:
- organic nutrient above a room's own level (the world file's) breaks
  down into inorganic, about half in one game season at the comfortable
  105, twice as fast for each 50 warmer (a quarter to four times);
- both seep through open doorways: down to a lower floor a little each
  step, level rooms even out slowly, water rooms mix;
- each room settles slowly back toward its own levels (surplus leaches
  away, a shortfall is made good), so the game's plant habitats hold.
Scripts' changes are taken up as they come; values stay 0-255. Saved
with the world ("nutrients ROOM organic inorganic base-organic
base-inorganic"); saves from before take the world file's levels as the
rooms' own.

A death leaves organic nutrient in the room (a baby 15, a child 25, an
adolescent 35, older 50).

The seasonal fruit bush (food.cos) earns 75 to 125 growth a step by its
room's inorganic nutrient (0 to 50 and up), a bud for each 100, so bare
soil grows a quarter slower and rich soil a quarter faster, and each
bud draws one unit up. Potted bushes on decks and ledges (OV05 = 1)
grow at the usual pace. Fallen fruit rotting under a bush enriches its
own soil: in a copy, inorganic nutrient under several bushes rose from 0
to 20-75 in six minutes.

OPENC2E_NO_NUTRIENT_CYCLE turns the engine part off (A/B testing).
Three matched 9-minute copies of Home, cycle and new bushes vs neither:
hungry 41.0 / 43.7 / 40.4 vs 42.5 / 48.2 / 43.5, starving 22.3 / 24.5 /
24.9 vs 24.7 / 28.7 / 24.8, meals 3,484 / 3,297 / 3,695 vs 3,712 /
4,038 / 3,357; no starvation or drowning deaths; no script errors.

#### Citations: Bleidorn 2022 listed once, noting it also informed temperament drift

[`29cac0fb`](https://github.com/DotairZee/zeetures/commit/29cac0fb8ac6d746078bdd837a925f9ebce86c36)

#### Temperament drifts with experience while young

[`b178ad5d`](https://github.com/DotairZee/zeetures/commit/b178ad5d049a04898e01018c00e2b9f57b4eea21)

A young creature's temperament now shifts with what happens to it
(owner's choices, 2026-11-17: young and tapering; up to half a step for
each kind of experience, about a full step for several together;
weighted by the evidence; children inherit the temperament life gave
their parents). Each kind keeps its own shift of each trait:
- hurt or frightened (pain, injury, punishment felt; a bad fall; nearly
  drowning): more reactive, less bold, less sociable;
- comforted or played with (reward, or the needs for pleasure, company
  and play eased, by the hand, another creature or a toy; not a meal):
  calmer, bolder, more sociable;
- in a fight (hurt by another creature; both get it, the one who hurt at
  half): quicker to anger, counting half, with 70% fading back over a
  few thousand mind ticks;
- something tried mostly for its novelty turning out well or badly
  (MindOutput::noveltyOutcome): more or less curious, counting least.
Stage scales it: 1 as a baby, 0.6 a child, 0.25 an adolescent, settled
from youth on. Each experience moves a trait less the further that kind
has already moved it (at most 0.5 a kind, 1 in all). The creature's
temperament is its inborn one plus the shifts; the mind takes it up on
its next tick, and a shift of another 0.1 in a trait is logged. Breeding
already passes on the parents' current temperament, so children inherit
what life made of them.

Saved with the world: "temperament-born" and "temperament-drift" beside
"temperament" (older saves: born is the temperament). The Owner's Kit's
Temperament page says what life has made a creature more of since it
hatched. OPENC2E_NO_TEMPERAMENT_DRIFT turns it off (A/B testing).

Paced in copies of Home: a first rate moved babies near the cap within
minutes (and counted meals as comfort), so a step is 0.003 and comfort
needs a carer; then in 9 minutes two youngsters had moved a trait 0.1,
the rest less. A test save held the born and drifted lines.

#### Walking goes the way the creature faces

[`a49699ee`](https://github.com/DotairZee/zeetures/commit/a49699eed6228769c13214348f627051a2c5900b)

A C2 creature asked to walk (WALK, the world's walk scripts, events
42/43) is faced first (POSE 59 east, 60 west), then takes the gait's
poses, which face "towards IT": that reads its direction, set only by
approaching something (or DIRN) and not saved. After a load it was
north, so every walk pose became the facing-out pose (9 for every part)
and the legs never strode: a creature walked in place until it next
approached something, and afterwards each walk went the way of whatever
it had last approached, not the way its mind chose. 153 of Home's 155
creatures walk this way (HomeostatMind's gaitWalk, taken up after one
plain step gets nowhere).

Found through 6KBY in Home's save: let out by the west sea airlock into
the pool under the west rock ledge, it walked in place there until it
starved and drowned, in every test copy (seven "drownings" counted
across the day's copies were this one creature). A copy made from its
genome walked; moved onto dry land, it froze again.

While walking (not approaching), its direction now follows the way it
faces (east or west). With the fix 6KBY walked out of the pool by tick
\71. OPENC2E_OLD_WALK_DIRECTION restores the old behavior (A/B testing).
Also: the gait's pose counter (gaiti) started unset, so a fresh or
loaded creature read its gait gene past its 8 poses once.

Three matched 9-minute copies of Home, fixed vs not: hungry 43.0 / 46.0
/ 48.8 vs 43.4 / 45.9 / 46.5, starving 24.7 / 25.7 / 27.6 vs 26.2 / 28.4
/ 26.2; walks ending blocked 5,028 / 4,673 / 4,036 vs 5,460 / 5,123 /
4,984; meals about 10% fewer (3,855 / 3,552 / 3,384 vs 4,248 / 3,840 /
3,847) with hunger the same; no starvation or drowning deaths.

#### Brain profiles: read search lobes and chemical-fed lobes from wiring

[`0933c8c7`](https://github.com/DotairZee/zeetures/commit/0933c8c74f613695f859f91dec1110b83f86d4b7)

Two rules so genomes like Canny Norns get their roles without an entry
in brain-profiles.txt (owner, 2026-11-17: the game should work these
out, not need a translation per genome):
- a lobe that reads the drive lobe and whose output reaches attention
  links needs to what to look for: search (Canny's search lobe, which
  the rules read as an attention learner);
- a lobe fed by chemicals, most of them made as needs change (products
  of reactions using a drive chemical or its increase or decrease
  chemical, 1-51, or of emitters on the drive levels): drive-change.
  Canny's echo lobe takes 17 such chemicals, each made from a drive and
  its decrease as the drive eases; it was read as having no counterpart.
The reasons now also say when a lobe's output goes into perception (its
gene's perception flag), the way Canny's echo lobe reaches the rest.

Checked with a profiles file that names no genomes: Nova Subterra, Canny
Norns v1.2 and the Akamai Canny pack all get from wiring alone the roles
their authors describe; in a copy of Home the creatures of the Kai,
Scorpio and other lines with added lobes get roles from theirs. The
known-genome entries stay for what wiring cannot show (Canny's
curiosity).

#### Changelog: version 0.10.0 in the version list

[`c964646f`](https://github.com/DotairZee/zeetures/commit/c964646f32b5cfe0bee4fbb63ec08b5f2bbd290d)

## Version 0.10.0

Temperament partly passed on from the parents. Old custom genomes work: eggs with an author's description read and hatchable (and Import Genome takes eggs); the genomes' sense, drive, decision and attention cells set from what the creature does, so their emitters work (the norns' kin pheromones among them); instincts naming senses or no drive become inborn nudges judged by the genome's reward and punishment chemicals; lobe 7 read as attention (Canny Norns' eating instincts); brain profiles giving added lobes roles in the mind (Nova Subterra and Canny Norns known, others read from their wiring), shown in the Breeder's Kit.

### 17 November 2026

#### Version 0.10.0

[`6e872d9a`](https://github.com/DotairZee/zeetures/commit/6e872d9adecdd85bab83e937c5fc40b39bef546e)

Help &gt; About and the README's Status now say 0.10.0 (owner, 2026-11-17).
The README also mentions temperament passed on from the parents and old
custom genome packs working: their chemistry and instincts, including
those tied to the original brain's cells, and roles for the brain lobes
they add.

#### Brain profiles for custom genomes; lobe 7 read as attention

[`a188934d`](https://github.com/DotairZee/zeetures/commit/a188934d16d24f42dcf0726682496e9d1e5b1ece)

Custom C2 genomes add lobes to the game's ten (Lummox JR's Nova
Subterra: search, sub-attention, afterdrive, change; Lis Morris and
Chris Double's Canny Norns: echo, search, choice). The original brain is
not run, so each added lobe now gets a role in the mind
(creatures/BrainProfiles.h), and each role multiplies some of the mind's
settings, applied after temperament:
- object-good, learns which things are worth attending to:
  betterLearning 1.2, curiosity 1.1;
- object-bad, learns which things to stay clear of: worseLearning 1.2;
- drive-change, notices its needs easing or worsening:
  unfeltLearningShare 1.4, learningRate 1.1;
- search, searches for what it needs: seekDiscount 1.2, forageShare 1.15;
- choice, weighs what an action on a thing will do: learningRate 1.15;
- a concept lobe bigger than the game's 640 cells is more memory:
  foodPlaceMax times the square root of the ratio, at most 2x.
Roles and known genomes are in brain-profiles.txt (built in, or the
player's own beside pheromones.txt), so players and pack makers can read
and change them. Known genomes are matched by their added lobes' place
and size: Nova Subterra and Canny Norns (plus curiosity 1.2 for Canny,
"much more curious than a normal norn"), from their authors' notes; a
hybrid keeps the roles of the lobes it inherited. A lobe no profile
names gets a role from its wiring: output reaching the attention lobe
(directly or through other added lobes) learns what to attend to, or,
taught only by punishment, what to avoid; reaching the decision lobe,
choice; reading the drives, drive-change; else none. From wiring alone
Nova's four lobes get exactly the roles its author describes.

The Breeder's Kit's import review lists a pack's brain ("Its brain, in
Zeetures": the known genome, each added lobe's role and why, the memory
size), and creatures log theirs at birth.

Lobe 7 is attention, not heard nouns. In the C2 CAOS guide's lobe order
(perception, drive, source, verb, noun, sense, decision, attention,
concept) lobe 2 is the source (what stimulated it) and 7 attention; the
genomes wire 7 to read 2 and 4. Instincts on lobe 7 had waited as heard
words: the game's norns' "hungry, attending to a flower: push or pull"
and every object instinct of Canny Norns, whose authors moved them
there (eat food and fruit when hungry, back away from cliff edges, eat
plants, not bad plants). Canny babies hatched without their eating
instincts and ate less. Lobe 7 now names the object as lobe 2 does, lobe
4 is heard nouns, and the attention lobe's cells are set for emitters
(nine pack genomes have one there). Saves before (state version 7)
unmark lobe-7 instincts so they are read on loading.

OPENC2E_NO_BRAIN_PROFILES reads and logs profiles without applying them
(A/B testing).

Tested, matched 9-minute copies of Home, each with three babies each of
the game's norn, Nova Subterra and Canny hatched together:
- profiles on vs off, before the lobe-7 fix: Canny 38.3 vs 26.6 meals a
  baby, 6% vs 11% of choices made hungry; Nova 39.6 vs 40.7; the game's
  norn (no added lobes) 42.2 vs 39.9;
- with the lobe-7 fix, profiles off: Canny 39.1 meals, 3% hungry, level
  with the others; population hungry 38-46% either way, as before.

#### Genome bridge: the genomes' brain-cell chemistry and waiting instincts work

[`fbcb5662`](https://github.com/DotairZee/zeetures/commit/fbcb566223cd7e4cfe96fa7ddecec2fcf532da70)

Custom C2 genomes (and the game's own) wire chemistry and instincts to
brain cells the mind replaces. Two parts make more of them work as their
authors meant, without running the original brain.

The genome's brain cells, set from what the creature does (option 2 of
genome-translation-options.md). Emitters reading the sense, drive and
decision lobes read nothing before, since those lobes never ran (only
the attention lobe and the mate cell were set). Now each mind tick sets:
- the sense lobe's 32 situations, with the meanings of the C2 CAOS guide:
  bumped, near a wall, in a vehicle; IT approaching or moving away, how
  near, object or creature, my sibling, parent or child; walking toward
  an edge, falling, landed. Stimuli light the cell their stimulus gene
  (sensory neuron, intensity) or script (full form's input) names, so a
  pat lights "patted", fading over a second or two;
- the drive lobe from the drives, and the decision lobe from the action
  under way.
The stock norns' parent, child and sibling pheromones, wired to sense
cells 16-18, are emitted for the first time. Each genome's reward and
punishment chemicals are read from its decision lobe's first two
chemical slots (52 and 53 in every genome checked).

The waiting instincts (option 1). Instincts naming a situation sense, or
rewarding no drive, waited (dormant). Now:
- "it is a creature" names the creature categories, "it is an object"
  any object;
- with a drive, the senses only narrow when; the drive's expectation is
  applied as before;
- with no drive they are inborn nudges (Params::innateNudge, 0.12 at
  full strength) added to an action's score while their situation holds:
  back away seeing a cliff edge, rest in a vehicle, back away when
  slapped or hit, eat plants and not bad plants, approach the hand. Each
  counts as legacyInstinctResilience x strength^2 tries; each try is
  judged by the approval felt (the genome's reward chemical rising, its
  punishment chemical falling; decision 1 of 2026-11-07), so a nudge
  that brings none fades as experience outweighs it.
Instincts on heard words still wait for language. Nudges are saved
(state version 6); saves made before mark the formerly dormant instincts
unread, so they take effect on loading.

OPENC2E_NO_GENOME_BRIDGE turns both off (A/B testing). With
OPENC2E_WATCH_CREATURES the lit senses and each nudge's tries are logged.

Tested: three matched 9-minute pairs, bridge on vs off: hungry 45.0 /
42.1 / 46.9 vs 45.2 / 48.9 / 44.9, meals per 1,000 choices 42.4 / 42.1
/ 41.0 vs 43.6 / 37.6 / 41.0, no starvation or drowning. Babies of four
genomes hatched side by side (Lummox JR's Nova Subterra, Lis Morris and
Chris Double's Canny Norns v1.2, the Akamai Canny Norns pack, and the
game's norn): 4, 0, 1 and 5 nudges; every sense cell above lit from
real events (patted about 1,100 times in 9 minutes).

#### Eggs with a description: read correctly, and hatchable

[`5d574694`](https://github.com/DotairZee/zeetures/commit/5d574694b6b6450408c898aa54a2d9e796379c7c)

C2 egg files carry a description after their 22-byte header (a 32-bit
length, then the text): empty in the game's own eggs, filled in by pack
authors, e.g. Nova Subterra's "Nova Subterra: Breed H female". Three
readers assumed the genomes start at byte 26, so for such eggs the
Breeder's Kit's install, its check of which breed slots a pack's eggs
use, and the breed mover all saw no genomes and left them unchanged.
One shared reader now handles the description (ReadC2Egg and
C2EggWithGenomes in Breeds.h).

A pack of genomes only, with no pictures of its own (Nova Subterra: six
eggs that use the game's norn pictures), had its eggs installed into the
Eggs folder, which nothing read, so they could not be hatched. The
Hatchery now lists every installed egg that belongs to no breed, labeled
with its author's description (or the file's name), with its own egg
picture when the shelf has it. Import Genome also takes .egg files (both
genomes) as well as .gen.

Tested with a copy of the user data folder: the Breeder's Kit installed
Nova Subterra's six eggs, the Hatchery listed them (24 eggs: 18 breeds
and the 6), and the packs whose eggs go with a breed were not doubled.

#### Temperament partly inherited from the parents

[`a883fc66`](https://github.com/DotairZee/zeetures/commit/a883fc66425e82b9bf2ccb6938fc825463a9fb21)

A bred creature's temperament now comes partly from its parents instead
of being drawn fresh at hatching. Each trait gets 0.4 of the parents'
average (heritability 0.4, within what twin and animal studies find) and
the rest from chance, drawn with the traits' usual links, so a child
correlates about 0.2 with each parent and the population's spread holds
from one generation to the next. As with the seasonal rhythm, the child's
temperament is written at conception beside its genome
(Genetics/NAME.temperament) and read when the creature is made from that
genome; the game's own eggs and older genomes still draw one at random.
The MATE line logs the child's temperament, and hatching logs "hatched
with a temperament from its parents".

Tested in a copy of Home: 18 conceptions in 9 minutes, and hatched
creatures carried the temperament written at their conception.

#### Developer aid: OPENC2E_MIND_PARAMS sets travel parameters for a test

[`e6115173`](https://github.com/DotairZee/zeetures/commit/e6115173c58d1666463eb4da1eae3c8f6511c33c)

OPENC2E_MIND_PARAMS="name=value,..." overrides the homeostat mind's
hungerPatience, travelHalf, floorChangeDistance, routeMaxCost and
seekDistanceCost, and prints the values in use once ("\* MIND PARAMS").
Without it nothing changes. First used for a doubled hungerPatience
(12 instead of 6) in three matched pairs of Home copies: hungry
65.4/65.2/64.0 vs 81.2/65.6/64.6, starving 42.0/40.2/40.9 vs
49.8/38.9/36.9, no consistent difference, so the default stays.

### 16 November 2026

#### Changelog: version 0.9.0 in the version list

[`ebceaa06`](https://github.com/DotairZee/zeetures/commit/ebceaa067fdeef2df98112d2f4125c56be6f5a94)

## Version 0.9.0

Seasons that shape breeding, bonds, rest and play, weighted by life stage (a spring breeding season, company as warmth and stronger loneliness in winter, more rest in winter and play in summer); curiosity that follows learning progress; eating ahead of a long time without food; food less pleasant the fuller a creature is; the research behind Zeetures listed in Help > About, with newer work beside each older source; about half the CPU use (a room lookup grid, cached record and kinship lookups, lighter status-bar and drawing work) and a built-in sampling profiler.

### 16 November 2026

#### Version 0.9.0

[`db095114`](https://github.com/DotairZee/zeetures/commit/db09511443e77c80704e986174243261ecea206b)

Help &gt; About and the README's Status now say 0.9.0 (owner, 2026-11-16).
The README also mentions curiosity by learning progress, eating ahead of
need, food less pleasant when full, the seasonal breeding, bond, rest
and play changes shaped by life stage, and the lower CPU use.

#### Performance: egg notices checked once a tick; off-screen creatures not drawn

[`28c4d0d8`](https://github.com/DotairZee/zeetures/commit/28c4d0d82e0d24d42870e0a9a887131b7bdf72b6)

The status bar checked every frame whether each egg notice's egg had
hatched, scanning every object and every record for each; eggs hatch
only on a world tick, so it now checks once a tick. A creature whose
drawn bounds (with a 32-pixel margin) lie outside the picture returns
before walking its body parts; the world is drawn three times a frame
for its wraparound. Together with the room grid and the record caches,
a normal-speed copy of Home went from about 57% to about 29% of a core,
and the world tick from 57 ms to 15 ms.

#### Performance: cache creature records by genome, and kinship answers

[`bbc317d4`](https://github.com/DotairZee/zeetures/commit/bbc317d4e23e1f20fc4c01520b7a80cd29f51cff)

historyManager::findMoniker(genome) scanned every record ever kept,
locking a weak pointer for each, and creatures' code calls it many times
a tick. Its answers are now kept by genome, checked against the record
on each hit and cleared whenever a record is added, removed or renamed.
closeKin, which built both creatures' ancestor sets as string sets for
every possible mate in sight, keeps its answers until the lineage
changes (historyManager::lineageGeneration: a conception or splice
event, a record added, removed or renamed, or parents restored from a
save). hasMoniker and getMoniker take their argument by reference.
Both caches were checked against the uncached answers in a headless run
(6.5 million record lookups, 100,000 kinship answers).

#### Performance: a lookup grid for which room is at a point

[`eb8d8574`](https://github.com/DotairZee/zeetures/commit/eb8d85747fd95b6a122746123c9ff53d852c1cea)

MetaRoom::roomAt and roomsAt scanned every room, copying a shared
pointer for each, and creatures' sight checks call them for every object
in range: a quarter of the program's time in a profile of Home. Each
metaroom now keeps a grid of 32-pixel cells listing, in the rooms' own
order, the rooms whose bounding box touches each cell, so a lookup tests
only those and returns exactly what the full scan did. The grid is
rebuilt when a room is added or any room's bounds change
(Room::boundsGeneration); points outside it fall back to the full scan.
Checked against the full scan on 231 million lookups.

#### Developer aid: OPENC2E_SAMPLE, a sampling profiler

[`466712b1`](https://github.com/DotairZee/zeetures/commit/466712b169c612e8a7271e31147b763632a19c54)

On Linux, OPENC2E_SAMPLE=FILE records the call stack every 2 ms of CPU
time (SIGPROF) and writes one line per sample at exit: the program's own
frames as offsets for addr2line, other libraries by name. The system's
perf is not available to users on the development machine.

#### Mind: food is less pleasant the fuller a creature is

[`56696abb`](https://github.com/DotairZee/zeetures/commit/56696abb89cfdd50ca1d01e95f0c1897fd594b12)

Owner, 2026-11-16: "look into" full creatures eating food the hungry
need. In matched copies 68% of meals that fed went to creatures with
Hunger under 64 and 13% to those at 150 or more. Eating eases the need
for pleasure, boredom and stress (the genomes' own eating stimulus,
about 0.06 each), so to a full creature food was entertainment.

Negative alliesthesia (Cabanac 1971; Berridge 1991; sensory-specific
satiety): below satiationHunger (0.35), what eating food (not other
creatures, categories 36-39) is expected to ease besides hunger is
scaled by hunger / satiationHunger, down to satedLiking (0.2) when
full. Hunger as felt counts, so the autumn appetite and eating ahead of
need still raise it. OPENC2E_NO_ALLIESTHESIA turns it off. CITATIONS.md
gains Cabanac (1971), with Berridge (1991), sensory-specific satiety
and a 2021 review.

Tests, three side-by-side pairs of fast copies of Home (with vs
without): hungry over the last 10 samples 22.3/24.4/24.4 vs
34.9/29.4/31.4; starving 12.1/17.3/12.7 vs 23.3/18.9/20.7; meals by
creatures under Hunger 64 202/203/167 vs 526/414/520 (share of meals
going to those at 150+ 21/15/19% vs 18/13/14%); meals per 1,000
decisions 19/17/17 vs 36/28/33 (the pleasure meals gone); no deaths.

#### Mind: eat ahead of need when a long time without food is coming

[`ae915649`](https://github.com/DotairZee/zeetures/commit/ae9156497491f86ae1f70d8d809a647504450add)

Owner, 2026-11-16 ("Yes, try it"), from the citations pass: regulation
also acts ahead of need (allostasis; Ramsay &amp; Woods 2014). With food at
hand (edible, reachable, within anticipateNear 250 px) and not yet very
hungry (below starveAwake), hunger is felt a share of the way to full
(anticipate 0.12 per cue, at most anticipateMax 0.3) for each cue that a
long time without food is coming: dusk (half at night), sleepiness (0.5
or more) and scarcity (at most one known food place of confidence 0.5
or more). The mind's perception gains the part of the day (timeOfDay).
OPENC2E_NO_ANTICIPATION turns it off, for comparison. CITATIONS.md says
what Ramsay &amp; Woods now informs.

Tests, three side-by-side pairs of fast copies of Home (on vs off):
choices to eat food 1105/1122/1192 vs 1034/1179/1034; share of them
made below Hunger 64 54/61/54% vs 52/56/57%; hungry over the last 10
samples 28.4/31.6/28.0 vs 27.9/21.4/30.8; starving 16.3/21.2/15.4 vs
17.8/11.5/22.6; no deaths. Little measurable change: creatures already
choose most meals with little hunger (median Hunger 0 at the choice),
eating being pleasant, so there is little room to add. Kept as a
principled, harmless part of the design.

#### Mind: curiosity follows learning progress

[`98a3dc00`](https://github.com/DotairZee/zeetures/commit/98a3dc00d286c59dc6dc0b8ba193cf5dcbdde27f)

Owner, 2026-11-16 ("Yes, try it"), from the citations pass: current
research treats curiosity as information seeking drawn to what one is
still learning (Kidd &amp; Hayden 2015; Gottlieb &amp; Oudeyer 2018; Oudeyer,
Gottlieb &amp; Lopes 2016), not novelty alone. The mind valued a (kind,
action) only for being little tried: curiosity / sqrt(1 + tries).

- Expectation keeps how wrong it has been lately: the summed size of
  its errors over the drives, before each update, as a fast (0.5) and a
  slow (0.1) running average (noteError). Learning progress is the gap
  between them, either way: falling errors (still learning) and rising
  ones (it began to surprise again) both count. The same for each
  individual creature's own record.
- Novelty: for the first firstTries (3) tries, as before; after that,
  curiosity x min(lpMax 1.5, progress / lpScale 0.05). What is fully
  known, and what is too random to learn, lose their pull.
- A record with no error history (new, or from a save made before this)
  starts both averages at its first error, so no progress is read into
  it.
- Saved with the mind ("lp CATEGORY DECISION FAST SLOW" after each
  "expect" line); older saves load as before.
- OPENC2E_COUNT_CURIOSITY: rarity alone, as before (for comparison).
- CITATIONS.md says what the curiosity research now informs.

Tests, three side-by-side pairs of fast copies of Home (learning progress
vs rarity), hungry over the last 10 samples 25.6/29.9/27.6 vs
29.7/29.4/27.7; meals that fed per 1,000 decisions 49/52/54 vs
53/45/43; different actions per creature 18.6/18.7/20.2 vs
20.0/20.1/21.3 (more focused); share of choices acting on other
creatures 0.33/0.31/0.32 vs 0.32/0.34/0.35; deaths 0/0/0 vs 1/0/0.

#### CITATIONS.md: newer research beside the older sources

[`51787523`](https://github.com/DotairZee/zeetures/commit/51787523259a79a8920b1ccaf75f3d503744b886)

Owner, 2026-11-16: "a lot of our citations are hella old!" Each older
work now has a "Since then" line (an indented sub-entry, "  - " in the
file, shown indented in Help &gt; About) giving more recent research on
the same idea, checked by web search: Ramsay &amp; Woods 2014 (allostasis)
for Cannon; a note that drive reduction alone is superseded, for Hull;
Schultz 2016 for Rescorla &amp; Wagner; Kidd &amp; Hayden 2015, Gottlieb &amp;
Oudeyer 2018 and Oudeyer et al. 2016 (curiosity as information seeking
and learning progress) for Berlyne; Berridge &amp; Robinson 2016 for their
1998 paper; Dunovan &amp; Verstynen 2016 for action selection; McDiarmid
et al. 2019 for habituation; Barrett &amp; Bliss-Moreau 2009 and a 2024
review for Russell; Shiner et al. 2012 for Thomas &amp; Chess; Wolf &amp;
Weissing 2012 for animal personality; Bleidorn et al. 2022 for
Roberts &amp; DelVecchio; Walum &amp; Young 2018 for pair bonds; Dunbar 2018
for grooming and friendship; Kiyokawa &amp; Hennessy 2018 for social
buffering; Le &amp; Agnew 2003 for Rusbult; Corns 2018 (a critique) for the
negativity bias, noting the threefold harm weight is a design choice;
Wyatt 2014 for pheromones. Details that could not be confirmed (a
volume and pages) were left out.

About's citation list also collapses the double space a removed link
leaves.

#### About: research in its own window; temperament and relationship sources

[`a683f0eb`](https://github.com/DotairZee/zeetures/commit/a683f0ebe0fd80dbe9029cb9e48eaea1c59dca66)

- Help &gt; About grew taller than the game's window with the research
  list open inside it (owner's screenshot). About now has a "Research
  behind Zeetures" button beside "openc2e on the Web" and "OK", which
  opens the list in its own window sized to the screen (80% of its
  height, up to 560 wide), scrolling, with a Close button; Escape closes
  that window first. About itself is never taller than 95% of the
  window, scrolling instead. Checked at 820 x 700.
- CITATIONS.md: the temperament and relationship sources, recovered from
  the two research briefs of 2026-11-11 (owner: "it's not enough to say
  we can't find what research we did"). Temperament: Thomas &amp; Chess,
  Rothbart, Kagan, Saudino, Reale et al. 2007, Sih et al. 2004, Gosling
  &amp; John 1999, Bell et al. 2009, Dochtermann et al. 2015, the clonal
  fish, identical mice and fly individuality studies, Ayroles et al.
  2015, Roberts &amp; DelVecchio 2000, differential susceptibility.
  Relationships: Aragona &amp; Wang 2004, Montoya et al. 2017, Dunbar,
  Carter &amp; Wilkinson 2013, Leedale et al. 2020 and the Westermarck
  effect, Hsu et al. 2006 and de Waal 2000, Harlow, Kikusui et al. 2006
  and Hennessy et al. 2009, Rusbult 1983, Gottman, Baumeister et al.
  2001, Almaatouq et al. 2016, Roberts &amp; Dunbar 2011, and the games
  consulted. Each with what it informed.

#### About: "Research behind Zeetures"

[`3a042888`](https://github.com/DotairZee/zeetures/commit/3a04288883e894dac7b4a2a44064b270199bbcdb)

The citations section in Help &gt; About, and the title of CITATIONS.md,
now read "Research behind Zeetures" (owner, 2026-11-16).

#### CITATIONS.md: the research behind the creatures, also in Help &gt; About

[`aaa621f8`](https://github.com/DotairZee/zeetures/commit/aaa621f8c640e446b1758002e849392e60a7717a)

Owner, 2026-11-16: "create a citations list somewhere for the research
we have used to inform behaviors and other stuff ... in the About
section". CITATIONS.md lists the published works and sources behind
the mind (from the mind model's design references), temperament and
relationships (general reading; the specific sources were not recorded
at the time), scents (from the scent design's research) and seasons
(today's research), each with what it informed and links where there
are any. It is built into the program (EMBEDDED_FILES) and shown in
Help &gt; About under "Research behind the creatures" (collapsed): "## "
headings as separators, entries wrapped, each link a small button that
opens it. The README's Status links to it. Developer aid:
OPENC2E_OPEN_KITS="about,citations" opens the section.

#### Seasonal behavior: a spring breeding season, winter bonds, winter rest, summer play

[`67bf1b87`](https://github.com/DotairZee/zeetures/commit/67bf1b8736b783db1a65152d5194541cf1298e36)

Owner, 2026-11-16: "A, B, and E", with "life stage as a mediating
factor" (proposal after research on seasonal patterns in animals and
people; sources in CITATIONS.md). Each effect is scaled by the
creature's inherited seasonal strength (SeasonalRhythm) and by a weight
for its life stage (SeasonalRhythm::stageWeight, stages 0-6):
breeding 0, 0, 0.6, 1, 1, 0.6, 0; bonds 1.5, 1.3, 1, 0.8, 0.8, 1.2,
1.5; rest 1.2, 1, 0.8, 0.8, 1, 1.3, 1.5; play 1.5, 1.5, 1.2, 1, 0.7,
0.5, 0.3.

- Breeding season: the chance of conceiving (Breeding.cpp) is
  multiplied by 1 + swing x strength x stage weight (both parents'
  averages, strength at most 2), swing +0.6 spring, +0.1 summer, -0.3
  autumn, -0.6 winter, never below 0.1. The mind feels the sex drive
  stronger in spring (springSex 0.25 of the way to full) and weaker in
  winter (winterSexDamp 0.4).
- Winter bonds: loneliness felt stronger in winter (winterLonely 0.25).
  Company warms: in a room colder than 50, each creature it is bonded to
  (affinity 0.3 or more) within 120 px, at most three, counts as 8
  degrees warmer for its air coldness sense, times its stage's bonds
  weight (oldCreature::companyNear, worked out every 20 ticks).
- Winter rest: sleepiness felt stronger in winter (winterSleepy 0.2).
- Summer play: novelty worth more in summer (summerPlay 0.5 extra).
- OPENC2E_NO_SEASONAL_BEHAVIOR turns all of these off (the autumn
  appetite and winter thrift keep their own switch).

Tests, season forced (OPENC2E_TEST_SEASON), two rounds of six
side-by-side copies of Home, on vs off: winter sleep choices per 1,000
decisions 80/79 vs 57/55; conceptions in spring 11/0 vs 6/0, in winter
0/0 vs 3/0 (round 2 conceived none either way); summer roaming (map
cells per creature) 17.9/18.0 vs 18.1/17.1, no change; loneliness was
already low (drive 6-9 of 255) and the test rooms were not cold, so
winter bonds hardly showed; no deaths.

#### Changelog: version 0.8.0 in the version list

[`db2498dd`](https://github.com/DotairZee/zeetures/commit/db2498dd721e11cdfc886edc5281183b46295a29)

## Version 0.8.0

The same default world on any machine: the world additions built into the program, their art shipped or made in the engine from the player's own game, Home's settings as defaults, and the Boney Grendels and Frog Norns V3 packs fetched and imported; an Intel Mac build; all sounds on to start. Seasons that change creatures' biology (eat more in autumn, save in winter), how strongly inherited from the parents. The west sea airlock lets creatures out into the pool east of it; mind fixes that freed creatures pinned at walls and waiting on food out of reach; real crowdedness, radiation and slope senses; the population limit counts every creature; more fruit and bushes; stranded objects set back inside the rooms; a census of hunger and food every 5 minutes; the relationship map's zoom and drag; where each creature died kept with the world.

### 16 November 2026

#### Version 0.8.0

[`f63adf35`](https://github.com/DotairZee/zeetures/commit/f63adf351203a3debf134c3a23135eebe4b9adf3)

Help &gt; About and the README's Status now say 0.8.0 (owner, 2026-11-16).
The README also mentions the inherited seasonal rhythm and that seasons
change creatures' appetite and metabolism.

#### Seasonal biology: fatten in autumn, save in winter, inherited

[`55745742`](https://github.com/DotairZee/zeetures/commit/55745742cfa0ef08c876bb2adf504fa01cde72da)

Owner, 2026-11-16: "should they eat more in autumn to get through
winter?", and chose that how strongly each creature does so be
inherited. A season is 2 hours of play and a lifespan about 9.7, so a
creature lives about one year and one winter.

- Seasonal rhythm (Temperament.h/.cpp, SeasonalRhythm): a z-score per
  creature like the temperament traits (0 average, clipped to +-2), but
  inherited: at MATE the child's is its parents' average plus chance
  (normal, sd 0.71, so the population's spread holds), written beside
  the child's genome as Genetics/NAME.season; NEW: CREA reads it when
  making a creature from that genome. A genome with none (the game's own
  eggs, older genomes) gives a random one. Saved with the world
  ("seasonal" per creature). Strength exp(0.5 z): about 0.37 to 2.7.
- Autumn appetite (HomeostatMind tickChoose): in autumn hunger is felt
  as if a share of the way to full (autumnAppetite 0.2 times the
  strength, at most autumnAppetiteMax 0.45), so a creature seeks food
  sooner and eats past its need; its own chemistry stores the surplus.
- Winter thrift (c2Creature::tickBiochemistry): in winter a share of
  biochemistry steps is skipped (0.2 times the strength, at most a
  half): burning, digesting and making all run slower, so stores last.
- OPENC2E_NO_SEASONAL_BIOLOGY turns both effects off, for comparison.
  OPENC2E_TEST_SEASON=n sets the season after loading (test aid).
- Owner's Kit, Temperament page: a "Seasonal rhythm" bar and a line
  saying it is passed on from the parents.
- MATE's log line gives the child's seasonal rhythm and the parents'.

Tests, three rounds of four side-by-side copies of Home (season forced
with OPENC2E_TEST_SEASON; on vs off), last five samples: autumn, meals
that fed per 1,000 decisions 65/61/74 vs 42/44/41, energy stores
(chemicals 70-74, mean per creature) 382/384/386 vs 360/355/349, hungry
50.6/34.0/27.0 vs 46.0/43.4/36.8; winter, stores 388/377/399 vs
371/383/361, hungry 40.0/38.2/24.8 vs 46.2/28.4/47.8, starving
25.8/26.8/13.2 vs 33.6/19.2/33.4; no deaths with, one without.
Inheritance: two test matings gave -0.85 (parents -1.27, -0.24) and
+0.72 (+0.35, +0.86), the first written to its .season file; a test
save wrote a seasonal line for each of 148 creatures.

#### A census of hunger and food every 5 minutes (C2)

[`889126b7`](https://github.com/DotairZee/zeetures/commit/889126b7a8cc94f82d3ff757e9bb18c44ab0734f)

Owner, 2026-11-16: "add a logger", to see hunger across seasons and
overnight. The hungry share in Home read 26% in a morning sample and
54% in an evening one, and copies swing from 40 to 90 hungry of 150
within minutes, so single samples mislead.

Every OPENC2E_CENSUS_EVERY world ticks (3000 by default, about 5
minutes at 10 a second; 0 turns it off) the engine appends a row to
census.csv in the world's folder (header on first write) and prints a
"\* CENSUS" line: local time, world tick, season, year, living
creatures, hungry (Hunger drive 150+), starving (216+, the level that
counts as starvation at death), adults (life stage 4) and adults
hungry, creature eggs (2 5 2, 2 5 3), edible food lying within 60 px of
its room's floor (as World::tickFoodScent counts reachable food), and
deaths since the last row by cause (starvation, drowning, old age,
other). Deaths are tallied in Creature's death report (censusDeaths()).

Test: a copy of Home with OPENC2E_CENSUS_EVERY=200 wrote the header and
a row every 200 ticks (e.g. 154 creatures, 52 hungry, 34 starving; 101
adults, 42 hungry; 31 eggs; 47 food in reach; 1 died of old age).

#### Close room 95's leftover doorway to the waterwheel pit

[`2cf21662`](https://github.com/DotairZee/zeetures/commit/2cf216628d194d01ce65aa38b5df9f25d1e443f7)

water-pocket.cos shrank room 95 (the water under the dock by the cave
lift) to x 2206-2308; its doorway (100) to room 206, the pit behind the
waterwheel (x 2800-3060), stayed, joining rooms 500 px apart. No
creature or object could cross it, but food smell spread through it
(World::tickFoodScent uses every open doorway). It is now 0.

Checked with the other open doorways between rooms that do not touch
(scan after b982eb66): drops over 61-65 (perm 100, overlapping in x
with 37 px of rock between) left all four creatures on room 61's
floor; walks across the 65-68 corner went through cleanly. 17-219
overlap (a real way through), and 108-109 and 157-179 are the game's
own and join rooms that are far apart; left as they are.

#### Close room 108's floor doorway into the rock below it

[`b982eb66`](https://github.com/DotairZee/zeetures/commit/b982eb6643b6ce78ee4332c285952c909328fc62)

Room 108 (x 1342-1473, floor 1300) has a doorway (perm 50) to room 112
below, but the two rooms do not touch: 140 px of rock lie between
(112's top is 1440). A creature or object falling onto 108's floor went
through it into the rock, out of the room map ("out of room system" at
x 1420-1450, y 1320-1340, seen in several test runs), and on down
through 112's own perm-50 floor to room 114, the lower west cave, whose
only way up is the cage lift into the dead-end corridor by the west
airlock. Creatures let out into the pool by the west airlock (bebe039f)
walk out over this floor, so more of them fell there; a live survey
afterwards found hungry creatures gathered in rooms 112 and 114.

westlock.cos now sets that doorway to 0. Test: four creatures dropped
onto 108's floor at x 1350-1440 with vely 12: all four stayed on it and
walked on east (before: two of three fell to 112, then 114).

A scan of the map found six other open doorways between rooms that do
not touch (17-219, 61-65, 65-68, 95-206, 108-109, 157-179); not changed
here (most are far apart or overlapping; 61-65, perm 100 across 37 px of
rock, is the next to check).

#### West sea airlock lets creatures out into the pool east of it

[`bebe039f`](https://github.com/DotairZee/zeetures/commit/bebe039f483817eb511cc85db6c8f114f08cc8f3)

The airlock's land door was in the rock at the east end of room 120,
the floor by the cage lift. That floor opens only into a corridor
(rooms 120, 119, 118 and the seam alcove 180) with no way out on foot,
only two cage lifts: over five test copies creatures spent a median 355
decisions there (90% 860), and many starved.

The land door (2 3 248, zwld) is now in the west wall of the shallow
pool just east of the sea door (room 110, x 669-1170, water 1352-1440),
at 662,1366, the place the owner chose from airlock-pool-exit-
mockup2.png. It is drawn at plane 9000, in front of the game's own rock
over the pool's west bank (scenery cvl2/cvlk, plane 8999), as in the
mock-up; at 1400 it was hidden. The car now goes down only to the
pool's level (top 1308, bottom 1418; the sea door shuts behind it from
1290), moves in one step from the rock to x 716 (just east of the door,
4 px over the pool's sloping bed), and shoots the rider east into the
water (velx 14, vely -5; it was -26, -9 across a dry floor). From the
pool a creature wades east and walks up the far bank to room 113, then
108 and the rest of the world; a creature holds its breath far longer
than the wade takes.

The pool's west end meets the seabed's low point at about the same
height (x 669, about 1372), so creatures wading back to the pool's west
wall were taken in again. A creature let out is marked (OV94 1) and not
taken in until it has left the pool's area (middle x 560-1200, feet
1250-1470), when the mark is cleared.

Tests (copies of Home): one creature placed at the low point was taken
in, let out at x 729 and walked out to room 108 within about 30 s; two
at once both reached x 1500+ within about 2 minutes, marked while in
the pool and cleared after; no re-takes. Seen on the way: brief
"out of room system" errors at x 1420-1450, y 1320-1340 (under room
108's floor), also present in earlier runs; the engine sets those back.

#### Mind gives up waiting on food it cannot reach

[`637b17b8`](https://github.com/DotairZee/zeetures/commit/637b17b88d693f879e91bc4fee12d7c3d5369d0a)

Hungry creatures gathered at the world's west seam (room 180, the
58 px alcove at the west end of the corridor 120-119-118 where the
west sea airlock lets creatures out) and waited there, up to 18 in a
copy. Two reasons, from their choices:

- The tomato plants of room 118 next door hang their fruit about
  100 px over the floor. Food seen kept its place on the food map at
  0.6 at every look and counted as "found food here", so the place
  never faded. Now only food within reach does that; food seen out of
  reach marks a new place at 0.3 (unreachableFoodConfidence; it may
  fall later) but does not renew a known one.
  OPENC2E_NO_FOOD_REACH_CHECK turns this off.
- The best remembered food was often the short way round, across the
  seam, which is shut at that height, or behind the alcove's wall. With
  the way barred and no route known, the food step offered nothing and
  the place stood as best until its trip timed out. Now a place whose
  way is barred from here is out of reach at once: confidence halved,
  not tried for foodCooldown ticks, so the next decision tries another.
  OPENC2E_NO_BARRED_GIVEUP turns this off.

Tests: 8 creatures with Hunger 180+ moved into room 180, counted every
10 s for about 4 minutes, three side-by-side pairs (both changes vs
neither): creatures in the alcove over the last 10 samples 6.4/7.3/2.0
vs 9.4/7.7/8.0; Hunger 150+ world-wide 70.2/69.9/75.8 vs 73.4/73.8/
77.7; meals that fed per 1,000 decisions 44/39/43 vs 43/49/43; deaths
the same. The reach change alone (three earlier pairs) did not empty
the alcove (8.6/5.6/16.6 vs 7.5/6.7/11.4).

#### No "Couldn't play" script errors when there is no audio device

[`79bee4b0`](https://github.com/DotairZee/zeetures/commit/79bee4b0eab6f5fdbcd7ab2f01e00a982a1b62ac)

Since -s selects the null audio backend (606d6367), every sound and
creature voice a script played failed and was logged as a script error:
about 25,000 "Couldn't play sound/voice" lines per 6-minute test copy,
burying the real errors. Agent::playAudio and the voice queue now skip
the error when the audio backend is the null one. With a real device a
missing sound is still reported.

Test: a -s copy of Home logged 0 script errors in 40 s (thousands
before).

#### Mind: interrupted walks and turning no longer mark a way blocked

[`53f2910f`](https://github.com/DotairZee/zeetures/commit/53f2910fcacefbca2b3770b44d35f9598c76e959)

Follow-up to 27be8d12 at the ledge under the east dock (room 14).
Instrumented there (every blocked mark, with its cause), the hungry
creatures' ways were still marked blocked in 640 of 754 choices, by two
causes:

- Turning itself moves a creature's middle 14-20 px, past the 10 px
  allowed for a bump behind a turning walk: turnBumpWay is now 30 px.
  The creature whose "west" walk really went east made 200 px of way
  first, and is still caught.
- A walk cut off within 2-6 ticks (in a crowd, another creature's poke,
  hit or pickup ends it) counted as "got nowhere" and marked its way
  blocked. A walk now has to run nowhereMinTicks (6) ticks first.
  OPENC2E_NO_SHORT_WALK_CHECK turns this off (with OPENC2E_NO_TURN_BUMP,
  the old behavior), for comparison.

Tests, three side-by-side pairs of fast Home copies (about 6 minutes
each, the same machine load): creatures with Hunger 150+ across the
world, averaged over the last 13 samples, 56.1/46.8/53.5 with, 60.9/
51.0/57.5 without (about 4 fewer); meals that fed per 1,000 decisions
77/85/85 with, 87/65/72 without; at the room 14 spot 6.5/2.6/5.9 with,
8.8/2.2/2.2 without (still no consistent change there); no deaths in
any copy.

#### Mind: a bump behind a turning creature is not a wall ahead

[`27be8d12`](https://github.com/DotairZee/zeetures/commit/27be8d122a37ca59a71866d9a04d4244b7578810)

A creature standing against a wall swings its foot past the wall as it
turns to walk away. The engine counts that as a wall bump on the side it
is leaving (SkeletalCreature walledDir), two bumps ended the walk as
"walled", and the mind marked both ways blocked at that spot: the side
bumped, and the walk's own direction too (kept for creatures whose
"west" walk really goes east). Creatures at the east wall of the ledge
under the east dock (room 14, x 6320) were left with every walk
filtered out there (674 of 724 hungry choices had both ways blocked),
starving among each other.

Now a travel walk's bumps on the side behind it, after less than
turnBumpWay (10 px) of way toward that side since the walk began, do
not count as walled. A creature whose walk really goes the other way
makes way toward the wall first and is still caught as before.
OPENC2E_NO_TURN_BUMP turns this off, for comparison.

Tests, three matched pairs of fast Home copies (about 6 minutes each):
meals that fed per 1,000 decisions 81/76/93 with, 72/77/85 without;
creatures at the room 14 spot 5-7/5-6/9-11 with, 8-10/8-9/5-8 without
(no consistent change there: the crowd is held mostly by acting on each
other); no crashes, deaths 0/0/4 with, 0/1/4 without.

#### Breed packs on Windows: curl runs with no console window

[`39940dd7`](https://github.com/DotairZee/zeetures/commit/39940dd738ebc286574412dabe4a1ac701d48608)

std::system ran curl.exe through cmd.exe, whose console window flashed up
over the game for each download. On Windows the download now starts
curl.exe directly with CreateProcessW and CREATE_NO_WINDOW and waits for
its exit code; the part file's path is passed as wide text, for user
folders whose names are not plain ASCII. Other systems are unchanged
(std::system with shell quoting).

Not yet run on Windows; compiled by CI.

#### Headless runs: -k null runs the world, -s opens no audio device

[`606d6367`](https://github.com/DotairZee/zeetures/commit/606d63677632f5dbef5c665f1c15a041fe03615a)

- NullBackend::run was empty, so -k null loaded the world and quit at
  once. It now ticks until the engine is done, sleeping 16 ms between
  ticks; SIGINT and SIGTERM end the loop (with no window, SDL's quit
  event never came and a TERM was ignored).
- Engine::drawWorld returns at once under the null backend (the world's
  drawing asserted on the missing render surface, World.cpp:739).
- -s (silent) now selects the null audio backend. It only muted sound
  at the start, and the player's own sound choices in settings.txt,
  loaded later, won: headless test copies run with -s played creature
  voices and world sounds through the speakers.

Tests: -k null on a copy of Home ran 40 s of world ticks and ended on
TERM within a second; a -s test copy logged no sounds played (it logged
many before).

#### C2 physics: objects stranded outside the room map are set back in

[`ad38fb77`](https://github.com/DotairZee/zeetures/commit/ad38fb772c2e9a507bb30c1d51b22ba630ffd0e1)

Objects (2 5 2, 2 15 9, 2 18 3) hung in mid-air or half in the rock at the
west sea airlock's land door, where the floor room 120 (x 399-573, y
1400-1540) opens east into room 115 through a doorway only 68 px tall
(from y 1472).

- findCollisionInDirection: while only testing a place (validInRoomSystem,
  s_nudging set), a box point outside every room now makes the place
  unfit (collided). It went on as for a real move: the test passed the
  place, logged "out of room system" and marked the object, so the C2
  nudge set objects back with their top-middle still in the rock, again
  and again.
- tryMoveToPlaceAround (C2): a last pass tries places below as well (up
  to the search height, any room), for an object whose top is caught in
  the rock over a room.
- Agent::healOutOfRoomSystem (C2, from physicsTickC2): an object physics
  marked out of the room map (family 2, collides and falls, not carried
  or riding) is tried back in after 10 ticks and every 10 after, then
  falls from there with its speed cleared. Before, it stayed stopped for
  good. Logged as "set back inside the room map".
- westlock.cos: at load, physics objects resting in the land door's
  doorway (middle x 560-680, bottom 1440-1545) are given vely 1, so ones
  saved hanging there fall (saves do not keep the out-of-room flag).

Tests (copies of Home): a 2 5 2 put in the doorway corner went back into
room 120 with no error; 2.5 minutes fast: 0 "out of room system" errors
(30 before, all 2 5 2 at that doorway). A census showed many physics
objects resting in mid-air on purpose (fruit on bushes, clouds), so
saved objects are not all made to fall.

### 15 November 2026

#### Four bushes where hungry creatures gather; a food census test aid

[`631441db`](https://github.com/DotairZee/zeetures/commit/631441db33e2d60db5b0a0df75e33d213c2f4066)

Home that evening (2026-11-16): almost every food object was edible,
but it piled up where no creature went (22 of 2 21 1 around 1800-2400,
1500; fruit around 2400-3600, 900-1500; and more), while 34 of 60 very
hungry creatures had no food within 1500 px on their floor. Owner: bushes
at the hungry spots.

- food.cos: four seasonal bushes, planted full-grown wherever missing,
  each wholly inside a room where hungry creatures were sampled (the
  first picks, made from the painted background, hung half outside their
  rooms; owner caught it) on a floor measured by dropping an object: the
  library (room 114, 1102/1714, potted, mossy, clear of its lift), the
  far west workshop (room 105, 2/2202, potted, left of the brick arch,
  clear of the potion machines; owner), the lower middle caves by the
  pipe roots (room 50, 4162/2148, mossy), the treehouse walkway left of
  the hut's door (room 38, 4554/1731, potted, warm rock). Mock-ups chosen
  by the owner: bush-spots-mockup2.png, bush-workshop-pot.png.
- OPENC2E_FOOD_CENSUS=N: at session tick N, every object that smells of
  food, by classifier: how many, how many edible now, carried, where.

#### Cliff-edge markers: hidden from creatures, no flashing arrows

[`7900f56a`](https://github.com/DotairZee/zeetures/commit/7900f56a76c528cc4dc0c304329d3d0fe655d9fc)

Owner, 2026-11-16 (screenshots): "weird little arrow graphics that flash
in and out" at deck edges and in the caves. They are the game's 18
cliff-edge markers (2 10 2, sprite edge: an arrow left, an arrow right,
a blank). Their timer shows the blank and makes a creature touching one
teeter; their activate scripts show an arrow, until the next timer tick.
Creatures could see them and, curious, acted on them: in one session 88
activations (each a flash) and 26 tries to eat one, among hundreds of
choices on category 10.

- edges.cos: the markers are invisible to creatures (ATTR + 16; the
  teetering is by touch, and our mind finds drops from the room map).
- script-patches.txt: their activate scripts no longer set the arrow
  poses (they keep setting the direction, OV00).

Home copy: attr 212 on all 18, both patches applied, activating every
marker left them blank, no creature choices on category 10 in 4 minutes.

#### C2 senses: crowdedness, radiation and slopes, which every genome reads

[`c2c7350a`](https://github.com/DotairZee/zeetures/commit/c2c7350abd1c67a099772e436e4dff665e3448a1)

Every C2 genome checked (norn, grendel, ettin, the Frog and Boney packs)
has emitter genes reading senses openc2e left at 0: crowdedness (an
inverted emitter of Loneliness, chemical 8, and one of Crowded, 9),
radiation (Heavy Metals, 140, above 48) and slope up and down (upatrophin
178 and downatrophin 177, digital above 64). With crowdedness at 0 every
creature was fed loneliness even in a crowd, and Crowded never rose.
Time of day, season and wind are read by no genome and stay 0.

Our own definitions (C2's are not documented): crowdedness 40 for each
other living creature within 200 px across and 120 up or down, to 255
(the living creatures' middles gathered once a world tick); radiation
the room's; slopes the floor's rise or fall under the creature the way
it faces, 512 per unit of rise per pixel (about 7 degrees reaches 64).
OPENC2E_NO_NEW_SENSES leaves them at 0, for comparing.

Two 20-minute Home copies side by side: loneliness 9 against 17,
Crowded 66 against 48, hunger 144 against 161 (very hungry 55 against
66), deaths 1 against 3; groups no smaller (biggest 11 against 9).

#### The population limit counts every creature, not only norns

[`ab840238`](https://github.com/DotairZee/zeetures/commit/ab840238e4a6c2f8b1e1df90ad34ce58fafda642)

Owner, 2026-11-16: "with limiting creatures in the Hatchery, please make
it so the limit is not for norns, but for \*all creatures\*."

- EGGL and HATL: the game's scripts (the norn egg's hatching, the kiss,
  the hatchery machine) compare the number of norns (TOTL 4 1 0, ENUM 4
  1 0) with them, so they now return the limit less the grendels and
  ettins alive: "norns &gt;= that" is "all creatures &gt;= the limit". No limit
  set: as before, a number larger than any population.
- grendels.cos: the egg pile's grendel eggs and the egg machine's (and
  gene splicer's) eggs wait (two minutes, then again) while the world is
  at the limit, with the same test, as the game's norn eggs do; they
  never checked it before.
- Hatchery: "Creatures in the world" and "Limit the number of creatures",
  counting every creature; the tooltip says so.

Home copy, limit 100, 151 norns, 1 grendel, 1 ettin: EGGL and HATL 98;
no limit: 1000000; no script errors.

#### Gene lab: fill the strip by its east wall with a sealed room

[`5103d917`](https://github.com/DotairZee/zeetures/commit/5103d9177b3fb0b6e8545bec58d0512cf6700f37)

Home's log (2026-11-16) showed a norn "out of room system" four times in
a session at about 6900-6914, 1711-1725. Shortening room 159 to end at
its drawn floor (6895, on 2026-11-14) left x 6895-6930 belonging to no
room (room 171, behind the east wall, starts at 6930); a creature turning
against the wall reached into it with a corner of its box, which the C2
physics treats as leaving the room system. gene-lab-pit.cos now makes
room 251 over that strip (6895-6930, 1470-1798, 159's values), with no
doorway to any room, so the wall stays a wall and the corner is still in
a room. A copy with three creatures placed against the wall: one
exception before, none after.

#### Relationship map: zoom with the wheel or a pinch, drag to move about

[`cb986bd8`](https://github.com/DotairZee/zeetures/commit/cb986bd8574e32085c3b6e369eb678eb5cccb9ff)

Owner, 2026-11-16: "I'll need to be able to zoom in on that relationship
chart, and also to be able to click drag to move around it. let's mouse
wheel zoom in and zoom out, and pinch zoom."

- The map is drawn in its own clipped area. The mouse wheel (15% a
  step) and a touchpad pinch zoom it about the pointer, 1x to 8x; a drag
  (more than 4 px) moves it about; a click without a drag still selects
  the creature under the pointer. "Reset view" (with the zoom shown)
  appears at its top left while zoomed. The view is kept while the kit
  is open. Dots and the centre grow a little with the zoom (up to 2x),
  and more names are placed as it zooms in (8 times the zoom squared),
  still only where they don't cover one another.
- Pinches over the interface were dropped (kept from the world, which
  would zoom the view of Albia); the SDL backend now adds their scale to
  UiPinch (common/UiPinch.h), which the map takes when the pointer is
  over it.
- OPENC2E_TEST_RELMAP="zoom,dx,dy" starts the view zoomed and moved
  (snapshot at 3x checked in a Home copy).

#### Owner's Kit: Relationships shorter and scrolling; Life Story only as wide as it needs

[`a45b3683`](https://github.com/DotairZee/zeetures/commit/a45b3683884ea1d01ce3b6bb71020521f51c447e)

Owner, 2026-11-16 (screenshots): "the Relationships tab of the Owner's
kit needs to be shorter and have a scroll! and the Life Story tab does
not need to extend as far out to the right as it does."

- Relationships: the list is about ten rows high and scrolls, its
  header kept in place, instead of a row for every creature in the world
  below the map. The map names only mates, the feared and the eight best
  liked, each only where it doesn't cover a name already placed (the
  best liked sit side by side round the ring), drawn last on a dark
  backing; with 150 creatures every name had overlapped into a smear.
  Pointing at any dot still names it.
- Life Story: the table is as wide as its longest date and sentence (the
  sentences wrap past 380 px) instead of a fixed 620, and the date column
  fits the dates instead of 200 px.

Checked in Home copies (snapshots of both pages).

#### Quieter log: C2 debug output, failed loci, sprite searches

[`1c3daaf4`](https://github.com/DotairZee/zeetures/commit/1c3daaf4b15979fecdd200bfbd8029168817f5e0)

A review of Home's logs (2026-11-16) found three kinds of harmless
lines filling them, besides the mind's own:

- C1/C2 objects' scripts' debug output (DBGM, DBGV, DBUG: "EAT - aim",
  "SCRIPTEXTRAEAT" and the rest, about 1,600 lines a minute in Home),
  which the original game sent to a debugger no player saw: printed only
  with OPENC2E_DEBUG_CAOS now (C3's DBG: OUTS and OUTV as before).
- "c2Creature::getLocusPointer failed to interpret locus" for every
  creature made (about 34 a minute; brain-lobe loci such as the original
  brain's dendrite settings, never finished in openc2e and unused under a
  mind): once a run for each locus now.
- "imageGallery couldn't find the sprite" from the creature picture
  searches (about 220 a minute), which try ages and breeds in turn and
  expect misses: imageManager::getImage takes quietIfMissing, used by
  SkeletalCreature's search and its stage-size lookup; other misses are
  still reported.

Home copy, 4 minutes: none of the first and third, 53 locus lines in all,
no script errors; the log a quarter of the size.

#### Keep where each creature died with the world

[`028c812b`](https://github.com/DotairZee/zeetures/commit/028c812b60d926b0f1e63052b84ab2655cd7bc10)

Where a creature died (its middle and the room there) was only printed
to home.log, which each relaunch replaces, so deaths could not be mapped
over a day (2026-11-16: checking the starvation and drowning fixes).
lifeLogEntry gains x, y and room, set by logLife for a death from the
creature's agent; the world save writes them as "life-place X Y ROOM"
after the entry (deaths before this have none) and reads them back.
Tested in a Home copy: a creature killed by OPENC2E_TEST_KILL and the
world saved: "life-place 4616 532 9".

#### More fruit, more bushes; a sea current from the west sea's shelf to the hatch

[`e996d0dc`](https://github.com/DotairZee/zeetures/commit/e996d0dc871ee6debab53a8e5ac8908bde3b202f)

Home's deaths over the last 24 game hours (the save's life records):
548, of them 291 old age, 145 starvation, 78 drowning, 34 organ failure;
149 creatures living, 53 very hungry (hunger drive over 200), spread
across the world. Owner, 2026-11-16: more food, and drowning in the same
pass.

- food.cos: the seasonal bushes were all 15 at their cap of 16 (a
  rotting fruit sprouts a seedling only below it), each full bush holding
  at most 4 fruit, adding one at a 25-second step half the time. Now the
  cap is 36, a full bush holds 6 (a young one 3), and a bud comes every
  step while there is room. Two Home copies side by side for 20 minutes
  (sampled each minute): with the change, very hungry creatures 26-31 at
  the end (33-55 without), average hunger about 90 (113), food lying
  about 166-175 (136-153), starvation deaths 0 (4).
- westlock.cos: the shelf in the west sea (rooms 134 and 135, x 500-840,
  floors at y 1096-1140 in open water) stands far above the seabed and
  the hatch's pickup point; creatures that sank onto it drowned there
  (three times in the notes, once more in a copy). A watcher (2 1 160)
  gives a creature 20 s there, then a current (2 1 161) carries it, not
  quite smoothly, west into open water, down through room 204 to the
  seabed and east to the low point, where the hatch takes it in. Moved
  by script, so no fall. Tested in a copy: a creature put on the shelf
  was carried down, taken in by the hatch and let out on the floor by
  the lift.

#### Quieter startup log: script patches and backward seeks

[`fd6eec4b`](https://github.com/DotairZee/zeetures/commit/fd6eec4bc88fbd2cec35ccd55c21c701d294bf77)

From the owner's Mac install notes, two kinds of harmless lines filled
the log at every start:

- "Script patch ... text not found (already patched, or another
  version)" for most patches: each built-in COB injected at load
  applies the patches quietly (so a script it just installed is
  patched), so by the final pass after the additions they were in
  already. The final pass now says nothing when the patch's
  replacement is already in the script, when the patch was applied
  earlier this load (Engine::patchesApplied, cleared at each world load;
  a later patch of the same script can change an earlier one's text
  again, as in 2 20 7 9, 2 18 3 9 and 2 18 8 9), or when the script is
  not in this world (an add-on not installed: 2 20 22 9). A patch that
  truly finds nothing is still reported.
- "WARNING: Backwards seek on file ..." (93 at a fresh start), upstream's
  developer hint, from reading COB files and the game's creatures2.exe:
  now only with OPENC2E_DEBUG_SEEKS.

Fresh folder with both packs: no seek lines, no "not found" lines, all
17 patches applied, 39 additions run, no script errors.

#### Breeds: scan a genome's appearance genes only up to "gend"

[`155d256b`](https://github.com/DotairZee/zeetures/commit/155d256b62ebfba410ad1dfe6a6b4b0de93f94ab)

The owner's Mac install logged "Breeds: genome layout not as expected
(6 vs 5 appearance genes); unchanged" five times around the breed-pack
import. editGenome cross-checks a raw byte scan for appearance genes
against the genome reader's, and scanned the whole 36001-byte block; C2
genomes keep leftovers past "gend", and in the game's gren.gen, the
Boney pack's bone.gen and five grendel genomes made from them those
leftovers hold one old appearance gene. The counts never matched, so
those genomes could not be changed: a grendel breed moved to another
slot on import kept pointing at its old one, and an egg for a grendel
breed with no genome of its own would hatch a standard grendel. (In the
logged cases nothing needed changing: Boney sits in its own slot.) The
scan now stops where the reader stopped (SpanReader::tell, just past
"gend"). Fresh folder, both packs imported, Hatchery open: no warnings,
and the per-gene byte check passes on every genome.

#### Made art: dump every frame's size; compare frames of different sizes

[`33313228`](https://github.com/DotairZee/zeetures/commit/33313228e279cb811813237e77825926128ca4f0)

The female ettin's head (a60a) has frames of several sizes (97 of 48x48,
the rest 45x48 to 56x52), which the dump's header (frame 0's size) and
art/compare_made.py (one stacked sheet) could not describe. The dump now
writes a second line with every frame's size, and compare_made.py reads
the frames one by one and takes the recipe's output from NAME.png or
NAME.s16 (Overrides pictures are .s16). With it: all 18 made pictures
(a60a and wfal included) identical to the owner's, and the 17 made
body-data files byte-identical (ettinhead's a60a.att, the 16 Golden
Desert .att files).

#### Made art: port ettinhead (the female ettin's head, a60a)

[`65dff8fc`](https://github.com/DotairZee/zeetures/commit/65dff8fcdeb05391907d3c3cf5cd9bedcb580b6e)

C++ port of art/ettinhead.py: a60a (120 frames, the female ettin's head
rebuilt from the game's male a20a.s16, replacing the game's own a60a) and
a60a.att (a byte copy of the game's Body Data/a20a.att).

Verified with OPENC2E_DUMP_MADE_ART against the Python recipe's output:
all 120 frames of a60a identical to its a60a.s16 as s16.py reads it (max
diff 0, every channel, transparent pixels included), and a60a.att
byte-identical.

#### Made art: port golden_desert_heads (Golden Desert head seating, 16 .att)

[`b6db9052`](https://github.com/DotairZee/zeetures/commit/b6db90524ab36b36e0a805a6da5325a76f75a8f8)

C++ port of art/golden_desert_heads.py: a{0,4}{0,1,3,5}k.att and
b{0,4}{0,1,3,5}k.att made from the game's own Golden Desert (k) and Hebe
(h) body data and head sprites, a and b of each sex and stage computed
once and cached.

Verified: built on Linux (Ninja, RelWithDebInfo) and ran with an empty
home and OPENC2E_DUMP_MADE_ART; all 16 dumped files are byte-identical
to the Python recipe's output in Overrides/Body Data.

#### Made art: body data made from the game's own

[`7f6e3f73`](https://github.com/DotairZee/zeetures/commit/7f6e3f730d268072fbf9395e848029547cf3bc40)

For the two appearance fixes the owner's Home has in its Overrides
folder (the female ettin's head, ettinhead.py; the Golden Desert norns'
head seating, golden_desert_heads.py), which rewrite body data as well
as pictures. MadeArt gains RegisterBodyData (a recipe for a .att file's
text, by file name) and bodyDataFile(), which writes the made text once
per run into the user data folder's "Zeetures made/Body Data" and is
asked by findFile for body data after the Overrides folder (a file of
the player's there still wins) and before the game's own. gameFile()
reads a file from the game's own folder only. OPENC2E_DUMP_MADE_ART
writes the made body data too. The two recipes' files are placeholders
for their ports.

#### Made art: build on Windows and 32-bit systems

[`9d9244b9`](https://github.com/DotairZee/zeetures/commit/9d9244b9b5629766195145987935a950664b639c)

The nightly's Windows, 32-bit Windows and 32-bit Linux builds failed:
treelift_top.cpp's PCG64 used unsigned \_\_int128 (not in MSVC, nor in
32-bit GCC), and cave_ramp.cpp used M_PI (not standard C++; MSVC lacks
it without \_USE_MATH_DEFINES). treelift_top now does its 128-bit
arithmetic in two 64-bit halves, as stream_bed.cpp does, and cave_ramp
has its own kPi. All 16 made pictures are still identical to the
owner's (max difference 0).

#### READMEs: all 39 world additions run on a fresh install

[`c928fdc2`](https://github.com/DotairZee/zeetures/commit/c928fdc25450d4f4f09a629891e9910bf6f27e9b)

With the from-scratch sprites shipped in zeetures-art and the pictures
cut from the game's own made by the engine (madeart/: cavepath,
cave_ramp, pillarfoot, stream_bed, treelift_top, tube_short,
volcano_passage, volcano_trunk, westlock's rock, each identical to the
Python recipes' output on the owner's machine), an empty user data
folder runs all 39 additions: none skipped, no script errors, no
missing sprites; the west sea hatch, the volcano passage and the
treehouse lift are there. The READMEs say so.

#### Made art: port volcano_passage (zvdr, zvpc, zvpr, zvps)

[`577e0653`](https://github.com/DotairZee/zeetures/commit/577e06538e32da20ce214238fb6d774d14332009)

C++ port of art/volcano_passage.py: the seam bands, the carved passage,
the ramp and the mirrored dor2 hatch door (5 frames). Includes numpy's
default_rng (SeedSequence + PCG64, Lemire integers, the shared 32-bit
half), the recipe's float32/float64 promotions, and PIL's GaussianBlur
(box-blur passes) and alpha_composite.

Verified with an empty home and OPENC2E_DUMP_MADE_ART against the owner's
pictures made by the Python recipe: all four the same size and frame
count, max channel difference 0.

#### Made art: port stream_bed (zsbd), volcano_trunk (zvtk) and westlock's rock (zwrk)

[`ce5bb707`](https://github.com/DotairZee/zeetures/commit/ce5bb707d1ff9e3590cfe80895b7e4fb8cc0e125)

C++ ports of art/stream_bed.py, art/volcano_trunk.py and the rock() picture
of art/westlock.py. stream_bed carries numpy's default_rng(7) (SeedSequence,
PCG64, Lemire-bounded integers) and its float32/float64 promotions;
volcano_trunk reimplements Pillow 10's polygon fill, fixed-point LANCZOS
resize and extended-box GaussianBlur.

Verified with OPENC2E_DUMP_MADE_ART (empty home) and art/compare_made.py
against the pictures the Python recipes made: all three the same size and
frame count, max channel difference 0. The ported RNG also matches numpy
for other seeds and ranges.

#### Made art: port treelift_top (ztla, ztlc) and tube_short (ztbf, ztde, zths)

[`88245555`](https://github.com/DotairZee/zeetures/commit/88245555f9024a75428925852345240ce5219fcb)

C++ ports of art/treelift_top.py and art/tube_short.py. ztla carries the
parts of art/flyingfox.py it draws with (Canvas, tube, lashing, noise,
cnoise), with numpy's default_rng (SeedSequence, PCG64), Pillow's bicubic
resize and alpha_composite reimplemented, and numpy 1.26's float32/float64
steps kept.

Verified with OPENC2E_DUMP_MADE_ART and art/compare_made.py against the
pictures the Python recipes made: all five are the same size and frame
count with max channel difference 0.

#### Made art: port cavepath (zajb, wfal) and cave_ramp (zcrp)

[`80c6bbb9`](https://github.com/DotairZee/zeetures/commit/80c6bbb94eb48578857dcd86da419182818fbe9f)

C++ ports of art/cavepath.py (zajb, the arch jamb; wfal, the waterfall
with only the water kept, replacing the game's own) and art/cave_ramp.py
(zcrp, the dock's planks sloping into the stream). cave_ramp reimplements
Pillow 10.2's LANCZOS resize, BICUBIC expand-rotate and RGBA/RGBa
conversions as its C code does them; each was checked bit-exact against
Pillow on random pictures.

Verified with the engine run on an empty home (OPENC2E_DUMP_MADE_ART)
against the pictures the Python recipes made: zajb 108x143x1, zcrp
300x120x1 and wfal 73x154x6 (the Overrides s16 read back) all match with
max channel difference 0.

#### Made art: pictures made from the game's own, in the engine

[`b386b3b8`](https://github.com/DotairZee/zeetures/commit/b386b3b80bba99a53eb389f61ebc47ea42516fe8)

Owner, 2026-11-15: the world additions are essential in every default
world. Seven of them need pictures cut from the game's own (Back.s16 and
sprites), which Zeetures cannot ship; the Python recipes in art/ made
them on the owner's machine only. madeart/ makes them in the engine, on
the player's machine, when a script first asks for one by name:

- MadeArt.h/.cpp: Rgba and Plane (float) pictures; backgroundRegion()
  and gameSprite() read the game's own Back.s16 and sprites
  (findGameImageFile: the game's Images folder only) exactly as
  art/background.py and art/s16.py do; recipes register by name
  (Register), made once and kept as RGBA creaturesImages.
  replacesGame lets a made picture stand in for a game sprite of the same
  name (wfal), unless the player has an Overrides/Images file for it.
- imageManager::getImage asks for a made picture when the player has no
  file of that name; Additions' art check counts what can be made.
- OPENC2E_DUMP_MADE_ART=DIR writes every made picture as NAME.rgba at
  startup; art/compare_made.py compares them with the recipes' PNGs.
- madeart/pillarfoot.cpp, the first port (zpft, art/pillarfoot.py):
  identical to the owner's zpft.png (max difference 0). The other
  recipes' files are placeholders for their ports.

#### Ship the world additions' from-scratch sprites

[`be44d4a8`](https://github.com/DotairZee/zeetures/commit/be44d4a86896740cf380d9cbae4783e12c74cda9)

Toward the same world on any machine: the 39 sprites the additions use
that the art recipes draw from scratch (no game pixels read: checked
recipe by recipe; lift.py only took positions and three rail colors
from a preview, westlock.py's doors and car are drawn, its rock is cut
from the background and stays out) are committed in art/sprites (4.8 MB,
marked binary in .gitattributes): bamboo walkways, bushes and planters,
sea plants and fruit, catwalk, plinth, sand ramp, pipes, ledges, the
lower deck, lifts and callers, the iris hatch, the lab treat, the flying
fox, the cave mouth, the memorial stone and tablet, and the west sea
airlock's doors and car. Four no script uses (zbrc, zfpd, zfpk, zfpm)
are left out.

They ship beside the program as "zeetures-art" (a post-build copy next
to the built program; installed into every download), not built in:
about 5 MB of arrays would strain the 32-bit Windows compiler.
findImageFile looks there last, for C2, after the player's and the
game's folders (so the player's own picture of a name wins; SDL's base
path finds the folder).

Fresh user folder: 32 of the 39 additions run, 7 skipped (their art is
cut from the game's own pictures: pillar, treehouse-lift,
volcano-passage, volcano-trunk, water-pocket, waterfall-path,
westlock), no script errors, no missing sprites. READMEs updated.

#### Start with all sounds on

[`75fc9ed3`](https://github.com/DotairZee/zeetures/commit/75fc9ed38ea0ad1c0f2c935157732d4c9697cdcf)

Owner, 2026-11-15: "for future nightlies: I want new compiles to start
with all sounds on." Music, world sounds and creature speech began muted
(2a793d9b); they now begin on unless sound is disabled on the command
line (-s). The player's own File &gt; Sound choices, kept in settings.txt,
still win at startup (LoadViewSettings), so a player who turned them off
keeps them off.

#### READMEs: playing on a Mac, from a real install on an M4 MacBook Pro

[`862c180b`](https://github.com/DotairZee/zeetures/commit/862c180bff3a1ec6e2773e0a5e9c71458bbdbbea)

From the owner's notes of a first install on a Mac with no developer
tools (MacBook Pro M4, macOS 15.7.3, commit 6c36fe1a), where Zeetures
built and ran natively (Metal, CoreAudio), made Home from Eden.sfc and
fetched and imported both breed packs:

- The game's files on a Mac: Steam for Mac won't install the game (it
  is Windows-only there), but its console downloads them
  (download_depot 1818340 1818341) into
  ~/Library/Application Support/Steam/Steam.AppBundle/Steam/Contents/
  MacOS/steamapps/content/app_1818340/depot_1818341; copy them to a
  permanent place such as ~/Games/Creatures The Albian Years. A new
  "On a Mac" section says so, and that Wine, CrossOver and Steam Play
  are not needed: only the game's data comes from Steam.
- The game folder is the one with Images, Sounds and Eden.sfc (C2 has
  no Bootstrap folder; that is C3/DS).
- The Intel Mac download (macOS 10.15 or later) in the table and the
  download's own README; the "not Intel Macs" note gone.
- Building on macOS: the command line tools' hidden-installer gotcha and
  its two fixes; CMake from PyPI in a venv with the tools' Python 3.9
  (Homebrew optional); --target openc2e; the harmless
  -Winconsistent-missing-override warnings; the flags for an Intel build
  on Apple Silicon; Python 3.9 stated as enough.

#### Nightly: a build for Intel Macs

[`1d6ab4e8`](https://github.com/DotairZee/zeetures/commit/1d6ab4e82db8fd2f8a27c1229ece757d22cfa174)

Owner, 2026-11-15: "can you build a version for Intel Macs?" A new
macos-intel job cross-compiles on the Apple Silicon runner
(CMAKE_OSX_ARCHITECTURES=x86_64; SDL3 from externals, the SDK's zlib)
for macOS 10.15 and later (CMAKE_OSX_DEPLOYMENT_TARGET 10.15, the first
with std::filesystem), checks with lipo that the engine is x86_64, and
releases zeetures-nightly-macos-x86_64.zip. Allowed to fail, like the
other new builds.

#### READMEs: user folders on every system, the art not yet shipped, Apple Silicon only

[`64283e64`](https://github.com/DotairZee/zeetures/commit/64283e6452c797191c06bd1a201a24abe32a347d)

The READMEs said the world additions (walkways, lifts, airlocks, plants)
are applied to every world; on a fresh install the 17 that draw new
pictures are skipped until their art ships (de8fd955), so they now say
so, in the Status's "Not done yet" too. The user folder is given for
each system (PathResolver's storageDirectory: ~/.openc2e/Creatures 2,
Documents\My Games\Creatures 2, ~/Documents/openc2e Data/Creatures 2),
and the macOS download is marked Apple Silicon only (M1 or later).

#### Fetch and import the Boney Grendels and Frog Norns V3 packs

[`a907fe1a`](https://github.com/DotairZee/zeetures/commit/a907fe1a2dfe2baa291a2c73982f5e17ca408aaa)

Owner, 2026-11-15: "do whatever is needed to make it so Zeetures ... gets
those two breeds", with their Eem Foo archive pages. Zeetures does not
carry or host them; on the player's machine, the first time a C2 world
runs without them, it fetches each from the archive that already hosts
it and imports it as Home has it.

- BreedPacks.cpp (BreedPacksTick, from DrawC2Kits once the world has run
  20 ticks): for each pack whose genome (bone.gen, frog.gen) is not in the
  user data folder's Genetics, the system's curl (curl.exe on Windows)
  downloads it on a thread into the user data folder's Downloads, with the
  pack's page as the referrer (the archive answers a bare request with the
  page); a file that does not start as expected (MZ, PK) is dropped. Then
  it is imported on the main thread and a line shows briefly above the
  status bar. Off with "breed-packs off" in settings.txt or
  OPENC2E_NO_BREED_PACKS; OPENC2E_BREED_PACK_DIR takes the files from a
  folder instead.
- BreedersKit: ImportBreedPackQuietly imports a pack without the review
  (a plan the player has open is kept), naming its breeds; with
  inPlaceOfGame, each breed goes in its own slot in the Overrides folder,
  in place of the game's (ImportGroup::inPlaceOfGame): the Frog pack in
  C2's frog norn slot l, as the owner set it up by hand on 2026-11-10.
- Engine: built-in objects with no COB at load (missingBuiltinCobs) are
  injected by injectMissingBuiltinCobs once their COB arrives: Bony
  Grendel Update, from the Boney pack, in the same session.
- READMEs say what is fetched, from where, and how to turn it off.

Checked on an empty user data folder with the network: both downloaded
and imported ("0|l|Frog|2538-FrogNornV3_1.zip|frog.gen",
"1|c|Boney|Boney Grendels.exe|bone.gen", as in Home's breeds.txt), the
Frog's 281 files and Boney's 62 identical in name and content to Home's,
Bony Grendel Update injected. Home already has both, so nothing is
fetched there.

#### Skip a built-in addition whose art is not installed

[`de8fd955`](https://github.com/DotairZee/zeetures/commit/de8fd9551dc9f787ac23a69d2899f609e925c0fd)

The world additions are built into the program, but their sprites are
not yet (the from-scratch art is to ship next, and the art made from the
player's own game files needs the recipes ported). On a fresh install the
17 additions that draw our sprites stopped at the first missing one,
after some had already changed rooms (a walkway's floor with no walkway
drawn). Now a built-in addition is checked first: any of our sprites it
names (four letters starting with z, outside comment lines) with no
.s16, .c16 or .png installed, and it is skipped whole, logged as
"World addition X (built in) skipped: its art (zabc) is not installed".
The player's own copies run as before. Fresh-folder check: 17 skipped,
22 run, no script errors.

#### Memorial stone: only creatures press it, and it eases their loneliness

[`702795bb`](https://github.com/DotairZee/zeetures/commit/702795bb9c7166b5b539a8f5fb523244dd8d0e1f)

Owner, 2026-11-15: "when a creature activates the Easter egg, I want
them to have a positive feeling--decrease in loneliness" and "the hand
cannot activate it--only creatures can".

- memorial.cos: the stone is ATTR 0 (was 16 + 4): creatures see it, and
  the hand cannot activate it (Agent::handleClick needs ATTR 4; a
  creature's activation does not). Its activate script acts only for a
  creature (FROM's family 4): STIM WRIT with 80 of Loneliness Decrease
  (chemical 42), then the memorial window as before (60 s at normal
  speed, or until clicked). Any other press is ignored, except the
  status bar's "Together" notice, which sets the stone's OV09 to 1
  before queueing its press, so the window still opens from the notice.
- OPENC2E_TEST_STONE=1: the first living creature presses the stone,
  with chemical 42 and its loneliness drive printed before, 5 ticks
  after and 100 ticks after. In a Home copy: chemical 42 0 -&gt; 51,
  loneliness 14 -&gt; 2 (still 2 later); the window opened.

#### Memorial stone: darker, weathered, modeled like a rendered sprite

[`e5e7db4a`](https://github.com/DotairZee/zeetures/commit/e5e7db4adaa79c82e917babc84a0be6701aacc68)

The owner asked for more texture and volume, chose the older, moss-grown
look from four mock-ups (gray headstone, moss-grown, sandstone gabled,
rough standing stone; all kept in LOOKS), then asked for it darker and
more 3D-rendered. The stone is now drawn as a lit height field: a
rounded 4.6 px bevel and a domed face, smooth large-scale bumps, pits,
16 chips in the rim and a hairline crack. It is lit by the wall lamp
from the upper left (Lambert plus a damp specular sheen, exponent 30), a
cool fill light from the right and cavity shading in the crevices, the
carving and the plinth joint. Albedo is a dark warm gray (88, 90, 84)
with dirt streaks and lichen, and moss grows in shaded cushions over the
plinth, up the stone's foot and on its shaded side. The plinth's top
face catches the light. The lamp casts the stone's shadow onto the
niche wall behind it, so the sprite is 52 px wide instead of 46 (left
edge and placement unchanged).

#### Build the world additions into the program; Home's settings as defaults

[`d632758a`](https://github.com/DotairZee/zeetures/commit/d632758af3895f973a7939029a756fefe4c36795)

Toward the same world on any machine (owner, 2026-11-15: install the
game, install Zeetures, get the same default world as Home's):

- additions/ (every .cos and the text files) is embedded at build time
  (scripts/embed_files.py gains --func, and a names function, so a
  second set sits beside data/'s art: EmbeddedAddition). The new
  Additions module answers for a file: the player's own copy (the
  Additions folder; pheromones.txt and breed-names.txt in the user data
  folder itself) wins, else the built-in one, which applies to C2 only.
  An empty file of the same name switches a built-in one off.
- The world-addition runner runs the merged set, sorted by name, logging
  "(built in)" for embedded ones (World::executeInitScriptText runs a
  script held in memory), then the world folder's own as before.
  script-patches.txt, announcements.txt, builtin-cobs.txt,
  hidden-cobs.txt, pheromones.txt and breed-names.txt are read the same
  way. Before the Agent Injector edits builtin-cobs.txt, the built-in
  copy is written to the player's Additions folder.
- Defaults are Home's launch settings: --mind homeostat unless --mind
  original (openc2e's original brain code) or another mind is given;
  per-stage scaling, part growth and the senile tint on unless
  OPENC2E_STAGE_SCALE=0, OPENC2E_STAGE_GROWTH=off or OPENC2E_SENILE_TINT=0;
  population limit 150 until the player sets one. READMEs updated.

Checked with an empty user data folder: all 39 additions run built in,
the script patches apply, 9 of 10 built-in objects are injected (Bony
Grendel Update needs the Boney pack), pheromones load, the mind is the
homeostat. The 17 additions that draw our sprites still fail there
until the art ships. Home's installed copies are identical to the
built-in ones, so Home runs as before.

#### Changelog: version 0.7.0 in the version list

[`e0829e03`](https://github.com/DotairZee/zeetures/commit/e0829e03a89b843d39f3e3cec06476de68fd80ca)

## Version 0.7.0

The memorial stone, an Easter egg for the four who grew old together, and the "Together" notice when four or more that hatched together die of old age together; the world's additions and art recipes in the repository; the waterwheel and seabed airlock rides a touch less smooth; this changelog.

### 15 November 2026

#### Version 0.7.0

[`e97a386e`](https://github.com/DotairZee/zeetures/commit/e97a386eabe31bf786d8c9b4522c483ab5e3dace)

#### The memorial stone: four who grew old together

[`e0671006`](https://github.com/DotairZee/zeetures/commit/e0671006d646448b358e03c5122a10d4236f613e)

An Easter egg (owner, 2026-11-15): four norns that hatched within
seconds of each other died of old age at the same moment. A small
headstone (2 3 254, sprite zmem from art/memorial.py) stands in the dark
niche of the stone arcade west of the incubator (x 4996-5042, floor
1172), where a 3-minute sample of Home saw 4 creatures against hundreds
by the incubator. It is behind creatures and unseen by them (ATTR 20).
Pressing it opens a sandstone tablet window (CBUB 2 1 159, sprite zmtb)
with the owner's words, announcement 40; a click or 60 s closes it.

When it happens again in a world, it is remembered:
historyManager::checkTogether runs after each old-age death and, when
four or more with known ages died of old age within 300 ticks of each
other and were born within 300 ticks of each other, logs a new
lifeLogEntry::together (names in cause, count in stage) and announces it
as a purple "Together: 4 norns" notice that leads to the stone and opens
its window. The tablet then adds announcement 41 with the latest group's
names and date. OPENC2E_TEST_TOGETHER=1 makes four living creatures die
of old age with one birth tick, to test it.

Announcement text parts now draw their letters on a see-through
background (palette 0 of the charset was opaque black, unseen on the
game's black text areas but solid bars on a light panel).

#### Add the world additions and the art recipes

[`7b6c9157`](https://github.com/DotairZee/zeetures/commit/7b6c91572b9e209ba39785eac5edbb71669ee1af)

Until now these lived outside the repository, in the project's working
folder. additions/ holds the scripts openc2e runs from the player's
Additions folder after a world loads (each .cos opens with what it
changes and why: map and room fixes, walkways, lifts, airlocks, the
waterwheel stream, seasonal bushes, boat catches and the rest), and the
text files the engine reads with them: announcements.txt,
script-patches.txt, builtin-cobs.txt, hidden-cobs.txt, pheromones.txt
(copied to the user data folder itself) and breed-names.txt. art/ holds
the Python recipes that draw the additions' sprites: most from scratch,
some from the player's own installed game (bring-your-own-data), so the
sprites themselves are not committed.

They are copied as they stand on 2026-11-15; their history before this
is in the changelog's engine commits that mention them. Not yet built
into the program or the downloads.

#### A changelog of every change since the fork

[`82f0be86`](https://github.com/DotairZee/zeetures/commit/82f0be868b5dbc6982d8bd578366bf2ef96bb6ff)

.github/changelog.py writes CHANGELOG.md from the commit history since
openc2e's dfcd46f2: every commit's full message, grouped by version
(boundaries and summaries from the version log) and by day, newest
first, each linked to its commit; commits that change only CHANGELOG.md
are left out, so updating the file does not make it stale. --check
exits 1 when the file is stale; a new CI job runs it. The README and
the nightly release notes link to it.

## Version 0.6.1

The first version published as Zeetures, with nightly builds for Linux, Windows and macOS. Notices kept through restarts, the hand's hot spot at the fingertip, scent dose bands and habituation, a narrower notice list, 32-bit Windows and Linux builds, and the README brought up to date.

### 15 November 2026

#### Version 0.6.1

[`2c1d95d0`](https://github.com/DotairZee/zeetures/commit/2c1d95d0ff872b6a5dfd1e1e574fe93a377e47ba)

#### Use 64-bit file offsets on 32-bit Linux

[`3edb6a6c`](https://github.com/DotairZee/zeetures/commit/3edb6a6c9491139c8e74971e84b8ab1b2494db7a)

FileReader asserts that off_t is 64 bits, which on i386 glibc holds only
with \_FILE_OFFSET_BITS=64 (upstream had a TODO for it). Define it for
every non-Windows build; on 64-bit systems it changes nothing.

#### README: Zeetures status, 32-bit builds, options; drop upstream leftovers

[`e06708c6`](https://github.com/DotairZee/zeetures/commit/e06708c689b7ba1d431089cdf4567903c539a19d)

The README still carried upstream openc2e's Status (C2 creatures "don't
work at all", no saving) and its SDL2-era Building, Compiling and
Running sections, which repeated and contradicted the Zeetures build
steps. They are replaced by a Zeetures status (what works, what is not
done yet), 32-bit download rows and build steps, the command-line
options Zeetures adds, and a "Built on openc2e" credit with the fork
point and license.

#### Build for 32-bit Windows and Linux

[`1db74f17`](https://github.com/DotairZee/zeetures/commit/1db74f17e8ee1852b81dabbd327bf4f701604f5c)

FileReader and SpanReader asserted at compile time that size_t is 64
bits; they now check sizes and offsets at run time, so 32-bit builds
compile and refuse files too big to address. breathTicks is constexpr
(macOS Debug could not link the reference std::min took to it). CI
builds win32 and linux-i386 (multilib, bundled SDL, X11 only), and the
nightly adds windows-x86 and linux-i386 downloads.

#### Size the older-notices list to its contents

[`4e17493b`](https://github.com/DotairZee/zeetures/commit/4e17493b637a0ad7be3dfe2cd9004c6f4f33bd77)

The "+N" list was a fixed 420 px wide, about twice what its notices
need. It is now as wide as its longest notice (plus the scroll bar when
it scrolls), and it follows the button each frame. OPENC2E_TEST_NOTICES
takes a count, and OPENC2E_TEST_MORE_NOTICES keeps the list open for a
snapshot.

#### Scents: dose bands (here&gt;X, here&lt;X) and habituation (habituate T)

[`5ba5c75a`](https://github.com/DotairZee/zeetures/commit/5ba5c75a9c922c7661f355111d2f230e31b92a35)

The last two of the owner's pheromone ideas (pheromones-design.md):

- Dose bands: smell rules may say "when here&gt;X" or "here&lt;X", the
  rule's scent where the creature is (0-1), so one scent can draw a
  creature when faint and drive it off when strong.
- Habituation: "scent NAME habituate T": a creature in the scent gets
  used to it over about T ticks (a level per creature and scent that
  follows the scent where it is), and what it feels is the scent less
  that level: follow and avoid urges and primer chemicals weaken by as
  much; away from it (or giving it off itself) it unlearns at the same
  pace. Cleared at world load.

Neither is used by Home's rules yet. Tests (Home copies; rules from
OPENC2E_PHEROMONES): care scent follow when here&lt;0.3 / avoid when
here&gt;0.3: 237 follows all where faint, 55 avoids all where strong (one
on the 0.30 boundary); care scent with habituate 600: the pull per unit
of scent 0.25-0.33 without, 0.05-0.08 with.

#### "A labour of love by an old fan"; the nightly release step safe against a push

[`c244cb29`](https://github.com/DotairZee/zeetures/commit/c244cb2927fed70c109c8576eccb1abea586bbe6)

Owner, 2026-11-15: note in the About box and wherever else relevant that
this build is a labor of love by an old fan. About Zeetures: under "by
DotairZee", "A labour of love by an old fan." (British, as the game's
text). The README's fork note, the download's README, the release notes
and the repository's description say it too.

Nightly: the release step first checks the branch is still at the
commit built. GitHub refuses the workflow's token a tag on an older
commit whose workflow files differ (403 "Resource not accessible by
integration"), and a push during the build (2026-11-15) left no release
at all, the old one already deleted. Now, if the branch moved on, the
release is left as it was for the next run.

#### README: what Zeetures needs, how to get it and build it; macOS nightly

[`34269e22`](https://github.com/DotairZee/zeetures/commit/34269e22b413cc383c50a0bcfe1612c6dd166170)

Owner, 2026-11-15: a section on GitHub saying what is needed (Creatures
2) and how to compile for each system, with a build for each.

- README: Zeetures is the page's title. What you need (your own Creatures
  2; no game data included), downloads by system from the nightly
  release, how to run it on Linux, Windows and macOS (Steam paths, the
  unsigned macOS download), and how to build it on each, current for SDL3
  (upstream's own section below still says SDL2).
- Nightly: a macOS job (Apple Silicon, macos-latest; allowed to fail, as
  Windows is: not built on macOS before), zeetures-nightly-macos-arm64.zip.
- The download's README: macOS lines.

#### Name the fork Zeetures, saying plainly it is a fork of openc2e and from where

[`d8a793eb`](https://github.com/DotairZee/zeetures/commit/d8a793ebd68306ea951a80fc8162907aa0ae38c9)

Owner, 2026-11-15: "let's call the game Zeetures"; the web link should go
to the GitHub page; say explicitly that it is a fork of openc2e, and
which version it forks from.

- About box "About Zeetures": under the name, "A fork of openc2e (from
  its commit dfcd46f2, 7 October 2026)"; the notes below name openc2e and
  its developers. Help &gt; "Zeetures on GitHub" (the repository) beside
  "openc2e on the Web".
- The C2 window title: "&lt;world&gt; - Zeetures".
- README, the nightly download's README, and the release notes say
  Zeetures is a fan fork of openc2e from that commit; the downloads are
  zeetures-nightly-\*. The program is still called openc2e (the
  launcher and desktop entries use that name), and the user data folder
  is unchanged.
- The fork point's date is 7 October 2026, when dfcd46f2 landed in
  openc2e (written on the 6th, which the About box said before).

#### Status bar notices kept with the world through relaunches

[`cea46d7e`](https://github.com/DotairZee/zeetures/commit/cea46d7e0806967691a92874609a775c1a26cee7)

Owner, 2026-11-15: the notices (births, deaths, eggs laid, pregnancies,
unconscious...) were lost at every relaunch: the bar kept them in a
static list of the interface's. They are now the world's own
(historyManager::shownNotices), saved in state.txt as "notice" lines in
the life log's format (with life-egg and life-cause after them), and
restored at load; any not yet put on the bar are saved too. A saved
moniker of "-" (none) now reads back as none.

Checked: a Home copy saved after 600 ticks wrote its one notice
("Unconscious: Norn 0843"); loading that copy showed it on the bar at
once.

#### C2 hand: its hot spot at the fingertip

[`b5893bc2`](https://github.com/DotairZee/zeetures/commit/b5893bc2f29ebe084ddeffa671434538abdd5146)

Owner, 2026-11-15: what the hand picks up on (pet, spank, clicks) should
be the tip of the pointing finger, and it "seems almost like it's not".
The hot spot was hard-coded at (2, 2) of the hand picture, which is
empty space up and left of the fingertip (syst.s16 frame 0: the nail at
about 3-5, 4-7), so the hand touched things a few pixels before the
finger reached them. C2 now uses (4, 5), on the nail; the drawn
fingertip sits on the mouse position. C1 unchanged.

Checked at 300% zoom: the hand left over a head's middle puts the
fingertip on the head, with the pet sign.

#### Nightly: strip the Linux programs (the engine was 138 MB with debug information)

[`f8598abb`](https://github.com/DotairZee/zeetures/commit/f8598abb421bbcce9084cab99354f896a2dbbb32)

#### Windows build: file times without to_sys/clock_cast; zlib linked to the core

[`cff8aed2`](https://github.com/DotairZee/zeetures/commit/cff8aed27b095beb14fdc89fc657604060358b82)

The first nightly's Windows build (2026-11-15) stopped on:
- CobManager.cpp: MSVC's std::chrono::file_clock has no to_sys. File
  times are now turned into system times by the two clocks' "now"
  (also in the Hatchery, whose clock_cast Apple's libc++ lacks).
- BreedersKit.cpp: zlib.h not found. On Windows zlib is built from
  externals and its headers come with linking ZLIB::ZLIB, which only the
  file-formats library did; openc2e_core now links it too.

#### Nightly releases for the fork; a fork notice in the README

[`cb34520b`](https://github.com/DotairZee/zeetures/commit/cb34520b32eaefc60bf4aeb59849ccf106896b9d)

Owner, 2026-11-15: publish the fork on GitHub with a nightly release
whenever there have been changes.

- .github/workflows/nightly.yml: at 09:00 UTC (or by hand), if anything
  was committed since the "nightly" tag, build Linux x86-64 (bundled
  SDL3, no AddressSanitizer) and Windows x64 (allowed to fail: not built
  on Windows before), and replace the "nightly" pre-release with the new
  downloads and the commits since the last one.
- .github/nightly/README.txt, in each download: what it is, that it
  holds no game data and needs the player's own Creatures 2, how to run
  it, the license.
- README.md: a short note at the top on what this fork is.

## Version 0.6.0

Signature scents; the hand takes things from creatures and keeps its mode; fertility switches; sunken and stranded creatures and objects kept out of the rock; the speed spin box; the portability audit for Windows and macOS; shut doors stop thrown creatures; a light old-age gray for breeds without old-age pictures; grown creatures tend the young and the senile; scents on the Ecology Kit's map; the "+N" notice list; pinch zoom; the hand pets on the head, spanks at the hips and pushes on the body.

### 15 November 2026

#### Hand on creatures: spank round the hips, legs push, signs at every zoom

[`ac33de2b`](https://github.com/DotairZee/zeetures/commit/ac33de2b98256b6dcceb3fd264a880eae4369daa)

Owner, 2026-11-15, after trying the zones: the whole body spanked was too
much ("just a radius around the bottom half"), legs should spank and
push too, and neither the pet nor the spank sign showed on the hand at
any zoom but 100%.

- Spank: on the body or a leg, within a circle round the middle of the
  two hip joints, its radius half the body picture's height (the bottom
  half). The push hand pushes on the body and the legs (thighs, shins,
  feet): SkeletalCreature::bodyOrLeg.
- Zoomed drawing drew only the hand's own picture, not what it shows
  with it (the angel, the devil, the push arrows); now the overlay too.
- World::partAt finds C1/C2 creatures by their drawn pictures
  (partUnder), not their box: the box is the 50 px physics width, so a
  head facing out past it was missed and the hand found whatever was
  behind (the pet sign came and went, about half the ticks, in a test by
  a machine); its see-through corners caught the hand where no creature
  is drawn. Pick up, clicks and the hover signs all go through it.
- Test aids: OPENC2E_TEST_ZONE_CLICK gains ",hover" (the hand kept over
  the zone, the view on the creature, every tick) and ",@X,Y" (the free
  creature nearest a point); OPENC2E_WATCH_HAND_SIGN traces the hand's
  sign each tick.

Checked: at 150% the angel shows over a head and the devil over the
bottom (screenshots); with the new hit test the sign held on 85 of 86
ticks (was about half); clicks: head pets, a thigh by the hips spanks,
the push hand on a foot shoves, on the head does not.

#### C1/C2 hand on a creature: pet on the head, spank at the hips, push on the body

[`126a355f`](https://github.com/DotairZee/zeetures/commit/126a355f272c2ba58a21d014995e23fca6edbee3)

Owner, 2026-11-15: petting when the hand is over the head's pictures,
spanking over the bottom within a few pixels of where the thighs meet the
body, picking up from any part of the creature, pushing anywhere on the
body. Was: the top half of the creature's box tickled, the bottom half
slapped, and the push hand pushed anywhere on the box.

- SkeletalCreature::partUnder: the body part whose picture is drawn at a
  point (front to back as drawn, frames as rendered: expression, blink,
  pregnancy), or -1 where the creature is see-through.
- handleClick (C1/C2): head (and ears, hair) -&gt; tickle (pet, the angel);
  within 6 px (4-8 with the creature's growth) of a body hip joint (body
  attachments 1 and 2), on the body, a thigh or the tail root -&gt; slap
  (spank, the devil); elsewhere nothing (the click still selects). The
  hover angel/devil follow the same rule.
- The push hand shoves, and shows its arrows, only over the body's
  picture. Pick up (right click) is unchanged: anywhere on the creature.
- OPENC2E_TEST_CREATURE_ZONES=N prints the selected creature's zones;
  OPENC2E_TEST_ZONE_CLICK="N,Z\[,push\]" clicks the middle of zone Z.
  Checked in Home copies: head -&gt; pet, hips -&gt; spank (on a thigh), push on
  the body shoved (hand pose 10), push on the head did nothing.

#### Pinch zoom: slower (a step per half again of spread, one per quarter second)

[`3272464e`](https://github.com/DotairZee/zeetures/commit/3272464e665a6f54162759ad21f8f1e055b438f6)

Owner, 2026-11-15: pinch zoom "moves way too fast". A zoom step now takes
a spread of 1.5x (a squeeze to two thirds), was 1.2x; at most one step
each 250 ms, and what was pinched past a step is let go (no run of steps
from one long pinch). OPENC2E_PINCH_STEP=s sets the spread for a step.
Test (12 updates of 1.1 per level): about 10 updates per step, was 2.

#### Pinch to zoom on a touchpad

[`5b774c18`](https://github.com/DotairZee/zeetures/commit/5b774c18cff95c4fe6edc7d09d51d4e2564ff9b2)

Owner, 2026-11-15. SDL 3.4's pinch updates (SDL_EVENT_PINCH_UPDATE, a
scale change, no position) become a backend eventpinch; C1/C2 zoom a step
(the zoom levels, as Ctrl+wheel) for each 20% or so of spread or squeeze,
about the hand, which sits where the fingers are on a touchpad. Pinches
over the interface (toolbar, kits) are left to it, as wheel events are.
OPENC2E_TEST_PINCH="N,scale,count" sends pinch updates for a check: 12 at
1.1 stepped 50% -&gt; 75 -&gt; 100 -&gt; 150 -&gt; 200 -&gt; 300; 12 at 0.9 from 200%
stepped down to 50%. Whether the desktop delivers pinches (Wayland
pointer gestures; X11 XInput 2.4) is for the owner to try.

#### C2 clicks: an object left "in use" with nothing running goes back to idle

[`6b0b6aae`](https://github.com/DotairZee/zeetures/commit/6b0b6aae3e9dea02bc3324d459cd145b921c9cfd)

Owner, 2026-11-15: the language computer by the submarine (2 9 28, the
laptop) answered the hand only at "a very specific place". It was saved
at ACTV 1: the engine sets ACTV on an activate, and its own script sets
it back at the end, so a script cut short (a pickup during it, or a save)
leaves it there. Its click table (clac 0 -1 -1) has no action at ACTV 1,
so every click did nothing.

Agent::handleClick (C1/C2): when the state has no click action, the
object has one when idle (clac\[0\]) and no script is running, it is put
back to idle and the click goes on (logged).

Also OPENC2E_TEST_HITGRID="N,x0,y0,x1,y1,step": a map of what a click and
a pickup land on around a place. Checked in Home copies: clicks on the
laptop's box now open it and it speaks (two points); the whole 68x76 box
answers both the click and the pickup.

#### Status bar: "+N" opens a scrollable list of the older notices

[`3f00e656`](https://github.com/DotairZee/zeetures/commit/3f00e6561f022e740cde4c1138124f0881a5e83d)

Owner, 2026-11-15: clicking the "+N" overflow made the notices all
disappear (it cleared them and opened the life log). Now it opens a list
above the button of every notice not shown on the bar, newest first, in
their colours, scrolling past about 300 px; each works as on the bar
(click to go to it, which clears it; right-click to dismiss it), with
"Open the life log" and "Clear these" under the list. Right-click on
"+N" still offers Clear all. OPENC2E_TEST_MORE_NOTICES opens the list
once for a snapshot.

#### Ecology Kit: scents on the map; the map page laid out in one row over a wider map

[`4fe36bff`](https://github.com/DotairZee/zeetures/commit/4fe36bff1a4c5877f9dcd135009e555015313496)

Owner, 2026-11-15: the scent overlay belongs in the Ecology Kit, not the
View menu; then "a ton of black space" in the map tab, and the map should
match the tab's width.

- Map page: a Show dropdown (the world's measures, then a Scents group
  from pheromones.txt) with Creatures, Food and "Tint the world too" in
  one row above the map. A scent shades the map from none to the
  strongest room (0-100); a room's tooltip lists the scents it holds.
  "Tint the world too" drives World::showScent. The Monitor page keeps
  its radio list of the world's measures (no scents: it records only
  those).
- The map is as wide as the tab, about 880 px (the kit's window fits
  what it holds, so the map sets the width), keeping its shape; the
  column of choices beside it had left the space under the map empty.
- The legend's middle and end labels were placed by SameLine offsets,
  which count from the window's left, so they sat past the colour bar;
  now placed by cursor position.
- View &gt; Scents removed. OPENC2E_ECO_SCENT=name picks a scent (and tints
  the world) at the start, for screenshots.

#### View &gt; Scents: rooms tinted by a chosen scent; see-through lines blend

[`69aa336f`](https://github.com/DotairZee/zeetures/commit/69aa336fcdb88cce7bd5a5d19c6ad10f79924ec7)

Owner's idea (pheromones-design.md, "seeing scents"): an overlay to watch
trails form. View &gt; Scents lists the scents in pheromones.txt (Off by
default); the chosen one tints each room in view by how much of it the
room holds, scaled to the strongest room in view (a scent spread wide
tinted everything alike), rooms under a tenth of that left clear.
OPENC2E_SHOW_SCENT=name starts with it on (for screenshots).

SDL draws lines without blending unless asked, so every line's alpha was
ignored: the tint first covered the view solidly, and Show Map's room
outlines (alpha 0xcc) were never see-through either. Lines with alpha
under 255 now blend.

OPENC2E_WATCH_SCENTS also counts rooms at half strength or more and at
the cap (Home: the care scent in 204 rooms, 19 at half or more, 6 at the
cap; the mate scents about 140 at half or more, about 30 at the cap).

#### Homeostat mind: tending (tickle and bring) for care scents; one trail per scent

[`149172d5`](https://github.com/DotairZee/zeetures/commit/149172d5156aa282c223ea468adaa126a2714567)

Owner's idea (pheromones-design.md): the young and the senile give off a
care scent, and grown creatures of their kind go to them, then tickle them
and bring them food or a toy (owner, 2026-11-15: babies, children and the
senile; tickle and bring food or toys).

- pheromones.txt: "WHO smell SCENT follow \[for DRIVE\] \[WEIGHT\] tend". A
  follow without a drive, marked tend, shrinks as the creature's most
  pressing need grows (times (1 - most pressing)^2).
- Mind (addTend): with one of its own kind at baby, child or senile stage
  in sight (350 px, reachable), it tickles it (activate 1 on that
  creature; not the same one again for 900 ticks) and brings it something:
  holding food, fruit or a toy, it walks it over and drops it within 60 px
  (not again for 1800 ticks); empty-handed, it picks one up within 250 px.
- Perception: what the creature is carrying (its category), and whether a
  seen object is carryable (C2 attr 1). Fruit still on its plant is not
  (bring pickups that left the hands empty repeated ~70 times a run); a
  pickup to bring that leaves the hands empty is not tried again for that
  category for 600 ticks.
- One trail per scent: the single shared trail was taken over by any
  scent's step that was only offered, so the care rules cut mates'
  following by about a fifth (per 1000 choices 21.5 without care rules,
  16-17 with). Now 28.6 without, 28.0 and 30.9 with (calm 0.3, calm 1).
- OPENC2E_PHEROMONES=FILE reads another rules file (test copies);
  OPENC2E_WATCH_TEND prints tickles and brings.

Tests (Home copies, 5 minutes, 3 runs each): calm strength 1: 71-98
tickles, 5-10 things brought; meals 2462-2586 against 2626-2668 with no
care rules. Weighed as loneliness instead: no tending in Home, where no
creature is lonely (loneliness 0 in all 153).

#### C2: a light old-age grey for breeds with no old-age pictures

[`979748cf`](https://github.com/DotairZee/zeetures/commit/979748cf718d4507ecfb830847a1d43c86480aa9)

An old creature (stage 5) whose breed has no old-age pictures of its own
(the C1 norns Brown Mouse, White Haired Pixie, Horse; Kai) showed its
younger pictures, scaled by the per-part growth, with nothing of age about
it until senility greyed it. Now its pictures are muted toward grey by
0.25 and lifted by 0.04, a lighter version of the senile tint (owner,
2026-11-15: scaling and colouring for the C1 norns, as long as sprites
added later do not clash).

Decided per creature by the body's pictures (part 0): when a breed has its
own old-age body set, the body is found at that age and the grey is off,
so an artist's old-age set takes over by itself. OPENC2E_OLD_TINT=g,l
tunes it; 0 turns it off.

Checked with lineups (youth, adult, old, senile) in Home copies: only the
old creature changes for Brown Mouse, White Haired Pixie and Horse (both
sexes shown; saturation about 83 -&gt; 69); Hebe, which has old-age
pictures, is unchanged.

#### C2: a shut door stops a free creature however far it was thrown

[`23ae3b39`](https://github.com/DotairZee/zeetures/commit/23ae3b39fa227ff66413ef1d53bf11a6d4d9c190)

snapDownFoot treated a foot that ended in a room it may not enter as a
wall only within 150 px of where it last stood; past that, the retry took
it for a script's move and accepted the room. A shove (the hand's push, a
hit) or a fling threw a norn more than that against the airlock's shut
land door (rooms 155/154, perm 0) and it ended in the chamber, or past
both doors in the sea column (owner, 2026-11-08: "walked through the
airlock's closed land door").

Now a free creature (not carried, riding, or held or moved by a script in
the last 5 ticks) meets a shut door between two rooms as a wall at any
distance. OPENC2E_NO_SHUT_DOOR_WALL turns it off (for comparisons).
OPENC2E_WATCH_ROOMS lines now give the door's permeability.

Test (Home copies): a norn put by the door and flung west with VELO -45
eight times: without the rule 3 times it went through (to x 6620, 6577,
and 6156 in the sea column); with it, never. Five-minute copies, two
each: sealed-pocket rescues 12/20 without, 11/11 with; out-of-room
errors 25/10 and 15/12; retries 1/1 and 0/0.

#### Saves: an animation saved at its end no longer crashes the load

[`b2fe1328`](https://github.com/DotairZee/zeetures/commit/b2fe13281395dfad36856614b13356fa7d61bf3d)

Home closed 39 ticks after loading (2026-11-15 07:55) on the assert in
AnimatablePart::updateAnimation (frameno &lt; animation.size()). A glow
(2 18 5) was saved at frame 3 of its 3-frame animation, which means
finished. applyPart restored the animation but set the frame only when it
was inside it, so the part kept the frame number from the world file's
own, longer animation (4).

- applyPart: a saved frame at or past the end restores the animation as
  finished (empty, which OVER also treats as over); a saved loop marker
  starts at frame 0; otherwise the saved frame as before.
- updateAnimation: a frame past the end is logged (classifier and part)
  and the animation started again, instead of the assert closing the game.

Checked: the Home save that crashed loads in a copy with no warning, both
glows in place; live Home relaunched on it.

#### C2 camera: the hand keeps its window place when scrolling hits the top or bottom

[`ef67f2f2`](https://github.com/DotairZee/zeetures/commit/ef67f2f217c2be1edac14781cd6dc66097104fa0)

MainCamera::moveTo moves the floated agents (the hand) by the view's move.
checkBounds clamps a move past the world's top or bottom by calling moveTo
again from inside it, and that inner call moved them back by the overshoot
as well, so each scroll step against the edge pushed the hand across the
window (owner, 2026-11-15: two-finger scrolling, vertical only; the sides
wrap). Only the outermost call moves them now.

Checked with OPENC2E_TEST_WHEEL_PAN and the new OPENC2E_TEST_WHEEL_XY
(wheel amounts per tick) in Home copies: scrolling hard down and up, the
hand stayed at 500,256 in the window (before: drifted to -2246 and 1902);
scrolling left across the wrap, unchanged from before.

#### Portability audit: Windows/macOS world switching, save line ends, names

[`15d343e6`](https://github.com/DotairZee/zeetures/commit/15d343e630d7e3eabc865f4c5c7c4844691fa2a2)

From an audit of the fork's code for Windows and macOS breaks (no Windows
or macOS compiler here: pattern scan plus a -Wpedantic -Wvla pass).

- Switching worlds on Windows and macOS: it only printed "run again"; now
  the world is saved and shut down, a new copy is started with
  SDL_CreateProcess (argv\[0\] made absolute, or SDL's base path plus its
  name), and this one exits. Linux keeps execv. OPENC2E_TEST_SPAWN_RELAUNCH
  takes the new way on Linux; OPENC2E_TEST_SWITCH_WORLD="N,NAME" switches
  after N ticks. Checked on Linux both ways with two Home copies.
  stdout/stderr flushed before exec and \_Exit (the switch line was lost).
- Text files the fork writes (state.txt, breed registry, cob lists,
  speech settings, hatchery-used, injector list, dumps) opened in binary
  mode, so their line ends are \n on every system.
- The save reader drops \r at line ends, including inside the mind-state
  block (a CRLF state.txt restored 1 creature of 136; now all 136, 41
  eggs, 17 kept objects, as the plain copy).
- World names: no leading or trailing space (Windows drops a trailing
  space, so "Home " would open Home's folder and overwrite its world
  file) and not CON, PRN, AUX, NUL, COM0-9 or LPT0-9.
- An AgentRef == Agent\* comparison C++20 calls ambiguous (an error under
  -Wpedantic; MSVC has had this one, see AgentRef.h); %p arguments cast to
  void\*; a volatile ++ (deprecated in C++20) rewritten.

#### Portability: 32-bit CI jobs, bundled-SDL option, Windows guards

[`47de1299`](https://github.com/DotairZee/zeetures/commit/47de1299b69ca163f29dcdb164f2c172dd0a1f33)

- CMake: OPENC2E_BUNDLED_SDL builds externals/SDL3 instead of the system's
  (default on for Windows and macOS as before), so a 32-bit Linux build
  does not need a 32-bit SDL3 package. Checked: a 64-bit Linux build with
  it on compiles and runs a Home copy.
- CI: build-32bit jobs for Windows Win32 and Linux i386 (-m32, bundled
  SDL). Untested until the fork has a GitHub repository.
- Hatchery: unistd.h only off Windows (\_getpid there).
- Agent injector: localtime_s on Windows instead of localtime_r.

#### C2 hand: clear the overlay pointer when the angel/devil branch removes it

[`8c689992`](https://github.com/DotairZee/zeetures/commit/8c689992d5beeffb13664d8f28fae85b2f704acf)

Over a creature with neither angel nor devil, the hand deleted its overlay
(for example the push arrows, after push was turned off) but kept the
pointer, so the next overlay change deleted it again.

#### C2: every lift lets riders out over its own shaft

[`bf38b8b9`](https://github.com/DotairZee/zeetures/commit/bf38b8b9ad3c1153ba13eae83dae9df58aaa49ec)

The let-out slide that moves a rider's foot toward the cabin's middle until
it is over the shaft's room was for the cage lifts (3 2 3) only. Of all the
world's lifts (checked with OPENC2E_LIST_POCKETS, which now prints each
stop's room width and the cabin's), one more has a cabin wider than its
shaft: the library's lift (3 2 4, cabin x 1333-1478, shaft rooms to 1473),
its east side over the rock beside the tree-trunk lift's shaft (room 102).
The slide now applies to every lift (boats and the submarine still not:
their middles are over water).

#### C2: the sealed-pocket rescue first steps a creature onto a floor just beside

[`808478d9`](https://github.com/DotairZee/zeetures/commit/808478d9f25628519fecb5aeeb8b1f89e9182f60)

A creature a little past a wall, its middle a few pixels into a sealed
room, was lifted to the first floor above it: at the gene lab's west wall
(x 6113), with its middle 8 px into the rock and its feet 30 px under the
lab's floor, that was the walkway 165 px up. The rescue now first looks for
a floor of the world within 32 px beside it, at or up to 40 px above its
feet, and steps it onto that floor with its box clear of the wall; then
lifts, then looks further beside, as before. (A creature sunk deep, like
the 130 px ones at the lab's east end, still goes up.)

#### C2: creatures at the tree-trunk lift's shaft stay out of the rock beside it

[`d1ac821e`](https://github.com/DotairZee/zeetures/commit/d1ac821e50dd8d059cd156da64498ed577f305ff)

Three ways creatures at the foot and top of the tree-trunk lift's shaft
(rooms 80/79, x 1546-1670) got into the sealed rock beside it (rooms
103/102), each followed by the sealed-pocket rescue moving them out:

- A hit from another creature knocks it sideways at 50 px a tick, and
  physics checks only the box's corners, which in the crowd there already
  reached into the rock. A shove that carries the foot into a room the one
  it stood in has no doorway to (open at its size) is now undone sideways.
- A foot placed afresh (after a load, a lift's let-out or a script's move)
  near the box's left, facing west, landed in the rock. A foot placed in a
  sealed room (no doorway into it ever open) with the creature's middle in
  a room that is not is moved into the middle's room
  (OPENC2E_NO_FOOT_MIDDLE turns this off, for comparison).
- The rescue judged "feet in the world" by the recorded foot room, which is
  empty right after such a placement; it now uses the room under the foot.

FAST copy runs, 5 minutes: rooms 102/103 rescues 43 before, 5 after; all
pocket rescues 95 before, 15 after; side by side from one save, 11 with
the placement rule and 23 without.

#### C2: speed spin box, eggs held between finger and thumb, test aids

[`a4fe7380`](https://github.com/DotairZee/zeetures/commit/a4fe7380b71d941637c00f494b76d072096ddc1a)

Toolbar: the game speed's up and down arrows are now stacked inside the
right end of the speed field, a spin box, in place of two arrow buttons
beside it. Typing a speed works as before.

The hand holds an egg by its tip between the forefinger and the thumb
(the carrying hand's picture, syst.s16 frame 6: the gap under the palm at
x 23, y 18-25), not at the picture's left edge, at the fingertips; checked
for all eight growth pictures.

Developer aids: OPENC2E_WATCH_ANIM="f g s" logs each ANIM, POSE and BASE
set on such objects and the script that set it; OPENC2E_TEST_UI_CLICKS
clicks screen points on given frames (headless tests of the toolbar).

#### C2: a creature lifted out of a sealed pocket stands with its foot on the floor

[`663ff68f`](https://github.com/DotairZee/zeetures/commit/663ff68fbd8edd8be120fb611127041380c09d52)

The sealed-pocket rescue kept the creature's box inside the room it lifted
it to, but a creature's foot is not in the box's middle (facing east, about
60 px from its left). At the east end of the dock (room 13, to x 6565) the
foot landed past the end, over the sea, and the creature dropped back into
the sealed water under the dock (room 31) and was lifted again, over and
over. The creature is now shifted so its foot is on the floor too.
FAST copy runs: 66 room 31 rescues in 150 s before, 8 after (2 at the
dock's end).

#### C2: keep stranded and dropped objects in their own room

[`bba392a7`](https://github.com/DotairZee/zeetures/commit/bba392a7f3da5e3ed64af0eda3e9a3e4b9099421)

Agent::tryMoveToPlaceAround (used for every drop and for objects found
outside the room map) now first tries only places in the room the object
is in, then anywhere as before. It could set things down through a wall:
a toy gun pushed partly past the gene lab's east wall landed in the
volcano chamber behind the rock, and creatures let out of the east boat
were put in the sealed water under the dock (room 31), then lifted back by
the sealed-pocket rescue (177 times in a test run; 30 after, all from
walking off the dock's end).

An object moved back in from outside the room map falls again (it was left
at rest in mid-air). C2 objects partly outside the room map are set back
at the nearest place, but not while a place is only being tested.
Creatures let out of a boat stand on the floor just above their feet.
OPENC2E_WATCH_VEHICLE_DROPS logs where vehicles let creatures out.

### 14 November 2026

#### Place the fertility checkboxes as the owner asked

[`d6be0b09`](https://github.com/DotairZee/zeetures/commit/d6be0b093e1b1eea5dae3cd19827dc113d55bba3)

The Hatchery's "All creatures fertile" sits on the norn-count line, left
of the population limit; the Breeder's Kit's "Infertile" sits under the
fertility reading.

#### Fertility switches in the Breeder's Kit and the Hatchery

[`552ef66b`](https://github.com/DotairZee/zeetures/commit/552ef66bcac12dd507b372f330ec8f2d165a653c)

The Breeder's Kit has an "Infertile" checkbox for the selected creature,
and the Hatchery "All creatures fertile" for the world; a creature's own
setting wins until set back to follow the Hatchery's. Conception checks
both. Both are saved with the world.

#### Start sound effects and voices silent

[`da109517`](https://github.com/DotairZee/zeetures/commit/da10951749827bd1d53c1f810e3edb8e4bd2f2f1)

The backend started a clip at full gain and the sound manager set its
volume (muted, or by its place on screen) a moment later, so the first
milliseconds of a muted sound could be heard: a "peep" with all audio
off. play_clip can now start a clip silent; the sound manager does.

#### Flying critters pass only fully open doorways

[`2d6b454a`](https://github.com/DotairZee/zeetures/commit/2d6b454a58d736e44e7a1d956314039f6bbf5d7c)

C2's small flyers (bees, size 40; flies, 20) slipped through the room
edges left at permeability 100 to stop creatures, which are mostly drawn
as solid rock, so they flew through rock. Genus 16-18 objects with
collisions and no gravity now need a door at 255.

#### Raise the fork version to 0.6.0

[`7530dbcd`](https://github.com/DotairZee/zeetures/commit/7530dbcd2f30ac82836fc797fbd515f5d0ed76bb)

Signature scents, the hand's fixes and the gene lab's sealed east end.

#### Signature scents; starving creatures walk to a teleporter in sight

[`33b3a6f5`](https://github.com/DotairZee/zeetures/commit/33b3a6f5739db0eae0c81cea10d59d3f075563e6)

Every living creature lays its own scent, by moniker, which spreads
through open doorways and fades like the named scents (pheromones.txt:
"signature spread S fade F strength X" or "signature off"). A mind
names the creatures it longs for most (wantedSignatures) and senses
their scents here and each way. A creature that came to where it last
met the one it longs for and found it gone now follows that one's
scent instead of giving up on it for a while. OPENC2E_NO_SIGNATURES
turns the following off. Reunions while longing are logged.

Starving-leave also walks to a teleporter in sight but out of reach:
from the gene lab's east wall the teleporter is in view 700 px away,
and neither part of the rule acted on it.

#### Lift a sunken creature back onto the floor above first

[`077541e3`](https://github.com/DotairZee/zeetures/commit/077541e376bfb9d74705270f495305a5dad03470)

The sealed-pocket rescue looked beside the pocket before above it, so
creatures sinking through the gene lab's floor at its east end were
moved into the space behind the room's east wall, 130 px lower in the
rock. A floor of the world within 150 px above now comes first.

#### The hand takes things from creatures, and keeps its mode

[`cbd6f3cc`](https://github.com/DotairZee/zeetures/commit/cbd6f3cce2dd0aafb14fc88e4c2897a6f52f87c6)

Picking up an object another agent holds now makes that one let go;
before, a creature still "carrying" an object the hand had taken pulled
it back to its hand each tick. The hand's mode (invisible, push) is
saved with the world and restored at load.
OPENC2E_TEST_HAND_PICKUP=carried takes the first object a creature holds.

## Version 0.5.0

Every object in the world saved with it (stage 3), C2's announcement windows, starving creatures walking to a remembered teleporter, and a repeated device press judged as one try.

### 14 November 2026

#### Raise the fork version to 0.5.0

[`60b22c3f`](https://github.com/DotairZee/zeetures/commit/60b22c3f8ecd0d9335f56cff136cdb0c313aafe0)

Every object is now kept between launches (stage 3 of world saving).

#### Judge a repeated device press as one try

[`a39ea3a9`](https://github.com/DotairZee/zeetures/commit/a39ea3a90b7563af833e8b9136277d7b698d0dde)

A creature that worked the same device from the same spot again while
its last try was still being judged restarted the judging, so a wrong
travel link was never forgotten (the west lab's fish-egg machine
pressed 55 times in a row as a route step).

#### Save every world object with the world (stage 3)

[`4dbb93cb`](https://github.com/DotairZee/zeetures/commit/4dbb93cb049f90a2c67d4bdb08522fc6f47fb876)

Plain and compound objects are now saved with the world: those from the
world file by their place in its object list, and those made during
play (fallen fruit, critters, seeds) in full, as injected objects
already were. At load the world file's objects take back their saved
place, state, variables and pictures, those gone during play are
removed, and the others are made again. Objects busy in a script when
saved keep the world file's pictures and ACTV. Scenery, creatures,
eggs, kept and injected objects, vehicles, lifts, call buttons and
objects the Additions make at each load are left as before.
OPENC2E_NO_WORLD_OBJECTS loads without them.

#### Starving creatures walk to a remembered teleporter

[`5d8b64f8`](https://github.com/DotairZee/zeetures/commit/5d8b64f8854a8ab7879d924217406fb49900e90e)

The starving-leave rule used a teleporter only in view. In the gene
lab's lower room the teleporter is at the west end and starving
creatures piled up at the east end, out of its sight. Now, with no way
out in view, a starving creature walks toward a teleporter it remembers
on the same floor; the in-view rule takes over on arrival.
OPENC2E_NO_STARVEWALK turns this part off.

#### Support C2 announcement windows (NEW: CBUB, BBTX, BBFD)

[`923285a8`](https://github.com/DotairZee/zeetures/commit/923285a8a3355f008cd8f8c685302f0d21e6e52e)

The gene lab console's script died at NEW: CBUB ("compound bubble
objects are not supported yet"). NEW: CBUB now makes the window as a
compound object (the script adds its frame and hologram parts), NEW: BBTX
adds a wrapping text line, BBTX fills it and BBFD sets its color. C2's
announcement texts are not in its data files, so they come from
announcements.txt in the Additions folders.

#### Log where a creature died

[`a8417fba`](https://github.com/DotairZee/zeetures/commit/a8417fba7035aa6095c98f4f58a7167bc4660cc9)

The death line now gives the creature's middle and room, so drownings
and starvation can be mapped from home.log.

## Version 0.4.0

Food that cannot be eaten no longer counts as food; stalled meals end; starving creatures leave a foodless place by a known way out; lift links only from real rides; bodies float away again; the game speed control on the toolbar; script-moved creatures glide between ticks; vivid babies.

### 14 November 2026

#### Raise the fork version to 0.4.0

[`82320cdf`](https://github.com/DotairZee/zeetures/commit/82320cdff2f9e65013aa97b4ed9e4217472e376a)

108 commits since 0.3.0: inedible food no longer counts as food, stalled
meals, starving creatures leave, lift links from real rides only, bodies
float away again, the toolbar speed control and vivid babies.

#### Draw babies with twice the color saturation

[`7f6bb481`](https://github.com/DotairZee/zeetures/commit/7f6bb481c5d7493d9dd2037ba4dbd2a6377c1faf)

Baby-stage creatures' sprites are pushed away from gray by 1.0 (the
reverse of the senile graying), cached per image like the old-age tint.
OPENC2E_BABY_VIVID=s sets another strength; 0 turns it off.

#### Creatures moved by scripts glide between ticks; rescue lines name the unid

[`c48a7602`](https://github.com/DotairZee/zeetures/commit/c48a76025cb3faafe8d3eaf48b628ff862a72cfc)

- A creature that a script moved in the last tick (MVTO: the stream under
  the cave lift's dock carries creatures a few px every tick) is drawn back
  along that move by the part of the tick not yet elapsed, as vehicles and
  their riders already were, rather than jumping ten times a second
  (owner, 2026-11-14: "really jerky"). Creatures record where they start
  each tick (SkeletalCreature::tickstartx/y); moves over 64 px are jumps.
- The sealed-pocket rescue messages give the creature's unid, so they can
  be matched to its mind's log lines.

#### Mind: a starving creature leaves a foodless place by a known way out

[`7d109809`](https://github.com/DotairZee/zeetures/commit/7d1098092efbd21ec12f90c6223016b444f0c098)

Starving (Hunger at least 0.85) and long without a meal (restlessRamp), a
creature takes a learned travel link from here that leads at least 800 px
away, or tries a teleporter (category 33) in view, worth 0.7 of a meal.
Teleporters join the devices whose use is learned as a travel link. Lifts,
call buttons, machines and doors are left out (the floor-change steps have
lifts; airlocks). A try from a spot that took it nowhere is not repeated
there for 300 ticks (the big teleporter pad only receives).

The gene lab's lower room has no way out but its teleporter; creatures that
had learned it still starved at its far end, acting on each other.
Matched 10-minute copies: creatures left in the room at the end 18/16/17
-&gt; 8/9/10 (first version, no repeat check); with a 900-tick repeat check
11/13/8 -&gt; 5/11/9, meals unchanged. OPENC2E_NO_STARVELEAVE turns it off.

#### Game speed on the toolbar beside the zoom; "Year" without a colon

[`f82ad887`](https://github.com/DotairZee/zeetures/commit/f82ad8870827e1a2c93d8227536c5a9e1ffa4b41)

- The speed control moves from Tools &gt; Game Speed to the toolbar, after
  the zoom: a box showing the speed (type a number and press Enter) and
  up/down arrows that change it by 0.5, from 0.5x to 10x (owner,
  2026-11-14). Engine::speed is now a float; tickInterval rounds
  ticktime / speed. The tooltip says what pace the world actually reaches
  when it is too busy to keep up (Engine::measuredSpeed, measured over
  about two seconds). OPENC2E_SPEED accepts fractions.
- The status bar reads "Year 016" rather than "Year: 016".

#### Mind: lift rides are learned only from working the lift

[`81842714`](https://github.com/DotairZee/zeetures/commit/818427144ca2dad4db02471a30b3495577797378)

A ride was credited to the creature's last action on the lift, whatever
it was. A creature that bit, hit or picked at the cave lift while
another's ride moved it learned "eat the lift to go up" and kept choosing
it as a food or route step, sitting in the cabin eating the lift dozens
of times in a row (chain steps learn nothing, so it never stopped). 43%
of the 4,204 lift links saved in Home (1,828) were of this kind.

Now only activate 1 or 2 and deactivate can be learned as a lift ride
(travel links already had this rule), and saved links of other kinds are
dropped on load. OPENC2E_KEEP_BADLINKS keeps the old behavior.

Three matched 8-minute pairs: non-working actions on the lift 2,451 -&gt;
1,398, the trip-driven ones (route/food steps that eat or hit the lift)
gone; the rest is ordinary play with the lift. Meals that fed and hunger
within noise (819 -&gt; 740, one pair low).

#### Mind: an action whose object changed ends; a meal that fed is not futile

[`92f76b33`](https://github.com/DotairZee/zeetures/commit/92f76b33dd59e6b115a23241808918186af24c1d)

About a third of meals ended "stalled". Two causes in the mind:
- The engine sees one thing of each kind. When the thing being acted on
  was eaten or went out of sight, it took up another of the kind and the
  creature walked to that one, often out of reach (on a ledge, past a
  wall). Now the action ends as "object gone" and the creature chooses
  again.
- A creature chewing a many-bite food gets no nearer to it, so the meal
  counted as stalled, and three such meals in a row marked eating there
  futile for 1,500 ticks. An action that eased its needs no longer counts.

Three matched 8-minute pairs of copies (off/on): meals that fed
195/182/170 -&gt; 196/224/188 (+11%); futile marks 1,112 -&gt; 989.
Developer aids: OPENC2E_WATCH_STALLS prints each stalled object action;
OPENC2E_NO_STALLFIX turns the fix off for comparison.

#### C2 food: cores, shells and unripe food no longer count as food

[`5bdeed6d`](https://github.com/DotairZee/zeetures/commit/5bdeed6d4cf838ee6732f60d7607bdb8193a79a4)

Creatures in Home "ate" tomato cores, nut shells and nuts past their
fresh poses again and again: of 1,767 meals in 51 minutes 489 fed. Their
eat scripts name nourishing chemicals, which the mind's food sense reads,
but C2 marks such food with no eat bit in BHVR's touch value, and its
tomatoes and nuts feed only in poses 0-1.

- Agent::c2touch keeps C1/C2 BHVR touch, from the SFC (bhvrtouch) and the
  BHVR command (previously ignored).
- edibleNow: food without the eat bit (64), or whose eat script feeds only
  inside "doif pose eq N" and is in another pose, neither smells of food
  (World food scent) nor looks like food (CreatureAI). edibleScent now
  takes the most one bite gives.
- Eating food without the eat bit does nothing (fireScript event 12).
- OPENC2E_IGNORE_TOUCH developer aid turns this off for comparison.

Matched 6-minute copies: meals that fed 44% -&gt; 47%, wasted tomato and nut
meals gone.

#### Let dead bodies rise and be removed again; restart a stalled death script

[`472ab0df`](https://github.com/DotairZee/zeetures/commit/472ab0dffdd545d00115cc941c45825ba5c54d1c)

A dead creature's body floats up through rock (attr 0, size 0) and is
removed at the top of the world. Since 2026-11-13 dead bodies were kept in
the room system, and both sealed-pocket checks caught rising bodies, set
them on a floor and zeroed their speed, so the death script waited forever:
39 bodies lay around Home, five of them against the gene lab's lower room's
east wall, where hungry creatures crowded round them and starved.

- oldCreature::tickBody: only a lying body (attr 64) is kept in the room
  system; after 40 s a body whose death script is not running gets it again,
  and a rising body that has stopped is set rising again.
- World's sealed-pocket rescue skips creatures without collisions.
- OPENC2E_WATCH_CANDS developer aid: every candidate of each mind choice.
- OPENC2E_TEST_KILL accepts +N (ticks into the session).

#### Game speed: Tools &gt; Game Speed (Normal, 2x, 4x, 10x)

[`8df36f02`](https://github.com/DotairZee/zeetures/commit/8df36f022594671a11f1c5df0f11f2a52c029df6)

World ticks run speed times as often. When ticks fall due faster than
frames are drawn, every due tick runs before the next frame, keeping to
the schedule, up to about 40 ms of ticks a frame; far behind, the
schedule is let go rather than spiralling. Gliding uses the shorter
interval. OPENC2E_SPEED=n starts at a speed (developer aid).

Measured on Home (about 150 creatures, ~18 ms a tick): 1x 9.9 ticks/s,
4x 40.0, 10x about 44 (as fast as the machine allows).

#### Push hand east shove frames; riders let out below a stop stand on its floor

[`049d58f8`](https://github.com/DotairZee/zeetures/commit/049d58f802edd0f6d2cde3322d7665e66bcafcae)

Push hand: a shove east plays the hand sprite's own pressing-east
drawings (frames 10, 11, 12) instead of the west shove mirrored; the
owner noted C2 animates the two directions differently.

Lifts can stop a few px past a stop (the bamboo-cage lift moves 4 px a
tick): riders let out with their feet up to 12 px below a floor they
cannot pass now stand on that floor. home.log had ten falls of about
330 px from the bamboo lift's middle stop (y 840) to the shaft's foot.

#### Cave lift crowd: false travel links and place-map steps

[`bb72cb89`](https://github.com/DotairZee/zeetures/commit/bb72cb8980e4a3900c4adedfbef679430da470cb)

Hungry creatures crowded the shaft of the cave lift by the waterwheel,
riding it up and down. Causes found and fixed:
- A lift ride shorter than linkMove (this lift rises 170 px) left its
  travel link pending, and the walk afterwards was credited to the lift.
  A ride is now judged when it ends; a lift or call-button press with no
  ride teaches nothing; short rides count against existing links so
  wrongly learned ones are forgotten.
- The place map took script moves (the stream carrying creatures along
  the channel) for walking, so it held a way east through the rock from
  the shaft's floor. Script-moved ticks are skipped (MindPerception
  scriptMoved), and a wall bump counts two failures against the steps
  leaving that cell that way.
- The cage lift's cabin is wider than its shaft; riders let out over the
  next room dropped into the channel. Cage lift (3 2 3) riders are slid
  over the shaft's own room when let out.
- Foragers that just changed floors and find both floors searched walk
  on instead of riding back.

#### Creature's View leaves out the creature itself

[`fe91a90d`](https://github.com/DotairZee/zeetures/commit/fe91a90d21fcf0e5ed31544fb973498096c4b67b)

The view is roughly what the selected creature sees, and it does not see
itself. World::drawWorld takes an agent to skip.

### 13 November 2026

#### Lineup test aid: keep scenery pieces (2 3) when clearing the floor

[`f2ebb096`](https://github.com/DotairZee/zeetures/commit/f2ebb096a3e7b5f2e607017c3eeea33e6207d732)

The lineup cleared every family 2 object near the creatures, which also
removed rock overlays drawn in front of them, so a picture could not show
a creature behind one.

#### Quit at once after saving; a smaller save

[`6cf509af`](https://github.com/DotairZee/zeetures/commit/6cf509afd0de55190e531c7fc184518a95fe4c9e)

- C1/C2: on quitting with the world saved on quit, the process ends right
  after the save (Engine::saveForQuit, std::\_Exit). Tearing down the world
  and audio afterwards hung Home: stopping the mixer made PipeWire free
  corrupted memory, the C library aborted and the crash report deadlocked
  in the abort. Test runs (--no-autosave) still shut down fully. A Home
  copy now quits in 0.5 s.
- The mind's save: rooms keep the 6 creatures met there most (every
  creature remembered every other one in every room: 287,000 lines with
  145 creatures); learned numbers are written to 4 significant figures;
  expectations never tried and empty, and impressions of an individual
  from one dealing only, are left out. Home's save: 28.5 MB -&gt; 18 MB; it
  reloads with every creature.

#### Creatures crossing the world's seam are no longer thrown into the library

[`de89b98d`](https://github.com/DotairZee/zeetures/commit/de89b98df8bfbf5bb77c0e3874b28c0cdfad0d0a)

Walking down the volcano passage into the potion room crosses Albia's
seam (x 8352 = 0). A creature whose middle was past the map's east edge
there was found in no room (Map::roomAt finds none past the edge), taken
for a creature in rock, and keepInRoomSystem's ledge rule "rescued" it to
the far edge of the potion room's first room, about 760 px east: it
vanished at the library's entrance and reappeared beyond the machines
(owner). The safety net now judges the creature in the map's own terms
across the seam and moves it back in its own. Test: 90 crossings into
the library, no jumps (3 in a run before).

BIG MOVE logging also measures moves the short way round the seam, so a
jump hidden in a wrap is logged.

#### Mind: no walks found blocked, for any reason; Observation Kit stops growing

[`ad1d8c86`](https://github.com/DotairZee/zeetures/commit/ad1d8c867348c0e98339a3a32dd807c0af1b94cb)

- Food, smell and route trips chose their walk without asking whether it
  was blocked there; a starving norn in the gene lab's lower room walked
  east into the end wall for good, food being somewhere east. At choice
  time every walk found blocked here (or with a wall right ahead) is now
  set aside. A walk stopped by a wall also blocks that walk action itself,
  not only the way it went (which way a creature's travel actions go
  depends on its genes).
  Six norns in the gene lab room, 3 fast minutes: worst wall hits per
  creature 110 -&gt; 42; all wall push-backs in the world 6646 -&gt; 3743.
- Observation Kit: the table filled the list, which fits itself to the
  table plus its scrollbar, so the kit grew wider every frame. The table
  keeps its own width (NoHostExtendX).

#### Observation Kit: room for the list's scrollbar

[`34c70551`](https://github.com/DotairZee/zeetures/commit/34c7055122e462c37033f7609c7bda56d7ba07ad)

The scrolling list fitted its width to the table without the scrollbar,
which then covered the Go buttons. A scrollbar's width is kept beside
the table.

#### Observation Kit: the creature list scrolls past 20 rows

[`5401c975`](https://github.com/DotairZee/zeetures/commit/5401c975a3ac15dbc9463b0403160086c73c50eb)

With 121 creatures the kit filled the screen. The list is now at most 21
rows (or 60% of the window) tall and scrolls; its width still fits the
columns.

#### Shoves stop at walls; test aids for creature parts and posed lineups

[`4aad8a10`](https://github.com/DotairZee/zeetures/commit/4aad8a10b9a32014cf0c9623e7dcf7cfbdb44937)

- A shove (another creature's push, the push hand) moved a creature
  sideways with no wall check; norns pushing a norn beside the wall of a
  sealed shaft west of the music room shoved it inside. A shove that would
  carry the feet into a room the creature could not walk into now ends.
- OPENC2E_DUMP_CREATURE_PARTS lists each creature's part pictures, frames
  and growth scales at snapshots (SkeletalCreature::describeParts).
- OPENC2E_LINEUP_POSE sets the lineup's pose string; the lineup shot sets
  OPENC2E_SNAPSHOT_ZOOM and re-aims the view just before the picture.
- OPENC2E_TEST_PUSH=-1 or =w (west, every 20 ticks).

#### Sealed pockets: a door open at any time joins its rooms

[`656da880`](https://github.com/DotairZee/zeetures/commit/656da880975d138ef45dbe1d99c210a3d528499b)

The rescue for creatures in sealed pockets groups rooms through open
doors once a minute. Taken while the east sea airlock was mid-cycle (both
doors shut), the chamber looked sealed and a norn in it was lifted 800 px
onto the dock. Doors now remember having been open this session
(RoomDoor::everOpen, set by Map::setDoorPerm), and such doors join rooms
for the grouping.

#### Ignore the build-fast directory

[`77c95940`](https://github.com/DotairZee/zeetures/commit/77c95940dff76a11bf4303f132ec29774c318c69)

#### Performance: creatures' looking about no longer scans every room

[`02e5108f`](https://github.com/DotairZee/zeetures/commit/02e5108ff41cdc36605190116ea976e37e280218)

With about 100 creatures a world tick took 235 ms (96 ms without
AddressSanitizer), so the game ran at a third of its speed. Nearly all
of it was creatures choosing what to look at: for every object in the
world, each looking creature's visibility test looked up rooms (a scan
of every room) before checking whether the object was even in range.

- AgentHelpers: the range test comes first (same results).
- chooseAgents works out the creature's eye point once per look, not
  once per object.
- The sense cell for a possible mate is refreshed on the mind's cadence
  (every 4 ticks), scanning creatures only, sight before kinship.
- CMake: OPENC2E_ASAN (on by default, as upstream) can turn
  AddressSanitizer off for a build to play with.
- Developer aid: OPENC2E_TICKRATE=s prints ticks a second, ms a tick and
  a breakdown by part (Profile.h, PROFILE_SCOPE).

Home copy, 109 creatures: 13 ms a tick without AddressSanitizer (was 96),
39 ms with it (was 235); drawing about 3 ms a frame.

#### Creatures sense a drop to the floor they would land on

[`cd2c000d`](https://github.com/DotairZee/zeetures/commit/cd2c000dcd6c8c67627f26fa34227e653dd15da1)

The drop ahead was the next room's floor line minus this one's, so where
that floor is open air (a room whose floor opens on rooms below), a big
drop read as a step: at the end of the upper-east ledge, 63 px, under the
100 px a creature counts as harmless, so no fall taught it to keep back,
while norns that stepped off fell 400 px. The look ahead now follows open
floors down from just past the side (as far as four rooms) to the floor
it would land on: 414 px there. OPENC2E_WATCH_DROPS prints the drops a
creature sees.

#### C2 ROOM can make a new room

[`af6494b3`](https://github.com/DotairZee/zeetures/commit/af6494b397d2bfd63426db2ba2f9dc91b7e734d3)

ROOM with a room number the map does not have built a Room and dropped
it. It is now added to the metaroom around its middle with the number
asked for (room ids past it stay free). Used to fill an unmapped pocket
of air under the upper-east ledge's extended tip, where creatures
stepping off the ledge were out of the room system (8 times in a test
before, none after).

#### Hatchery: a limit on the number of norns

[`822cd8e9`](https://github.com/DotairZee/zeetures/commit/822cd8e9f6ff2c604cf7e28b397bb03fe649c455)

Below the nursery: the number of norns in the world and "Limit the
number of norns" with how many (saved as the player's setting
population-limit; off by default). C2's own egg and hatchery limits
(EGGL, HATL), which its kiss and egg scripts compare the number of
norns with, return it; with no limit they stay open as before.
OPENC2E_POPULATION_LIMIT sets it for tests without saving.

#### UI: shorter Creatures menu, centred thought pictures, Chronicles fills its window

[`7479923e`](https://github.com/DotairZee/zeetures/commit/7479923ef74ccdc6b889d508a17e02bb0521a570)

- The Creatures menu stops at 20 rows (or 60% of the window) and
  scrolls. OPENC2E_TEST_MENU=Creatures with OPENC2E_TEST_MENU_SHOT opens
  it and takes a picture (the world does not tick while a menu is open).
- Thought bubble pictures sit 2.5 px lower; at the body's arithmetic
  middle they looked 2-3 px high. OPENC2E_TEST_THOUGHT makes the
  creature nearest the view's middle think the given symbols.
- Chronicles Kit: the History list fills the window (it was a fixed
  800 px, leaving an empty band and cutting details short); Statistics
  stretches its counts across and sets the records as many across as
  fit whole (up to three).

#### Homeostat mind: an action that walks into a wall stops

[`9e9ca2a1`](https://github.com/DotairZee/zeetures/commit/9e9ca2a121ec93979a0ca5c96aee1b24e7a9e1b0)

Creatures paced into walls for long spells: the blocked-way memory
covered only the mind's own walks east and west, while walking up to
something past a wall (another creature, food, a smell, or C2's
decision 23, which approaches the attention target) went on into the
wall. SkeletalCreature now counts wall push-backs and their direction;
the mind ends any action pushed back twice, notes the way as blocked
there and passes over an object it was walking to.

Also, while settling on a floor after a hop, a pose change no longer
moves a creature into a room it could not walk into: hits had stepped
norns through the doorway at the end of the floor under the lab dome
onto the open floor beyond.

Six norns on that floor, 150 s fast: wall bumps 328 and 355 before,
67-118 after, no falls through.

#### C2 creatures pushed back from a wall stay on their floor

[`b822ffb0`](https://github.com/DotairZee/zeetures/commit/b822ffb01e04b7f7de14975ebd17b4785d50b00c)

When a walking creature's foot ran into a wall, it was pushed back to its
last good foot point and its room looked up there. That point lies a few
pixels under the floor line, so where the room below has its top at the
floor, the lookup found the room below and the creature fell through a
solid floor: walking into the wall at the east end of the floor under the
lab dome dropped norns 300 px onto the hut platform (four of six in a
test). The push back now returns the creature to the room it stood in
when that room spans the foot. Two retests: over 300 wall bumps each,
no falls.

#### C2 vehicles keep riding creatures inside the cabin

[`af8df85e`](https://github.com/DotairZee/zeetures/commit/af8df85ebc59e9be4fa5fc04812296e04dab7df7)

A creature's box is a fixed 50 px wide from its x, but its picture can
be far wider, and it was pushed into a vehicle's cabin by that box only
once, on boarding; its pose changes while riding and nothing kept its
feet down. A grown grendel in a cage lift hung out past the cage's
side. Each tick a C2 vehicle now keeps each creature's picture within
the cabin's sides (centred when wider than the cabin) and raises feet
that have sunk below the cabin floor. SkeletalCreature records its
drawn width per pose (drawnWidth).

#### C2: circling stars over unconscious creatures

[`93aa7256`](https://github.com/DotairZee/zeetures/commit/93aa7256a8b33c6b056baa712316bc6947504f29)

C2 draws a turning ring of stars over a creature that is out cold; no
world script does it, so it is the engine's. Each live unconscious C2
creature now gets the player's own star.s16 (ten frames, one a tick)
over its head, where the selection halo sits, its middle a little above
the head's top. Drawn with the thought bubbles (not in photos).

#### C2 creatures: floor friction for sideways speed

[`8438955d`](https://github.com/DotairZee/zeetures/commit/8438955da42d43346411fa551bc7485b4182c028)

The world's creature-on-creature hit script knocks the victim back at
50 px a tick, and creature physics had only air friction (a tenth a
tick), so a hit creature slid about 450 px and bounced off walls; a
norn on the volcano's east ledge was hit twice and thrown into the
volcano. With its feet on the floor and no vertical speed, a creature's
sideways speed now halves each tick. A test knock-back on flat floor
now ends 184 px away (hop included) instead of sliding and bouncing on.

#### Status bar notices: right-click menu with Dismiss and Clear all

[`81c3bebc`](https://github.com/DotairZee/zeetures/commit/81c3bebcb24d7a31d080ae5ff558122e1119b2e6)

Right-clicking a notice (or the +N button) opens a menu: Dismiss removes
that notice, Clear all removes every notice. OPENC2E_TEST_NOTICE_MENU
opens the menu once for snapshots.

#### C2 creatures no longer walk sideways in mid-air

[`04933c5b`](https://github.com/DotairZee/zeetures/commit/04933c5b48e2b32c984b7df210040cce50985dcb)

A falling creature's walk carried on: each gait pose change moved it on
by the planted foot's stride, and nothing checks walls in the air, so a
norn that went off the ledge by the west lift (x 494, y 1540) came down
184 px further east, through the rock face into the room below. While
falling and moving up or down, or well clear of the floor, a pose change
now keeps the creature where it is across. OPENC2E_WATCH_FALLS also
prints each tick of a fall and where a fall ended.

#### Kits: egg-laid notices dismiss on click; Reproductive tab shows life stage

[`f7f9adb8`](https://github.com/DotairZee/zeetures/commit/f7f9adb823a4c2fdfaf00e9847a336e87a5cf063)

Clicking an egg-laid notice on the status bar now goes to the egg and
removes the notice, like every other notice (the Hatchery's Find eggs is
for finding eggs later). The Breeder's Kit Reproductive tab names the
creature's life stage beside its sex and kind, and notes when it is too
young to breed (creatures mate from adolescence).

#### Falls hurt by height; creatures learn which drops to keep from

[`f258abc5`](https://github.com/DotairZee/zeetures/commit/f258abc522e889e2fd39ae6d53ba2f67e23830fd)

Owner: creatures that walk off a ledge should drop and learn from it.
A fall of a creature's own (not from the hand, a vehicle or a script in
the 5 ticks before) hurts on a dry landing: nothing up to 100 px, then
Pain (chemical 1) by 0.6 a px, and past 300 px Injury (chemical 14) by
0.4 a px. The mind is told each fall (height, hurt, where it stepped
off) and learns: the spot (direction and place) and a hurt per pixel of
drop beyond 100 px. The innate 150 px cliff rule is gone: drops are
judged by what falls have taught (fallHurtExpected), for wandering and
place trips as well as food trips, so a young creature walks off ledges
until a fall teaches it. Saved with the mind (fallrate, pdrop lines).
Test (six young norns near ledges, 4 min): 27 falls, 14 that hurt, each
learned; no creature fell at the same drop twice.

#### Creatures drop off ledges; blocked ways remembered by place

[`5f7d1fe5`](https://github.com/DotairZee/zeetures/commit/5f7d1fe5328ecaf9a3e09afbf4f7ae4abb8b111b)

Ledges (owner: creatures that walk off a ledge should drop straight
down): a foot reaching out over the lower floor pulled the body down
before it was past the edge, into the rock under the ledge's end, and the
room safety net then lifted it back onto the ledge (the upper-east deck,
room 145 over 179). A creature found in rock with a dry room of the
world beside it, open above and with its floor not above its feet, within
its width, is now moved just inside that room's edge and falls. Tested:
norns walking east off room 145 land in room 179; none lifted back.

Blocked ways (owner's screencast: norns walking into a wall for long
spells): each spot where a walk got nowhere is remembered, and meeting it
again doubles how long it is avoided (to 16 times blockedTicks); plain
wandering now records and respects blocks too; a room an explore trip got
blocked on the way to is not explored toward for a while.

#### Thought bubbles: one bubble at a time per creature

[`9550c7f3`](https://github.com/DotairZee/zeetures/commit/9550c7f3f681ccd59ec309d23fe3d8513d274710)

A creature shows a speech bubble or a thought bubble, not both (owner):
while its speech bubble shows, its thought is not drawn
(speechBubbleShowing).

#### Thought bubbles in pictures; water changes count once they last

[`ad4e32fc`](https://github.com/DotairZee/zeetures/commit/ad4e32fc8441638605049cc1aeb1f1b40eea87c0)

Thought bubbles (owner chose the pictures style, in C2's own thought
bubble): when a creature sets about something new its mind says why, as
picture tokens (MindOutput::thought: a symbol's name, cat:N for a thing of
that kind, who:MONIKER for a creature's face) and a key so one long trip
is not shown again within 30 s. creatures/Thoughts draws them for 3.5 s
over the creature's head in C2's thought bubble (syst.s16 frames 24-29,
from the player's game, placed as the hand's typing bubble), with our own
symbols (data/thoughts/make_art.py, icons.png, embedded) and pictures of
things and faces from the running game. Thoughts: food trips, following a
smell, foraging, getting out of water, looking for a remembered thing,
missing a mate, exploring, favourite places, leaving a disliked room,
backing away from a feared creature, sleep, and a pleasant surprise.
View &gt; Thought Bubbles: every creature, the selected one, or off (kept in
settings.txt). imageManager::fromFrames makes a game picture from our own
frames; SkeletalCreature::headSprite gives a creature's face. (Drawn with an
explicit size: RenderOptions::scale is applied twice on that path.)

Mind: in or out of the water counts only once it has lasted 8 ticks. A
creature against the wall between the sea column (room 123, water) and a
dry room read wet and dry by turns every tick, learned the device it had
just pressed as an exit and a flooder over and over and kept pressing it
(the gene lab's pumps; owner's screencast).

OPENC2E_DUMP_RENDERS now prints each part's position, sprite and frame.

#### Mind: longing for a mate; MCRT stands a creature on the floor

[`34e113c2`](https://github.com/DotairZee/zeetures/commit/34e113c228ef025bd37b65ac5382df32de698021)

Longing (owner chose it over raising the drive in lone adults): a male's
Sex falls to nothing within a minute of being apart, so time with an
adult of its kind and the other sex while wanting to mate builds a
longing for that one, fading over about half an hour apart. Strong
enough, it sends the creature to where they last met even with no Sex
drive left (and finding them gone lowers it a little). Saved with the
mind; the Mind Kit's Places tab lists whom it longs to find. Test
(adults met, then males moved away, 7 min): 50 mate trips (2 before in
18 min), 35 matings tried, 4 conceptions.

MCRT (C2's teleporters only) put a creature's top at the destination
pad's centre, so at the pad by the stone arch (3001,1257, floor 1285)
its feet went 70 px into the floor; it now stands on the floor of the
room at that point. All six pads checked.

#### Mind: a sense of place

[`b48afc7d`](https://github.com/DotairZee/zeetures/commit/b48afc7dc4809292d2f5440e85f2a7245525dcb6)

The engine now tells the mind which room the creature is in (by its
feet), the room's middle and floor, its metaroom, and the rooms next to
it through doorways it fits; and, for creatures in view, their genus,
sex and life stage.

Each creature remembers the rooms it has been in (and those it has seen
next door): visits, time spent, what it saw there, whom it met there, and
each need's change while there against its change anywhere (the room's
feel). Saved with the mind. Uses (owner chose all four):
- finding things again: a wanted kind of thing out of sight with no
  last-seen spot left is sought in the room it was seen in most often and
  most lately; a room checked and found without it is not tried for a while
- mates: an adult wanting to mate with no mate in view goes to where it
  last met an adult of its kind and the other sex
- favourite places: a strong need (cold, heat, tired, sleepy, lonely) sends
  it to the room that eased that need best (rest and sleep only as a
  short stroll while not yet exhausted); avoided places: a room where
  pain, cold, heat, fear, injury or suffocation rose is left for the
  neighbour that fared best, and is never a destination
- exploring: curiosity about unvisited rooms next door, more when bored
Place trips are valued by time like food trips (patience grows with the
need). Mind Kit: a Places tab, and favourite and avoided rooms on the
Memory map.

#### Hatchery card: Sprites lists the ages with sprite files, then whether fitted

[`07ffbc3e`](https://github.com/DotairZee/zeetures/commit/07ffbc3e790b383e7e6d206655936713ce733da3)

The row names the life stages a breed has sprite files for; a breed drawn
at one age only is named by the age its pictures look (by size against
its species' reference; ettins taken as adult), e.g. "youth-sized". When
the engine scales and ages the pictures for the other stages (per-part
growth, or one-set stage scaling) a second line reads "(fitted to every
age)" (owner's choice of wording).

#### Hatchery: no credit for the engine's growth of a breed's pictures

[`6400fb27`](https://github.com/DotairZee/zeetures/commit/6400fb27b540c57c21f1bc5a4f9c579e5260b3b4)

Breeds drawn at one age and grown to every life stage by the engine
were credited to the owner on the egg card; no new pictures are made,
so the By line names only the breed's own makers (owner request).

#### Breeder's Kit: body data alone in Overrides keeps a game breed the game's

[`92354290`](https://github.com/DotairZee/zeetures/commit/923542904663de6a7604890c68390213bcd6a52e)

Corrected joint files (Golden Desert's neck points, made on the player's
machine) no longer mark the breed as installed in place of the game's;
its card says the joints were corrected in the data folder.

#### Prototype: per-part growth for every life stage; senile grey (off by default)

[`ffa45f3c`](https://github.com/DotairZee/zeetures/commit/ffa45f3c68006051561225811ff09fe5ee8e6d09)

Owner's option 3 (2026-11-13), behind switches until approved:
OPENC2E_STAGE_GROWTH=parts draws a stage a breed has no pictures for from
the set the engine falls back to, each body part scaled by how much that
part grows between the two ages in its species' reference breed (ages the
reference lacks interpolated on a log scale), for every breed including
norns; one-set breeds as before, but per part. OPENC2E_SENILE_TINT=1
(or =grey,lift) mutes a senile creature's pictures toward grey and
lightens them a little (ImageUtils::Mute, imageManager::muted, cached).

Developer aid: OPENC2E_TEST_LINEUP="Breed,sex\[,x,y,gap,a-b\]" clears
plain objects from a stretch of floor, makes the breed at those stages
(its own genome name per process), poses them standing facing the viewer
on the floor and pauses; OPENC2E_LINEUP_SHOT="x,y,file" then saves a
screenshot there and quits.

#### Script patches after injection; KILL of a gone object does nothing (C1/C2)

[`77277921`](https://github.com/DotairZee/zeetures/commit/7727792165202a35403776615e660f62db625959)

The Additions folders' script-patches.txt was applied only at a world
load, so an object added from the Agent Injector ran unpatched until the
next one. Engine::applyScriptPatches now also runs after each injection
(quietly: already-patched scripts are not reported). Used for the
Anteater's patch (its search for ants, owner, 2026-11-13).

C1/C2: KILL of an object that is already gone (or dying) is a no-op, as
MESG WRIT to one is: the Anteater kills the ant it ate, then kills it
again on its next tick, and openc2e threw and stopped its timer. Test
(copy of Home, Anteater added by hand): patched on adding, it walked and
ate 4 ants in a minute, 0 errors (was 70 in 50 s, never moving).

#### Hatchery card: breeds grown for every life stage, credited to DotairZee

[`df0f685b`](https://github.com/DotairZee/zeetures/commit/df0f685be5dda57fbeced7933fd4ed16cc8f4a0a)

Boney grendels and C2's own ettins have one set of pictures, which
openc2e grows to fit each life stage (OPENC2E_STAGE_SCALE); the card
listed only the age the files are filed under ("baby"). Such a breed now
reads Sprites "every age (one set, grown to fit)" and its By line adds
"grown for every life stage by DotairZee" (owner, 2026-11-13).
BreedGrownForEveryStage() uses the engine's own scaling rule.

#### Sunk-object rescue: wrap across the world's seam; leave creatures on open floor

[`34d8419e`](https://github.com/DotairZee/zeetures/commit/34d8419ef14cc449ad2f06f7e518fb75db46c06d)

A creature against the library's west wall (x ~0, y 2290) has its top-left
wrapped to ~8320, so World::tick's sunk-object rescue took its middle as
past the east edge, inside the sealed pillar (room 213), and lifted it
490 px into room 175: three times in one night in Home and the soak tests
(2026-11-13). Found with BIG MOVE's new stack line (World.cpp, the rescue
every 100 ticks). The middle is now wrapped into a wrapping metaroom, and
a creature whose feet stand in an open room is left alone, as the pocket
safety net already does. Test (copy of Home, creatures put at 4,2290,
three runs): no engine moves, no errors; one walked off across the seam.

#### Mind: following a scent persists

[`89ad211f`](https://github.com/DotairZee/zeetures/commit/89ad211f6db1047b39d24c3713aa0dd899ec45e2)

Owner's choice (2026-11-13, option B): mates started apart never met in
10 fast minutes. Each follow was a 12-tick step weighed afresh against
everything else, and it stopped wherever old scent had pooled, where
"here" smelled strongest. Now a creature that sets off along a scent
(pheromones.txt follow urges) keeps that way with its want raised x1.5
for up to 20 decisions while the urge holds, ending when the scent falls
below half the best it smelled on the way or after 4 decisions with
nothing stronger ahead (so it walks on through a pool of old scent).
Logged as follow:&lt;scent&gt;:on.

Test (copy of Home, 3 adult males at 1953,1200, 3 adult females at
5160,690, ~10 fast minutes): before, no mating; now 15 MATE attempts,
2 conceptions and a baby hatched; 32 of 39 follow decisions kept to a
trail. No script errors; eating and sleeping shares unchanged.

#### Creatures: keep bodies in the room system too; lift wholly onto the floor

[`77bdc355`](https://github.com/DotairZee/zeetures/commit/77bdc35590084b2664ac0771a41701ac23408aa7)

Overnight in Home (2026-11-13) a dead ettin came to rest in the rock under
the upper-east deck's end (7743,968), out of every room, and raised "out
of room system" 737 times until it was cleared: the room safety net only
ran for the living. It now runs for bodies too. Its "inside the ground"
lift also slides the creature wholly onto the floor it lifts it to (as the
sealed-pocket lift already did): a body lifted onto the deck (room 145,
ending at x 7745) at the same x overhung the end and dropped back into the
rock, 34 times in a minute. Test (a copy of Home, the creature killed and
its body moved into that rock): lifted once to 7693,870, then still.

#### BIG MOVE: log the engine's call stack when no script moved the creature

[`a4676f4e`](https://github.com/DotairZee/zeetures/commit/a4676f4e03387c84cf8de62cbbcdcec5caacaa36)

A 10-minute soak of Home (2026-11-13) logged a grendel moved 489 px up at
the world's seam (8324,2290 to 8324,1801) "by the engine", and the pocket
safety net had not done it. When the engine itself makes a big move, the
log now has a "\* BIG MOVE stack:" line of module+offset frames; resolve
with addr2line -f -C -i -e build/openc2e OFFSET (a build changed since
launch needs the binary that ran).

Developer aid: OPENC2E_TEST_BIG_MOVE=1 moves the selected creature 400 px
up from engine code at tick 100.

#### Hatchery: eggs on a shelf look different

[`4be28a78`](https://github.com/DotairZee/zeetures/commit/4be28a780219e2546faf8cb0207cc9e523f3a632)

Each egg keeps its stable picture unless one of the ten before it on the
shelf has it, then takes the next free one: up to 11 eggs (the pictures
there are) never repeat, and a longer shelf never repeats nearby. Bulbous
and Boney had the same egg.

#### C2 physics: the bottom of the map is a floor

[`1a018c93`](https://github.com/DotairZee/zeetures/commit/1a018c9334cae03e42dcb71b95f0f6952d8e8755)

The bottom pool (room 43, floor y 2399) is the map's lower edge. A walking
pumpkin from the Halloween pack (2 6 3) that sank to its floor slipped
2 px below it, out of every room, and raised "out of room system" every
tick (532 times in 50 s). An object below the metaroom's bottom is now
put back on it, as the top of the map already acts as a ceiling.

Found by injecting each of the player's 29 injector objects into its own
copy of Home for 50 s (2026-11-13): 0 errors with this fix (was 532);
a plain copy shows no change.

#### Frogs: Force Ageing for the kiss; turning back is a change, not a death

[`9eecd514`](https://github.com/DotairZee/zeetures/commit/9eecd5141819590c50f0ffc201719dd6beb075d2)

The Frogs COB's adult frog, kissed by a norn, makes a creature from the
"frog" genome (the player's frog.gen: the V3 Frog Norn pack's) of the
other sex and calls SYS: CMND 32865 four times. The game's menu resources
name 32865 Force Ageing: the selected creature now grows one life stage
(as natural ageing, the life chemical moved to match), so the frog norn
is an adult, as intended. Its watcher (2 1 94) KILLs the frog norn when it
strays from the pond and makes a frog in its place; that is now logged in
the life log and on the status bar as "Changed: turned back into a frog,
away from the pond" (lifeLogEntry::changed), not a death (a plain KILL
logged nothing, leaving the record as if alive).

Tested in a copy of Home: the kiss's own lines gave an adult (stage 4)
female frog norn by the pond; moved away, it turned back within 12 s.

#### Agent Injector: build objects into every world, uninstall, put back

[`e18d2c37`](https://github.com/DotairZee/zeetures/commit/e18d2c37a900363bbc1b76065a7eba24d59e80d2)

Owner, 2026-11-13: tabs for the objects in the injector, those built into
every world, and those uninstalled. "Build into every world" adds the
object to the Additions folder's builtin-cobs.txt (every world gets it at
each load) and takes it out of the injector; in the open world, ones the
player added by hand become the built-in ones (no longer saved with the
world), else one is added now. At a world load, a built-in object that a
world already has from being added by hand before adopts those instead of
making a second set. "Uninstall" takes an object out of the injector
(uninstalled-cobs.txt in the user data folder; its COB file is kept).
"Take out of every world" and "Reinstall" undo them. Both moves ask first.

Tested in copies of Home: an Anteater added by hand and saved, then built
in, loads as one Anteater (adopted, then made by the built-in list); a
world without one gets one; take out and uninstall edit the lists, and
the comment written for an entry goes with it.

Developer aid: OPENC2E_TEST_INJECTOR_LIST="build=Name,uninstall=Name,
takeout=Name,reinstall=Name,set=N,select=Name" (writes the real lists).

#### Hatchery: drag to scroll, compact layout, muted burlap

[`3469cbce`](https://github.com/DotairZee/zeetures/commit/3469cbce5adf7474e3d0bc2e63518babfb4cb0a1)

Owner, 2026-11-13: the shelves scroll by dragging them with the mouse
(a press that moves more than 4 px drags; one that doesn't chooses the
egg), by the wheel or a trackpad's sideways swipe, and by the arrows; the
scroll bar is gone. Shelves sit closer (140 px each, was 200), the card's
egg is drawn at 2x, and the window fits its content (about 500 px tall,
was 700). The Sprites row just lists the ages. The hessian wall is less
saturated and a little lower in contrast.

Developer aid: OPENC2E_TEST_HATCHERY_MOUSE=1 drags, clicks and swipes the
first shelf with real mouse events and prints the results.

#### Hatchery card: makers as Creature Labs; Sprites row names the ages drawn

[`64a8ed8f`](https://github.com/DotairZee/zeetures/commit/64a8ed8ff58948ccd6ee67f6ac582fca7cb2ce16)

The Ages row listed the life stages a breed has its own sprites for; it is
now labelled Sprites and says that other ages wear the nearest younger set.

#### Hatchery: the egg nursery

[`75c496eb`](https://github.com/DotairZee/zeetures/commit/75c496eb9961f47d8d3bb360b44d888883224f73)

C2's Hatchery is now an egg nursery: one rough-cut shelf per kind of
creature on a draped hessian wall, each scrolled left and right (wheel,
arrows, or the bar), an egg in a woven nest for every breed, with a worn
card label that keeps the whole name (two lines or smaller type). The
chosen egg lifts under a lamp; its card shows it large, its breed, sex,
genome file, makers and ages, and places it: by the incubator, or in the
incubator, where it hatches at once (it boards the machine and its own
drop script runs, as when the hand lets go of it there). Shelves can be
filtered, searched and sorted (breed slot, name either way, recently
added, imported first, recently placed; the choice is kept). Every egg is
back each time the Hatchery opens.

The wall, board and nests are our own procedural art, generated by
data/hatchery/make_art.py and built into the program
(scripts/embed_files.py, EmbeddedFile()). Labels, shadows and the lamp
are drawn at runtime; egg pictures still come from the player's own
Omelette.s16. A bold system font is loaded at a few exact sizes for the
labels and headings (ImGuiUtils::NurseryFont), falling back to the
default font.

Developer aids: OPENC2E_TEST_HATCHERY="select=Frog,taken=Ron,search=..,
filter=N,place|incubator,reopen" and OPENC2E_HATCHERY_SORT=n.

#### Agent Injector: search, sort, list or tiles, details pane

[`5a37ace4`](https://github.com/DotairZee/zeetures/commit/5a37ace44d7d91ef7926ca4cc49022db71068594)

The owner's approved redesign (agent-injector-mockup.png). A search box
(names, descriptions and kinds); Sort: name A to Z or Z to A, kind of
object (with headings and counts), recently added, imported first, in
the world now, recently used; a List view (thumbnails, badges for
imported objects and ones added to this world) or Tiles; and a details
pane with the picture, kind, source, date added, how many the player
added to this world, the classifier it makes, the description, and Add
to world / Remove from world. Double-clicking an object adds one.

CobManager now reads each C2 object's description, its kind (the
category of the first classified object its install script makes,
passing over genus-1 helpers), the file's date and whether it was
imported, and keeps when each was last added (injector-used.txt). The
sort and view are kept in settings.txt. Developer aid:
OPENC2E_INJECTOR_VIEW="sort,list|tiles\[,search\]".

### 12 November 2026

#### Creatures: survive mutated genes (pigment colour, rates, quantities)

[`1b716dc1`](https://github.com/DotairZee/zeetures/commit/1b716dc1fdc61aa6ee45b7c2a21148a8b0be1258)

Since NEW: GENE works, every ettin and grendel egg gets a self-cross
genome with the usual mutations, and Home crashed when one hatched
(genome zxyi): a mutated pigment gene named a colour past tintinfo's
five slots, and the write overwrote the creature's next members, which
its mind then read. Fixed with a check (red, green, blue only). A stress
test at 40 times the mutation rate found two more: an emitter with rate
0 (modulo by zero) and a reaction with quantity 0 (division by zero;
only asserted, and asserts are off in this build). Rates and
quantities of 0 are now taken as 1. Also an off-by-one: floating locus 8
was allowed (8 slots, 0-7). After the fixes, three runs of 12 heavily
mutated norns, grendels and ettins each ran with no crash.

#### Developer aids: print the word list; dump the injector's thumbnails

[`37f74db9`](https://github.com/DotairZee/zeetures/commit/37f74db9561e8f919b3e7e2f0efa9deace2a43a9)

OPENC2E_PRINT_WORDS prints the game's word list (C2: the object category
nouns are words 16-55). OPENC2E_DUMP_COB_PICTURES=&lt;folder&gt; saves each
Agent Injector object's thumbnail there, with list.txt naming them.

#### C2: eggs are carried by the top of the egg

[`4ae8fb5d`](https://github.com/DotairZee/zeetures/commit/4ae8fb5d694e6afd9432232cf6cd22e037699268)

A carried C2 object hangs by its picture's top-left corner, and an egg's
picture has empty space above and beside the egg, so the hand and
creatures held eggs well away from them. An egg (2 5 \*) is now held by
its topmost painted pixel (the middle of that row), found once per
picture and kept. Developer aid: OPENC2E_TEST_HAND_PICKUP="family genus
\[species\]" has the hand pick up such an object and prints where it is
held.

#### Hatchery: one egg of every breed, sex unknown

[`b69ca2c4`](https://github.com/DotairZee/zeetures/commit/b69ca2c4f4756489d8799c7435bceb8eb2cb6a91)

The Hatchery held the ten eggs of C2's Eggs folder. It now holds one egg
of every breed with a head and a body (norns, then grendels and ettins;
18 in Home), each the breed's genome (its own, or the species' standard
one) wearing the breed's parts, as the Breeder's Kit lays them. Every
egg shows its sex as unknown; it is drawn when the egg is placed in the
world, from the sexes the breed has pictures for. Tiles show the breed's
name, the tooltip its full label. Import Genome still adds eggs.

#### Birth photos: stand the posed creature on its floor

[`30212b46`](https://github.com/DotairZee/zeetures/commit/30212b462bb2c76c499fdd3f571d59b7ed05b75c)

A portrait re-poses the creature in place (its corner kept), and the
portrait poses reach lower than the pose it was standing in, so the
photos showed creatures sunk into the ground. The posed creature is now
moved so its lowest foot is where its foot stood. The portrait test aid
(OPENC2E_TEST_PORTRAITS) runs 30 ticks into the run instead of at world
tick 30, which a restored world never meets.

#### Breeder's Kit: credit the breed packs' authors

[`3fb1a20c`](https://github.com/DotairZee/zeetures/commit/3fb1a20ce6746f678a24c5cac1346fae144c4775)

The Breeds page now says who made each imported pack: Frog Norn V3 by
Alex (Norns of the Round Table), an overhaul of Nornenmeister's V2;
Midsummer Norns by Martina Hilsenbeck (Nimue) and Amanora, genome by
Katira; Goat Norns by Angela Allan; Kai Norns by Wafuru (pictures, and
the milk bottle), genome by Lis Morris; Scorpio Norns by Wafuru
(pictures and body data), genome by Mannkind; the Boney Grendels an
official Creatures 2 release by Creature Labs, updated by Lis Morris.

#### Sealed pockets: lifts join rooms; rescue from any sealed group

[`c3896106`](https://github.com/DotairZee/zeetures/commit/c389610654946040070cbf3686cb198d30abd156)

The safety net grouped rooms by open doors and only rescued creatures
from pockets of one or two rooms, taking bigger groups as places
creatures might be brought on purpose. The tunnel under the bridge
(rooms 100, 101, 104, 105), sealed by an addition's door change, was
such a group, and a grendel starved there. Lifts now join the groups of
the rooms at their stops (the cage lift makes the airlock's underground
west side, ten rooms closed by doors, part of the world), and a creature
in any group outside the world is rescued. On today's map that changes
nothing; re-sealing the tunnel on a copy, a creature put in the cave was
lifted to room 93 above. OPENC2E_LIST_POCKETS also lists every lift's
stops with their rooms and groups.

## Version 0.3.0

Renumbered past openc2e's last release (v0.2, June 2020), since the base already ran 825 commits past it. Hand, toolbar and menu work, sealed-pocket rescue, speech and walking fixes.

### 12 November 2026

#### About: version 0.3.0, the openc2e base, and openc2e's developers

[`35cfb114`](https://github.com/DotairZee/zeetures/commit/35cfb114355befe3f7148fd575dddf34c4e236c6)

The fork started from openc2e at commit dfcd46f2 (6 October 2026), an
unreleased development build 825 commits after openc2e's last release,
v0.2 (June 2020). The fork's version goes from 0.1.0, which read as
older than that, to 0.3.0. The About box names the base commit and
credits openc2e's developers from its AUTHORS file and history: Alyssa
Milburn, Michael Maltese, Bryan Donlan, Jeremy Apthorp, Eric Goodwin,
Dylan William Hardison, Sven Eckelmann and Bertram Bourdrez, with
contributions from expwnent, Michael Lynch, Shreddan, pushfoo, Jelle De
Loecker and callym.

#### Mind: walk by gait when stepping gets nowhere; walk watch aids

[`d3151bc3`](https://github.com/DotairZee/zeetures/commit/d3151bc36323cc1d566393df0df74fe34e038889)

7YZS, a senile grendel, starved in the cave under the bridge (room 100)
with its only exit 700 px west. Travelling west or east runs the world's
stepping scripts (26/27: poses 61-64 as an animation); for it they moved
nothing, and the mind took both ways for walls. The homeostat now
switches a creature to its gait walk (walk-a/b, scripts 42/43, WALK)
after a stepping walk gets nowhere while its senses show the way open
(no wall, water or drop), and keeps that in its saved state (gaitwalk).
7YZS itself was beyond it: starving, its gait walk (gait 5) also ran in
place.

Developer aids under OPENC2E_WATCH_CREATURES: "WATCH GAIT" (each change
of walk cycle and its poses) and "WATCH WALK" (a walk's flags every 20
ticks); OPENC2E_TRACE_CREATURE_EVENTS traces creatures' own script
events.

#### Sealed pockets: set creatures wholly on solid floor

[`1cbf54f4`](https://github.com/DotairZee/zeetures/commit/1cbf54f4704560c84131b57fda7fe4041bef1880)

A creature at the east end of the dock (room 13 ends at x 6565) overhung
the open sea beyond, fell through by its corner into the sealed pocket
under the dock (room 31) and was lifted straight up to the same place,
again and again: 3RPA, starving, was lifted 1,228 times in 20 minutes.
The lift now slides a creature sideways so its whole width is on the
floor it is set on. The sideways move, tried first, no longer picks air
over open water (room 140, east of the dock, has the sea through its
floor; a creature moved there dropped to the seabed).

#### C2: MCRT centres a creature on x

[`71934e76`](https://github.com/DotairZee/zeetures/commit/71934e7691920f64bd5631c07e4588ee36d468cf)

MCRT was MVTO, which moves an object by its top-left corner. C2's
teleporters pass the destination pad's centre (POSX, POSY), so a
teleported creature arrived to the right of the pad, half off the small
pads' ledges. A creature is now centred on x (top at y; the placement
sets its feet on the floor). Other objects move as before.

#### Toolbars: movable bands, as in C2

[`9a131058`](https://github.com/DotairZee/zeetures/commit/9a131058bb1bc30e729b989038c69fc4fee3830e)

The Standard, Pointer, Applets and Favourites toolbars are now bands,
each with a grip (two etched bars) at its left. Dragging a grip moves the
band: onto another band puts it before that one, past a line's end adds
it there, and above or below the others gives it a row of its own; a
marker shows where it will go. A band that doesn't fit in the window
wraps onto the next line (with Text Labels on, the toolbar ran off
narrower windows). The layout is kept in settings.txt (toolbar-layout,
rows split by ';', bands by ','); hidden bands keep their places.
Developer aids: OPENC2E_TOOLBAR_LAYOUT sets a layout, and
OPENC2E_TEST_TOOLBAR_DRAG="band,x,y" drags a grip with the mouse
(without saving).

#### Menus: keep the View, Camera and Sound switches between runs

[`78372df5`](https://github.com/DotairZee/zeetures/commit/78372df550e1982cef7231a9d91a33033eb40aad)

The four toolbars, Text Labels, Status Bar, ToolTips for Objects, Scroll
Bars, Smooth Scrolling, Ambient Music, World Sounds and Bibble are kept
in the player's settings file (settings.txt in the user data folder,
beside Speech Bubbles and Autosave): read at the first frame, written
when changed in a menu. Headless tests that set their own view
(OPENC2E_VIEW, OPENC2E_OPEN_KITS) don't read them.

#### Speech: chatter can't loop forever when a creature knows every word

[`71df9edc`](https://github.com/DotairZee/zeetures/commit/71df9edcfb41267213e433b790dbc162bc772f9d)

Idle chatter drew random words until it had one to three the creature
didn't know. For a creature that knows every word (the Stone of
Knowledge teaches them all; 6PBN in Home knows all 82) the draw never
ended, freezing the game; Home needed force-quitting twice. The hang
reports (ce2c56c1) caught it in phraseFor/isWord in the live game and
in three headless copies. Chatter now picks from the words it doesn't
know, and a creature that knows them all tries out a known word.

#### Hang reports: print the main thread's stack when frames stop

[`ce2c56c1`](https://github.com/DotairZee/zeetures/commit/ce2c56c1886263b306bd487f94eaacb65b394c71)

Home froze twice (force quit) with nothing in its log, and ptrace is
restricted here, so a debugger can't be attached after the fact. The main
loop now beats once a frame; a watcher thread, after 8 s without a beat,
signals the main thread (SIGUSR2), which prints its own stack to stderr
("\* HANG: ...", offsets for addr2line) and carries on; again every 20 s
while still stuck, and "\* HANG: over" if frames resume. The crash printer
shares the frame printing (no demangling or allocation in the hang case).
Developer aid: OPENC2E_TEST_HANG=N spins the main loop N seconds at tick 50.

#### C2 menus: full screen, text labels, object tooltips, norn indicator, smooth scrolling

[`72fee6df`](https://github.com/DotairZee/zeetures/commit/72fee6dfa56c549dac7de43fb1a6efeabffbc6ae)

The View and Camera menus' remaining items now work:
- Full Screen (F12, and the menu): the backend gains isFullscreen and
  setFullscreen (SDL borderless at the desktop's size).
- Toolbars &gt; Text Labels: each toolbar button's name under it.
- ToolTips for Objects: resting the hand on a creature names it; on an
  object, says its kind with the game's own word for its category.
- Norn Indicator: the halo over the selected creature, as the toolbar's
  Halo button (the game's own macro); the halo code is shared.
- Smooth Scrolling: the interface's go-to moves (favorite places, next
  creature, the Creatures menu, kit go-to and look-at, the Ecology map,
  the Hatchery) glide instead of jumping.

Camera panning was a TODO: moveTo with smoothscroll (or
smoothscrollifvisible, within a screen) now starts an eased glide of
0.3-0.6 s on the main camera, stepped once per frame drawn; any plain
move ends it. Developer aids: OPENC2E_VIEW="labels,tips,smooth" and
OPENC2E_TEST_PAN="dx,dy".

#### Mind Kit: how its last choices turned out

[`4c0d7dcb`](https://github.com/DotairZee/zeetures/commit/4c0d7dcb90ec00c647c5140bcdc3ce30b0e65e43)

The homeostat mind keeps its last eight episodes (what it did or went to
find, how long, why it stopped, the overall reward and each need's
change) in MindView::outcomes. The Choosing page lists them newest first
under the options it weighed, each with a bar from a middle line (green
to the right when its needs eased, red to the left when they grew), the
two needs that changed most, and every change on hover.

#### C2: MONK and NEW: GENE work (clones, splices, monster eggs)

[`c8f0cfca`](https://github.com/DotairZee/zeetures/commit/c8f0cfca937b331ae95d11937a178b87ba71a82e)

MONK returned 0 and NEW: GENE copied its first argument, so the
Metamorphosis device threw at NEW: CREA 0 (after harming or, in its
humane version, killing the donor), the gene splicer could not work, and
every ettin and grendel egg hatched an exact copy of the stock genome.

MONK now returns the creature's moniker as an integer. NEW: GENE crosses
the two named genomes (a living creature's own or a Genetics file) the
way a conception does, stores the result under a new name and returns
it. A creature whose mother and father are the same moniker shows in the
Owner's Kit as cloned from that creature.

#### Creatures keep their footing: placed creatures fall, saves keep the feet

[`47eafb5d`](https://github.com/DotairZee/zeetures/commit/47eafb5df7f0f0b94a8da3fd31bea17f6603dfed)

- A creature just placed (put down, let out of a vehicle, moved by a
  script) whose floor is far below now falls to it instead of being set on
  it in one step (after a world's start).
- A placed foot a few pixels under a floor stands on it, not on the room
  below.
- The save records where a creature's feet were (foot), and a restore puts
  the feet back there: a grendel saved lying down came back in its
  standing pose with its feet 29 px under room 34's floor and dropped
  366 px to the room below at every load.

#### C2 physics: the top of the map is a ceiling

[`092932ae`](https://github.com/DotairZee/zeetures/commit/092932ae896c2c6a9bf08066cacd71370ed37079)

The east seabed's launcher (a spring toy in launch mode) throws a creature
so hard that its top went above y 0, out of every room, logging "out of
room system" until it fell back. An object moving above the map's top now
bumps it and falls; the launch still lands the creature on the shore by
the east dock.

#### Life stage chooser: from the Creatures menu to the Owner's Kit

[`e3e1b74e`](https://github.com/DotairZee/zeetures/commit/e3e1b74ee7d345a9e10ff796cd1ea032651b798b)

The selected creature's life stage is set on the Owner's Kit's
Registration page, under its ID card, for a living creature (owner,
2026-11-12); the Creatures menu no longer has it. It still changes a
stage at a time, as natural ageing.

#### C2: UNCS and INJR work

[`5b029a30`](https://github.com/DotairZee/zeetures/commit/5b029a30f8208afcda7aa4ea01d0612e1e74f227)

UNCS read always 0, so an unconscious creature still walked and reacted:
the creature scripts for walking (10, 11) and for being patted, slapped
and pushed (0-3, 12) ask "doif uncs ne 0 stop". It now reads the engine's
own unconsciousness, and UNCS 1/0 faints the creature or brings it round
through the same path (logged, in its life story). UNCS is a C2 command.

INJR did nothing. It now takes amount/255 of one organ's full life force
(-1: an organ at random) from its short-term life force, which the
organ's repair brings back: a slap from the hand (10), hard landings (up
to 150) and an object that strikes creatures.

#### Test aid: type words beside a creature (OPENC2E_TEST_TYPE)

[`c8eef239`](https://github.com/DotairZee/zeetures/commit/c8eef239f165fd8aa7a39642f70fed566e117cdb)

OPENC2E_TEST_TYPE="N,words" puts the hand beside the selected (or first)
creature at tick N and types the words through the engine's own key
handlers, then Enter, so the hand's bubble opens, fills and is spoken as
when the player types; OPENC2E_TEST_TYPE_REPEAT=R repeats it every 30
ticks. It logs the creature's familiarity with each typed word. Test: a
youth heard "lift" and "toy" 53 more each time and learned both on the
third hearing; a word not in the word list was ignored.

#### Breeder's Kit: move an imported breed to any free slot

[`ae824d6c`](https://github.com/DotairZee/zeetures/commit/ae824d6c996eab3b3fc36135dfc6d34f4e7cbe63)

MoveBreedTo generalizes MoveBreedToNornSet: an imported breed goes to any
free slot in its own set or another (files copied under the new names,
its pack's genomes and eggs rewritten, the registry updated). The kit has
a "Move to &lt;set&gt; slot" box for imported breeds already in their set. Test
aids: OPENC2E_MOVE_BREED="0,ak,0,ka" moves one breed;
OPENC2E_TEST_BREED_LAY=1 lays an egg of the breed shown. Used to put the
Kai norns at ka, the slot the owner asked for, past the old iv limit
(owner, 2026-11-12).

#### Breed slots past iv: 16,384 per species

[`3cca4af9`](https://github.com/DotairZee/zeetures/commit/3cca4af9fb8d174b7421339127dd4e3ad96010fe)

A C2 genome's appearance gene names a body part's breed slot in one byte,
so the last slot a genome could name was iv (255). The gene's species byte
only ever held 0-3; its upper six bits now carry the slot's upper bits
(geneBreedSlot/geneBreedSpecies/setGeneBreed in BreedSlots.h), giving
256 x 64 = 16,384 slots per species, with every C2 genome reading as
before (slots up to iv write exactly C2's bytes). Slot names go on past zz
as aaa ... (file stems up to 6 characters). Every reader and writer of the
appearance gene uses the helpers: body sprites and face, the Breeder's Kit
(breeds, rewriting genomes), creature import, the Science Kit's dDNA. A
missing picture in a high slot falls back to the game's 26 slots rather
than stepping down through thousands. Owner, 2026-11-12: "I want that
fixed."

#### No interface calls without an interface context (-k null crashed)

[`0a3fdd96`](https://github.com/DotairZee/zeetures/commit/0a3fdd96dda7df956322cccdae814a2801aa3718)

The null backend creates no ImGui context; Openc2eImGui::Init used the
style anyway and crashed at startup. Init, Update and the viewport
offsets now do nothing without a context.

#### Hatchery: each egg's breed name, as the Breeder's Kit names it

[`6225eb08`](https://github.com/DotairZee/zeetures/commit/6225eb083e4b76130d81ebcc498d24ff0cb65f5e)

The egg tooltip said "Female egg (256i)", the egg file's own code. It now
gives the breed its genome looks like (its head's appearance gene), named
as in the Breeder's Kit: "Female egg (Golden Desert)" (owner,
2026-11-12). BreedNameOfGenome in Breeds.cpp; imported genomes too. The
log lists the Hatchery's eggs and breeds when it loads.

#### C2 lifts: take the stop from where the cage is when asked to move

[`a2d185ae`](https://github.com/DotairZee/zeetures/commit/a2d185ae30ff90cc3ef879364bf2c8fc1ef9f039)

A lift keeps the index of the stop it is at, which could disagree with
where the cage really is (after a world loads, the cage is where the save
or the world file left it). A request from a creature or the hand then
picked its next stop from the wrong one: refused (no stop further up) or,
once moving, the stop check fired at once and snapped the cage with its
riders to a far stop (a rider on the west cage lift jumped 417 px). Now a
lift standing still with no call waiting takes the stop nearest its cage
before choosing the next. Test (copy of Home): a norn rode the west cage
lift up and down with the cage, step by step.

#### Safety net: look across the seam, and accept areas joined by lifts

[`c682e21d`](https://github.com/DotairZee/zeetures/commit/c682e21dda3ae7f83bf7d98d0f100055e144e8de)

A creature in the sealed pocket at the far east edge (room 183) has the
far west's room 180 beside it across the seam; the sideways search used
the map's own room lookup, which does not wrap, and only the largest
group of rooms joined by open doorways counted as somewhere to put it,
which leaves out areas joined to the rest only by lifts (the far west,
rooms 117-125 and 180-185). It now searches the wrapping metaroom and
accepts any group bigger than a two-room pocket. Test: a norn put in room
183 was moved beside it into room 180 (x 1) instead of 900 px up.

#### Safety net: trust the feet, and look beside a pocket before above it

[`fd5a1856`](https://github.com/DotairZee/zeetures/commit/fd5a1856f3a4ac1286f63f9141022b404ff21988)

keepInRoomSystem judged a creature by the middle of its body: one pressed
against the far east wall (x 8240) had its middle 2 px into the sealed
pocket beyond (room 183) while its feet stood in the room it was in, and
was lifted 900 px to the surface. A creature whose feet stand in a room of
the world is now left alone, and one really in a pocket is first moved to
the nearest room of the world beside it (within 80 px) before the search
upward.

#### Dock edge: stop a stride short; room-change watch names the script

[`1b60c3ed`](https://github.com/DotairZee/zeetures/commit/1b60c3edf8f250db31bc88ff552daa6780db9d10)

Walking up to an object, the edge stop measured from the foot on the
floor (within 14 px), but the feet take turns and the front one lands a
stride on, past the edge: creatures still stepped off the east dock's end
in their act-on-object scripts. Now within 40 px of the edge. Test (copy of
Home, creatures put at the dock end, 40 s): held at x 6550, no falls; in
longer runs the only steps past the end are boardings of the moored boat,
which ferries them to the far shore.

OPENC2E_WATCH_CREATURES room changes now name the script the creature is
running and whether it is walking.

#### C2 creatures: stop at a dock's edge, unstick hung falls, log big jumps

[`a8db3fe3`](https://github.com/DotairZee/zeetures/commit/a8db3fe39a3206741cda6f5f261f8013938fdcd5)

- Walking up to an object (APPR), a creature at its floor's edge does not
  take a step that ends in water it cannot breathe in; it touches what it
  can from the edge (one walked off the east dock's end reaching for an
  object there and sank).
- A falling creature with a corner in rock that belongs to no room could
  not move (physics checks the four corners) and hung in the air: a frog
  norn stepped off the upper-east deck's end with its side over the rock
  below. After four ticks without moving it is eased away from that side.
- Diagnostics: a creature moved more than 300 px in one go is logged with
  the script (and its owner) that moved it, or the engine (BIG MOVE). A
  norn went from land to the east seabed in 18 ticks with no trace.
  g_runningVM names the VM whose op is running.

#### Kits in one style of their own, without C2's panel art

[`e0daf559`](https://github.com/DotairZee/zeetures/commit/e0daf559eb19e42c23307493fd166d1216927ed4)

Owner's choice (2026-11-12) after mock-ups: the kit pages drawn on C2's
Applet Data pictures now use the kits' own flat style.
- Shared: KitBar is a flat bar in green, amber or red; new KitHeading,
  KitStatTile (a reading with a trend line), KitBeginCard/KitEndCard
  (gold-edged panels, as the Family cards) and KitCreaturePhoto. Pages
  with no creature selected say so instead of showing a C2 cover.
- Health: Fitness shows overall health with its weakest organ explained,
  temperature, pulse and life stage tiles with trends, and the five
  weakest organs; Mind shows what it is doing and its strongest needs;
  the Doctor's medicines (and the Breeder's aphrodisiacs) are tiles.
- Owner's: Registration is an ID card (photo, name, born, age, parents,
  children, moniker) with our own drawn seal once registered; Photos are
  framed thumbnails instead of a film strip.
- Breeder's Reproductive: the creature's photo with its bars, pregnancy
  progress and hormones as bars.
- Science Syringe: our own drawn syringe, same animation.
- Graveyard: a memorial card instead of the headstone.
- Hatchery: eggs as tiles with their sex, instead of the incubator picture.
Pictures of the game's own items (eggs, medicines, add-ons) stay.

#### Mind: give up at once on objects past a wall

[`452c4ce9`](https://github.com/DotairZee/zeetures/commit/452c4ce9f351a4fd4c689754ef276beb1497ba7f)

As for objects past water: a stalled reach for an object on the side
where the creature stands against a wall is given up after one try.
Creatures pressed against the east wall of the dead-end pocket at the
end of the lava corridor (x ~8240), reaching for critters bouncing
against it, three tries for each.

#### C2: reopen a world where the view was, at its zoom

[`104580f6`](https://github.com/DotairZee/zeetures/commit/104580f638f5018ccfd9e28dac72fea9ca4c7a28)

The saved view (its top left) is now saved with the zoom, and applied
again once the world has finished starting: the built-in objects
installed at load end their install scripts with SYS: CAMT, which moved
the view after the save had restored it, so a world always opened
somewhere else at 100%. Snapshots (OPENC2E_SNAPSHOTS) take -1,-1 for
wherever the view is.

#### Water ahead counts small steps into water; notices open kits only where useful

[`c784cd1d`](https://github.com/DotairZee/zeetures/commit/c784cd1dc84689ab7c0abb6d0064acefcd81c473)

Water ahead: a step to the side that ends in water now counts however
small the step. The east dock ends 24 px above a floor that is the sea's
surface, open to the water below, so a norn following a food smell walked
off it and sank: only drops over 60 px were followed down. The creature
watch log (OPENC2E_WATCH_CREATURES) reports changes to the sense.

Status bar notices: clicking "unconscious" or "came round" goes to the
creature without opening the Owner's Kit (owner: not needed there); births
still open Registration. Each notice's tooltip now says what a click does
(deaths said Owner's Kit but open the Graveyard).

#### C1/C2: DIRN of no creature reads 0

[`a307736b`](https://github.com/DotairZee/zeetures/commit/a307736ba22b5076a635b0b0a722bce3b12a3bad)

Object 2 13 2's timer turns it the way its carrier faces with TARG CARR
then DIRN, also while nothing (or no creature) carries it. That threw
"Invalid agent handle" about once a second (1,299 times in one Home
session). With no creature targeted, DIRN now reads 0 in C1/C2, which
such scripts treat as no direction.

#### C2: room values for objects above every room come from the room below

[`70f23c3b`](https://github.com/DotairZee/zeetures/commit/70f23c3ba3ecffbcf2ca2fac7673ccd302267972)

TEMP, PRES, WNDX, WNDY, LITE and the other room-value commands threw when
their object was in no room. C2's clouds rise above the highest rooms as
they build up rain and read WNDX and TEMP there, so their timer script
failed every run (over a thousand errors in a few minutes) before it
reached the check that turns them into rain, and some rose off the top of
the map for good. An object above every room now reads the air of the
room below it (from the map's top edge if it is above the map). ROOM
still answers -1 for such objects.

#### Mind: give up at once on objects that lie past water

[`18170a10`](https://github.com/DotairZee/zeetures/commit/18170a10c3cd20edf100ab5b16d5f5984d3e5cdc)

A stalled reach for an object counts as hopeless after one try, not
three, when the object is on the side where the way goes into water the
creature cannot breathe in. A hungry norn stood at a pool's edge for
about two minutes reaching for food floating in it, three tries for each
kind of food it saw. The log notes when this rule applies.

#### C1/C2: MESG WRIT/WRT+ to a missing agent does nothing

[`8368352a`](https://github.com/DotairZee/zeetures/commit/8368352a002721866eba335eff0f8348825387c2)

The creatures' eat-after-pickup script (4 0 0 28) tells the food it is
eaten, the food deletes itself, and the script then tells it that it is
dropped. openc2e threw "Invalid agent handle" there, cutting the script
short before IMPT 0 and DONE, dozens of times a session. In C1/C2 a
message to an object that is gone now goes nowhere; C3/DS is unchanged.

#### Hide speech bubbles of creatures riding in invisible vehicles

[`9f4e15f5`](https://github.com/DotairZee/zeetures/commit/9f4e15f53ac392c35a188d4a0a65c934b46d87d1)

A creature carried out of sight (in a vehicle with ATTR 16, such as the
west sea airlock's hidden car) shows no speech bubble, and a bubble it
already had is removed when it is taken in. Its voice is still heard.
The speech watch log notes speech made out of sight.

#### An agent with no part has no size instead of crashing

[`8167d42d`](https://github.com/DotairZee/zeetures/commit/8167d42d9a678dd303a6fb3d15e4817ef6c9d16d)

A vehicle made by NEW: VHCL gets its picture from NEW: PART; GPAS on one
without crashed reading its width (found building the west sea airlock,
2026-11-12). getWidth/getHeight are 0 for an agent with no part yet.
OPENC2E_DUMP_AGENT also prints an agent's first three object variables
(OV00-OV02), for following a script's state.

#### The hand stays drawn across the world's seam when zoomed

[`525b73ae`](https://github.com/DotairZee/zeetures/commit/525b73ae1b56e854d3d8faa8987cbb0ac8710e74)

Albia wraps (its east end joins its west), so while a view straddles the
seam the hand's x can be small while the camera's is near the world's
width. At 100% the world drawing wraps and the hand showed; zoomed, the
hand is drawn over the scaled world by a plain subtraction, which put it
thousands of pixels off screen (owner: the hand vanished while panning
with the middle button at any zoom but 100%; found from the owner's log
of a real drag). handViewX takes the short way round a wrapping
metaroom; Ctrl+wheel zoom anchors on the hand the same way. The
temporary middle-button log is removed.

#### Middle-button log and wheel-pan test aid, for the vanishing hand

[`c38dc809`](https://github.com/DotairZee/zeetures/commit/c38dc8096e44831ab817ac3cda1d7793dead713c)

The owner sees the hand vanish while panning with the middle button at
zooms other than 100%; synthetic middle drags and wheel pans (through
the backend) keep it drawn. While the middle button is held, home.log
now gets "\* MIDDLE" lines: the button and wheel events, mouse moves, and
every tenth frame whether the hand was drawn (zoom, over a window and
which, visible, its place on screen, the view's size, the interface's
mouse position).

Developer aid: OPENC2E_TEST_WHEEL_PAN="N,ZOOM,FILE" scrolls the view for
20 ticks with the pointer still, as a TrackPoint's middle button + stick
does.

#### DROP no longer clobbers a self-dropping object's script; test aids

[`7401c078`](https://github.com/DotairZee/zeetures/commit/7401c07804d81d1fe54132e5f1047350fe3f141e)

C2 runs a creature's drop on the carried object's own script, so the
object's script can call DROP on itself. DROP fired the object's dropped
script (5) at once on that same VM, replacing the running script
mid-command: every time a creature put an egg down the log showed
"Stack imbalance detected" (the egg still hatched). When the carried
object is the VM's own owner, its dropped script now runs from the
world's script queue at the next tick; other drops stay instant.
The imbalance error now names the command.

Developer aids:
- OPENC2E_TEST_EGG_DROP=N: a creature egg by the incubator, then its
  dropped event (reproduces the above).
- OPENC2E_TEST_MIDDLE_DRAG="N,ZOOM,FILE": a middle-button drag through
  the backend at a zoom, with pictures during and after.
- OPENC2E_TIMER_STATS=N: each kind's timer rate, runs and skips (found
  two add-on watchers running 3-8 times slower than set).
- OPENC2E_DUMP_AGENT now lists simple agents' sprites too.

## Version 0.1.0

The first five days of the fork: the pluggable creature mind interface and the homeostat mind; C1/C2 creatures interacting with objects; breeding from courtship to hatching; world saving; the C2-style menus, toolbars and kits; speech and Nornish; temperament and relationships; map fixes and engine crash, deadlock and hang fixes. The version number first shown in Help > About.

### 12 November 2026

#### Tools menu in the toolbar's order; Help menu with About openC2e

[`99b5f845`](https://github.com/DotairZee/zeetures/commit/99b5f845b3508affad46794cb99421a238ecf5f6)

Tools lists the Hatchery and every kit in the applet toolbar's order and
groups, adding the Graveyard, Mind and Ecology Kits it lacked.

Help keeps what can be offered: Help Topics (the Help window), openc2e
on the Web, and About openC2e (the fork's version, 0.1.0, by DotairZee,
with openc2e's credit and licence). C2's items we cannot provide are
gone. The Help window's kit list covers every kit.

Test aid: OPENC2E_OPEN_KITS="about" opens the About box.

#### Observation Kit and Agent Injector fit their content

[`2fc39bb2`](https://github.com/DotairZee/zeetures/commit/2fc39bb2cc33e426c817490c13e1f3410d590697)

The Observation Kit was the one kit sized by hand, its tables
stretched to fill a remembered 867 px. It now fits like the others:
Details columns are as wide as what they hold, and the Alarms intro
wraps without widening the kit.

The Agent Injector opened too short to show the line under its
buttons. It now fits what it holds; its preview is the list's width
(it had filled the window, which then could not shrink), and import
reports wrap at that width.

#### British spelling in the game's text; + for favourite places

[`3ed7ce03`](https://github.com/DotairZee/zeetures/commit/3ed7ce0310c86b2e78df6bb348174d440b8ea0dd)

The owner chose British English for everything the player sees, to
match C2's own text (2026-11-12): Favourites, Add/Remove Favourite
Place, the place list's hover, Minimise, the Mood page's colour. The
mind's "toward &lt;moniker&gt;" key is unchanged; the Mind Kit shows it as
"towards &lt;name&gt;".

The favourites toolbar loses its Go button (choosing a place already
goes there) and gains a + that names a new favourite place, as the
Camera menu does; disabled when all six are named.

#### Kits: review fixes, one health measure, shared creature header

[`4fc4b5bd`](https://github.com/DotairZee/zeetures/commit/4fc4b5bd4872651da7c56acf8bb63a380c9ecf08)

From a review of every kit page side by side.

- Health is the weakest organ everywhere (Health Kit, Observation Kit,
  status bar), through KitOrganHealth.
- Health, Science, Breeder's, Owner's and Mind Kits share a creature
  header: a list of every living creature with previous/next arrows.
- Places away from a favorite place are named by their part of the
  metaroom ("low down in the east"), in KitWhereAt for every kit.
- Text stays at the left and long hints wrap without widening their
  window (KitNote); pictures and charts are centered (KitCenterNext):
  the birth certificate, the Relationships map, the Feelings dial.
- Health Kit's Mind page: the developer readout and the stale
  Neuroscience Kit line replaced by an "Open the Mind Kit" button.
- Breeder's Kit: stale "mating not built yet" line removed.
- Observation: things named by the game's word for their kind, ages
  in parts of a day, health hover explained.
- Science: chemical 0 hidden, long organ names wrap, brain genes
  marked inert for creatures with their own mind.
- Life Story and History columns headed; History fits "Unconscious".
- Ecology: pinned rooms saved with the world; the Census explains its
  egg count; the Hatchery says "No creature eggs".
- Hatchery and Agent Injector chips carry their toolbar icons.
- American spelling in menus (Favorites) and comments.
- Test aid: OPENC2E_OPEN_KITS selects any creature when no norn lives.

#### Ecology Kit: room map, monitor and census

[`8637d19f`](https://github.com/DotairZee/zeetures/commit/8637d19f39b49c297ef314c452ec75210933e36b)

A new kit on C2's Ecology Kit button, drawn with the color scale and
pins from the player's own EcologyKit.exe (read at runtime):

- Map: every room shaded by light, temperature, air pressure,
  radiation, organic or inorganic nutrients (and wind, when the world
  has any), from the lowest room to the highest. Hover a room for all
  its readings, click to go there, right-click to pin it. Creatures and
  food can be shown on top.
- Monitor: up to seven pinned rooms graphed over the last 15 minutes,
  with the time of day and how long until the next part of the day.
- Census: everything in the world by the game's own categories, how
  many of each can be eaten, and the food from west to east.

Scriptorium::hasOwnScript tells whether a kind has an eat script of its
own, not one inherited from its family. The map picture and the place
names are shared with the Mind Kit (KitMapPicture, KitWhereAt).
Developer aids: OPENC2E_ECO_PINS, OPENC2E_ECO_MEASURE, OPENC2E_ECO_FOOD.

### 11 November 2026

#### Mind Kit: Feelings centered around its dial; Mood has a labeled y axis

[`d39af630`](https://github.com/DotairZee/zeetures/commit/d39af63044ef25f7082295a5d04ff01f9eb319a3)

Feelings: the dial in the middle of a narrower page, the words above and
below wrapped and centered on it, the key centered (owner). Mood: the
contentment axis labeled 0-100% (owner).

#### Mind Kit Feelings shows each need's pull; status bar health made real

[`2e12d216`](https://github.com/DotairZee/zeetures/commit/2e12d216225d611f159455dec2068c14fe581c17)

Feelings (owner's choice) now shows how hard each need is pushing the
creature: its level to the mind's power times how much the need matters
to this creature (temperament changes some weights), with the plain level
faint behind; hover gives the level, how much it matters and its share of
what is pushing it. The Health Kit's Drives keep the body's levels.

Status bar health (owner: is it real?): the bar was the creature's
glycogen and the heart never beat. Now the bar is its weakest organ's
strength, flashing while it is unconscious or an organ is failing; the
heart beats, faster with adrenaline, slowly while unconscious, blue when
dead; both say what they show on hover.

#### Hand hidden over windows; Memory map key shows the marks; no day tooltip

[`5b62451b`](https://github.com/DotairZee/zeetures/commit/5b62451b44bd4672413660a38c638578f65ebd95)

While the mouse is over a kit, the toolbar or any other window, the hand
is not drawn (it went on moving behind the kit, owner). The Memory map's
key draws each mark as it appears on the map (the triangle for where the
creature is, the fading dots, the food dot, the ride line), and the marker
names itself on hover. The status bar's day has no tooltip (owner).

#### Mind Kit in place of the Neuroscience Kit; status bar pictures with tooltips

[`fc9c1328`](https://github.com/DotairZee/zeetures/commit/fc9c1328bf608e8770b2441f52ac2cc868b93e31)

The Mind Kit (owner's request; C2's Neuroscience button opens it) shows
the selected creature's mind in its own terms, from a snapshot the mind
gives on request (CreatureMind::view, MindView):
- Feelings: the 17 needs around a dial, colored by kind (body, rest,
  company, mind), and how content it is
- Choosing: the options weighed at its last choice, each bar split by the
  needs it should ease, plus curiosity, with the reason in words
- Learned: what doing each thing to each kind of thing (or a creature it
  knows one by one) does, against its usual result; tooltips give the
  changes in each need
- Memory map: its place map, remembered food and lift rides over a small
  picture of the metaroom
- Mood: how content it has been over the last 15 minutes and its
  strongest need then
Things are named from the game's word list; actions from the world's
creature scripts.

Status bar (owner): the season picture, the time-of-day picture and the
temperature picture, each named on hover (the temperature as a feel and
the number; click for Celsius/Fahrenheit), then Day and Year; the season
and temperature text are gone.

#### Rescue creatures from sealed pockets; time of day named on hover

[`b8a167bb`](https://github.com/DotairZee/zeetures/commit/b8a167bbe3fcaf364c9499b09a9ca4c4d9e30479)

A norn lay trapped in room 208, a pocket under the incubator lift's shaft
that only solid (permeability 0) doors join to anything (owner). The room
safety net now groups rooms through open doors (the largest group is the
world, refreshed every minute) and lifts a creature whose feet are in a
pocket of one or two rooms outside it to the floor of the first room
above that belongs to the world; bigger cut-off groups are left alone (one
of 12 rooms may be reached on purpose). After any such lift the feet find
their room afresh: they kept the pocket's floor and the norn fell straight
back. Tested on a copy of Home: lifted once to the shaft's bottom, then it
walked out west and back. OPENC2E_LIST_POCKETS lists the rooms outside the
world.

Status bar: hovering the time-of-day picture names it: Dawn, Morning,
Midday, Dusk, Night (owner's words; C2 has none built in).

#### Light: no edge at the world's seam; weather solid again

[`7660b796`](https://github.com/DotairZee/zeetures/commit/7660b796334a55c9d1f775650ee0d25a1b6f9c4d)

The outdoors map's widening and smoothing now wrap across the seam of a
wrapping world (stopping at the picture's edges left a hard edge in the
light by the volcano, owner), and reach 250 px either side, so the ground
under a palm canopy counts as outdoors. With the light now a filter over
everything (owner: "so perfect"), clouds and weather are drawn solid
again (undoes 681e542c's checkered see-through).

#### Birth portraits show the hatchling's strongest temperament trait

[`3c101902`](https://github.com/DotairZee/zeetures/commit/3c10190299837292e9fae3a9e53d5977694246cb)

Owner's idea: the pose and face come from the creature's strongest trait
(|z| 0.9 or more, as Temperaments::describe takes it), its high or low
end: bold (chest out, tail high), shy (turned away, peeking back), curious
(looking up, reaching out, interested), incurious (half asleep), energetic
(mid-stride), placid (eyes closed, smiling), sociable (arms open, big
smile), aloof (nose in the air), quick to anger (an angry stare),
easygoing (relaxed smile), highly strung (startled, ready to bolt),
steady-nerved (a calm look), persistent (reaching on ahead), flighty
(about to dash off); a plain hello when no trait stands out. No arm is
raised, so nothing covers the face (owner).

#### Birth portraits: each hatchling's picture, posed, on the bare background

[`5a3c050d`](https://github.com/DotairZee/zeetures/commit/5a3c050d244f374cb296dde1504c3031b4f91a24)

Owner's idea: ten ticks after a creature hatches (C2), a 240 x 180 picture
is made of it alone on the bare background where it was born (no objects
or other creatures), happy, in one of six poses chosen by its moniker:
hello (facing you), wave, cheer, wave facing left, looking up, arms out.
C2 has one picture per part facing you, so the lively poses are side views
with the face turned to you. Small creatures are drawn at twice their
size, framed with their feet near the bottom. The picture goes first in
its photo album.

SkeletalCreature: memoPose/restorePose put a creature back exactly after
posing it; poseInPlace poses without moving the feet. drawWorld: a bare
mode. OPENC2E_TEST_PORTRAITS=&lt;folder&gt; draws every pose of every creature
(OPENC2E_TEST_PORTRAIT_POSES tries other pose strings).

#### Light through the day as a filter over the whole view; east shove mirrored

[`ee312a72`](https://github.com/DotairZee/zeetures/commit/ee312a72765145a925a874262105b3f97ba6cc43)

Owner's idea: instead of recolouring the traced sky, everything in the
world view (the unmodified background, objects and creatures) is
multiplied by a colour for C2's time of day: rosy at dawn, warm at dusk,
dark blue at night, none in the morning and afternoon, fading over about
30 seconds. Caves and the underground get half of it: the sky trace now
only says how much of the outdoors each spot is (full down to each
column's lowest sky, fading over 400 px below), kept as a 1/8-size map
stretched smoothly over the background. The hand and speech bubbles stay
unfiltered; creatures' photos are taken without it.

Renderer: a multiply blend and smooth scaling option, and stretched images
are culled by the size they are drawn at.

Push hand: C2 has no shove drawn eastward (it plays its button-press
click), so a shove east is the west shove mirrored, a nudge with the back
of the hand (owner); the arrows are compensated.

#### Push hand: C2's own shove animation, facing the push

[`414d9237`](https://github.com/DotairZee/zeetures/commit/414d9237f8aa8ae3bd13e88082bd01076b30e735)

Watched in real C2 (the hand's pose read over DDE about six times a
second while the owner pushed a grendel): in push mode the hand stays the
usual pointer while hovering, with the arrows showing the way; a click
plays frames 8, 9, 8 for a shove west (palm turned west) or 1, 2 for a
shove east, about half a second, then the usual pointer again. The hand
no longer holds frame 9 the whole time. OPENC2E_TEST_HAND_PUSH also plays
both shoves and prints the poses.

#### Sky tint: grade the whole outdoor scene, keep the sky out of the sea and dunes

[`48f97615`](https://github.com/DotairZee/zeetures/commit/48f9761596357a192a196f583437164842705c9e)

The time of day now lights the whole outdoor scene gently, down to each
column's lowest sky and fading below, with the traced sky in its own
stronger colours. Fixes from play:
- the sky seen through the bell tower's arches is found by comparing with
  the local sky colour on a 32 px block grid
- the sea below a long, sharp horizon is cleared of the trace, and the
  halo pass no longer cascades through sky-coloured water
- traced haze below each column's lowest blue sky takes the scene's light
  (no orange flecks in the dunes at dusk); small islands are dropped
- texture is measured over inner sky pixels only, and sky pixels are wholly
  sky, so nothing standing against the night sky gets a pale rim
- the night sky's brightness is smoothed over 15 px
- OPENC2E_SKY_DUMP writes each layer over the background for checking

#### Graveyard Kit; toolbar tooltips all "Name: what it does"

[`c2b107b3`](https://github.com/DotairZee/zeetures/commit/c2b107b3d1d7fb37f7ac48e813417b4bad07db62)

Graveyard Kit (owner, 2026-11-11; after C2's Creature Graveyard, on C2's
gravestone picture from the player's Applet Data): a stone for each
creature that has died, newest first; step through them or pick one by
name (with its cause). On the stone: a photo the owner picks from its
album (the newest by default), set in an oval; its name; when it hatched
(or was first seen) and died; its age; what it died of; and the owner's
epitaph. Epitaph and photo choice are kept with the records of the dead
("epitaph", "gravephoto"). The toolbar's gravestone button opens it, a
death notice opens its stone, and the Chronicles' history has a "Grave"
button for each death. With no deaths it shows C2's graveyard cover.
OPENC2E_GRAVE=&lt;moniker&gt; opens at that stone (developer aid). The Owner's
Kit's photo loader is shared (KitPhotoTexture).

Tooltips: every toolbar button now reads "Name: brief description" (the
kits, next creature, eye view, follow, halo, invisible hand, web, zoom had
bare names or descriptions only).

#### Chronicles: what each death was from, in plain words

[`53ba6787`](https://github.com/DotairZee/zeetures/commit/53ba67877816b457cd322b12c92ffa9b97d2f8e7)

Owner, 2026-11-11. A death in the life log now keeps its cause: "old
age", "starvation", "drowning", "organ failure", "illness" (the genome's
own death signal, C2's dead locus) or "unknown causes" (killed by a script
or a kit); saved with the world ("life-cause"). The Chronicles' History,
the status bar's death notice and the Life Story show it; deaths recorded
before causes were kept read "old age" or "cause not recorded".

Tested on a copy of Home (fast): the starving norn died, and the saved
log has "life-cause starvation" with its death.

#### Death and the rescue window: unconscious, then dead unless the cause is mended

[`fed42fb1`](https://github.com/DotairZee/zeetures/commit/fed42fb1081af667b3633faa7b1cb18005c7fac6)

Owner, 2026-11-11 (after real C2: a drowned norn fell unconscious first and
could be saved if pulled out quickly). Until now creatures died only of old
age; starving and drowning bodies sat at zero energy for ever.

- Organs take the damage their genes give for running without energy
  (organ gene "damage due to zero energy", applied per activation without
  enough ATP; openc2e had left it a TODO). Kept as a float: it overflowed a
  byte. (Was stash "organ damage at zero energy (WIP)", 2026-11-07; the
  stash itself is kept.)
- A C2 creature out of energy (ATP, chemical 99, at 4 or less) for 300
  ticks falls unconscious: C2's own faint (creature event 70: lying down,
  groaning) runs again as it ends, and its mind stops. Once ATP stays at
  40 or more for 100 ticks it comes round.
- If an organ's lasting health reaches nothing, it dies, of "drowning"
  (under water, out of breath), "starvation" (hungry) or "organ failure";
  the Life Story says so ("Died of starvation.").
- Rescue: out of the water, a Health Kit injection, or hand-feeding: food
  the hand drops on an unconscious creature runs its own eat script (event
  12) for that creature.
- Status bar notices "Unconscious" (red) and "Came round" (green); Life
  Story lines for both. OPENC2E_WATCH_CREATURES also traces the
  unconscious.

Tested on copies of Home: a norn out of energy collapsed; given glucose
and glycogen it came round; left alone (fast mode) its weakest organ went
from 39% to nothing in about 4000 of its ticks after the collapse and it
died of starvation.

#### C2 weather drawn see-through, checkered like the invisible hand

[`681e542c`](https://github.com/DotairZee/zeetures/commit/681e542cda6745502ecbda5f3960a12f841760e3)

Every picture from C2's weather sprite file "weat" (clouds fair to stormy,
rain, snow, lightning) is now drawn stippled, so the sky's colour through
the day shows through the clouds at dawn, dusk and night (owner,
2026-11-11). Set when a sprite part is made or changes picture.

#### The sky changes colour through the day (C2)

[`0bd365cd`](https://github.com/DotairZee/zeetures/commit/0bd365cd94deff7813ca7fe20bd142bfad5b7db0)

Owner, 2026-11-11. The painted sky is found once per background: grown from
sky-coloured pixels on the picture's top edge through smoothly changing
colour, plus sky walled in by what stands in front of it (matched against
the sky within 300 px along the row: the painted sky runs from blue in the
west to pink in the east) and the clouds painted in it. Three tinted
versions are made, cropped to the sky (8352 x 769 in C2's Albia, under a
second at load): dawn (rose gold), dusk (orange-red) and night (deep blue,
with stars). Each frame the layer for C2's time of day is drawn over the
background before any agent, so only the sky changes; morning and afternoon
show it as painted; a change fades over about 30 seconds. All of it is made
from the player's own background; nothing is shipped or saved. The weather
clouds are agents and keep their colours.

Developer aids: OPENC2E_SKY_LAYER=0/1/2 shows dawn, dusk or night;
OPENC2E_TEST_HAND_PUSH="x y" turns on the push hand at a spot (for
snapshots).

#### Push hand: C2's shoving pose and direction arrows

[`b335778e`](https://github.com/DotairZee/zeetures/commit/b335778ebc14ab25758c12c052d7549aa682b827)

While the Pointer toolbar's push is on, the hand holds the edge-on shoving
pose of C2's own hand sprite (syst frame 9; the owner's pick of the three
open-palm poses), and over a creature it shows which way a click would
shove it with the sprite's arrows: frame 14 (east) right of the hand, frame
13 (west) left of it, drawn as C2 draws the angel and devil. The pose before
push mode comes back when it is switched off (owner, 2026-11-11).

#### Status bar: the day of the season

[`afe3f591`](https://github.com/DotairZee/zeetures/commit/afe3f59186f18282b6a7e7b697493e35806ea08e)

"Day N" between the season and the year (C2's seasons are four days long),
with a tooltip (owner, 2026-11-11).

#### Homeostat mind: a starving creature expects no recovery from rest or sleep

[`4da460f9`](https://github.com/DotairZee/zeetures/commit/4da460f93d1941b62efac2ece5afb0240e913033)

In C2's chemistry a starving body does not recover from tiredness or
sleepiness, whatever the rest and sleep scripts report (a sleeping norn at
hunger 254 kept Tired 136 through 1292 ticks; a resting one kept Tired 223
through hundreds of rests). The mind trusted those reports, so a starving,
worn-out norn rested 335 times in a row instead of finding food (owner,
2026-11-11). At hunger 0.85 or more, no action is now predicted to relieve
Tired or Sleepy, so food comes first.

Tested: on a copy of Home the starving, exhausted norn ate 19 times in 3
minutes, ranging x 5681-8207 (resting was not among its top choices). Batch
(8 runs): meals 65.6 per run (best so far; 59.8 with the seam fix),
starving share 0.02, creatures that ate 5.5 of 6.

#### Homeostat mind: a tired creature's slow shuffle is not a blocked way

[`c9fcb8c8`](https://github.com/DotairZee/zeetures/commit/c9fcb8c861770f5878b8d312bc07c257a406a31a)

Past Tiredness 174 a norn's genome switches it to its tired walk, a shuffle
of a few pixels a step. Walks that moved under 3 px counted as blocked, so
a starving, worn-out norn by the far-east lift marked both ways blocked
over and over and rested 335 times in a row (owner, 2026-11-11). With the
tired walk on, only a move under a fifth of that counts as blocked.

Tested on a copy of Home: the same norn shuffled both ways and rode the
lift up a level (it was standing still before). It still rests far more
than it should: the world's rest script reports relief the starving body
never gets, and the mind trusts that report (next step).

#### Homeostat mind: a weak food lead no longer stops a hungry creature searching

[`dab67843`](https://github.com/DotairZee/zeetures/commit/dab67843489f7d76b683b198c6a7d1883eebb298)

Foraging stood aside whenever any food trip or smell was on offer, however
little it was worth. A norn starving in the world's bottom-right corner
(hunger 252) remembered two food places it had all but given up on
(confidence 0.06); their trips were worth almost nothing, so resting
(action 23) won every time: 135 choices in a row (owner, 2026-11-11).
Foraging now steps aside only for a lead worth at least as much.

Tested on a copy of Home: the same norn searched at once, found the corner
closed both ways, used its button (a lift) to leave, and went on searching
in the west caves.

#### Amphibious: judge by where the genome's air and oxygen come from

[`ee3133f0`](https://github.com/DotairZee/zeetures/commit/ee3133f00c39bf0937ce48cfbae2ca5dd3061c55)

C2 bodies get oxygen (chemical 95) from air (96), which an emitter makes
from the air sense (locus 9, emptied under water). The Frog Norn pack did
not remove that emitter: it moved it to the always-on sense (locus 0). A
creature now breathes water when some emitter makes air or oxygen from
anything but the air sense, or when nothing reads the air sense, so a
breed that keeps its usual emitter and adds another source also counts.
Read from each genome as it is: old genomes need no changes (owner,
2026-11-11). A line is printed for each such creature.

Checked over all 56 installed genomes: water-breathers are the Frog Norn
pack's genome, the game's own frog.gen, and one bred genome that inherited
the frog's emitter (b02m); in Home, the frog norn (7ILK).

#### Physics: lift objects lying wholly under a water room's bed onto it

[`afd770f1`](https://github.com/DotairZee/zeetures/commit/afd770f17dc00a6a8d89c4b38784f482251b07a6)

The floor check that lifts an object found under its room's floor line
skipped water rooms (fish near the banks were set on the bed). Objects that
sank below a bed then stayed in the painted earth: a beach ball under the
bog (owner, 2026-11-11). In water an object is now lifted only when it lies
entirely beneath the bed line, which a swimming fish never does. With the
bog's new bed line (Additions bog.cos, our own script) a ball buried at
5231,2361 came to rest on the bed at y 2339.

#### Amphibious creatures: a genome with no air-sense emitter breathes water

[`85d36c3e`](https://github.com/DotairZee/zeetures/commit/85d36c3e68a545d4f073569d05cc3f004baa4a1c)

Owner, 2026-11-11: frog norns should breathe air and water, as their genome
says. The Frog Norn pack's genome has no emitter on the air sense (C2:
creature, sensorimotor, locus 9); every stock genome has one (it makes
chemical 96, which becomes suffocation chemical 95). So its body never
suffocates, but the engine and the mind still treated it as drowning.

- c2Creature::breathesWater() reads that from the genome once.
- Under water such a creature holds no breath, keeps its air sense full
  and blows no bubbles.
- The mind knows (perception breathesWater): water is not an emergency
  (no escape urgency) and it may sleep in water.

Tested on a copy of Home: in the boardwalk pool the frog norn made no
escape choices in a minute (it played with a toy); an ordinary norn there
chose to escape in 8 of 9 choices.

#### Hatchery and Agent Injector: minimize to the status bar like the kits

[`482b1537`](https://github.com/DotairZee/zeetures/commit/482b1537a43df6390fc4a7a02462667459e01992)

Both get the kits' minimize button in the title bar (owner, 2026-11-11);
minimized, each is a chip on the status bar that restores it, and opening
it again from the toolbar shows it. Test aids: OPENC2E_OPEN_KITS may name
"injector"; OPENC2E_KITS_MINIMIZED starts these two minimized as well.

#### Hatchery: remove the breed dropdown

[`bf45354a`](https://github.com/DotairZee/zeetures/commit/bf45354a2d73871d32c193f9767b9ae6e059514e)

"As in the egg" and what the dropdown did (recast every egg taken in a
chosen breed) were unclear (owner, 2026-11-11). Eggs from the Hatchery now
hatch as laid; breeds are chosen in the Breeder's Kit.

#### Egg locator: creatures' eggs only, not the world's grendel and ettin eggs

[`b800cd61`](https://github.com/DotairZee/zeetures/commit/b800cd61967a490562805c569f44d8d27cf4a4cc)

The Hatchery's Find eggs (and the egg-laid notices' lookup) now take only
2 5 2 eggs, those laid by creatures or taken from the Hatchery, leaving out
the grendel and ettin eggs the world makes itself (2 5 3 and 2 5 4; owner,
2026-11-11). In Home: one egg, where there were 22.

#### Creatures found inside the ground are lifted back into the room map

[`f30cbcd6`](https://github.com/DotairZee/zeetures/commit/f30cbcd63d075a775c4e9366d68e0490c315a570)

An old norn swimming in the underground pond (room 43) sank through its bed
into the rock below (owner, 2026-11-11). Agent::physicsTick's floor check
leaves water rooms alone, so nothing brought her back, and her mind took
being outside every room for dry land and learned a button as the way out
of the water.

- About twice a second, a creature outside every room (not held, not
  riding) is moved to the nearest room straight above it, a dry one if
  any is within 800 px, else water, and stood on its floor; a line is
  printed.
- The perception says whether the creature is inside a room; the mind
  learns water exits, and remembers being in water, only inside one.

Tested on a copy of Home: a norn put under the pond's bed (5378,2452, where
she was found) and one under a dry floor were both lifted onto the floor
above.

#### Hatchery: fit the window to its contents so Find eggs shows; creature eggs first

[`cc4c1e2f`](https://github.com/DotairZee/zeetures/commit/cc4c1e2f00e1a5b6d8b2d33e49bf04a9217d0fae)

The Hatchery kept the size it had before the egg locator existed, and with
no scrolling the Find eggs button sat below its edge (owner, 2026-11-11). It
now sizes itself to what it holds. The locator visits eggs with a creature's
genome before the grendel and ettin egg pile, its tooltip gives the counts,
and an egg with no timer reads "not counting down yet". Test aids:
OPENC2E_OPEN_KITS may name "hatchery"; OPENC2E_TEST_FIND_EGG presses Find
eggs once.

#### Homeostat mind: a place map for routes to food; hunger keeps creatures awake

[`f1ce6d64`](https://github.com/DotairZee/zeetures/commit/f1ce6d64190299118a84d637753b034701947889)

Place map (a sense of place, owner 2026-11-11). Each creature learns the
world in 120 x 100 cells and the ways between them it has found under its
own power (walking, climbing, dropping; a level step is assumed to go back
too), with what each cost, plus its remembered lift rides and device
travel links. When the best remembered meal is on another floor, or the
walk to it is barred (water, a wall, a drop), it searches the map for the
meal best worth its way and takes the first step: a walk, a device, or
going to a lift, calling it and riding. Steps that meet a wall count
against that way; a route that gets no shorter for 600 ticks rests that
place. Saved with the mind (pedge lines). Routes everywhere crowded out
searching (batch norns kept going back to emptied spots), so a meal along
the same floor is still walked to directly.

Starving creatures stay awake (starved animals sleep less and forage; flies
and rats both show it): at hunger 0.85 or more sleep is not chosen, and a
sleeper is woken, unless sleepiness or tiredness is exhausted (0.97). A
Home norn had slept away 300 s at a time at hunger 254: the sleep script
felt restful but its drives never fell.

The perception now says when the hand carries the creature (not walking).

Tests. Home copy (5 min): of four starving norns, three ate (before: the
sleeper and one other never did); the fourth fell into water and is still
trying to climb out. Batch of 8 (start hunger 180): starving share 0.03
(seam fix alone 0.01, baseline 0.07; noise about 0.02), recoveries 0.88.

#### Eggs: laid-egg notices, a Hatchery egg locator, script patches; autosave

[`d9117f31`](https://github.com/DotairZee/zeetures/commit/d9117f3101637acbb2cb4b617fd623ccfba280c0)

Owner, 2026-11-11 (a laid egg was lost):
- The status bar announces "Egg laid" (gold). Clicking it goes to the egg
  wherever it is now (found by the baby's genome name, the egg's OBV0),
  or to the baby once it has hatched; the notice stays until it hatches
  or is dismissed. Its tooltip says where the egg is and whether it is
  waiting (in water, timer stopped). Saved with the life log (life-egg).
- Hatchery: "Find eggs (N)" goes to the next unhatched egg in the world
  on each click, with its place and state.
- Script patches: script-patches.txt in the Additions folders (scrp,
  find, replace) patches the world's own scripts at load, on the
  player's machine. The project uses it to stop the norn egg's drop
  script halting the timer, so moved eggs hatch.
- Autosave every 5 minutes by default (File &gt; Autosave: 1-30 minutes or
  only on quit; settings.txt "autosave-minutes"). Not with
  --no-autosave.
- The toolbar's web button opens https://openc2e.github.io/.
- Test aids: OPENC2E_TEST_LAY=N (the first pregnant creature lays at
  tick N); OPENC2E_TEST_CONCEIVE also skips the liking check.

Tested on copies of Home: a laid egg's notice saved with its genome; a
moved egg's timer kept running and it hatched; two autosaves in 140 s at
1 minute.

#### Old brain: a dendrite whose source lobe is missing is left unconnected

[`35e200b1`](https://github.com/DotairZee/zeetures/commit/35e200b12d366956913c1d27906d0fb7cb61f061)

oldLobe::connect asserted that every dendrite's source lobe exists. A bred
genome can name one it lacks (a crossover or mutation moved the source, or
the lobe gene was lost), so hatching such an egg stopped the game: Home
crashed when the owner put a laid egg in the incubator (2026-11-11). Those
dendrites are now cleared and the rest of the brain is built; a line is
printed. Tested: the same egg hatched (missing source lobes 10 and 12).

#### Homeostat mind: give up on futile steps; measure distance the short way round the seam

[`f3153dea`](https://github.com/DotairZee/zeetures/commit/f3153dea41f59ee0295da76baf6421be8bb67c84)

Futile steps. A norn on the eastern shore chose "row west" on a boat it
could not reach thirty times in a row while it starved: steps toward a
destination learn nothing themselves, so nothing told it to stop. An
action on an object that ends stalled (never got nearer) with the
creature within 30 px of where it began, three times in a row, is now
not offered within 400 px of that spot for 1500 ticks. A food trip's
progress also needs the creature itself to have moved, so a food place
that shifts as food falls no longer keeps an unreachable trip alive.

The seam. C2's world joins its far west edge to its far east, and the
body already walks across (f2b92bee), but the mind compared positions
flatly: a norn at x 26 read food at x 8300 as 8274 px east rather than
78 px west, and dithered at the seam. The perception now carries the
wrap width, and every east-west distance and direction in the mind goes
the short way round (dxTo, wrapD).

Batch (8 runs, start hunger 180, 16 samples of 25 s), against the
baseline: starving share 0.07 -&gt; 0.01; far east 0.34 -&gt; 0.04; creatures
that ate 5.0 -&gt; 5.9 of 6; meals 54.8 -&gt; 59.8 per run.

#### Out of the room map: tracked apart from the debug core, cleared when back

[`390513d9`](https://github.com/DotairZee/zeetures/commit/390513d99b329966aedfeeaf3bc2dc87ac5e1409)

An object physics finds outside the room map was marked by turning on
its debug core drawing (a magenta diamond and joint points round it),
which stayed on for good and players saw (owner). It is now tracked by
its own flag, cleared as soon as the object is back inside (so it is also
tried again for nudging back in), and logged with the object's name; the
core is drawn for it only with OPENC2E_SHOW_CORES (or the CAOS debug
command).

#### Breath bubbles start at the snout, higher when looking up

[`841c53ea`](https://github.com/DotairZee/zeetures/commit/841c53ea8701666fabe0d367169bddf59553b3ae)

Owner's rule, chosen from previews of every head angle: 3 px outside the
head's visible edge on the side it faces, level with its snout tip (the
middle of the head picture's outermost column on that side); looking up,
higher: at least 45% (slightly up) or 25% (fully up) of the head's height
from its top, or the head's mouth point if higher. A head picture's
visible box and snout tips are worked out once per frame and kept.

#### Breath bubbles start three tenths of the head's height up

[`a6d29199`](https://github.com/DotairZee/zeetures/commit/a6d29199c3f425f5d31ea8cc6e777550a8347f98)

Owner: 30% of the head picture's height above its bottom corner, 3 px
out on the side the creature faces.

#### Breath bubbles start a fifth of the head's height up from its corner

[`14d44db8`](https://github.com/DotairZee/zeetures/commit/14d44db8aac0588cd882ada3b5ccabc576dc4c10)

Owner: not at the head picture's bottom corner but about 20% of its
height above it, still 3 px out on the side the creature faces.

#### Breath bubbles start beside the head, on the side it faces

[`3ed59584`](https://github.com/DotairZee/zeetures/commit/3ed5958407cf478533985857dbf773870bff564c)

Owner's rule: a breath bubble starts 3 px outside the head picture's
bottom corner on the side the creature faces (bottom-right facing right
or toward the viewer, bottom-left facing left or away), its bottom level
with the head's. SkeletalCreature::headBox gives the head picture's box
and view. OPENC2E_TEST_BUBBLES=1 blows a bubble every few ticks, for
looking at them.

#### Breath bubbles rise from the mouth

[`65106cc7`](https://github.com/DotairZee/zeetures/commit/65106cc7f8318817ad464fe7d5715ad2e737aec5)

The game's bubble object (2 1 27) has no movement of its own, so a
creature's breath bubbles hung where they were blown while it sank or
walked on, away from its face (owner). Breath bubbles are now marked and
the world makes them rise about 2 px a tick with a slight wobble, popping
at the surface or after about 10 seconds; they start at the mouth.
OPENC2E_DEBUG_BUBBLES logs the mouth position used.

#### Breeds: norn breeds into norn slots, Geat; toolbar: tooltips, Speech History, Help, Web

[`31c7f9c9`](https://github.com/DotairZee/zeetures/commit/31c7f9c9060506afa86d026c3ea566bb61eb8606)

Breeder's Kit: C2's fourth species set is called Geat. A norn breed whose
pack filed its pictures in another set can be moved into the norn set (its
letter if free, else that letter followed by a, b ..., or a, b ...
followed by it, since slots end at "iv"): its pictures and body data are
copied under the norn set's names and its genomes and eggs rewritten; the
old files stay for creatures hatched before and are not listed. The
importer installs such packs in the norn set, asking for a letter on a
clash. OPENC2E_MOVE_NORN_BREEDS=1 moves the installed ones.

Toolbar: tooltips for the Hatchery, Agent Injector, play, pause, Speech
History, say, Places and go buttons. Speech History lists what creatures
and the hand said lately; the say button has the hand say the chosen line
again (creatures nearby hear it). The scroll button opens the Chronicles
Kit; Help opens a short guide; the web button opens the openc2e project.

#### Life Story page and the Chronicles Kit; breeds in letter order

[`5bc277e4`](https://github.com/DotairZee/zeetures/commit/5bc277e40a4a4ac6dec6f7b224e66ac5ea9820e5)

Each creature's records keep its own life story, saved with the world:
hatched, life stages, pregnancies and fathering, laying the egg, children
hatching, first word and word counts, friends, dislikes and fears, photos,
death. The Owner's Kit shows it as a Life Story page in place of the
world-wide life log, which moves to a new Chronicles Kit (after C2's own)
with statistics over every creature the world has had. The Breeder's Kit
lists breeds in slot-letter order within each kind.

#### Test aid: OPENC2E_TEST_CLICK can press the right button

[`fd277226`](https://github.com/DotairZee/zeetures/commit/fd277226e77bd4a7bd2732b159b01bac6c556de4)

"N,X,Y,right" presses the hand's right button (pick up or drop), for
checking what the hand can move.

#### Teaching machines teach: edge distance, one press per word, repeating

[`47a9e5d8`](https://github.com/DotairZee/zeetures/commit/47a9e5d8e8e13f0f974525fb11b4fffd4527c0c0)

A creature counts as looking at a teaching machine when attending to it or
within 120 px of its edges (was 150 px of its centre; the computers are 157
px wide). A lesson hearing counts 64, so one press (two hearings) teaches a
word. A creature being taught says the word back: in its own tongue until
it has learned it, then the word, as C2's creatures repeat what the machine
shows. OPENC2E_TEST_BLACKBOARDS=N lists the world's blackboards.

#### Temperament signature behaviours

[`d32609a0`](https://github.com/DotairZee/zeetures/commit/d32609a011b787e5ae2dd9f72dbac89acd84cf05)

The bold are drawn to strangers and the shy put off (scaled by how
strange), and shy creatures back away from strangers close by; sociable
creatures go toward company, more toward those they like; hot-tempered
creatures hit other creatures sooner, more when angry; energetic creatures
pace about and placid ones settle when nothing presses. Each scales with
the trait's z-score.

#### Breeder's Kit: list breeds by kind of creature; names for the game's breeds

[`74f5286d`](https://github.com/DotairZee/zeetures/commit/74f5286d4a8e73758a6a7057d2e9ed9565656670)

Breeds are grouped by what their genome's genus gene makes them, not by
the picture set their files are filed in: pack authors put norn breeds in
the grendel set or the unused fourth set (Midsummer, Goat, Kai), so those
showed as grendels and "species 4". The breed's page says where its
pictures are filed. The game's own breeds take default names from the user
data folder's breed-names.txt (species|slot|name).

#### Owner's Kit Temperament: neutral bars, a colour for each trait

[`ffb59d81`](https://github.com/DotairZee/zeetures/commit/ffb59d8128b16e224fdaa1482cee7cce8123c673)

Each trait's bar fills in its own colour to where the creature stands,
with a mark at the middle (average); nothing reads as good or bad.

#### Individuals in the mind, alongside kinds

[`7024c14f`](https://github.com/DotairZee/zeetures/commit/7024c14f035b7d1601c11e737daa73d001c9691b)

The engine gives the mind every creature in sight (not only the one it
notices for each category), each with its moniker and what this creature
feels about it (bonds), and lets the mind attend to a particular one. The
homeostat mind keeps expectations per individual and decision beside the
per-kind ones; a choice about a particular creature blends the two by
experience (weight n / (n + 3)), so a stranger is judged by its kind and a
known creature by its own record, and both are learned from each encounter.
A feared creature close by is backed away from. Individual memories are
saved with the mind.

#### Breeder's Kit: choose the egg's sex when laying an egg of a breed

[`0935d11e`](https://github.com/DotairZee/zeetures/commit/0935d11e377546c9950198fcec892d5337a51d3d)

Either (at random among the sexes the breed has parts for), female or
male; a sex the breed has no parts for is greyed out.

#### Temperament: seven inborn traits, applied to mind and body

[`18e65463`](https://github.com/DotairZee/zeetures/commit/18e65463fdb059a5641030a25956ae1afdf65a82)

C2 under a mind: each creature draws a temperament at hatching,
regardless of genome: boldness, curiosity, energy, sociability, temper,
reactivity and steadiness, continuous scores (every creature has some of
each), loosely linked through one hidden factor. Each scales the mind's
settings (drive weights, novelty, restlessness, exploration, mood,
learning from bad versus good outcomes, commitment), walking pace and how
bonds grow and sour; survival needs are left alone. Saved with the world.
The Owner's Kit gains a Temperament tab.

#### Relationships: per-pair bonds and the Owner's Kit Relationships page

[`e44dff5f`](https://github.com/DotairZee/zeetures/commit/e44dff5f00e66761782f3e3893b6257eb2c58f54)

C2 under a mind: each creature keeps, for each creature it has met and
per direction, familiarity, recent feeling, a slow affinity meter, fear,
labels (friend, enemy, mate, raised together) and a tally of what happened.
Felt help from another creature's stimulus raises it; harm lowers it three
times as hard and adds fear; time together builds familiarity, and liking
when lonely. Bonds act back on conception (refused between those who
dislike or fear each other, likelier for liked partners, rarer for those
raised together), sex drive, fear and stress, and loneliness for an absent
mate. Saved with the world.

The Owner's Kit gains a Relationships page: a map around the selected
creature (nearer is better liked, line halves show each direction) and a
list. OPENC2E_WATCH_BONDS logs bond changes.

#### Pheromones: scents in the rooms, rules from pheromones.txt; kin avoidance

[`d1494327`](https://github.com/DotairZee/zeetures/commit/d1494327a10ac53322083a12fc599e64fdcf4d91)

C2 under a mind: named scents carried by the room system (spreading
through doorways, fading), given off by creatures (by kind, sex, life stage
and conditions), objects, and agents a creature felt harmed or helped by
(marks). Smelling a scent can add a body chemical (primer) or urge the mind
to follow it or move away from it (releaser), weighed against its other
needs. A creature does not smell a scent it gives off. All of it is data in
the user data folder's pheromones.txt; without the file nothing changes.
OPENC2E_WATCH_SCENTS logs scents, marks and urges.

Close kin (one descends from the other, or they share a parent) are barred
as mates while either has an unrelated partner of its kind alive: no
conception, and not counted as the other sex in sight.
OPENC2E_TEST_KIN=N prints kin status for every pair at tick N.

#### View &gt; Speech Bubbles, remembered between sessions

[`57cde74a`](https://github.com/DotairZee/zeetures/commit/57cde74a6eb93184f32f908c32d5f04c7ca3a994)

A checkbox under View turns creatures' speech bubbles on or off (voices and
word learning go on either way; turning it off clears the bubbles showing).
The choice is kept in the user data folder's settings.txt, a small
key-value file for player settings.

#### Creature vocabulary, Nornish bubbles, bubble-free photos

[`2508daa6`](https://github.com/DotairZee/zeetures/commit/2508daa6ee94f8759f59bc900fe08ccbf8f2d5d7)

C1/C2 creatures under a mind no longer speak English until they learn it.
Each creature keeps how familiar each word of the game's word list is
(oldCreature::vocab, saved with the world). Words are learned from learning
machines (BBD: EMIT, formerly a stub), the hand's typed speech, other
creatures' learned words, and all at once from SYS: CMND 32777 (C2's
Instant Vocabulary, used by the Stone of Knowledge) and VOCB.

A word not learned is shown in the creature's own tongue: the syllables its
voice file speaks for the word (Nornish, and the grendel and ettin
dialects). The voice still speaks every word that way.

Owner's Kit photos leave out speech bubbles. OPENC2E_TEST_PHOTO=N waits
until world tick N.

#### C2: no floor-line lift inside water

[`f093b36b`](https://github.com/DotairZee/zeetures/commit/f093b36b9a51af36d221489cb85226fbd52c864a)

A water room's floor line is a bed for creatures to wade along (the pond
by the waterfall, for the hidden cave path); near the banks it rises to
the surface. The lift that sets a moving object under a floor line back
onto it set fish swimming there on the line, at the surface and out of
the water (owner, 2026-11-10: fish stuck above the pond). It no longer
applies in water rooms. (The pond's open sides, through which fish also
swam out onto the bank, are handled by a world script watcher.)

#### C2: restart a stalled metabolism; select creatures under other things

[`642318a3`](https://github.com/DotairZee/zeetures/commit/642318a3340abb0754be6cd42fe2b28902974bcd)

Metabolism (owner, 2026-11-10: a grendel ate berries but was never
satiated): it had ATP 0 with starch 255 and fat 255 in store. Every C2
organ needs ATP to run, so once ATP reaches 0 nothing can digest food or
burn fat to make more, and the creature starves for good with food in
its stomach (the Home ettin's "ATP collapse with fuel in store" too).
Every 100 ticks a living creature with no ATP but with fuel (glucose,
fatty acid, glycogen, starch, fat; 20 or more) gets its genome's starting
ATP, as one from an older save does at load, and the restart is logged.
Test (copy of Home): the grendel was restarted once, digested its
starch, Hunger 253 to 0; it then ate again and was satiated again.

Selecting (owner, 2026-11-10: a click with the teach hand should select
the creature): a plain click on a creature already selected it; a click
that lands on something drawn over a creature (its speech bubble, what
it carries, the hand's typing bubble when teaching it a word) now
selects the topmost creature under the pointer too (World::creatureAt).
OPENC2E_TEST_CLICK="tick,x,y" clicks the hand there (developer aid).

### 10 November 2026

#### C2: creatures speak; reflexes run under a mind

[`a18c4c68`](https://github.com/DotairZee/zeetures/commit/a18c4c6895682ea3e3abf74bc87c667ed2c98673)

Vocalizations (owner, 2026-11-10):
- The mind says things (MindOutput::say, a meaning) and the engine words
  them for the creature's age (CreatureSpeech): babies babble ("waah",
  "mama"), children say single words ("hungry", "yum!"), the grown short
  phrases ("me very sleepy!", "hi norn", "ow!"), the senile trail off
  ("me sleepy..."). HomeostatMind speaks of what just happened or what it
  meets: hurt, a good meal, a pleasing kiss, anger, a kind of creature not
  seen for a while, now and then a pressing need; babies and children
  babble when content. Never asleep, with pauses between remarks.
- SAYN (the world's "express need" action ends with it) is no longer a
  stub: the creature says its strongest need; the same need is not said
  again within about 30 s (OPENC2E_TEST_SPEECH lifts that, a test aid).
- In C2's own speech bubble (Bubble, the game's pictures and charset from
  the player's installed copy), one per speaker, for 3.5 s, and in the
  creature's voice: pitched by age (baby 1.4 ... adult 1 ... senile 0.88)
  and paced (baby 0.8, youth 1.15, senile 0.7). Other pitches play a
  resampled copy of each syllable, made once in the user data folder's
  "Voice cache" (SoundManager::playVoice). OPENC2E_WATCH_SPEECH logs what
  is said.

Reflexes: under a mind none of the involuntary actions ran (they were in
the original brain path the mind replaces), so a pregnant female never
laid her egg, and nobody sneezed, coughed or shivered. They now run as
the genes trigger them, interrupting what the creature does; each script
sets its own latency. Test: a forced conception was laid as an egg and
hatched, and the pair went on to conceive twice more.

#### C2: vigor by life stage; held things in front unless facing away

[`c9fcd686`](https://github.com/DotairZee/zeetures/commit/c9fcd68629d9fdc4cd37cea674e6d0a3c0605b1b)

Vigor with age (owner, 2026-11-10):
- Walking pace: a walk's steps come at a share of the usual rate (a step
  every other tick) by life stage: baby 0.6, child 0.85, adolescent 1.0,
  youth 1.1, adult 1.0, old 0.75, senile 0.55 (SkeletalCreature::stagePace).
- Stamina: each step of a walk adds 1 of Tiredness (chemical 6) with a
  chance of 0.15 when old and 0.35 when senile, so the genes' tired walk
  comes sooner (stepTiredness).
- The courting walk (gait 13, set by a norn's genes from its sex drive
  chemical) is weighed by stage: 0.6 adolescent, 1 youth and adult, 0.4
  old, 0 otherwise (c2Creature::getGait).

A held toy gun still flicked behind an ettin (owner, 2026-11-10): what a
creature carries was drawn in front only while it faced left or right.
Facing the viewer its hands are in front of its body too, so now only
facing away puts what it holds behind it.

#### C2: breeding (conception, the child's genome, pregnancy, courtship)

[`d0ba53b2`](https://github.com/DotairZee/zeetures/commit/d0ba53b2e8ea29c8b704f01449b64e7153957682)

openc2e's C2 MATE and GENE CROS were empty, so no creature could
conceive (owner, 2026-11-10: breeding is next in the queue). The world's
own scripts already handle the kiss, egg laying and hatching; this fills
in the engine's part and the fixes found on the way.

- A creature's action on another creature runs the world's
  creature-on-creature script (classifier 0 0 0: kiss, tickle, push,
  hit) instead of its own script for acting on an object.
- MATE (Breeding.cpp tryConceive): between a female and a male, she
  conceives with a chance of her fertility locus, half to all of it by
  her receptiveness, times both partners' life-stage shares (0 baby and
  child, 0.25 adolescent, 0.6 youth, 1 adult, 0.2 old, 0 senile; owner).
  The child's genome (crossGenomes, our own recipe: C2's is not
  documented) pairs the parents' genes by type, subtype and identifier,
  copies stretches from one parent then the other, and as each gene's
  flags allow, mutates it a little (about three small changes in a norn),
  or very rarely duplicates or drops it; its genus gene names the parents.
  It is stored in the user Genetics folder under a new four-letter name,
  which she carries as BABY; her pregnant locus is set. Laying the egg
  (the world's script clears BABY) ends the pregnancy; a pregnancy is
  saved with the world ("baby" line). NEW: CREA records a bred creature's
  parents (conceived event) from its genus gene.
- Body chemistry: a C2 digital emitter fires only above its threshold,
  as digital receptors do (at "threshold 0, at or above", progesterone
  flowed in every female and held her fertility off for good). The
  reproductive switches (fertile, receptive) add up their receptors'
  signed outputs, each receptor's gain read as its output at full signal,
  instead of the last receptor overwriting the rest (an inverted "off"
  receptor held every female's fertility at 0). Only these two loci.
- Under a mind, two of the replaced brain's sensory cells are set from
  perception, since the genome's sex chemistry reads them: the attention
  lobe's cell for the attended category, and sense 19 (the other sex of
  the same genus in sight). Without them no creature felt sex drive.
  The norns' sex instincts (Sex Drive with the other sex in sight: kiss)
  are translated for the mind as acting on their own kind, and other
  creatures can always be acted on.
- OPENC2E_TEST_MATE=N and OPENC2E_TEST_CONCEIVE (developer aids).

Tests (copies of Home, two grown norns): fertility now cycles from the
female's own genes; sex drive rises with the other sex in sight; the
mind learned to kiss and the kiss reached MATE about twenty times in a
run. Conception through to a hatched egg is still being tested.

#### CAOS: STPL, a part drawn stippled, for C2 scripts

[`5242448b`](https://github.com/DotairZee/zeetures/commit/5242448b977763e2c28ecf11619de1ce63a93a35)

openc2e's own command: STPL on part_no draws a part of TARG with every
other pixel left out in a checkerboard, as the invisible hand is drawn,
so what is behind it shows through; 0 draws it whole again
(SpritePart::setStippled). The pond's waterfall is drawn in front of
creatures so the hidden path really passes behind it; the owner asked
(2026-11-10) for it to turn checkered while a creature is behind it, which
a world script (waterfall-path.cos) now does.

#### C2: no lifting onto a floor line that has water under it

[`c4f2086d`](https://github.com/DotairZee/zeetures/commit/c4f2086daaccc756e213a87a7480497e50a416f0)

Fish kept turning up on the boardwalk over the sea by the airlock (owner,
2026-11-10). The boardwalk is a floor line 22 px above the bottom of the
air room over the sea (room 14), and the sea (room 123) opens straight
into that room. A moving object with gravity under a solid floor line is
lifted onto the line (e21b781b: under the line is the inside of the
ground, where critters were sunk), so a fish that broke the surface was
set on the boardwalk. Where the room under the line's room is water, the
line is a boardwalk over it and nothing is lifted.

Test (copy of Home): the three fish on the boardwalk put back in the sea;
after about three minutes none was on it.

#### C2: free objects and creatures from sealed pockets of rock

[`9515a67c`](https://github.com/DotairZee/zeetures/commit/9515a67cd5ee0b8a6952c8acc77ac71daa2b256c)

A ball, a bat and a critter's leaving lay at the map's bottom under the
east caves' bamboo-cage lift (owner, 2026-11-10), in room 48: a pocket of
rock in the room map with every doorway solid. It is a room, so the
safety net for objects outside every room never saw them; a bat flies
home at dawn through the rock with its collisions off and can wake in
such a pocket.

The net in World::tick now also takes objects (family 2, attr 64) and
creatures out of a sealed pocket: a room with every doorway shut when
first checked and still (Room::allDoorsShut), with no floor line and no
drop status. That leaves the walled terrarium of critters and plants in
the west caves (room 85) alone, and an airlock unless both of its doors
were shut at the start. They go onto the floor of the nearest open room
above, within 600 px.

Test (copy of Home): a ball and a norn put in room 48 were put on the
lift shaft's floor (room 47, 2255); the run also freed a food item from
pocket 186 and two critters from pockets 190 and 183.

#### C2: creatures wade shallow water, and sense from its bed

[`80601fbb`](https://github.com/DotairZee/zeetures/commit/80601fbbba870a0a4118205a3cd9cd2794fd4d5c)

The hidden path to the cave behind the pond's waterfall runs along the
pond's bed (owner, 2026-11-10). Creatures refused to walk from dry ground
into any water, and the mind counted no way through it. Water no deeper
than Room::WADE_DEPTH (55 px; on the whole map only the pond by the
incubator, room 19, 53 px deep) is now walked like ground: the foot snap,
the mind's way-finding, its water-ahead and fall checks use
Room::deepWater(). An adult's head stays above it; breathing is judged by
the head as before, and a wading creature with full breath is not "in
water" to the mind.

A wading creature's middle is in the air room above the water, so its
senses of direction (smell, walls, water ahead) came from that room and
the berries' smell never reached it from the tunnel side. They now come
from the shallow water its feet are in.

Test (copy of Home, bank food cleared): a norn waded the bed, climbed the
ramp into the tunnel and came out of the arch, two runs out of two.

#### Snapshot aid: OPENC2E_SNAPSHOT_EVERY and OPENC2E_SNAPSHOT_START

[`144eed74`](https://github.com/DotairZee/zeetures/commit/144eed741c56e66c4a82cc4b83e8e0c70b9457be)

Snapshots can be taken a set number of frames apart (default 40) after an
optional start delay, for filming a creature walking somewhere; the camera
now moves only when the next snapshot is somewhere new.

#### C2: DIRN reports the way a creature's body actually faces

[`608d499c`](https://github.com/DotairZee/zeetures/commit/608d499c937f42f429c010b891da5f043df410d6)

C2 creatures turn by their poses, which never updated the direction DIRN
sets, so scripts reading DIRN always saw the same value. It now reports
the facing of the current pose (0 away, 1 toward, 2 right, 3 left).

#### C2: a short jump of a creature's foot into a room it has no way into is a wall

[`7ede79a9`](https://github.com/DotairZee/zeetures/commit/7ede79a9b323ac6b9d795dadf748a9c8195e9fa5)

A foot that left its room by more than 50 px was taken as a script move
and the creature accepted into whatever room the foot was in. At the
world's seam a grendel turning at the library's west wall had its foot
shift ~58 px west across the seam and was taken into the sealed pocket
beyond (room 213), where it fell to the map's bottom and was trapped
(owner, 2026-11-10). A jump of up to 150 px into a room the old room has
no doorway to, open at the creature's size, now pushes it back as a wall.
MVTO and MVBY make a creature's foot room be found afresh, so script
moves are not mistaken for such a jump. Developer aid OPENC2E_WATCH_ROOMS
prints each change of a creature's foot room and how it was found.

#### C2: a creature let out of a vehicle into water steps onto dry floor just above

[`72e68d6a`](https://github.com/DotairZee/zeetures/commit/72e68d6ae37f076665977512992380bb3d06ae77)

The submarine lets passengers out at its east dock with their feet below
its berth's floor (room 146, y 906), in the sea under it, then throws them
upward; the berth's doorway down to the sea is too small for a creature to
come back up, so they sank to the seabed. Now a creature dropped with its
feet in water, with dry floor within 80 px above, stands on that floor.

#### C2: a creature inside a vehicle breathes its air, not the sea's

[`117a9362`](https://github.com/DotairZee/zeetures/commit/117a93621b8543898fd45dade05c4fa3baed9af3)

A norn riding the submarine under the sea blew bubbles (owner, 2026-11-10).

#### Place the window before it is first shown, and again once mapped

[`2d6c9198`](https://github.com/DotairZee/zeetures/commit/2d6c919837ff225f30f4129addc289d157ccb035)

The window is created hidden, moved to its remembered (or requested)
place, shown, moved again and synced before maximizing: GNOME's window
manager placed a newly mapped window itself, so it always opened on the
first monitor. Under native Wayland no client can choose its window's
place; the launcher uses X11 there when several monitors are connected.

#### C2: File &gt; Import Creature brings in an exported creature (.exp)

[`e77eb77d`](https://github.com/DotairZee/zeetures/commit/e77eb77d8125fff88b848d5b3bf160de903d4510)

Its genome, name, sex and life stage are read from the export; the
creature is made anew from them and handed to the hand to place.
OPENC2E_TEST_IMPORT_CREATURE imports a file once the world runs.

#### C2: objects outside every room are put back on the floor above

[`88615dc3`](https://github.com/DotairZee/zeetures/commit/88615dc3224cb296f61fa832df7e37958cbbff60)

A beach ball (2 13 4) and two flying critters (2 18 4) lay below the map's
bottom edge at the east-west seam, out of the room system for good. Every
100 ticks, a family-2 object that collides with the rooms (attr 64) and is
outside every room, not carried and not in a vehicle, is moved onto the
floor of the nearest room above it within 600 px (logged with
OPENC2E_WATCH_SUNK). Test on Home: all three were put back on the library
floor; an invisible marker that does not collide is left alone.

#### Breeder's Kit sees breeds in the user Overrides folder

[`e47c03b8`](https://github.com/DotairZee/zeetures/commit/e47c03b82e20f24b9abdd0dc1c2e78077cd2d766)

The Breeds page scanned only the game's and the user's Images and Body
Data folders, and the kits load pictures through the main-folder lookup,
which skipped Overrides; so the Frog Norn pack moved into slot l showed C2's
old frog portraits, "Ages: baby" and "In the game's folders" (owner,
2026-11-10). The scan now includes Overrides ("Installed in your data
folder, in place of the game's", marked \* in the list), "Images/&lt;name&gt;" and
"Body Data/&lt;name&gt;" asked of the main folder look in Overrides first, and
the import's duplicate check reads Overrides first. New developer aid
OPENC2E_TEST_BREED_SELECT="0,l". Snapshot: the pack's portraits, ages baby
to senile.

#### A genome in the user data folder replaces the game's of the same name

[`f114d07d`](https://github.com/DotairZee/zeetures/commit/f114d07d006e1fbbd650af26fded965370fa8992)

World::loadGenome chose at random among same-named genomes in every data
folder, so with the imported Frog Norn pack's frog.gen and C2's own, each
frog was either. The player's installed copy (user data folder) now wins.
With it, the pack replaces C2's frog norn outright (owner, 2026-11-10): its
pictures and body data moved from slot d to slot l in the user Overrides
folder (taking the place of C2's slot-l files) and its genome repointed
to l.

#### Reopen the window on the monitor it was last on

[`84a51c6b`](https://github.com/DotairZee/zeetures/commit/84a51c6b2b5aea69352018cdb03a16fd734f131e)

OPENC2E_WINDOW_MEMORY names a file holding a point inside the monitor the
window was last on, written when the window moves to another monitor and
at shutdown. At start, if that point is on a monitor connected now, the
window opens there; otherwise OPENC2E_WINDOW_POS, otherwise the window
system decides (owner, 2026-11-10).

#### Hand-injected add-ons' scripts are saved with the world

[`67c09b53`](https://github.com/DotairZee/zeetures/commit/67c09b5329cae80dc5c3e7143b4f3c9b24dd988c)

Objects added by hand from the Agent Injector were saved, but their COB's
scripts were installed again from the COB file at each load, so they
stopped working if the file was moved or deleted. The scripts are now kept
(world.injectedScripts, from the COB when injected or reinstalled), saved
as "injscript" lines, and installed from the save at load; saves without
them still reinstall from the COB file. The pictures and sounds a COB
brings are already copied into the user folders when it is injected.
Test: an add-on renamed in the save to a name no COB has came back with its
objects and scripts.

#### Stage scaling: the species' standard breed as reference; norns on request

[`85a3edc2`](https://github.com/DotairZee/zeetures/commit/85a3edc2c963647b57400281f4de05f20094b4a9)

The growth reference is now the species' standard breed when it has
several ages (C2's own norn, breed h from norn.gen; its grendel, breed a),
not whichever breed has the most: an imported norn pack would otherwise set
the norns' growth. Norns (C2's frog norn, breed l, drawn at one age) scale
only with OPENC2E_STAGE_SCALE=norns until the owner has seen previews:
factors by stage 0.76, 0.86, 0.86, 1, 1, 1, 0.86 (C2's senile norns are
drawn smaller).

#### C2 creatures carry in the near hand, drawn in front of the body

[`3f266e3e`](https://github.com/DotairZee/zeetures/commit/3f266e3ec8aeb1c0fcaadf20cf88711005fccd43)

Everything a creature carried hung from its left forearm (part 9) and was
drawn 100 planes behind it. Facing right, a C2 creature's left forearm is
drawn behind its body and the right one (10) over it; facing left, the other
way round. So a toy gun an ettin carried jumped between its hands and
behind it as it turned (owner, 2026-11-10). Now a creature carries in the
hand on the viewer's side (right facing right, else left) and draws what it
carries just in front of itself when that hand is in front (facing left or
right), behind when facing toward or away. The carried object is re-sorted
for drawing when that changes (Agent::carriesInFront).

Not yet seen in the running game: built and reasoned from the body-part
drawing order (cee_zorder).

#### Mind: travel links come from working devices; failed links are forgotten

[`e289cca9`](https://github.com/DotairZee/zeetures/commit/e289cca99e9655857e75e9112a51fdbb59dd4ddf)

A travel link ("this object, used here, takes me there") was learned from
any action on a device-like object followed by a long move. A grendel that
ate the airlock's land door and was soon let out far away learned "eat the
door" as a way to travel, and on every food trip bit the door again, facing
it for minutes (owner, 2026-11-10: 28 bites in 80 s in a replay). Links now
come only from activate 1, activate 2 and deactivate; saved links by other
actions are dropped on load; a link that twice takes the creature nowhere
within the link window is forgotten. Replay: the grendel walked out of the
airlock instead.

#### Not under water with the feet on dry floor

[`b6e410a1`](https://github.com/DotairZee/zeetures/commit/b6e410a13cfd28680326bb6663b3a97f604ecdfb)

A creature counted as under water (holding its breath, bubbles at the
mouth) whenever its mouth was inside a water room's box. Beside 22 dry
floor edges on the map a water room's box reaches higher than a head: at
the east end of the green walkway past the incubator (room 14) a norn
facing the sea's room (31) blew bubbles (owner, 2026-11-10). Now the feet
must be in water too, or the creature falling or swimming (no foot room).

#### Creatures take step-ups between rooms instead of falling back

[`610ed81c`](https://github.com/DotairZee/zeetures/commit/610ed81ccb1220a69b1b73a21a8ffc2c9630d213)

snapDownFoot lets a creature step into the next room when that room's
floor is up to 40 px above the foot, but a room change that moved the
creature more than 20 px turned into a fall, so a step up of 21-40 px
dropped it straight back. The fall now applies only when the new floor is
lower. Found with the far-west boardwalk pool's new way out (pool.cos):
creatures at the top of the bank fell back from the 33 px step onto the
planks; now they walk out (two of two in a test run).

#### CAOS: PLNE (command and value) available in C2

[`c9bb84b4`](https://github.com/DotairZee/zeetures/commit/c9bb84b4684df5c989a374417af8d183ef3a60ff)

C2 fixes an object's plane when it is made; the world file's foreground
cut-outs (scenery at planes 200 and 9000) hid the middle caves' bamboo
lift at its top stop. PLNE, openc2e's from the later games, is now
available to C2 scripts too, so a world addition can layer the lift above
the glass tube's cut-out (owner, 2026-11-10). Planes below 0 wrap as
unsigned but already sort as negative (c2PlaneKey).

#### Images and body data in the user Overrides folder replace the game's

[`630972c1`](https://github.com/DotairZee/zeetures/commit/630972c1f50bf6eaeefb467704b78433a3333815)

The user data folder is searched after the game's own folders, so a file
there with the name of one of the game's never took its place. Pictures
and body data in &lt;user data&gt;/Overrides/Images and Overrides/Body Data are
now found first. For the female ettin's head, rebuilt on the player's
machine from their male ettin's by the project's ettinhead.py (owner,
2026-11-10); no game files ship with the project.

#### Mind: a refused sleep teaches nothing; relearn sleep saved before

[`5ab351ad`](https://github.com/DotairZee/zeetures/commit/5ab351ad6a0b338b5823b1d50fd570febd1209b9)

A sleep the world's sleep script refused (ended within 5 ticks without
falling asleep) was learned as sleeping that did nothing, so grendels and
ettins, who could never sleep under C2's rule, came to rate sleep at
nothing and stopped choosing it. Refused sleeps are no longer learned
from. Saved learning (state version 4 or older) has its sleep expectation
dropped and the sleep instincts applied again (state version 5).

With sleep.cos (grendels and ettins sleep when sleepy, owner 2026-11-10),
in a copy of Home the grendel and the ettin fell asleep and brought
Sleepiness from 255 to 0-47 within minutes.

#### Scale one-set grendel and ettin sprites by life stage (off by default)

[`e1c6f65a`](https://github.com/DotairZee/zeetures/commit/e1c6f65aaec39f09c6adb0f2cbdf631fb3c7e622)

A breed drawn at one age only (Bony grendels, C2's own ettins) shows the
same pictures from hatching to old age. With OPENC2E_STAGE_SCALE set, its
parts are scaled to the growth of a reference breed: the species' standard
breed when it has several ages, else the standard grendels (the owner's
guideline; measured from C2's own grendel art, babies' parts are 72-77% of
adult size and children's 83-88%). The age a one-set breed was drawn at is
the reference age nearest its body size; with no reference of its own
species it counts as adult. Every joint point in the part's body data is
scaled with the pictures, so the parts still meet. Grendels and ettins
only for now; logged once per breed as STAGE SCALE.

ImageUtils::Scale: area-sampled scaling keeping C2's on/off transparency;
imageManager::scaled caches results by name and factor.

Off until the owner has reviewed previews (2026-11-10).

#### No cap on the creature population (EGGL, HATL)

[`0fc97b37`](https://github.com/DotairZee/zeetures/commit/0fc97b37537710b20eb748915cb350fb09d96361)

C2 read its egg limit and hatchery limit from the game's options; openc2e
returned 16 and 12. The world's scripts compare the number of norns with
them: norn eggs (2 5 2) do not hatch at the egg limit, the kiss script
does not mate norns at it, and the hatchery (3 7 1) shuts itself down at
the hatchery limit. The owner asked for no hard cap (2026-11-10): both now
return 1,000,000.

#### CAOS: NEW: CALL adds a stop and a call button to a C1/C2 lift

[`8a652a23`](https://github.com/DotairZee/zeetures/commit/8a652a23d7a05b631aac4c14fae110185e97fb21)

C1/C2 lifts and their call buttons come from the world file: each lift
keeps the heights it stops at, and each button is linked to one of them,
so a script could not give a lift another stop. NEW: CALL lift stop_y
sprite count first plane makes a call button for the lift, adding the
stop at stop_y (the lift's bottom edge there) in order from top to bottom
and renumbering the lift's other buttons; the new button is the target.
Used by the treelift.cos world addition (owner, 2026-11-10: the middle
caves' bamboo lift comes down to the extended deck under the lower hut).

#### Mind: a call button pressed blind that brings nothing is a dud there

[`c7f3e161`](https://github.com/DotairZee/zeetures/commit/c7f3e16195202e81366cd7486624fa765330e6c6)

With no lift in view and no learned link, a floor change toward food
presses any call button on the creature's floor. Our boat call switches
are category-2 buttons too, and call buttons are exempt from passing
over, so two starving norns on the east sea's dock pressed one for ever
(owner, 2026-11-10). A blind press that brings no lift into view, no ride
and no change of floor within 100 mind ticks marks a dud at that spot; no
blind presses within 150 px of it for 2000 mind ticks. Presses toward a
lift in view or a learned link are unchanged. Test: after the dud a
starving norn on the dock stopped pressing and worked the fishing rod.

#### Mind: stop retrying a sleep the world's sleep script refuses

[`4e7263e8`](https://github.com/DotairZee/zeetures/commit/4e7263e8fc9bfaa2e74e01a6b8039dd9e6a7120a)

C2's sleep script (4 0 0 41) sleeps only while Tiredness is above 48, and
a rested creature in the dark is very sleepy all the same: in C2's own
grendel genome walking tires a grendel only once it is old, so a grendel
with Sleepiness 255 chose sleep again and again and each ended in two
ticks. A sleep that ends within 5 ticks without the creature ever falling
asleep now counts as refused, and sleep is not offered for 300 mind ticks
(logged as "sleep refused").

#### Judge creatures at Albia's seam by their foot; sense walls ahead

[`3d7ec078`](https://github.com/DotairZee/zeetures/commit/3d7ec078a0acf615db9af770925187c0a78139a3)

openc2e wraps an agent by its top-left corner, so at the wrapping world's
seam a creature can stand with its foot just inside the library (x ~2)
while its body's middle is at x ~8300, inside the sealed rock beyond the
library's west wall. The mind's senses, its reach check and its sight
all started from that middle, so a grendel saw the volcano's critters
through the wall and walked into it for good.

- SkeletalCreature::footRoom()/footX(): the down foot's room and its x
  wrapped into the map.
- CreatureAI: a creature straddling the seam is sensed from its foot's
  room; neighbouring rooms across the seam are placed beside it (side
  tests for scent, water, drops); walkableTo follows rooms across the
  seam the short way round (walkOnFrom factored out).
- Sight (agentPartlyVisible with an eye point): a straddling creature
  looks from above its foot.
- New wall-ahead sense (MindPerception wallWest/wallEast): the foot is at
  its room's edge and no doorway it fits through goes on at walking
  height or down. HomeostatMind drops plain walks and trips that way.
  OPENC2E_WATCH_CREATURES logs WATCH WALL changes.
- Developer aid OPENC2E_TEST_KILL=N: the selected creature dies at world
  tick N (headless).

#### ENUM loops skip agents that died since the loop began

[`861f3cfd`](https://github.com/DotairZee/zeetures/commit/861f3cfd23ed8ce3dbd8011fe5817b57b48421e3)

#### Draw the invisible hand see-through (checkerboard stipple)

[`9971321c`](https://github.com/DotairZee/zeetures/commit/9971321c1e6778251ccfdfb29a5634d49861ce94)

#### Breed import: duplicate check and a slot-clash dialog

[`4622280b`](https://github.com/DotairZee/zeetures/commit/4622280b2d285ba3ccb254849261b658633b943c)

Each breed in a pack is compared with the installed ones; an identical
breed is skipped unless asked. A taken slot opens a dialog (next free
slot, don't install, or another slot). Moves are applied at once and
remembered, so later genomes using the old letter can follow them.

#### Save objects injected by hand with the world

[`d36e2f35`](https://github.com/DotairZee/zeetures/commit/d36e2f359887f3ca0c8ceb2c7d8244b36f6fe634)

Objects added from the Agent Injector, and what they make later, are
marked with the COB they came from and saved in full (parts, hotspots,
vehicle cabins, physics, variables including links between objects);
their COB's scripts are installed again at load. Also fixes the wildcard
file search, which skipped the user data folder's Objects folder.

#### Walking: stop stale walking, open the desert lift's shaft, water is a wall

[`0949e9fb`](https://github.com/DotairZee/zeetures/commit/0949e9fb126880fb1eef226e01b09e3b54f2fe42)

A new action stops any walking the previous one left going, and APPR
with no target stops the creature (an eat whose fruit had gone left a
norn walking into a wall). A neighbouring room whose floor line can be
walked beneath (a lift shaft, floor value above 0) is judged by its
bottom edge, so creatures can walk into the desert lift's lower stop
instead of stepping in place at its doorway. From dry ground a water
room is a wall: creatures fall or are dropped in, but never walk in.

#### Breed import: old add-on installers, genomes from exported creatures, COBs

[`3f961a21`](https://github.com/DotairZee/zeetures/commit/3f961a219b65ab8db1a1aeb0006921a45e89bfcd)

Imports now read the self-extracting installers some C2 add-ons came
as (a zlib payload after the program, never run), in both the Breeder's
Kit and the Agent Injector. A breed pack's exported creatures (.exp)
give their genomes, so packs without a genome file lay eggs of their
own genome; COBs in a breed pack go to the user Objects folder.

### 9 November 2026

#### Built-in and hidden Agent Injector objects

[`4e70f5a8`](https://github.com/DotairZee/zeetures/commit/4e70f5a82b76ae1e701fbe1f83e08a264d118bfd)

Agents named in the Additions folder's builtin-cobs.txt are injected
from the player's own COB files at each C2 world load (injected objects
are not saved); those and the ones in hidden-cobs.txt are not offered
in the Agent Injector.

#### Science Kit syringe animates; Agent Injector imports COBs and zips

[`59d5de6d`](https://github.com/DotairZee/zeetures/commit/59d5de6d74016eb68bdf4769f98275d4317c6b1c)

The syringe draws C2's plunger and glass front over the dose; Inject
presses the plunger down with the fluid, then draws up the next dose.

The Agent Injector imports COB files and zips (COBs, loose sprites and
sounds) into the user data folder; nothing is overwritten: a clashing
COB is renamed, a clashing sprite or sound is left out and reported.

#### Pointer toolbar: invisible hand and push mode

[`feda08e0`](https://github.com/DotairZee/zeetures/commit/feda08e01488402afa07e6502fa7d35c672555fc)

The three hand buttons now work: the invisible hand is left out of what
creatures notice; teach (tickle and slap, as before) and push are the
hand's two modes. In push mode a click on a creature slides it a short
way along the ground, away from where the hand pressed.

#### Only activatable objects are activated; keep wanderers out of water

[`898bfc1b`](https://github.com/DotairZee/zeetures/commit/898bfc1b3972353a3758fbf22b213263eaf402a4)

C2 marks what may be activated (attr 4). The mind offered activation on
anything in view, so a norn ran the airlock's drain machine (attr 0)
directly and drained the chamber with its doors shut. Activation,
water-escape device tries, travel links and call buttons now need the
mark, and attention prefers objects a creature can use.

From dry land, a way through water no longer makes an object reachable;
plain walking never steps off dry land into water; a creature wading
with its feet in water counts as in water. (The pond's new ramp let
wandering norns walk in and wade; regression batches reg2-reg4.)

The sunk-object lift now applies only where the floor value is 0: in
the desert lift's shaft, whose bottom is walked beneath its floor line,
it popped a stumbling norn up to the desert.

#### Objects behind walls are not reachable; halo over the head

[`99ae765a`](https://github.com/DotairZee/zeetures/commit/99ae765a5c80cc5db9f8fd759c44cde135885e5a)

A seen object counted as reachable whenever its height overlapped the
creature's, so a norn on the incubator bank chose the lift beyond the
pond and walked into the wall between them. Now the creature must be
able to walk to it through doorways it fits, without a climb (walking
under a high floor line along a room's bottom counts), and attention
prefers objects it can walk to.

C2's halo now sits over the middle of the selected creature's head
picture instead of its bounding box.

#### Family tree: thumbnails that zoom in, one card per creature, tree zoom; name script-hatched creatures

[`6102b8de`](https://github.com/DotairZee/zeetures/commit/6102b8ded0bdddb69137b887b1cedf3f987d1673)

The Owner's Kit family tree now lays out one card per creature however
it is related (every role listed), with lines from each parent shown to
each child shown, routed around cards in between. Photos are small
thumbnails that grow into a large view over the tree. The tree zooms
25-200% (wheel about the pointer, slider, Fit), pans with the middle
button and scrolls.

C1/C2 hatching scripts never call BORN, so hatchlings got no default
name or birth entry; NEW: CREA now records the birth, and any living
creature left unnamed gets the next number.

#### Owner's Kit family tree with photos; numbered default names

[`52f071b6`](https://github.com/DotairZee/zeetures/commit/52f071b67753f95bd4ff8ded44611f22c5f69a4d)

- Monikers are saved and given back on load (renameMoniker), so parent
  links and the life log keep finding creatures; parents (mother/father,
  from conceived/spliced events) are saved; the dead are kept as records
  (name, dates, photos, parents) without a body.
- Default names "Norn 0005", "Grendel 0021": a per-world count per genus,
  saved; births take the next number, creatures already in a world are
  numbered once, oldest first.
- Family page: a tree of grandparents, parents, the creature and its
  children, each card with its newest photo (arrows to flip), sex, stage
  and age or death date, Go for the living; a photo opens an album.
- Fix newMoniker's retry loop (shadowed variable).

#### Owner's Kit: isolated photos, life log of deaths; life notices

[`c3839bc5`](https://github.com/DotairZee/zeetures/commit/c3839bc50d7d0995da4308f3fc4bcc2409978df1)

- Photos draw only the subject: World::drawWorld can skip every other
  creature and whatever another creature carries.
- Births, deaths and pregnancies go into a life log kept with the world
  (historyManager lifelog, "life" lines in state.txt). Restored creatures
  are not announced (history quiet while WorldSave rebuilds them); a
  pregnancy is logged when the pregnant locus turns on.
- Owner's Kit: Life log page; a dead creature's certificate is stamped
  DECEASED with its date, age and stage at death below.
- Status bar: Born/Died/Pregnant notices on the right; a click selects and
  goes to the creature and opens the Owner's Kit (birth, death) or the
  Breeder's Kit (pregnancy) at the right page (OpenC2KitAt).
- Test aids OPENC2E_TEST_DEATH and OPENC2E_TEST_NOTICES.

#### Owner's Kit and Observation Kit; creature names, birth times and photos kept with the world

[`3ef24d46`](https://github.com/DotairZee/zeetures/commit/3ef24d46b418e0826c19e52dfd12039243264db1)

Owner's Kit: Registration (the name written on C2's birth certificate,
species and sex, born or first seen, age, life stage, moniker; register
the birth; notes), Photos (take a photo of the selected creature, kept in
the world folder's photos/, shown on C2's film strip with the date taken;
view full size; delete), Family (parents and children from the history's
conception events; none yet until breeding).

Observation Kit: Details (every living creature: name, kind, age, organ
health, pregnancy, what it is doing, nearest favorite place; sortable,
resizable; click to select, Go to look), Alarms (a chemical above or below
a level in one creature or any; checked whether the kit is open or not, a
note pops up over the world, a log of alarms gone off; kept in kits.ini).

monikerData keeps the owner's records (borntick, bornknown, registered,
photos, notes; the name it already had), set at birth and saved and loaded
with the world (state.txt lines name, registered, born, photo, notes).
RenderTarget::saveToFile (SDL: a 24-bit BMP of an off-screen target). The
Creatures menu shows given names.

#### Write C2 genomes as C2's fixed 36001-byte block; genome round-trip tests

[`b36632c0`](https://github.com/DotairZee/zeetures/commit/b36632c065d20b3be629a275c23a1df8588e5077)

The genome writer was not losing genes: every gene and the "gend" marker
came back byte for byte. Creatures 2 keeps each genome in a fixed block of
36001 bytes (genome files and the genomes inside eggs alike), with zeros
after "gend" in the norn genomes and leftovers of older genes in the
grendel and ettin ones; readers stop at "gend". A written C2 genome is now
padded to that block, so files openc2e writes (world saves, eggs, breed
imports) match what C2 and its tools expect.

test_fileformats: a synthetic C2 genome round-trips, and with
OPENC2E_TEST_GENOME_DIR set, every .gen there keeps its genes exactly and
comes back as a zero-padded 36001-byte block (all six in the C2 install
pass). A save and reload of a Home copy restores both creatures.

#### Breeder's Kit with breeds and import; breed slots past z; organ and gene names; C/F; zoom box

[`3a58b803`](https://github.com/DotairZee/zeetures/commit/3a58b8037c166bb2aa7c30cbbce948de3f82ce38)

Breed slots for C2 body parts go on after z as aa, ab ... az, ba ...
(creatures/BreedSlots.h); a body-part stem is 4 or 5 characters. A C2
genome's appearance gene holds the slot as 0-255, so slots reach "iv".

Breeder's Kit: Reproductive (C2's figures, the growing egg in the womb
while pregnant, sex drive, fertility, receptiveness, pregnancy, hormones),
Aphrodisiacs (the Applet Data COBs on C2's shelves; the shelf is now shared
with the Doctor page), and Breeds: every breed found in the data folders by
species, portraits of the adult faces, parts, ages and sexes; lay an egg of
a breed; import a breed pack (files, a folder, a zip or a COB's pictures)
into the user data folder, moving it to a free slot when its own is taken
(genomes and eggs in the pack follow the move), with a review before
installing; rename and remove imported breeds (breeds.txt keeps their
names and files). The Hatchery gets a Breed choice for the eggs it lays.
Appearance genes are patched byte for byte, since openc2e's genome writer
does not round-trip C2 genomes (36001 bytes came back as 19033).

Science Kit: organs and genes are named from the species' gene notes
(norn.gno, gren.gno, ettn.gno), with the notes' comments on hover. Click a
temperature (status bar or the Health Kit) to switch Celsius and
Fahrenheit, kept in kits.ini. A Zoom box beside Places on the toolbar.

#### C2 kits in the game window: framework, Health Kit and Science Kit

[`bef61baa`](https://github.com/DotairZee/zeetures/commit/bef61baaa0a38b0ccb02d4ee64cbad331dfb6fe7)

Kits are floating panels with a minimize button in the title bar; a
minimized kit becomes a labeled chip (with its applet icon) on the status
bar, and clicking it restores the kit where it was. Open and minimized
state and window places are kept in kits.ini in the user data folder.
Kit art (Applet Data backgrounds, LED digits, bars, shelves, syringe) is
loaded at runtime from the player's own Creatures 2.

Health Kit: Fitness (pulse and body temperature on the LED screens, a
thermometer, organ health), Mind (what the creature is doing, its
strongest needs, the mind's state), Drives (all 17 as bars), Doctor (the
Applet Data medicines on the shelves, added to the world as COBs).

Science Kit: Physical (every organ's health, clock, energy, reactions,
receptors and emitters), dDNA (genes grouped by type and kind, with
summaries and header details), Biochemistry (up to six chemicals graphed
over the last two minutes, chosen by ChemGroups), Syringe (ChemMixtures or
any one chemical at any amount; no advanced-mode lock).

Tools menu and the applet toolbar open both kits. Creature's View's
activity caption is shared (DescribeCreatureActivity). Test aids:
OPENC2E_OPEN_KITS, OPENC2E_KIT_PAGE, OPENC2E_KITS_MINIMIZED.

#### PNG sprites with real transparency for C2; draw vehicles gliding between ticks

[`4da40d5e`](https://github.com/DotairZee/zeetures/commit/4da40d5e949042c45bbd9092c6b7b7902aec204a)

C2 sprites (.s16) have one transparent colour and no partial
transparency, so glass, glows and soft edges can't be drawn. openc2e now
loads NAME.png before NAME.s16 for C2: 8-bit RGBA (or RGB) PNGs, frames
stacked top to bottom, the count in a tEXt chunk "openc2e-frames". They
load as a new image format, if_rgba32, drawn with their alpha; pixels
below half opacity count as transparent for clicks. The game's own
sprites are unaffected (they are all .s16).

Vehicles (lifts, boats) and their riders now glide between world ticks:
each tick remembers where every vehicle started, and drawing places it
back along that tick's move by the part of the tick not yet elapsed
(moves over 64 px are jumps and are not smoothed). A lift moving many
pixels a tick no longer looks jerky.

#### Creature's View follows attention; zoom the C1/C2 world view

[`4fedfc9d`](https://github.com/DotairZee/zeetures/commit/4fedfc9de82461d7276359919d684dee647a19f5)

Creature's View now shows what the selected creature is paying attention
to: it frames the creature with the object when both fit, or shows the
object when they are far apart, glides between targets, marks the object
with corner brackets and captions it with what the creature is doing
("eating some food"). OPENC2E_OPEN_EYE_VIEW=1 opens it on the first norn
for headless snapshots.

Zoom: the world view can be shown at 50%-300% (View &gt; Zoom In, Zoom Out,
Actual Size; Ctrl+wheel zooms about the pointer, Ctrl+0 resets). The
world is drawn 1:1 into a texture the size of the view in world pixels and
scaled onto the window; the main camera's size is in world pixels, the
pointer maps window positions back through the zoom, and the hand is drawn
on top at its usual size. OPENC2E_SNAPSHOT_ZOOM=z takes snapshots zoomed.

#### Allow ALPH, TRAN and CABP in C2 scripts; add a draw-order dump

[`cbb04e76`](https://github.com/DotairZee/zeetures/commit/cbb04e7685a5623bab875b21e76d748615487516)

ALPH (part transparency), TRAN (click-through parts) and CABP (the plane
a vehicle's riders are drawn at) were C3-only. Opening them to the C2
dialect lets an add-on build a see-through vehicle whose riders are drawn
between its back and front parts; in C2 a vehicle's riders are otherwise
drawn at plane 1, behind nearly everything. In C2 a TRAN part is never
clicked, so a front part does not hide what is behind it.

OPENC2E_DUMP_RENDERS="x0,x1" prints the draw order of the agent parts
between x0 and x1 (and of all creatures) on every snapshot.

#### C1/C2 wraparound: an object exactly at the far edge wraps too

[`f29cedbd`](https://github.com/DotairZee/zeetures/commit/f29cedbdd17cf8aa8ad944f717201f5b452ea4f2)

moveTo wrapped an object's corner only when past the world's width, so
one sitting exactly on x = width found no room. Use &gt;=.

#### Homeostat mind: no sleeping in water; never eat what smells of poison

[`b08c685b`](https://github.com/DotairZee/zeetures/commit/b08c685b6cef23afbde121fde952ba5007bf9477)

From the chemistry recorder: the remaining sudden collapses were norns
asleep in the seas (rooms 123, 151), never woken or dozing off again
after cold wakings, and a grendel collapsed by a single bite of the
game's cyanide berries. Sleep is no longer offered in water, a creature
asleep in water wakes at once (as for suffocation), food carrying a
poison now smells of poison (edible scent -1) and the mind never offers
to eat it. Batch chem3 (before these): poisonous berries eaten once in
16 runs, against 25 in chem2.

#### C2 creatures blink

[`7b9b0488`](https://github.com/DotairZee/zeetures/commit/7b9b04882e426305a7d909bc2289a937709a7310)

openc2e closed creatures' eyes only for sleep and death. Draw the head
with its eyes shut for about a fifth of a second every 3-8 seconds (a
drawing-only state, apart from the eyes-closed state that sleep uses).

### 8 November 2026

#### Poisoned food: no food smell, and it tastes harmful; floor changes use own-floor buttons

[`6150a4bf`](https://github.com/DotairZee/zeetures/commit/6150a4bf1163969725e33e698aa184b8cef261e3)

From the chemistry recorder: young norns' sudden ATP collapses were the
game's poisonous berries (2 15 9: cyanide 100, starch 100, alcohol 255
per bite). Their starch made them smell like food and the mind kept
eating them. Food whose eat script gives a poison (heavy metals,
cyanide, belladonna, geddonase, glycotoxin, toxins 234-239) now has no
food smell, and a poison taken in is felt as Pain (alcohol at half), so
one bite teaches a creature to leave it alone.

Floor changes only use a call button on the creature's own floor and
within reach: far-east norns on a ledge pressed the call button of the
deck below them over a thousand times (batch chem2: far east meals 1.5-4.4
to 6.3, starving 46-62% to 41%).

Developer aid OPENC2E_WATCH_CHEM: each creature keeps its last 400 ticks
of chemistry and organ life force and prints them once if its ATP
collapses from 180 or more to under 30.

#### World save keeps the calendar

[`503b1321`](https://github.com/DotairZee/zeetures/commit/503b13212be208a40988959ac8f466f2c37e47b3)

The C2 calendar (a part of the day every 3600 ticks; 5 parts a day, 4 days
a season, 4 seasons a year) was never saved, so every session began again
in spring of year 0. Save world ticks, part of the day, day, season and
year, and restore them on load.

#### C2 creatures: use the body data that matches the sprite found

[`32f4a23d`](https://github.com/DotairZee/zeetures/commit/32f4a23d3b133068811dd874c9b58443d1d1d7e4)

A body part's sprite is looked up female-first, but its attachment (ATT)
file was always looked up as male. Female golden norns (breed k, whose
male and female heads differ in frame size and neck point, more so as
they grow) got the male neck points and their heads rode high above the
shoulders. Look up the ATT file for exactly the sprite found (same sex,
age and breed), falling back as before only if there is none.

#### Homeostat mind: a cold or hot place is not sleep's fault

[`3c511ab2`](https://github.com/DotairZee/zeetures/commit/3c511ab2e1b72a44681020a1d46a61fc793fad36)

A norn woken by the cold at the far west edge learned that sleeping
itself chills (reward -0.14) and ranked sleep too low ever after. A
sleep's learned effect now leaves out Cold and Hot unless felt as a
stimulus, and after a waking by cold or heat the creature does not sleep
within 300 px of there for 1200 ticks. Batch sleep1 against air2: meals
47.7 to 48.7, starving 8 to 9%, wakings by cold or heat 6 to 4.

#### Window icon: just the egg from openc2e's logo

[`c5fd0a5d`](https://github.com/DotairZee/zeetures/commit/c5fd0a5de453d1f9fbc232fd7024c2b49347940f)

The whole logo is twice as wide as tall, so as a square icon it came out
small. Use only the egg inside the norn head silhouette, with a thin plum
outline so it reads on light and dark taskbars.

#### Window icon: openc2e's winged-egg logo

[`873d27e2`](https://github.com/DotairZee/zeetures/commit/873d27e2d14483b0a58d8b8476bf1ca4ee2d3fec)

The 2008 openc2e logo (the website's transparent copy, the same art as
Openc2e-logo2008.png), centred on a 128 x 128 square and built in, set as
the game window's icon.

#### Homeostat mind: search past failed devices; remember devices that flood

[`027b6dfe`](https://github.com/DotairZee/zeetures/commit/027b6dfedd3f907b3f52cea3061e241398999730)

From the hunger batches: norns in the airlock's west end only ever saw the
sea door, passed it over and were shown it again (172 and 159 uses), and
many wet spells began with the norn's own device use. Devices passed over
while escaping are not offered again for a while; with every device in
view tried, the creature walks away from them to search the room (toward
the land door). A device worked while dry (activate 1/2, deactivate) that
leaves the creature in water soon after is remembered as a flooder (saved)
and not worked while dry near there again. Batch air2 against food2: wet
spells 57 to 44, none over 500 ticks (2 before), still wet at the end 7
to 1, no airlock deaths.

#### C2 creatures: judge the foot wrapped at the world's east-west seam

[`618695db`](https://github.com/DotairZee/zeetures/commit/618695dbd9b35565a9cb1748967a1a2dc9df1184)

openc2e wraps an agent by its top-left corner, so a creature near the east
edge of a wrapping world (C2's Albia) can have its foot, about 50 px right
of that corner, past the map. Room lookups wrap on their own, but floors
were measured at the out-of-range x: one creature fell through a solid
floor and out of the room system. The foot check now wraps the foot's x
into the map (keeping the last good foot position in the creature's own
terms), the old seam move (undone by moveTo's own wrapping) is dropped,
and the moved-by-script test measures across the seam the short way.
OPENC2E_DUMP_ROOMS also prints the metarooms.

#### C2 planes: keep the pointer on top

[`cd16ae13`](https://github.com/DotairZee/zeetures/commit/cd16ae134a26a140e97d4f8f8f2f3e7aecdd8836)

Reading C1/C2 planes as signed put the pointer (plane 2^31) at the very
bottom. Read only small negative planes (down to -65536) as negative and
leave every other plane as it is.

#### C2: draw objects below plane 0 behind the rest

[`34b6641e`](https://github.com/DotairZee/zeetures/commit/34b6641e6c90267ee7db62d93575d1371886fb62)

The draw order (renderablezorder) compared parts' planes unsigned, so an
object at a negative plane was drawn in front of everything, even the
pointer. Compare signed for C1/C2, as the click order now does.

#### C2: let objects sit below plane 0

[`6450d6f3`](https://github.com/DotairZee/zeetures/commit/6450d6f318e4d9ff0772e64fdd78d568a58bb6b2)

Separate objects are drawn in order of their main plane, compared as
unsigned for C2, so an object placed at a negative plane (to stand behind
the plane-0 machines: lifts, the submarine, the genetics machine) wrapped
round to the very front. Compare signed for C1/C2, as for the BMP
renderer, and save kept objects' planes signed so they reload.

#### C2 physics: lift a moving object under its room's floor line onto it

[`e21b781b`](https://github.com/DotairZee/zeetures/commit/e21b781b43203b2990030c2948e49eede41a3ede)

The floor-line (SSFC) check only catches an object crossing the line from
above, so an object already below it, such as a critter the world file
starts inside the ground, rested on the room's rectangular bottom out of
sight. A moving object with gravity and collisions that finds itself under
a floor line that is solid for it (size &gt; floor value, as in the floor
check) is moved onto the line.

#### Remove the unused Send To item from the File menu

[`703b3122`](https://github.com/DotairZee/zeetures/commit/703b3122c34c531d8d00e021d902dbf5868c893b)

#### Start with all sound muted and the window maximized

[`2a793d9b`](https://github.com/DotairZee/zeetures/commit/2a793d9b8abc8234873aeaca7f287e20dab220fd)

Music, world sounds and creature speech begin muted; the player turns
them on from File &gt; Sound. The window is maximized on whichever monitor
it opens on (OPENC2E_WINDOW_POS still picks the monitor).

#### Sound menu: Ambient Music, World Sounds, Bibble

[`15e3cbe5`](https://github.com/DotairZee/zeetures/commit/15e3cbe5e4525e69476e71bcd7ccefff8fdea130)

The File menu's single Ambient Music toggle becomes a Sound submenu with
three switches: ambient music, the world's sounds (everything but music
and speech), and Bibble (creatures' speech). Creature speech now has its
own mute, so muting the world's sounds leaves it playing and the reverse.

#### Hatchery: File &gt; Import Genome makes an egg from any genome file

[`edc17785`](https://github.com/DotairZee/zeetures/commit/edc177851e80e75b5a7677df35c0ccfdbb60f100)

The C2 Hatchery window gets a File menu. Import Genome opens the desktop's
file chooser (SDL3's dialog, through the xdg file-chooser portal where
there is one); each chosen Creatures 2 genome ("dna2") becomes an egg in
the Hatchery tray, which is clicked into the world like the others. One
parent genome, so the creature is that genome with the usual mutations.

#### Camera follows tracked agents; toolbar halo button works

[`11275277`](https://github.com/DotairZee/zeetures/commit/11275277b82e368571333fbafce84ff9644d3b6c)

The camera's per-tick tracking update was never called, so following a
creature (toolbar follow button, View &gt; Track Norn) only centred on it
once. The engine now ticks the camera every world tick.

The halo button runs the game's own system macro (world-file script
0 1 0 0), which makes or removes the halo object (2 1 3); the halo's
timer script, also from the world file, keeps it over the selected
creature's head.

#### World save keeps objects a script marks with OV99 = 7461

[`22d3996a`](https://github.com/DotairZee/zeetures/commit/22d3996a70478213bbeb7e14003f72d62c1e2397)

C2 world saves kept only creatures and eggs; everything else came from the
world file or was remade by scripts at load. Objects whose OV99 is 7461
are now saved and restored like eggs (sprite, image count, picture, plane,
attributes, timer, place and object variables), so script-made things that
change over game time, such as plants that grow, carry on between
sessions.

#### OPENC2E_WINDOW_POS places the game window

[`e4692f5d`](https://github.com/DotairZee/zeetures/commit/e4692f5d924e7c57bc7d67bf44b830452adc11be)

OPENC2E_WINDOW_POS="x,y" moves the window's top-left corner to those
desktop coordinates after it is created, so it can open on a chosen
monitor (works under X11 and XWayland); otherwise the window system
decides as before.

#### Developer aid: dump agents in a box with plane and sprite

[`17068abd`](https://github.com/DotairZee/zeetures/commit/17068abdbe3f8b863fe762af40ffa83c243fd25a)

OPENC2E_DUMP_AGENTS="x0 y0 x1 y1" prints, once after load, every agent
overlapping the box: classifier, plane, size, and each part's sprite file,
image and offset. Used to find the foreground overlay pieces (family 1,
sprite fore, plane 8999) drawn over scenery edits.

#### ROOM resizes a room's walls, not only its corner numbers

[`ad7072c1`](https://github.com/DotairZee/zeetures/commit/ad7072c1c534d68f465617bd618e1c0c5205f83e)

Changing an existing room with ROOM (C1 and C2) set x_left, x_right and
the floor and ceiling numbers but left the wall lines built when the room
was made. Containment (roomAt), floors without floor points and collisions
all use those lines, so a resized room kept acting at its old size. Room
now has setBounds, used by the constructor and by both ROOM variants.

#### Remove a duplicate room-dump aid

[`72c72618`](https://github.com/DotairZee/zeetures/commit/72c726182284df1b38cb8e3e8186cf52839c2422)

Engine.cpp already has OPENC2E_DUMP_ROOMS (comma-separated box, with
floors, floor values and door permeabilities); the copy added in World.cpp
read the same variable with spaces instead.

#### Developer aids: dump the room map around a box, list sunk agents

[`1282a7b6`](https://github.com/DotairZee/zeetures/commit/1282a7b619fdd62bd2a9ea5d488aa70e4e0ad806)

OPENC2E_DUMP_ROOMS="x0 y0 x1 y1" prints, once after load, every room
overlapping the box with its C2 data, floor points and doors.
OPENC2E_WATCH_SUNK=N lists, every N ticks, agents whose bottom is more
than 20 px below the solid floor of their room, or outside every room.

#### C2 creatures fall through floors they are light enough to pass again

[`b3410f1c`](https://github.com/DotairZee/zeetures/commit/b3410f1c785275dc9cae069cc444c1c740260242)

SkeletalCreature::snapDownFoot's check for falling through the floor (C2:
the creature's size against the floor value, or against the permeability
of the door to the room below) sat inside 'if (engine.version &gt; 2)' since
a 2008 change that meant to extend it to C3 (it kept the C2 code inside).
So C2 creatures never fell through any floor and stood on the sea surfaces
(a norn placed over the east sea stood on room 140's floor at y 693). Now
'&gt;= 2': it drops through to the seabed and walks for dry land.

Test batch (16 runs, hungry start) against the previous one: upper east
starving 72% to 49% of the time, a real meal in 5 to 9 of 16 runs,
recovering 31% to 44%; far east starving 27% to 45%; recoveries per run
0.69 to 0.88. 20 creatures were in water at some point, and frozen at the
end rose 0.19 to 0.38 per run (drowning after falling in, now that water is
water).

#### C2 vehicles: DPS2 drops passengers (was a stub); log boarding when watching

[`d345e5b3`](https://github.com/DotairZee/zeetures/commit/d345e5b3f765d9774475235c03275741d39918b9)

C2's boats let passengers off at the far shore with DPS2, which openc2e had
as an empty stub, so they never did: a passenger rode to the far end, rode
back, and was dropped wherever the ride ended, often over the sea (a norn
seated correctly in the east-sea boat's cabin was carried back to the
mooring and fell in). DPS2 now drops creature passengers as DPAS does; with
a non-zero argument they fall, with zero the boat's own script places them.

Vehicle::carry logs where it seats a passenger when OPENC2E_WATCH_CREATURES
is set.

#### Creatures sense drops ahead and do not walk off docks and ledges unintentionally

[`0d19d7bf`](https://github.com/DotairZee/zeetures/commit/0d19d7bf23e96f841c2cabfa518b4ec82798c4ab)

A norn on the east dock walked off its end (x 7070, y 672): the room beside
a dock is air above the water, not water, so the water-ahead check missed
it, and it fell into the sea and on down into the underground sea.
Perception now reports, for each side the creature fits through, how far
the floor drops stepping through it (more than 60 px counts), and a drop
whose fall lands in water (following open floors down up to three rooms)
counts as water ahead. Searching, smell-following and same-floor food trips
treat a drop of more than 150 px as a wall, unless the trip is headed that
much lower.

Tested on the east dock: the norn no longer walked off; it boarded the boat
(activate 2, rowing west) and learned the crossing as a travel link. It was
then let off mid-sea: the boat's script sets passengers down 50 px up-left
of where they stand, assuming they stand at its front, and this norn stood
behind the boat's right edge. Where passengers sit aboard is next.

#### C2 SETV DOOR: use the permeability, not the direction

[`884aa979`](https://github.com/DotairZee/zeetures/commit/884aa97984bcb4a364e091794af60c92a4ee07b2)

SETV DOOR direction room1 room2 permeability: the parameters were read
shifted, so the direction (0-3) became the door's permeability and the real
permeability was ignored. Every door a C2 script opens or closes ended up
0-3 permeable: the airlock's land door 'opened' to 1, too narrow for any
creature (size 224), so creatures could never leave the airlock that way.
The same command is what add-ons such as the Walls and Bridges pack use to
connect new rooms.

#### Senses of direction count only doorways a creature fits; boat switches learned; switches clickable

[`c810f538`](https://github.com/DotairZee/zeetures/commit/c810f5383e170fab9513b63b551a3d5aae00e0e0)

- Perception (smell west/east/up/down, water ahead, the way to dry land) now
  counts only doorways whose permeability is at least the creature's size
  (C2: 224, as snapDownFoot uses). Smell still spreads through narrower
  gaps, but following it there walked creatures into walls: at the middle
  lift's lower stop (x ~5778, y 2204) food smelled strongest to the west
  through a gap no norn passes, and east-cave norns rode the lift up and
  down for ever. Test batch (16 runs, hungry start): east caves starving
  100% to 89% of the time, a real meal in 0 to 4 of 16 runs, recovering
  0% to 25%; upper east real meals 2 to 5 of 16; far east starving 46% to
  27%; recoveries per run 0.38 to 0.69.
- The mind learns what a button does for boats: pressed to call a boat,
  then a boat within reach within 200 ticks, it is a known boat switch
  (saved, preferred, worth 1.2x); a button that brings no boat more often
  than it does is not pressed for boats again. Logged as callboat:.
- (Project additions script) the boat switches are the game's pole call
  button standing on the docks, with BHVR 2 so clicks activate them.

#### World additions run at load; creatures call boats with switches

[`430fe6f2`](https://github.com/DotairZee/zeetures/commit/430fe6f2249f0b7189f3f9dbc955d5aa9cc57e34)

Owner request: switches, like the lifts' call buttons, that summon a boat
to the creature's side of the sea.

- Engine: after a C1/C2 world loads, every .cos file in the user data
  folder's Additions folder (all worlds) and then the world folder's own is
  run (scripts and commands, like a COB). Objects they make are not saved,
  so they are made anew at each load. GALL (a TARG's sprite name) is now
  readable in C2.
- Boat call switches (tools/additions/boat-switches.cos in the project,
  copied to ~/.openc2e/Creatures 2/Additions): one switch on each shore of
  each of the three boats, using the game's own lift-button picture (lbtn)
  from the player's installed copy. Pressing one rows its boat over if it is
  moored on the far side. Tested: the west sea's east-shore switch brought
  its boat from x 280 to 760, the west-shore switch sent it back, and a
  switch on the side where the boat already is does nothing.
- Mind: a trip that runs into water with no boat within reach presses a
  button within reach (category 2: these switches, or harmlessly a lift
  button) to call one, worth 0.8 of the crossing; the boat that arrives is
  then boarded as before.

Not yet demonstrated: a creature calling a boat and riding it on its own.

#### Homeostat mind: travel links learned from devices; CHEM with a negative amount subtracts

[`23335187`](https://github.com/DotairZee/zeetures/commit/233351874580657ff15a9ba0363ca5e1db79593e)

Owner's idea: think about boats the way the airlock exit is thought about.
Using a device (boat, lift, button, door, machine) and soon being
somewhere quite different (a ride ending 300 px or more away, or on another
floor) is remembered as a travel link: this object, used here, takes me
there (saved, up to 16). Heading for a remembered food place, a creature
uses a link that starts within 350 px of it and ends at least 400 px
closer; foraging, it uses a link to somewhere not visited lately. Boats,
lifts and the airlock become cases of one idea.

CHEM (C1/C2) with a negative amount was cast to a byte and added (-255
added 1); it now takes chemical away (an openc2e extension; real C2
ignores negative amounts). Needed for tests that empty the stomach.

Test batch (tools/batch, 16 runs of 6 freshly grown adults, hungry start):
884 travel links learned and 2,035 uses on food and forage trips. East-cave
creatures still never get a real meal (only snacks), upper east starve 71%
of the time.

#### C2: fix the runaway boat; creatures use boats only within reach, with the right actions

[`1026fd47`](https://github.com/DotairZee/zeetures/commit/1026fd47f6fbe97518bc8feb6022b6d7a54cf7fd)

The runaway boat (owner, 2026-11-07): the boats' scripts set PART for their
own oars, then fling passengers ashore with POSE 128 aimed at each creature.
SkeletalCreature::part threw for any part but 0, so the script died
mid-trip with the boat still moving (velx -20), and it sailed round the
wrapping world for ever with its passenger. A C1/C2 creature is a single
part; any part number now means the creature itself. Tested: the boats
stay at their moorings and finish their trips, with no boat script errors.

Boat use by the mind:
- C2 decisions are 0 idle, 1 activate 1, 2 activate 2; the boat and the
  water-escape device attempts used 0 and 1 (idle and activate 1). Fixed.
- A boat already moored on the far side ignores "that way": a press that
  gives no ride within 60 ticks makes the mind try the other action for a
  while (learned rides still decide in the long run).
- Only a boat the creature can reach at its own level is offered (one seen
  from below, on the sea surface above, stalled every attempt).

A full creature-chosen crossing has not been seen in a test yet.

#### Creatures avoid walking into seas and use boats to cross them

[`fa4d3337`](https://github.com/DotairZee/zeetures/commit/fa4d33375c519b2c537657b8febe39a7c7d1e99a)

Owner request. Perception now says whether the room through the door on
each side is water. Trips that walk east or west (food map, foraging,
smell) no longer walk into water from dry land; with a boat in view
(category 31: the boats 3 6 1/2/3) the mind offers rowing that way instead,
worth 0.9 of the trip. Which boat action goes which way starts as an innate
guess (activate 1 east, activate 2 west, as the boat scripts are written)
and is replaced by learned rides (how far east or west a ride carried the
creature; saved).

Known problem: in a test a hungry norn chose the western boat (3 6 2) and
then it and the boat travelled west around the whole world without stopping
(x 280, 8028, 7084, 4720, 1996, 7604...), the boat a constant 68-88 px from
the norn. This looks like the runaway boat the owner saw on 2026-11-07, now
triggered reliably; vehicle and passenger movement needs its own fix.

#### Creatures hold their breath for 5 minutes under water

[`8cfb360a`](https://github.com/DotairZee/zeetures/commit/8cfb360a4a967d29169acdef20476e7a36e3d104)

Owner's choice: creatures are good at holding their breath. Under water a
C2 creature now keeps its air sense (so the genome keeps making air and
oxygen) for 3000 ticks, 5 minutes at C2's 10 ticks a second, before it runs
out and suffocation sets in as before; surfacing refills the breath within
a few seconds. The mind sees the breath left, and the urge to get out of the
water grows with (1 - breath)^2 (up to 0.3 more).

Staged test: a healthy norn in the flooded airlock kept oxygen 159-255 for
about 5 game minutes, then ran out and lost consciousness. It used the
airlock pump to get dry, remembered it as an exit, but kept using it (the
pump is a toggle, so it flooded the airlock again); leaving through the land
door once dry is still to come.

#### Creatures escape water: sense of water and the way to dry land, device attempts, exit memory

[`76121d06`](https://github.com/DotairZee/zeetures/commit/76121d064b9457e8bf84e178ce86d7d7b468cdd3)

Owner request (escape-from-water instinct). Perception now says whether the
creature is in a water room (C2 room type 2) and which way the nearest dry
room lies through open doors on foot (sideways or down: a creature cannot
climb up through water to the surface, and an earlier version that allowed
it sent airlock norns swimming into the sea).

In water the mind adds an escape option worth the relief of clearing
Suffocate plus a little urgency: toward the dry room if there is a way on
foot, otherwise trying the devices in view (C2 categories are genus
numbers: call buttons 2, machines such as the airlock pump 3, doors such as
the airlock's 9, lifts 27); passing over handles devices that do nothing.
Leaving the water within 400 ticks of using a device makes that device, there,
a remembered exit (saved; tried first next time). Only devices count: a
creature that left the water just after hitting a norn had learned that.

Staged test (airlock drained, flooded again with creatures inside): a
creature used the pump (the airlock drained), then the land door (it
opened), and remembered both as exits. A norn placed in the flooded airlock
suffocated (ATP 0) within about 8 s before it could act. Hunger runs: mean
hunger 179 and 163 (lowest recent), meals 27 and 25 (run-to-run spread is
large, 15 to 117 on similar builds), about 3 recoveries.

#### Creatures notice objects partly in view; attention moves on from useless objects

[`eda9725b`](https://github.com/DotairZee/zeetures/commit/eda9725bc9bd15ef1fd9fb37cefaa715eaaabf32)

A creature notices one object per category (the nearest), so a creature in
the airlock only ever saw the sea door and never the land door that lets it
out. Two changes:

- Sight: a creature now sees an object if any part of it is in view (the
  centre, or failing that the nearest point of its bounding box), not only
  its centre. The airlock's land door (2 9 8, x 6700-6920) straddles the
  airlock's east wall (x 6722) and is centred outside it; from inside it was
  invisible. Scripts' own visibility tests are unchanged.
- Passing over: when the mind has used the same object twice in a row for
  no benefit (overall drive did not drop, the creature did not move to
  another floor or far away), it tells the body to pass over that object;
  for about 3 minutes it counts as six times farther away when the
  representative object of its category is chosen, so the next one gets
  noticed. Steps toward a destination (food trips, floor changes, foraging,
  following smell) are exempt: a lift's call button helps only through the
  ride that follows, and passing over call buttons halved meals in tests.

Hunger runs (two runs, 6 creatures): real meals 103 and 81 (best pair so
far; 117/41 after the seam fix). A norn in the airlock now walks to the land
door and has it among its top choices, but it was not yet seen to use it;
others left the airlock through the sea door into the water.

#### C2 creatures: walk across the world's east-west seam; DIRN command for C2

[`f2b92bee`](https://github.com/DotairZee/zeetures/commit/f2b92bee986a942c02fca5c705cb1029d71b0cdc)

C2's Albia wraps: the room at the east edge (184, x 8240-8352) has an open
door to the room at the west edge (181, x 0-58). The metaroom was already
marked as wrapping, but SkeletalCreature::snapDownFoot looked for the room
under a foot that had stepped past its room by the foot's unwrapped x, found
none past the seam, and pushed the creature back, so creatures walking east
were stuck at the edge (one starved there in every hunger run). Rooms beyond
the seam are now judged with the foot's x wrapped to their end, and the
creature carries on from the other end of the world. Tested: a norn walking
east went x 8261, 8314, then 76, 190, 282 onwards.

DIRN (set facing) is now registered for C2, as its read form already was.

#### Homeostat mind: restlessness, so long hunger without a meal widens the search

[`24c877a4`](https://github.com/DotairZee/zeetures/commit/24c877a44a85eb0f2dfa2a6b2b1f27e9628a8bf8)

Owner's idea: the longer a creature stays hungry without a meal that eases
it, the sooner and harder it searches and the more readily it leaves its own
floor. Restlessness builds over ~1500 mind ticks (about 10 game minutes)
spent at Hunger 0.3 or more and resets with a good meal (saved with the
creature). At full restlessness foraging starts at Hunger 0.25 instead of
0.45, is worth up to three times as much, and each new heading has up to a
60% chance of trying another floor (the less recently visited of the floors
above and below); a floor whose nearby parts have been visited both ways
recently counts as searched and is left. The choice log shows restlessness.

Hunger runs (two runs, 6 creatures each): the widest search spans grew from
at most 887 px to 1,247-1,988 px. Recoveries from starvation were 1 (3 in
the previous pair; within run-to-run noise): most creatures still starving
are physically trapped, in the airlock (room 154), a dead-end pocket at the
far east edge (x ~8300, y ~980) and a cave pocket (x ~6630, y ~2170).

#### Homeostat mind: food map, travel valued by time, foraging, giving up on dead ends

[`9790a2ae`](https://github.com/DotairZee/zeetures/commit/9790a2aef9cf4b9552f7aa7db95e48a9ea30fadd)

Starving creatures in the eastern caves never reached food: the mind
remembered only the last place each kind of object was seen (not saved),
subtracted a flat cost per pixel so a meal 2,000 px away scored below zero
even when starving, and had nothing to do when no food was seen, smelled or
remembered, so it pushed machines and slept.

- Food map: up to 8 places where food was seen (smelling at least 0.3
  nourishing) or a meal eased hunger, merged within 200 px, saved with the
  creature. Arriving and finding nothing halves a place's confidence
  (food regrows, so places fade rather than vanish).
- Travel valued by time (owner's idea), walking speed taken as constant: a
  remembered meal keeps half its value per 700 px (a floor change counts as
  400 px), stretched by hunger (x (1 + 6 hunger^2)), so a starving creature
  will cross the world for food and a peckish one will not.
- Foraging: hungry (above 0.45), nothing in sight, remembered or smelled:
  walk toward the part of this floor visited least recently (400 px bins),
  keeping the heading; turn around if it has not carried the creature 80 px
  in 60 ticks, and try another floor if both ways are stuck.
- A food trip that has not come 100 px closer in 300 ticks marks that place
  out of reach from here (confidence halved, not tried for 600 ticks).
- Choice log shows food:/forage: choices, position and the food map.

Hunger runs on a copy of Home (two runs each, 6 creatures): creatures that
recovered from starvation 0 before, 3 now. Those still starving live far
east, where food is on floors the lift there does not stop at; reaching it
needs a learned map of how floors connect.

#### C2 creatures: organs no longer re-add their genes at each life stage; C2's own reaction table

[`b8c8029b`](https://github.com/DotairZee/zeetures/commit/b8c8029b287b10da0d3068d224ddb13a24013fee)

c2Organ::processGenes turned every gene of the organ into a reaction,
emitter or receptor each time it ran, and it runs at every life stage (and
when a saved creature is restored, which replays the stages). So a child ran
every body reaction twice, an adult five times: oxygen, heat, water, stress
hormones and stores all went wrong at the first change of stage (a fresh
newborn was healthy, then crashed to ATP 0 within minutes of becoming a
child). Each organ now processes only the genes added since its last call.
Genes that switch on at later stages also joined whichever organ was last in
the list; c2Creature::processGenes now walks the genome in order and gives
each gene to the organ it follows.

With that fixed, openc2e's original reaction table (on rate / 8, "snaffled
from real creatures") matches every reaction timed in a real C2 norn: rate 0
reacts fully per step, 16 55% (table 55.5%), 32 20% (18.3%), 40 10-12%
(9.6%), and rate 80 takes 7 of 250 muscle every 8 steps, exactly. It is the
default again; the curves fitted on 2026-11-07/08 were artifacts of the
duplicated reactions and a probe whose injections never arrived
(OPENC2E_C2RATE_CURVE=1 keeps the last one for comparison). A newborn now
matches the real one closely (glycogen 196 vs 194, oxygen store full, muscle
223 vs 206).

Life stage at will now goes stage by stage (owner request): the Creatures
menu ages through the genome's own ageing receptors (the life chemical is
lowered to where the stage ends), with a 2 s pause between stages; younger
is rebuilt one stage at a time. DBG: STAG stage does the same from CAOS (C1/C2).

#### C2 world: room climate (heat spreading, heat sources) and temperature senses

[`2b3ad0b6`](https://github.com/DotairZee/zeetures/commit/2b3ad0b6f42105510edff3cfd41435bfb79562ea)

Room temperatures now change over time (World::tickClimate, about once a
second). Heat spreading is decoded from the real game: a room raised to 255
lost 1/8 of its gap to each fully open neighbour per step, about once a
second, with total heat conserved, so each door passes perm/255 \* 1/8 of the
difference. Heat sources are our own design, because C2's barely act (a
cooled volcano room did not recover in minutes, and the +10 desert rooms were
cooler than the incubator): a room with a heat source is pulled toward 105
shifted by 30 \* sqrt(|source|), and every room relaxes slowly toward 105.
Rain and lightning scripts still nudge rooms; temperatures stay within 0-255
(openc2e had rooms at -1000 and below, from rain with nothing to warm them).

On a copy of Home after ~25 game minutes: incubator area 120-129, desert
168-177, volcano 204 (surroundings 153-158), sea 14-22.

Creatures sense it: air coldness and hotness (the genome's emitters fire
above 8) are 3 per degree outside a comfort band of 50-140. The hot side
agrees with real C2 (a newborn heated up in a ~170 room, not at 89-114); the
cold side is our choice, as real C2 creatures felt no cold even at 30.

#### C2 biochemistry: reaction half-life as a power law of rate, fitted with a rate-80 point

[`7087ced3`](https://github.com/DotairZee/zeetures/commit/7087ced3004e454031b12c5ee44e576683d93479)

A rate-80 reaction timed in a real C2 norn (muscle tissue to amino acid)
converted 2.8% per organ step, six times the exponential fitted this
morning. Half-life (rate / 17.5)^2.09 organ activations fits all five timed
rates (0, 16, 32, 40, 80) within about 7%; the best exponential in rate is
off by about 30%. Newborns still hold ATP (lowest 119 in a 75 s headless
run). OPENC2E_C2RATE_K / \_P override the curve.

#### C1/C2 creatures: change a creature's life stage at will, older or younger

[`5294a602`](https://github.com/DotairZee/zeetures/commit/5294a60268ce311bf1ebb9f269fea3648bf6e9dd)

Creatures &gt; Selected Creature's Life Stage (Baby to Senile) and AGES in C1/C2
CAOS (now registered for c1/c2; a negative count makes the creature younger,
an openc2e extension). Older steps through ageCreature as natural ageing
does. Younger rebuilds the creature at the earlier stage from its genome
with the save/restore code, keeping its chemistry, organs, mind state,
position, selection, moniker and history (new monikerData::transferCreature),
since genes that have switched on cannot be switched off. Either way the
genome's life chemical is set to where the new stage begins (the previous
stage's ageing-receptor threshold; the starting level for a baby), so the
ageing receptors neither undo the change nor keep the old timing.

Tested headless on a copy of Home: adult to stage 2 (life chemical 194),
then to stage 5 (19); same place, ATP healthy, creature count unchanged,
learned state kept.

#### C2 biochemistry: reaction curve refitted to timed real-C2 reactions; relay cells hold

[`30034917`](https://github.com/DotairZee/zeetures/commit/3003491755fc8c9da24e987188c3edd03fd38ecd)

Reactions timed in a real C2 norn over DDE (inject a reactant, sample every
~0.2 s): per organ step, rate 0 reacted fully, rate 16 55%, rate 32 20%,
rate 40 10-12%, with organ step intervals (0.8-2.5 s) matching openc2e's
organ clocks. The new curve, half-life 0.25 \* 1.9^(rate / 8) activations,
fits every point. The previous fit (2.2^(rate / 8)) rested on injections
that never arrived and ran mid-rate reactions 4-9 times too slowly, so
newborns burned their energy store and froze at ATP 0.

The chemical relay lobe now settles toward rest with its leakage rate read as
a half-life, as the regular lobe update does, instead of a guessed 15% of
the gap per tick that emptied cells between receptor updates.

Headless tests on a copy of Home: a newborn holds ATP 149-255 for 75 s
(froze within 15 s before); fed adults hold ATP 200-245 and eat to Hunger 0.
Not yet measured: reactions above rate 48.

#### Homeostat mind: survival needs outweigh comfort; pressing needs quiet curiosity

[`9a5ef770`](https://github.com/DotairZee/zeetures/commit/9a5ef7707a3061895a8b889539e7bae46dcc7b85)

Pain, Hunger, Cold, Hot, Injury, Suffocate and Thirst now weigh 1.5 in the
overall drive, and Lonely, Crowded and Bored 0.5, so Hunger at 0.5 counts as
much as Boredom at about 0.72. The strongest survival need also scales
curiosity down (by up to 80%), so a hungry creature is less drawn to novelty.

In two test runs on a copy of Home, real food eaten rose from 15-17 to 26-34
per run and mean hunger fell from 117-156 to 102-121; loneliness and boredom
rose only slightly.

#### C2 creatures: underwater check and bubbles at the mouth

[`00ed72a6`](https://github.com/DotairZee/zeetures/commit/00ed72a6bdfbf11573fce01986060bdc184c3069)

The creature is underwater when its mouth (point 1 of the head part's
attachments) is in a water room, and bubbles start there, instead of near
the top centre of the body. A creature lying with its head out of the
water now breathes.

#### Homeostat mind: food smell refinements; log smells with each choice

[`a1902815`](https://github.com/DotairZee/zeetures/commit/a19028157467daf978ce5161988e7c84d817a307)

- Following food smell is valued by the best hunger relief the creature
  has actually had from eating food-smelling things, when that is more
  than the typical guess.
- Something that does not smell of food is not expected to ease hunger
  (its other effects still count). C2's eat scripts give the eater a small
  pleasure stimulus whatever it eats, and a leftover "eating helps hunger"
  hunch made hungry norns snack on seeds instead of following a strong
  smell to real food. Replaces the earlier share-based discount.
- The choice log line shows the smell here and to the west, east, up and
  down.

#### Homeostat mind: sleep, felt effects, blocked ways, unscented food

[`b1fcd31d`](https://github.com/DotairZee/zeetures/commit/b1fcd31d693a1e42f223f4ab32f8cd348959a6df)

- Sleep: the world's sleep script keeps the creature asleep and resting
  until rested, then wakes it. The mind used to end the episode after 12
  ticks and queue another action, cutting the script off: the creature
  stayed asleep for good, unable to do anything (other action scripts stop
  when asleep). The mind now lets the sleep script run, wakes the creature
  early only when a bodily need (pain, hunger, cold, heat, fear, injury,
  suffocation, thirst) has risen sharply since it fell asleep, and wakes a
  creature that is asleep without having chosen to sleep (an interrupted
  script, or a world saved mid-sleep).
- Felt effects: a drive that an episode's stimuli touched (C2 chemicals
  18-34 raise drives 1-17, 35-51 lower them) is learned from what was
  felt; other drives from the change beyond drift, at a quarter of the
  rate, since that change mixes in whatever else was going on.
- The drift baseline can no longer take a drive past empty or full over a
  long episode.
- No decisions on the first tick after birth or loading, when the drives
  still read all zero.
- Blocked ways: walking toward something that left the creature where it
  was marks that direction as blocked from there for a while.
- Eating something that does not smell of food, in a category that
  usually does (a seed among fruit), is expected to help much less.
- Saved learning before these changes is mostly noise: worlds saved with
  mind state version 2 or 3 relearn from instincts and smells (rides
  between floors are kept). Mind state version is now 4.

#### C2 creatures: adults feel numbered stimuli; light sense; felt chemicals for minds

[`a5db3b5c`](https://github.com/DotairZee/zeetures/commit/a5db3b5c183728145b8bffda1ffe45efc7ad5336)

- Numbered stimuli (STM# WRIT: sleeping, walking, playing, being patted)
  looked up their stimulus gene with shouldProcessGene, which only passes
  genes switching on at the creature's current life stage. Almost all
  stimulus genes switch on at birth, so for any creature past babyhood
  these stimuli changed nothing in the body; for example a sleeping
  creature never got less sleepy. They now use genes switched on at this or
  an earlier stage (Creature::isGeneExpressed).
- The light-level sense now reads the light of the creature's room (it was
  always 0, which the genome reads as constant darkness).
- Stimulus events passed to a mind carry the chemicals the body actually
  received (feltChem/feltAmount), for either form of stimulus.
- Choosing the agent that stands for a category: food-smelling agents
  count as nearer, so in a category mixing food with things that are not
  (a fruit and its seed) the food is noticed first.

#### C2 biochemistry: reaction speeds from a smooth curve fitted to real C2

[`3f3f12f0`](https://github.com/DotairZee/zeetures/commit/3f3f12f03a1046fffbc29adfd3f82b93580e8934)

openc2e ran C2 reactions with C1's rate table on rate / 8 (marked untested):
fast reactions crawled, oxygen settled around 30-50, adrenaline rose and
drained glycogen, so creatures had no reserves and starved within minutes.

A real C2 norn, measured over DDE (inject chemicals, sample every 2-4 s):
glycogen ~195 and stable, oxygen 190-255, adrenaline 0, fat slowly rising.
Now a reaction's half-life is H0 \* B^(rate / 8) organ activations and that
share of the possible amount reacts each activation, with fractions carried
over so slow reactions still happen in whole units. Of the curves tried,
B = 2.2, H0 = 1 (the curve openc2e uses for C3) reproduces the real levels:
glycogen 202-223, oxygen 193-255, adrenaline 0, fat 181-196 and rising.

Headless runs of the owner's world, creatures starting with real-like
reserves: all healthy norns kept their energy for ~23 game minutes (before:
collapse within ~6). They still eat too rarely to refill their stores.
Starch digests about 5x slower than in real C2 (not yet explained).
OPENC2E_C2RATE_B=0 restores the old table; OPENC2E_C2RATE_H0 also adjusts.

### 7 November 2026

#### Developer aids: log C2 reactions that consume glycogen (OPENC2E_WATCH_REACTIONS)

[`9a9ab2f2`](https://github.com/DotairZee/zeetures/commit/9a9ab2f2b3280fea14055d91e75b84f8177c9474)

#### C2: food smell spreading through the room system; creatures follow it

[`dd408680`](https://github.com/DotairZee/zeetures/commit/dd408680fb272879c51ed16a43ae6ab8ae753d5e)

An addition to C2 (owner's idea). Every 5 ticks: a room holding edible food
near its floor smells at least as strongly as its most edible item (edible
scent from eat scripts, now a shared helper in AgentHelpers); smell spreads
to neighbouring rooms through doors in proportion to how open each door is
(permeability / 255, walls pass nothing) and fades 3% per step, giving a
gradient of about 20% per room away from food (0-1).

The mind perceives the smell in its room and the strongest through the
doors on each side (west, east, up, down). When no reachable food is in
view, following the smell is a candidate: walking toward a stronger side,
or changing floors (lift links) when the smell is stronger above or below.

Headless runs: 194 of 251 rooms carry some smell, peaking in rooms with
food, and hungry creatures follow it, but survival is not better yet: meals
are not being stored (glycogen stays near 0, unlike a real norn's 189), so
the next step is checking C2 reaction rates against the real game.

With OPENC2E_WATCH_CREATURES a summary of the smell map is logged.

#### Developer aids: log when APPR reaches its target (OPENC2E_WATCH_CREATURES)

[`eccd5708`](https://github.com/DotairZee/zeetures/commit/eccd5708f7d06ec1b81d7f76315cb1174880f5a2)

#### Creature mind: only reachable objects, and patience while getting closer

[`e1747a3f`](https://github.com/DotairZee/zeetures/commit/e1747a3fc9da5351a47beeb0b1b2fa0e99bee5d7)

Tracing eat attempts showed two failures. A third of them aimed at food far
above or below the creature: APPR only walks sideways and counts as arrived
when the two touch, so such food could never be reached. And most attempts
ended on the episode timer while the creature was still walking over (IMPT,
C2's "do not interrupt", is a stub), so the next choice interrupted the
creature's eat script.

The engine now tells the mind whether each seen object overlaps the
creature's height (reachable by walking); unreachable objects are not
candidates for actions on objects. An action on an object now lasts while
the creature keeps getting closer: it ends when the script finishes, the
object goes, no progress for 25 mind ticks, or after 150.

Headless runs: eat attempts on food succeed 75-80% of the time (was under
10%). Norns still run low because reachable food is in view only about 11%
of the time while they are hungry, which spreading scent should address.

#### Creature mind: edible scent, and walking distance counts when seeking

[`7ae757f8`](https://github.com/DotairZee/zeetures/commit/7ae757f898a227cdb18859f8e2945c1cfcb1115e)

Edible scent (owner's idea): the engine reads each seen object's eat script
(event 12) and sums the nourishment its stimulus gives the eater (glucose,
fatty acid, glycogen, starch, fat, protein, amino acid, and the direct
"Hunger Decrease" chemical), scaled to 0-1 and cached per script, so it
works for add-on objects too. In the owner's world: fruit 0.2-1.0, roots
0.78, seeds 0.4-0.5, processed food 0.6-0.8, bugs 0.12, buttons and drinks
\0. The mind turns a scent into an expected hunger relief for eating that
kind of thing (up to 0.4, weighted as 6 experiences); no food smell removes
the vague "eat anything" hope about hunger.

Seeking a remembered place now costs 0.00015 per pixel of walking (was the
general 0.00002), so far-off food no longer beats food close at hand.

Headless runs: hungry norns now mostly try real food, but fewer than 1 in 10
eat attempts ends with food eaten, and they still starve; next is finding
where those attempts fail.

#### Creature mind: learn lift rides and use them to change floors

[`e8475a03`](https://github.com/DotairZee/zeetures/commit/e8475a0365126280e8acd3a26713271fd122d70b)

The engine tells the mind when the creature is riding a vehicle. The mind
holds still while riding and, when a ride ends more than 60 px higher or
lower, remembers it as a link: at this lift position, this action takes it
from one height to another. When something it wants is remembered on another
floor, it walks to a known lift going the right way, calls the lift with a
call button if it is not there, and boards it with the learned action; with
no link known yet, it goes to the nearest lift and tries activating it. The
only innate part is that lifts (C2 category 27) and call buttons (2) are for
changing floors; which action goes which way is learned. Links are saved.

Seeking and changing floors now go only after things the creature has
experienced or that a specific instinct names. The genome's "any object"
instincts are a hunch that sets starting expectations but adds no weight
against experience.

Headless runs: creatures learn both directions of a lift within minutes.
Survival is still poor and needs a separate tuning pass (several needs at
maximum at once, sleep, lingering hopes from hunches).

With OPENC2E_WATCH_CREATURES, boarding and leaving vehicles is logged.

#### C2 lifts: choose a destination when a creature activates the lift

[`5cd7c330`](https://github.com/DotairZee/zeetures/commit/5cd7c330488519ce20510fba0e2983cfe5c9f08b)

Lift::fireScript only picked the next call button (1 down, 2 up) when the
hand activated the lift. When a creature did, the destination stayed the
floor the lift was already on, so it stopped at once and dropped its
passengers: creatures could never ride a lift. Now any activator except the
lift itself (which already chose one when answering a call button) picks the
next floor in the requested direction. Headless runs: creatures now ride
between floors (e.g. from y 1954 up to y 1276 and back down).

#### C1/C2 creatures: see the nearest object of each kind, not a random one

[`e8555c80`](https://github.com/DotairZee/zeetures/commit/e8555c8066c329a1fdb18dee9b12fcf55e943360)

Creatures attend to one representative object per category. openc2e picked
a random visible one (a TODO) and kept it while it stayed in sight, so a
hungry norn often fixed on food on another floor, 230-460 px above or below,
that it could not walk to, and the attempt timed out. Now the representative
is the nearest, with height counting four times as much as width, and it is
re-picked each time except for the object the creature is acting on.

Headless runs of the owner's world: 3 of 6 norns still fed at the end of
long runs (was 1 of 6); eat attempts on actual food rose to about a quarter.

#### Developer aids: log what creatures act on and what gets eaten

[`237c6989`](https://github.com/DotairZee/zeetures/commit/237c6989b8780c337759c3e5cb2fc321c5a84c20)

With OPENC2E_WATCH_CREATURES, each decision the mind starts on an object
is logged with the target's classifier, category and offset, and each eat
event (12) on a C2 object is logged with its classifier and the eater.

#### Crash handler: run on its own stack and print module offsets

[`a0dfefbc`](https://github.com/DotairZee/zeetures/commit/a0dfefbc7f769bb977ab8bb69650fb6750616199)

install_backtrace_printer used signal() without an alternate stack, so a
stack overflow killed the process before the handler could print anything.
It now uses sigaction with SA_ONSTACK on a 256 KB alternate stack, and each
frame also shows its offset inside its module (\[+0x...\]) so addr2line can
turn it into a source line even without exported symbols.

#### C2 creatures: stop snapDownFoot recursing until the stack overflows

[`a2e1bdb4`](https://github.com/DotairZee/zeetures/commit/a2e1bdb48ef734967d83ce2c7c8394f2fd29b431)

When a creature's foot had no room it could enter and it had moved more
than 50 px since its last good position (a script moved it), snapDownFoot
looked up the room under the foot and called itself again. If that call
also found no room it could enter (more likely since neighbouring floors far
above the foot count as walls, 0f5d9772), it recursed until the stack
overflowed. The game then vanished without saving or printing anything (the
crash handler had no stack to run on): the owner's game disappeared twice
this way, and 2 of 3 long headless runs crashed. Now it retries once, then
accepts the room under the foot instead of recursing again. Four 5-minute
fast runs afterwards: no crashes.

#### Creature mind: genome instincts, seeking remembered places, longer object actions

[`8ffa88de`](https://github.com/DotairZee/zeetures/commit/8ffa88de4c12a5944fe7280715c4d4f7d6d5abd1)

Instincts (gene-translation.md): the engine translates C2 instinct genes into
neutral terms (conditions on drives, attention categories, heard words or
senses; an action; reward or punishment; a strength) and hands them to the
mind as they are expressed. The homeostat turns each into an innate
expectation: the action is expected to lower (reward) or raise (punishment)
the named drives, weighted as a number of remembered experiences that grows
with strength squared up to 12. Learning is slowed by that weight and speeds
up as real experience outweighs it, so strong instincts resist change and
weak ones give way. Instincts that name no object are a weak hunch about
objects in general (weight at most 1.5). Instincts on heard words, senses or
with no drive wait for language, senses and the approval feeling.

Seeking: the mind remembers where each kind of object was last seen. When
something it would act on is out of sight, walking west or east toward that
place is a candidate, valued at 60% of the relief it promises; arriving
without seeing it forgets the place.

Actions on an object (walk there, then act) now get up to 50 mind ticks to
finish instead of 12; most attempts to eat real food were being cut off on
the way.

Learning saved before body chemistry worked (minds saved without
"statever 2") is discarded on load, since every action then seemed to do
nothing.

Headless runs of the owner's world: hungry norns chose to eat about 40% of
the time (was about 8%) and go through real hunger cycles; most still
starve eventually (food is scarce and they try many inedible objects), so
death is not enabled yet.

#### C1/C2 worlds: restart metabolism of creatures saved before relays ran

[`f59ce04d`](https://github.com/DotairZee/zeetures/commit/f59ce04d0ed16797867600022de7ffac86c2a31c)

Saves made before the chemical relay lobe ran can hold creatures whose ATP
ran out because of it. With no ATP, C2 organs do nothing and the relay alone
cannot restart them. When a save without the new "chemical-relays 1" header
line loads, each living C2 creature with no ATP gets its genome's starting
ATP back, once. New saves carry the header, so a creature that later runs
out of ATP is genuinely starving.

#### C2 creatures: run the genome's chemical relay lobe alongside a mind

[`039e05b6`](https://github.com/DotairZee/zeetures/commit/039e05b634c37b91dddca08e39d14fc500ffe1b6)

C2 genomes route much of a creature's body regulation through a brain lobe
used purely as a relay: receptor genes write chemical levels into its cells
and emitter genes read the outputs. In the stock norn this is lobe 9:
lactate to Suffocation, water to Thirst, glucose to Hunger and Insulin,
starch, fat and protein to Fullness, ADP to phosphoglycerokinase (needed to
turn glucose into energy), adrenaline to Stress, and several hormones. A
creature mind replaces the brain, so the relay never ran: creatures could
not make energy, ATP ran down to 0, and since C2 organs do nothing without
ATP every creature's body chemistry froze (no digestion, fullness or
suffocation; "eat" was chosen about 60% of the time).

A lobe counts as a chemical relay only if receptor genes feed it, it has no
dendrites and its state rule just keeps the state; thinking lobes never
qualify (in the stock genomes only lobe 9 does). The update is our own,
reading the lobe gene: the state fades toward the rest state at the
leakage rate and the output is the state above the threshold.

Verified headless on the owner's world: with the relay, norns given ATP
keep it at 200-250 and release the enzyme; without it ATP collapses to 0
within 6 seconds. Norns now digest, feel full (fullness ~248), release
insulin and choose "eat" 11% of the time; under water oxygen falls to 2,
lactate rises to 255 and Suffocation to ~245.

OPENC2E_NO_RELAY disables it for comparison; with OPENC2E_WATCH_CREATURES
the lobes and relay cells are logged.

#### Fix abort when a vehicle drops a passenger that a rider was carrying

[`894fb49d`](https://github.com/DotairZee/zeetures/commit/894fb49d26eb055d461bb72f664011ef359b6c40)

Agent::beDropped puts an agent dropped by a carrier who is in a vehicle
back into that vehicle. When the vehicle itself was dropping the agent (for
example because something else picked it up), that put it straight back in,
and Vehicle::drop's assertion that the passenger had left aborted the game.
Only re-add the agent when it was not already in a vehicle. Seen in a long
headless run of the owner's world (C2 boat or lifts).

#### Developer aids: room dump shows temperature, light, radiation, pressure and nutrients

[`ed5aac76`](https://github.com/DotairZee/zeetures/commit/ed5aac76c5afc37c1558f7c62baa61f4480e5eea)

#### C2 creatures: no air with the head under water; bubbles rise

[`4d174efb`](https://github.com/DotairZee/zeetures/commit/4d174efb41bbd3f0ffdee3c4fb27e16095365564)

C2 creatures breathe through their genome: an emitter on the air-quality
locus (sensorimotor 9) makes Air, which genome reactions turn into oxygen.
openc2e fixed air quality at 255 everywhere, so creatures could sit under
water indefinitely. Air quality is now 0 while the head (near the top of the
body) is in a sea room (room type 2, "in the sea" in the C2 CAOS guide).
While under water, a bubble rises from the head every 12-40 ticks, using the
world's own bubble object (classifier 2 1 27, sprite "bubs"), whose script
animates it, plays its sound and removes it.

Verified headless: a norn moved into the swamp has air quality 0 and Air 0,
with bubble objects appearing. It does not yet suffocate: the genome raises
Suffocation through a brain lobe used as a chemical relay, which does not
run while our mind replaces the brain.

#### C2 creatures: treat a neighbouring floor far above the foot as a wall

[`0f5d9772`](https://github.com/DotairZee/zeetures/commit/0f5d9772fc9a56c8a34b0043ec9cf651f0bd7d80)

When a walking creature's foot leaves its room, snapDownFoot picks a
neighbouring room it may enter. If the level exit was closed to it (door
value below the creature's size), it could pick a room whose floor was far
above the foot, then "snap up", decide it was falling and drop. At the
garden's left edge in Eden the door down to the watermill ramp is valued 100
(creatures are 224), so creatures were moved into the open-air room above
the ramp and fell into the watermill pit, whose exits are also valued 100:
3-6 of 9 creatures placed in the garden ended there within 150 seconds. Now
a neighbouring floor more than 40 px above the foot is skipped like a wall,
so the creature is pushed back and gets its collision event: 0 of 9 in
three runs, and no buried or stranded creatures in three general runs.

The watcher (OPENC2E_WATCH_CREATURES) also logs each change of foot room.

#### C2 creatures: stop burying creatures below solid ground

[`d2f077ac`](https://github.com/DotairZee/zeetures/commit/d2f077ac8b4cbe5920483ebb46e26ada2a6c6400)

C2 rooms can have floor points, a ground line inside the room above the
room's real bottom. openc2e's "walking below floors" rule pinned any creature
whose top fell below that line to the room's bottom, and kept it there. That
is right where the ground is passable and there is real space beneath (a
floor value above 0, e.g. the lift shaft by the desert, whose bottom is the
lift's lower stop), but most rooms with floor points have a floor value of 0:
solid ground with only scenery beneath. There a short creature that touched
the line from below (for example walking off the pond bottom into the bank)
was buried 75-150 px deep and walked along under the earth. Now the rule only
applies where the floor value is above 0; elsewhere the creature stays on, or
returns to, the ground line.

Verified headless with the owner's world: before, three of four runs buried creatures
in solid-ground rooms 414-695 times each; after, none in about 85,000 ticks,
and a grendel saved buried in the doghouse room is lifted out on load.

Also adds developer aids: OPENC2E_WATCH_CREATURES logs creatures below the
ground of solid-ground rooms, large upward jumps, the below-floor rule and
positions every 200 ticks; OPENC2E_FAST runs the world as fast as possible;
the room dump now prints floor points.

#### C2 status bar: show temperatures below 0 °C correctly

[`76b5a5ed`](https://github.com/DotairZee/zeetures/commit/76b5a5edb43da253c86690e08def668991927b99)

The Celsius value was computed and printed as unsigned, so any room colder
than 0 °C wrapped around (for example -13 showed as 4294967283°).

#### Developer aids: dump an agent's parts

[`6e85704b`](https://github.com/DotairZee/zeetures/commit/6e85704b1b89a4d304862a8f48f41e68cc911db9)

OPENC2E_DUMP_AGENT="family,genus,species" prints each matching agent's
position and, for compound agents, every sprite part's offset, depth, sprite
file, base, pose and current frame. It prints on every OPENC2E_SNAPSHOTS
capture, and on every world tick with OPENC2E_DUMP_AGENT_EVERY_TICK set.

#### C2: run 5 CAOS instructions per tick instead of 1

[`801fbe7d`](https://github.com/DotairZee/zeetures/commit/801fbe7da05b5a6ff3a76d86a354b8d4d9299768)

openc2e has given C1 and C2 scripts one instruction per tick since 2006,
noted at the time as only known to be correct for C1. In C2 this splits
groups of commands that are meant to land together: the incubator's scripts
start its two door halves with "part 0 anim \[...\] part 1 anim \[...\]", and at
one instruction per tick the halves ran two frames apart, so while the door
moved the front half showed an open frame over a back half that had already
moved on, leaving a see-through gap. At 5 (c2e's value) the halves stay in
step (0 mismatched ticks over three open/close cycles, against 26 at 1).

C1 keeps 1. OPENC2E_C2_TIMESLICE=N overrides the C2 value for testing.

#### Developer aids: dump rooms and doors, show room outlines

[`5004bd7f`](https://github.com/DotairZee/zeetures/commit/5004bd7fed024c48fa85af8decde512e2e21c3f5)

OPENC2E_DUMP_ROOMS="x0,y0,x1,y1" prints the rooms overlapping that area with
their floors, C2 floor values, drop status and doors (with permeability).
OPENC2E_SHOW_ROOMS draws room outlines from the start, as View &gt; Show Map does,
so they appear in OPENC2E_SNAPSHOTS captures.

#### C2 creatures: stop pose changes pushing falling creatures through floors

[`2c1ae2fd`](https://github.com/DotairZee/zeetures/commit/2c1ae2fd1c73f4a2e4a45fb8add6059c6a638c6c)

recalculateSkeleton keeps the previous down foot in place when the pose
changes. While a C2 creature is falling (including landing and bouncing),
nothing snaps its feet back to a floor, so a new pose whose other foot reaches
lower pushed the body's bottom edge through the floor into the room below.
Physics then treated the creature as already in that room and let it keep
falling: creatures dropped from a height fell through the pond in Eden into
the cave beneath. While falling, a pose change may no longer move the lowest
foot down.

#### Developer aids: scripted screenshots and a fixed window size

[`5e5ddef7`](https://github.com/DotairZee/zeetures/commit/5e5ddef71130159c16cd98446d252cf607f83f16)

OPENC2E_WINDOW_SIZE=WxH sets the starting window size.
OPENC2E_SNAPSHOTS="x,y,file;..." moves the camera to each point in turn and
saves a BMP screenshot there; with OPENC2E_SNAPSHOTS_PAUSED set, the world is
paused first so captures differ only by camera position. Useful for checking
rendering headless (SDL_VIDEO_DRIVER=offscreen).

#### SDL backend: fix culling of sprites near the bottom of the view

[`a66d404f`](https://github.com/DotairZee/zeetures/commit/a66d404f15d44db04c328174c0be86716009f6ec)

renderTexture receives destinations in window coordinates (callers add
viewport_offset_top), but compared them against the viewport height, which
excludes that offset. Any sprite whose top edge fell within the last
viewport_offset_top pixels of the view was skipped, so objects and single
creature body parts popped in and out of view as the camera scrolled. Compare
against the viewport height plus the top offset instead.

#### C1/C2 worlds: auto-save on quit and when switching worlds

[`c231f8f1`](https://github.com/DotairZee/zeetures/commit/c231f8f113ba2fcdd389fbf829a34ddffc64164b)

Engine::shutdown saves the open world once it has finished loading, so
quitting (window close, File &gt; Exit) and switching worlds (which restarts
into the other world) keep the creatures, minds and eggs. --no-autosave turns
it off. Nothing is saved when quitting from the world chooser.

#### C1/C2 worlds: save and restore creatures, minds and eggs

[`e884d87d`](https://github.com/DotairZee/zeetures/commit/e884d87d2b7b089a699c097ed9006088b579027c)

File &gt; Save World writes the world folder's save/state.txt (readable text)
plus save/genomes/\*.gen: each living creature's genome, sex, variant,
position, life stage, age counters, all 256 chemical levels, organ
lifeforce, and its mind's learned state (CreatureMind::saveState/loadState,
implemented for the homeostat mind), plus every egg (classifier, sprite, pose,
attributes, timer, position and variables). The save is written to a
temporary folder and swapped in, so a failed save keeps the previous one.

Opening the world loads its world file as before, then the saved creatures
and eggs replace the ones it contains. Other objects still start as in the
world file. Save World As creates a new world from the same world file with
the current state, and continues there.

OPENC2E_TEST_SAVE_AFTER_TICKS=N saves after N ticks and quits (testing aid).

Tested headless: a world saved with 2 creatures and 21 eggs reopened with the
same creatures (stages kept), the same 21 eggs (no duplicates from the world
file) and minds continuing their learning (episodes 14-&gt;20, 37-&gt;44).

#### c1/c2: select a newly created creature if none is selected

[`abeb2c61`](https://github.com/DotairZee/zeetures/commit/abeb2c61d13671aaa4445f1f3fe79150b0cf85ec)

C1/C2 hatching scripts create creatures with NEW: CREA but never call BORN,
so the newborn selection in Creature::born never ran for hatched norns.

#### Show the world name in the window title

[`145d035d`](https://github.com/DotairZee/zeetures/commit/145d035d324e02427c14dfd6e3485410f03d36b4)

Adds Backend::setWindowTitle (SDL_SetWindowTitle in the SDL backend) and
titles the window "&lt;world&gt; - Creatures 2", as the original showed its world
file in the title bar.

#### C1/C2: multiple worlds with a start-up world chooser

[`9eb42967`](https://github.com/DotairZee/zeetures/commit/9eb42967a4b2b66ebe9f8f5d067d381a223ed885)

Worlds live in &lt;user data&gt;/Worlds/&lt;name&gt;/, each starting from a copy of one
of the game's world files (Eden.sfc, a fresh Albia, listed first). At start-up
a chooser lists the worlds and can create new ones; --world &lt;name&gt; opens one
directly and --no-world-chooser keeps the old behaviour (load Eden.sfc).

C1/C2 worlds load once at start-up, so File &gt; New World and Open World
restart openc2e into the chosen world, much as Creatures 2's separate World
Switcher did. Save World / Save World As are shown greyed out until saving is
implemented.

#### C2 Hatchery: load eggs from the Eggs folder and place them in the world

[`0598b758`](https://github.com/DotairZee/zeetures/commit/0598b7589f6efb9a0d9b9664b94265a837788608)

The C2 Hatchery window only drew ten eggs at random positions. It now reads
the .egg files from the game's Eggs folder (gender, egg picture, monikers and
one or two genomes) and shows each egg's own picture with a tooltip.

Clicking an egg places the world's own egg object (2 5 2) near the incubator
at the start of development, with a genome made by NEW: GENE from the egg's
genomes (written to the user Genetics folder) and the egg's gender, and moves
the camera there. The world's egg scripts develop it, and dropping it into
the incubator hatches it.

#### c1/c2: pan the world view with two-finger touchpad scrolling

[`62bae656`](https://github.com/DotairZee/zeetures/commit/62bae6567ba59ff8d6ad3b161c75421f590fec0d)

The SDL backend now passes SDL's precise wheel amounts through (wheel_x,
wheel_y) instead of discarding horizontal scrolling and reducing vertical
scrolling to wheel up/down buttons. In C1/C2, whose scripts have no use for
the wheel, the camera pans by those amounts in any direction (51 px per
unit), following the system's natural-scrolling setting. Manual panning
stops camera tracking. C3/DS wheel handling is unchanged.

#### creatures: homeostat mind (mind model v0)

[`1680761e`](https://github.com/DotairZee/zeetures/commit/1680761e3786ce3e884a3fb98fff8d082edc9acb)

An original creature mind built from published research on motivation and
learning (see the project's mind-model-v0.md for design and references).
Select with --mind homeostat.

\1. Drive and reward: overall drive D = (sum w_i d_i^n)^(1/m), n &gt; m, so severe
   needs dominate; an action's reward is the drop in D (homeostatic
   reinforcement learning).
\2. Expectations: per (object category, action), the expected change in each
   drive beyond a running drift baseline, learned with the delta rule. A new
   category starts from that action's experience elsewhere; instincts are the
   fallback before any experience.
\3. Choice: predicted relief given current needs, plus curiosity (raised by
   Boredom), minus distance; best object per action, then softmax over the
   top five actions, temperature falling with arousal; commitment bonus.
\4. Habituation: uneventful categories lose attention and recover over time.
\5. Affect: valence (average reward) and mood (drift-adjusted reward).
\6. Instincts: weak innate expectations (eat, sleep).
\7. Facial expression from affect: angry, sad, happy or neutral.

All tunables live in one Params struct so they can later come from editable
breed files and genes.

#### creatures: pluggable mind interface for C1/C2 creatures

[`9ba47a90`](https://github.com/DotairZee/zeetures/commit/9ba47a90836abb9805527dfb2449b3cf6282667b)

Adds CreatureMind (CreatureMind.h): each brain tick the engine passes a
MindPerception (drives, chemicals, visible agents per category with distance,
stimuli, available actions, action-finished flag) and applies the returned
MindOutput (attention category, decision = creature event 16 + n, sleep,
facial expression). Selected with --mind &lt;name&gt;; without it the original
oldBrain path is unchanged. Includes a 'null' mind that only logs.

Engine wiring, active only when a mind is selected:
- C1/C2 creatures get 40 perception categories (chosenagents was sized only
  for C3), so chooseAgents() works for them.
- Stimuli reach creatures: STM# WRIT/SHOU/SIGN/TACT, STIM WRIT/SHOU/SIGN/TACT
  and STIM FROM (C1/C2). Full stimuli add their chemicals; numbered stimuli
  add the chemicals from the creature's genome stimulus genes. SHOU uses an
  approximate hearing range (same metaroom, 500 px).
- The action vocabulary is read from the world's creature scripts (events
  16-47), flagging those that act on the attended object.
- OPENC2E_DUMP_SCRIPTS=&lt;file&gt; writes all world scripts at startup.

#### C2 interface: selection, scroll bars, eye view, Places, C2 menu bar

[`74f62006`](https://github.com/DotairZee/zeetures/commit/74f6200682be8a25c35f93ede2244fd829eda126)

Toward matching the original Creatures 2 interface, using graphics and layout
read from the game's own creatures2.exe at runtime:

- Clicking a C1/C2 creature selects it; a newborn is selected if nothing is.
- Toolbar: Next creature (cycles living creatures, centres the camera), Track
  (camera follows the selected creature), Creature's eye view, and the
  Places dropdown with Go.
- Scroll bars for the C2 world view (arrows, draggable thumb, track paging),
  with a right-side viewport offset added to the backend.
- Creature's Eye View window rendered off-screen (RenderTarget::getNativeTexture).
- World files hold six favourite places (name, x, y = camera top-left); load
  all six instead of only the first, and allow adding/removing them.
- C2 menu bar laid out after the game's own menu resource (File, View, Camera,
  Tools, Creatures, Help), with View &gt; Toolbars/Status Bar toggles and a live
  Creatures list; items not yet supported are greyed out.

#### C2 CAOS compatibility: digit-led labels, OBDT; developer aids

[`9205954b`](https://github.com/DotairZee/zeetures/commit/9205954be052932e836bde284ad81a2919d55f0e)

- caoslexer: accept words that start with a digit followed by a letter, as
  C1/C2 scripts use labels like "gsub 2bed" (Nesting Bluebirds COB). Tests added.
- OBDT (C2): distance from TARG's centre to the edge of its room in a
  direction, ignoring size, floor and door values, per the C2 CAOS Language
  Guide (used by the Halloween pack's witch).
- caos_to_bytecode: optional --dialect argument (was hard-coded to c3).
- OPENC2E_TRACE_EVENTS=1 logs each object event fired, its sender, and
  whether a script ran.

#### c1/c2: let creatures interact with objects; accept agents in DBGV

[`75f51ec6`](https://github.com/DotairZee/zeetures/commit/75f51ec6b16df5ae9bba06368723ceb5e49ace3b)

Agent::fireScript refused creature-triggered activate/deactivate/hit/pickup/eat
events unless the object's cr_can_\* flags were set, but those flags only come
from BHVR, which exists in C3/DS only. In C1/C2 no creature could ever push,
pull, hit, pick up or eat anything. Only enforce them for engine.version &gt;= 3.

DBGV is declared (anything): C2's own eat script does "dbgv \_it_", which
aborted the script with a parameter type mismatch. Agents print as their
classifier.

#### SDLMixerBackend: fix deadlock between channel lock and SDL stream lock

[`37307df1`](https://github.com/DotairZee/zeetures/commit/37307df1c5d90148ce82c4b08c946419a6fef2d0)

The main thread holds s_channels_mutex while calling MIX_\* functions, which
take SDL's audio stream lock. The track-stopped callback runs on SDL's audio
thread while it holds the stream lock, and took s_channels_mutex to erase the
channel: a lock-order inversion that froze the game at random (all threads in
futex waits; main thread in audio_channel_set_pan/get_state -&gt; LockTrack).

The callback now only queues the stopped key under a leaf mutex; the main
thread erases stopped channels at the start of each channel access.

Tested headless with SDL_AUDIO_DRIVER=dummy: the old build deadlocked after
about 25 minutes, the fixed build ran 40 minutes without stalling. Possibly
related to the race mentioned in issue #208.

#### case_insensitive_filesystem: fix out-of-bounds read in path_startswith

[`d2f2c865`](https://github.com/DotairZee/zeetures/commit/d2f2c865a5416b389b3a972581411005a22e08ff)

The three-iterator std::equal(prefix.begin(), end, path.begin()) reads past
path.end() when a cached path is shorter than the prefix. Since the move to
std::filesystem (dfcd46f2) this crashes openc2e on startup in World::init()
(first image lookup) with C2 data. Compare component by component and stop
when path runs out.
