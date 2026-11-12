#pragma once

// Birth portraits (C2; openc2e addition, owner 2026-11-11). A moment after a
// creature hatches, a picture is made of it alone, in one of a few cheerful
// poses (side views with its face turned to you, or facing you), happy, on
// the bare background of the place it was born, with every object and
// other creature left out. It is the first picture in its photo album.
// Nothing is shipped: it is drawn from the player's own sprites and
// background, and kept with the world like any photo.

#include "AgentRef.h"

#include <string>

namespace BirthPortrait {

// a creature has just hatched: make its portrait in a few ticks, once its
// hatching script has put it in place
void schedule(AgentRef creature);

// called every world tick
void tick();

} // namespace BirthPortrait
