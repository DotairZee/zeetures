#pragma once

#include <string>

class Agent;

namespace Openc2eImgui {

// What a creature is doing, in plain words ("eating some food", "Asleep"),
// as Creature's View captions it; also used by the Health Kit.
std::string DescribeCreatureActivity(Agent* creature);

void DrawEyeView();
bool IsEyeViewOpen();
void SetEyeViewOpen(bool open);

} // namespace Openc2eImgui
