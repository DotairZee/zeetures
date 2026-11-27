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
// - C1 grendels (owner, 2026-11-27: "let's go with them!", from reports/C1
//   grendel genome plan.md): RudeDog's C1-in-C2 Grendel pictures (eem.foo
//   68a3) on a genome Zeetures builds on the player's machine from their own
//   gren.gen (makeC1Genome). Gluttons and thieves, deaf to the hand, touchy
//   loners, fearless, clean (no germs carried; they can fall ill), about by
//   day and asleep at night, few instincts, quick to learn, and mostly
//   barren: about one adult in three lays a lone egg of its own once.
// OPENC2E_NO_KINDS switches all of it off (and the scent rules that use it
// find nothing to answer); OPENC2E_WATCH_KINDS logs.
namespace Kinds {

// off: OPENC2E_NO_KINDS, or the world's "Grendel and ettin personalities"
// unticked (Preferences > World; on by default, saved with the world). Off,
// grendels and ettins are stock: none of this module's rules or the scent
// rules' personalities section, C2's grendels and ettins sterile as their
// genes say, and the C1 grendels' genome converted from Creatures 1's own
// (makeC1StockGenome) in place of the grouchy one; living grouchy grendels
// keep their genes but none of their ways
bool off();
bool personalities(); // the world's setting
void setPersonalities(bool on);
bool watching();

// genes: a reproductive receptor that is switched on (Boney grendels, norns;
// not C2's grendel or ettin)
bool reproGenes(const genomeFile* g);
bool isBoney(oldCreature* c); // a grendel with working reproductive genes
bool isBulbous(oldCreature* c); // C2's own grendel (its reproduction off), not a C1 grendel
// a grendel or ettin whose genes switch reproduction off (not C1 grendels:
// they have their lone eggs instead)
bool sterileGenes(oldCreature* c);

// C1 grendels: a genome carrying the marker gene makeC1Genome adds (an
// initial-concentration gene, never expressed and not mutable, of chemical
// C1_MARK_CHEMICAL at C1_MARK_AMOUNT); inherited, so the kind is
// whichever its genes say, whatever it looks like
const int C1_MARK_CHEMICAL = 191, C1_MARK_AMOUNT = 193;
bool isC1Genome(const genomeFile* g);
bool isC1(oldCreature* c); // a grendel with the C1 marker
// a grendel that carries germs for life without falling ill (Disease.h): any
// grendel but a C1 one
bool carriesGerms(Agent* a);
// the hand's words go unheard (CreatureSpeech): C1 grendels
bool deafToHand(oldCreature* c);
// the C1 grendel genome from the game's grendel genome (loaded fresh), on
// the pictures in grendel breed slot `slot` (0 a, 1 b, ...): the recipe in
// Kinds.cpp; written to the user data folder's Genetics as C1_GENOME.gen
const char* const C1_GENOME = "c1gr";
bool makeC1Genome(genomeFile& g, int slot);
// with the personalities off, C1 grendels' genome is Creatures 1's own
// converted (g: the game's gren.gen; c1: C1's Gren.GEN; each game's chemical
// names, normalised, to renumber by): Kinds.cpp. writeC1Genome writes the
// one the world's setting asks for.
bool makeC1StockGenome(genomeFile& g, genomeFile& c1, const std::vector<std::string>& c1names, const std::vector<std::string>& c2names, int slot);
bool writeC1Genome(int slot, std::string& why);
// the name players see for the kind (owner, 2026-11-27: "I want grouchy
// grendel!"); in the code they stay "C1 grendels". The pack was first
// installed under C1_OLD_NAME (0.45.0-0.45.1), still recognised
const char* const C1_KIND_NAME = "Grouchy grendel";
const char* const C1_OLD_NAME = "C1 grendel";

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
