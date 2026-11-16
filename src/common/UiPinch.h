#pragma once

// A touchpad pinch over the interface (owner, 2026-11-16: zoom the Owner's
// Kit's relationship map by pinching). When the pointer is over an ImGui
// window the SDL backend keeps pinch events from the world (which would
// zoom the view of Albia) and adds their scale here instead; a widget that
// zooms takes it with TakeUiPinch().

#include <cmath>

namespace UiPinch {

// the pinches since last taken, as the log of their combined scale
inline float& pending() {
	static float logScale = 0;
	return logScale;
}

inline void Add(float scale) {
	if (scale > 0)
		pending() += std::log(scale);
}

// the combined scale since last taken (1: none), and clears it
inline float Take() {
	float s = std::exp(pending());
	pending() = 0;
	return s;
}

} // namespace UiPinch
