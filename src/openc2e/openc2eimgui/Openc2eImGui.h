#pragma once

namespace Openc2eImGui {

void Init();
void Update();
int GetViewportOffsetTop();
int GetViewportOffsetBottom();
int GetViewportOffsetRight();
// the toolbar, status bar and scroll bars hidden (the trailer director,
// OPENC2E_DIRECTOR: "bars off"); kits and windows still drawn
extern bool g_hideBars;

} // namespace Openc2eImGui