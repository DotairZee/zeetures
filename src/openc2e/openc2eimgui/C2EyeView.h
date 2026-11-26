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
// the eye's lids (Preferences): 0 blinks, droops and shuts in sleep; 1 only
// droops and shuts in sleep; 2 always open
int EyeViewBlinking();
void SetEyeViewBlinking(int mode);
// what else the view shows (Preferences; each off unless ticked): the
// creature itself, its own speech and thought bubbles, and its name in the
// window's title
enum EyeViewShow { EYE_SHOW_SELF, EYE_SHOW_BUBBLES, EYE_SHOW_NAME };
bool EyeViewShows(EyeViewShow what);
void SetEyeViewShows(EyeViewShow what, bool shown);
// the picture's width (120-960 pixels; it keeps its shape)
void SetEyeViewWidth(int width);

} // namespace Openc2eImgui
