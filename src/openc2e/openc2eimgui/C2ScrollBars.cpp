#include "C2ScrollBars.h"
#include "ImGuiUtils.h"

#include "C2StatusBar.h"
#include "C2ToolBar.h"
#include "Camera.h"
#include "Engine.h"
#include "MetaRoom.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace Openc2eImgui {

static bool s_show_scrollbars = true;
static constexpr float BAR = 16.0f; // scroll bar thickness, in pixels
static constexpr int ARROW_STEP = 64; // pixels per arrow click

bool AreC2ScrollBarsShown() {
	return s_show_scrollbars;
}

void SetC2ScrollBarsShown(bool shown) {
	s_show_scrollbars = shown;
}

int GetC2ScrollBarThickness() {
	return s_show_scrollbars ? (int)BAR : 0;
}

// One scroll bar. 'pos' is the camera's left/top edge in world coordinates,
// 'view' its width/height, and [lo, lo + range) the metaroom's extent.
// Returns the new camera position, or -1 if unchanged.
//
// 'wraps': the world goes round (Albia does, east to west): the track is
// the whole circle, the thumb the view's share of it, and the thumb runs off
// one end and on at the other as the view crosses the join (owner,
// 2026-11-22). Before, the thumb sat still for the last view's width of the
// world, going east it then took that long to reach the end, and going west
// past the join it leapt straight to the far end.
static int ScrollBar(const char* id, bool vertical, ImVec2 origin, float length, int pos, int view, int lo, int range, bool wraps = false) {
	int result = -1;
	ImGui::PushID(id);
	ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);

	// first arrow
	ImGui::SetCursorScreenPos(origin);
	if (ImGui::ArrowButton("##less", vertical ? ImGuiDir_Up : ImGuiDir_Left))
		result = pos - ARROW_STEP;

	// track
	ImVec2 trackpos = vertical ? ImVec2(origin.x, origin.y + BAR) : ImVec2(origin.x + BAR, origin.y);
	float tracklen = std::max(length - 2 * BAR, 1.0f);
	ImVec2 tracksize = vertical ? ImVec2(BAR, tracklen) : ImVec2(tracklen, BAR);
	ImGui::SetCursorScreenPos(trackpos);
	ImGui::InvisibleButton("##track", tracksize);
	bool trackheld = ImGui::IsItemActive();
	bool trackclicked = ImGui::IsItemClicked();

	wraps = wraps && range > view;
	float frac_view = range > 0 ? std::min(1.0f, (float)view / range) : 1.0f;
	float thumblen = std::max(tracklen * frac_view, 12.0f);
	float scrollable = wraps ? (float)range : std::max(range - view, 1);
	float frac_pos = (float)(pos - lo) / scrollable;
	if (wraps)
		frac_pos -= std::floor(frac_pos);
	else
		frac_pos = std::clamp(frac_pos, 0.0f, 1.0f);
	float thumbstart = (wraps ? tracklen : tracklen - thumblen) * frac_pos;
	// (wrapping: what runs off the end, drawn again from the start)
	float overrun = wraps ? std::max(0.0f, thumbstart + thumblen - tracklen) : 0.0f;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImU32 trackcol = ImGui::GetColorU32(ImGuiCol_ScrollbarBg);
	ImU32 thumbcol = ImGui::GetColorU32(trackheld ? ImGuiCol_ScrollbarGrabActive : ImGuiCol_ScrollbarGrab);
	dl->AddRectFilled(trackpos, ImVec2(trackpos.x + tracksize.x, trackpos.y + tracksize.y), trackcol);
	ImVec2 t0 = vertical ? ImVec2(trackpos.x + 1, trackpos.y + thumbstart) : ImVec2(trackpos.x + thumbstart, trackpos.y + 1);
	ImVec2 t1 = vertical ? ImVec2(trackpos.x + BAR - 1, t0.y + thumblen) : ImVec2(t0.x + thumblen, trackpos.y + BAR - 1);
	if (overrun > 0) {
		if (vertical)
			t1.y = trackpos.y + tracklen;
		else
			t1.x = trackpos.x + tracklen;
	}
	dl->AddRectFilled(t0, t1, thumbcol);
	if (overrun > 0) {
		ImVec2 w0 = vertical ? ImVec2(trackpos.x + 1, trackpos.y) : ImVec2(trackpos.x, trackpos.y + 1);
		ImVec2 w1 = vertical ? ImVec2(trackpos.x + BAR - 1, trackpos.y + overrun) : ImVec2(trackpos.x + overrun, trackpos.y + BAR - 1);
		dl->AddRectFilled(w0, w1, thumbcol);
	}

	// interaction: drag the thumb, or click the track to page
	static float s_grab_offset = 0;
	static const char* s_dragging = nullptr;
	float mouse = vertical ? ImGui::GetIO().MousePos.y - trackpos.y : ImGui::GetIO().MousePos.x - trackpos.x;
	if (trackclicked) {
		if (mouse >= thumbstart && mouse <= thumbstart + thumblen) {
			s_dragging = id;
			s_grab_offset = mouse - thumbstart;
		} else if (mouse < overrun) { // (the part run on at the start)
			s_dragging = id;
			s_grab_offset = mouse + tracklen - thumbstart;
		} else {
			s_dragging = nullptr;
			result = pos + (mouse < thumbstart ? -view : view); // page
		}
	}
	if (trackheld && s_dragging == id && wraps) {
		float f = (mouse - s_grab_offset) / tracklen;
		f -= std::floor(f);
		result = lo + (int)(f * scrollable);
	} else if (trackheld && s_dragging == id && tracklen > thumblen) {
		float f = std::clamp((mouse - s_grab_offset) / (tracklen - thumblen), 0.0f, 1.0f);
		result = lo + (int)(f * scrollable);
	}
	if (!trackheld && s_dragging == id)
		s_dragging = nullptr;

	// second arrow
	ImGui::SetCursorScreenPos(vertical ? ImVec2(origin.x, origin.y + length - BAR) : ImVec2(origin.x + length - BAR, origin.y));
	if (ImGui::ArrowButton("##more", vertical ? ImGuiDir_Down : ImGuiDir_Right))
		result = pos + ARROW_STEP;

	ImGui::PopItemFlag();
	ImGui::PopID();
	return result;
}

void DrawC2ScrollBars() {
	if (!s_show_scrollbars || !engine.camera)
		return;
	MetaRoom* m = engine.camera->getMetaRoom();
	if (!m)
		return;

	ImGuiIO& io = ImGui::GetIO();
	float top = (float)GetC2ToolBarHeight();
	float bottom = io.DisplaySize.y - (float)GetC2StatusBarHeight(); // horizontal bar sits just above the status bar
	float right = io.DisplaySize.x;

	int camx = (int)engine.camera->getX(), camy = (int)engine.camera->getY();
	int camw = (int)engine.camera->getWidth(), camh = (int)engine.camera->getHeight();
	int newx = -1, newy = -1;

	const ImVec2 o = ImGuiUtils::GameWindowPos();
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));

	// vertical, along the right edge of the world view
	ImGui::SetNextWindowPos(o + ImVec2(right - BAR, top));
	ImGui::SetNextWindowSize(ImVec2(BAR, bottom - BAR - top));
	ImGuiUtils::KeepInGameWindow();
	if (ImGui::Begin("##c2vscroll", nullptr, flags)) {
		newy = ScrollBar("v", true, o + ImVec2(right - BAR, top), bottom - BAR - top, camy, camh, (int)m->y(), (int)m->height());
	}
	ImGui::End();

	// horizontal, along the bottom of the world view (corner square left empty)
	ImGui::SetNextWindowPos(o + ImVec2(0, bottom - BAR));
	ImGui::SetNextWindowSize(ImVec2(right, BAR));
	ImGuiUtils::KeepInGameWindow();
	if (ImGui::Begin("##c2hscroll", nullptr, flags)) {
		newx = ScrollBar("h", false, o + ImVec2(0, bottom - BAR), right - BAR, camx, camw, (int)m->x(), (int)m->width(), m->wraparound());
	}
	ImGui::End();

	ImGui::PopStyleVar(3);

	if (newx != -1 || newy != -1) {
		// scrolling by hand stops following a creature
		engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
		engine.camera->moveTo(newx != -1 ? newx : camx, newy != -1 ? newy : camy);
	}
}

} // namespace Openc2eImgui
