#pragma once

namespace Openc2eImgui {

int GetC2StatusBarHeight();
bool IsC2StatusBarShown();
void SetC2StatusBarShown(bool shown);
// its parts, each shown or hidden (Preferences > Status Bar)
enum { SB_SEASON, SB_TIME, SB_TEMPERATURE, SB_DATE, SB_KITS, SB_NOTICES, SB_HEALTH, STATUSBAR_PARTS };
bool StatusBarPartShown(int part);
void SetStatusBarPartShown(int part, bool shown);
const char* StatusBarPartName(int part);
void DrawC2StatusBar();

} // namespace Openc2eImgui