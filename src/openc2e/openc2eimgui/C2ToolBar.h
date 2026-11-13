#pragma once

#include "Camera.h"

namespace Openc2eImgui {

int GetC2ToolBarHeight();
void DrawC2Toolbar();
// how the interface moves the view to a place or creature: a glide with
// Camera > Smooth Scrolling on, else a jump
panstyle UiPanStyle();

} // namespace Openc2eImgui