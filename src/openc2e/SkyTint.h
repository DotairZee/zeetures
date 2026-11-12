#pragma once

// The sky through the day (C2; openc2e addition, owner 2026-11-11). The
// painted sky in the world's background is found once per background (the
// sky-coloured region reached from the picture's top edge, with the clouds
// in it), and three tinted versions of it are made: dawn, dusk and night
// (with stars). Each frame the one for C2's time of day is drawn over the
// background, before any agent, so only the sky changes; morning and
// afternoon show it as painted. A change of time of day fades over about 30
// seconds. Nothing is shipped or saved: it is all made from the player's
// own background at load.

class MetaRoom;
class RenderTarget;

namespace SkyTint {

// Draws the sky layer for the current time of day over the background that
// drawWorld has just drawn at (destx, desty) (and again one width to the
// right in a wrapping metaroom).
void draw(RenderTarget* surface, MetaRoom* m, int destx, int desty);

} // namespace SkyTint
