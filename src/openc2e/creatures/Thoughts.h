// Thought bubbles (openc2e addition, owner 2026-11-13): what a creature is
// thinking, shown for a few seconds over its head when it starts something
// new, as pictures in C2's own thought bubble (syst.s16, the player's own
// game): our symbols for needs and directions (data/thoughts/icons.png),
// pictures of things from the game as it runs, and creatures' faces.
// View > Thought Bubbles: off, the selected creature, or every creature.
#pragma once

#include <string>
#include <vector>

class Agent;
class RenderTarget;

namespace Thoughts {

// a new thought from a creature's mind (MindOutput::thought): each item a
// symbol's name, "cat:N" (a thing of category N) or "who:MONIKER"; a
// thought with the same key as the one it had lately is not shown again
void think(Agent* creature, const std::vector<std::string>& items, const std::string& key);
// draw the bubbles of the moment over the world (after the agents)
// (skip: a creature whose thoughts are not drawn, the one whose eyes the
// Creature's View looks through)
void draw(RenderTarget* surface, int adjustx, int adjusty, int wrapWidth, Agent* skip = nullptr);

enum Mode { OFF = 0,
	SELECTED = 1,
	EVERY = 2 };
Mode mode();
void setMode(Mode m);

} // namespace Thoughts
