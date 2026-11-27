#pragma once

#include <iosfwd>
#include <sstream>
#include <string>
#include <vector>

class Agent;
class genomeFile;
class oldCreature;

// Grendels and ettins, each with a role of their own (owner, 2026-11-26:
// "let's do it!", from reports/Grendel and ettin roles.md; our own design,
// built on what their C2 genes already hold). Most of the roles are scent
// rules (pheromones.txt, "Grendels and ettins"): each kind gives off its own
// scent, the others answer to it, and the "bring" urge carries things up a
// scent (eggs to the grendels' lair or to the incubator, trinkets to an
// ettin's hoard, food to the hungry). This module holds the rest:
// - which grendels are Boney (the pack's genome, with working reproductive
//   genes) and which bulbous (C2's own, whose reproductive genes are off);
// - fertility for kinds whose genes switch it off (C2's grendel and ettin):
//   fertile, and longing for a mate, only while an adolescent-to-old of the
//   other sex of the same kind lives somewhere in the world; carrying for a
//   while once with young, then laying (the world's own egg-laying script);
// - bulbous grendels' night: wide awake in the dark, drowsy by day;
// - wariness: a creature that falls ill soon after being near a grendel
//   learns to link the two, and keeps away from grendels' scent ("wary");
// - each ettin's hoard spot, a quiet place it found, which gives off the
//   "ettin-hoard" scent its trinkets are carried to.
// OPENC2E_NO_KINDS switches all of it off (and the scent rules that use it
// find nothing to answer); OPENC2E_WATCH_KINDS logs.
namespace Kinds {

bool off();
bool watching();

// genes: a reproductive receptor that is switched on (Boney grendels, norns;
// not C2's grendel or ettin)
bool reproGenes(const genomeFile* g);
bool isBoney(oldCreature* c); // a grendel with working reproductive genes
bool isBulbous(oldCreature* c); // C2's own grendel (its reproduction off)
bool sterileGenes(oldCreature* c); // a grendel or ettin whose genes switch reproduction off
// an adolescent-to-old of that agent genus (1 norn, 2 grendel, 3 ettin) and
// sex is alive (worked out every 50 ticks)
bool mateAlive(int agentGenus, bool female);

// wariness of grendels (0-1): learned by falling ill after being near one
bool wary(oldCreature* c);
float wariness(oldCreature* c);
void noteCaught(oldCreature* c); // Disease: it has just caught something

// how many grendels and ettins the game's own eggs keep alive at a time: the
// grendels' egg pile and the egg machine hatch another while fewer live
// (owner, 2026-11-26: "only one ... at a time" by default, set in
// Preferences > World; a world setting, 0-20; CAOS GRNL and ETNL)
int gameKeeps(int agentGenus); // 2 grendels, 3 ettins
void setGameKeeps(int agentGenus, int n);

// steps a pregnancy lasts for kinds whose genes cannot carry one
const int GESTATION_STEPS = 1500; // biochemistry steps (5 a second): 5 minutes

void tick(); // each world tick
void reset(); // at each world load, before the save is read
void save(std::ostream& out);
bool readLine(const std::string& key, std::istringstream& line);

// for the Health Kit and Help: what its kind is, and what that means for it
// ("Bulbous grendel" and its traits), and its fertility now
std::string kindName(oldCreature* c);
std::vector<std::string> traits(oldCreature* c);

} // namespace Kinds
