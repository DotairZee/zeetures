#include "AnimatablePart.h"

#include "Agent.h"

#include <fmt/core.h>

void AnimatablePart::updateAnimation() {
	if (animation.empty())
		return;

	if (frameno == animation.size())
		return;
	if (frameno > animation.size()) {
		// (was an assert, which closed the game: owner's Home, 2026-11-15,
		// on loading; a frame number from before the animation was changed)
		fmt::print("* Animation frame {} past the end of a {}-frame animation on {} {} {} part {}; started again\n",
			(int)frameno, (int)animation.size(), parent ? (int)parent->family : -1, parent ? (int)parent->genus : -1,
			parent ? parent->species : -1, (int)id);
		frameno = 0;
	}

	if (animation[frameno] == 255) {
		if (frameno == (animation.size() - 1)) {
			frameno = 0;
		} else {
			// if we're not at the end, we ought to have at least one more item
			assert(frameno + 1 < animation.size());

			frameno = animation[frameno + 1];

			if (frameno >= animation.size()) {
				// this is an internal error because it should have been validated at pose-setting time
				std::string err = fmt::format("internal animation error: tried looping back to frame {} but that is beyond animation size {}",
					(int)frameno, (int)animation.size());
				parent->unhandledException(err, false);
				animation.clear();
				return;
			}
		}
	}

	setPose(animation[frameno]);
	frameno++;
}