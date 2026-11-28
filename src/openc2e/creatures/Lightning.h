#pragma once

#include <iosfwd>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

class Agent;
class oldCreature;

// Lightning (owner, 2026-11-26: "jolt plus all options!", then twenty-two
// more, powers, and Frankenstein's creatures). C2's storm cloud (2 14 2)
// throws bolts of five segments (2 14 4) and meant a creature touching one
// to feel it (+200 Pain), but tested contact with a variable it never set;
// a script patch (script-patches.txt) makes the jolt work. This watches each
// new bolt segment for what it touches. With "Lightning has strange effects"
// ticked (on unless unticked) a strike may also do something stranger (one of
// thirty-odd effects, minutes to days), now and then give a power (longer,
// saved), and a creature struck just after dying may rise again as
// Frankenstein's creature: immortal and infertile, until it finds a bride
// (two of them mate: both mortal and fertile again) or is struck again
// (then it dies for good). A struck egg's unborn gains mutations, and a
// struck plant fruits. OPENC2E_STRANGE_LIGHTNING=0/1 (developer aid)
// whatever the setting; OPENC2E_WATCH_LIGHTNING prints every strike.
namespace Lightning {

bool strange();
void setStrange(bool on);

// a bolt of Zeetures' own weather (Weather.h) struck this rectangle of
// the world: what it touches is struck, as by C2's storm bolts
void strikeArea(float x0, float y0, float x1, float y1);

void reset(); // at each world load, before the save is read
void tick(); // each world tick

// what other parts of the game ask
enum Look { NONE, SCORCHED, GLOWING, PALLID };
Look lookOf(Agent* creature); // its body's colour (SkeletalCreature::skeletonInit)
float sizeFactor(Agent* creature); // drawn bigger or smaller (1: as usual)
float paceFactor(Agent* creature); // walks faster or slower (1: as usual)
float pitchFactor(Agent* creature); // its voice higher or lower (1: as usual)
bool backwards(Agent* creature); // its words come out reversed
bool parrotNow(Agent* creature); // says back what it hears (and it may now)
float learnFactor(oldCreature* listener); // learns words this much faster
float teachFactor(Agent* speaker); // others learn from it this much faster
bool electrophile(Agent* creature); // drawn to machines (HomeostatMind)
bool waterBreather(Agent* creature); // c2Creature::breathesWater
bool foodFinder(Agent* creature); // smells the nearest food (CreatureAI)
bool plantEater(Agent* creature); // eats only plants (Frankenstein's creature)
bool immortal(Agent* creature); // can't die (Frankenstein's creature)
// Frankenstein's creature feels that another of its kind is alive: which way
// the nearest is (false if none, or it is beside it)
bool kindredDirection(Agent* creature, float& dx, float& dy);
bool infertile(const std::string& moniker); // no young (Frankenstein's creature)
// two Frankenstein's creatures mating (Breeding): the spell broken for both
bool brideFound(const std::string& a, const std::string& b);
void slapped(Agent* creature); // Frankenstein's creature, spurned: angry
// the couples on the memorial stone (Bubble: its tablet): their names ("A
// and B") and the world tick they died
std::vector<std::pair<std::string, unsigned int>> couplesOnStone();
// the names of what it has now (Health Kit: nothing at all until struck)
std::vector<std::string> statusOf(Agent* creature);
// a sentence or two on what one of those names does (Health Kit: shown when
// the pointer rests on the name; owner, 2026-11-26), or "" if unknown
std::string describe(const std::string& name);
// a photo to take now (a creature just risen as Frankenstein's creature: the
// Owner's Kit takes it, as on its first day), or none
std::shared_ptr<Agent> takePhotoRequest();
// extra mutations owed to its next child (Breeding: taken at conception)
int takeGermline(const std::string& moniker);

void save(std::ostream& out);
bool readLine(const std::string& key, std::istringstream& line);

} // namespace Lightning
