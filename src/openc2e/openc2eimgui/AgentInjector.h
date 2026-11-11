#pragma once

namespace Openc2eImgui {

bool IsAgentInjectorEnabled();
void SetAgentInjectorOpen(bool);
void DrawAgentInjector();
// C2: a minimized Agent Injector's chip on the status bar (DrawC2KitChips calls it)
void DrawAgentInjectorChip();

} // namespace Openc2eImgui