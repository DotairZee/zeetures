#pragma once

// The light through the day (C2; openc2e addition, owner 2026-11-11). A
// filter over the whole world view, multiplying everything by a colour for
// C2's time of day (rosy at dawn, warm at dusk, dark blue at night), with
// caves and the underground getting half of it. Where the outdoors is comes
// from the painted sky, traced once per background at load. Nothing is
// shipped or saved: it is all made from the player's own background.

class MetaRoom;
class RenderTarget;

namespace SkyTint {

// Draws the light filter for the current time of day over the world drawn
// with its background at (destx, desty) (and again one width to the right
// in a wrapping metaroom).
void draw(RenderTarget* surface, MetaRoom* m, int destx, int desty);

} // namespace SkyTint
