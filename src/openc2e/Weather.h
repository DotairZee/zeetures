#pragma once

// Zeetures' own weather (owner, 2026-11-27; reports/Weather proposal.md in
// the project folder). A weather spell is chosen for the whole world every
// so often, by the season's odds: clear, light showers, showers,
// thunderstorms (in summer sometimes with hail), torrential rain, fog, a
// cold snap with snow, a heatwave, or a wind storm. It draws all its own
// pictures (owner: "none of the stock weather sprites"): clouds across the
// sky on top of everything, thinner and paler the milder the weather; rain,
// hail and snow falling through open air to the first real surface; fog
// banks; blowing dust and leaves; and lightning, from the storm clouds, with
// Zeetures' strike effects on creatures. On by default; Preferences > World
// turns it off (saved with the world), which brings back Creatures 2's own
// clouds, rain and storms.
//
// Where weather can be is worked out once per background: from the rooms
// (their floors, ceilings, doors and water) and the background picture (a
// bridge's planks along its floor line), with a few outlines traced from
// Eden's background (the volcano's crater, the seas' surfaces, walkways the
// picture test misses), used only on a world with Eden's map.

#include <iosfwd>
#include <sstream>
#include <string>

class MetaRoom;
class RenderTarget;

namespace Weather {

enum Spell {
	CLEAR,
	LIGHT_SHOWERS,
	SHOWERS,
	THUNDERSTORM,
	TORRENTIAL,
	FOG,
	SNOW,
	HEATWAVE,
	WINDSTORM,
	SPELL_COUNT
};

// the new weather in use in this world (Preferences > World; on unless
// turned off; OPENC2E_WEATHER=0 for the old weather)
bool on();
void setOn(bool on);

// the spell now, its name ("Thunderstorm"), and whether this one brings
// hail (a summer thunderstorm, now and then)
Spell spell();
const char* spellName(int spell);
bool hail();
// how long the spell has left, in world ticks
unsigned int ticksLeft();
// the player's choice from Preferences: this spell now, for a while
void setSpell(Spell s);

void reset(); // at each world load, before the save is read
void tick(); // each world tick

// draws the weather over the world just drawn (after every object, before
// the hand, speech bubbles and the light of the day), the view's top left
// corner at world (adjustx, adjusty)
void draw(RenderTarget* surface, MetaRoom* m, int adjustx, int adjusty);

void save(std::ostream& out);
bool readLine(const std::string& key, std::istringstream& line);

} // namespace Weather
