#include "C2EyeView.h"

#include "Agent.h"
#include "AgentHelpers.h"
#include "Camera.h"
#include "Catalogue.h"
#include "Engine.h"
#include "Map.h"
#include "MetaRoom.h"
#include "ImGuiUtils.h"
#include "Vehicle.h"
#include "World.h"
#include "common/backend/Backend.h"
#include "creatures/Creature.h"
#include "creatures/CreatureAgent.h"

#include <cmath>
#include <fmt/format.h>
#include <imgui.h>
#include <memory>

namespace Openc2eImgui {

// A small window showing what the selected creature is paying attention to.
// When the creature and the object it attends to both fit, the view frames
// the two of them; when they're far apart, it shows the object. With nothing
// in particular in mind, it shows the creature. The view glides from one
// target to the next, the object is marked with corner brackets, and a
// caption says what it is and what the creature is doing.

static constexpr int VIEW_W = 320;
static constexpr int VIEW_H = 240;
static constexpr float FRAME_MARGIN = 48; // keep both this far inside the view's edges

class EyeViewCamera : public Camera {
  public:
	unsigned int getWidth() const override { return VIEW_W; }
	unsigned int getHeight() const override { return VIEW_H; }
};

static bool s_eye_view_open = false;
static std::unique_ptr<EyeViewCamera> s_camera;
static std::shared_ptr<RenderTarget> s_target;
static AgentRef s_followed;
static float s_cx = 0, s_cy = 0; // the view's centre, in world pixels

bool IsEyeViewOpen() {
	return s_eye_view_open;
}

void SetEyeViewOpen(bool open) {
	s_eye_view_open = open;
}

// What the creature is doing, in plain words (C1/C2 decisions; the world's
// creature scripts carry them out).
static const char* actionWords(int decision) {
	switch (decision) {
		case 0: return "resting";
		case 1: return "pushing";
		case 2: return "pulling";
		case 3: return "stopping";
		case 4: return "going to";
		case 5: return "backing away from";
		case 6: return "picking up";
		case 7: return "dropping";
		case 8: return "asking about";
		case 10: return "walking west past";
		case 11: return "walking east past";
		case 12: return "eating";
		case 13: return "hitting";
		case 25: return "sleeping by";
		case 26: return "walking west past";
		case 27: return "walking east past";
		default: return "looking at";
	}
}

static std::string describe(Agent* a) {
	if (!a)
		return "nothing in particular";
	if (dynamic_cast<CreatureAgent*>(a))
		return "another creature";
	std::string name = catalogue.getAgentName(a->family, a->genus, a->species);
	if (!name.empty())
		return name;
	if (dynamic_cast<Vehicle*>(a))
		return "a lift or vehicle";
	float edible = edibleScent(a);
	if (edible < 0)
		return "something poisonous";
	if (edible > 0)
		return "some food";
	return fmt::format("an object ({} {} {})", (int)a->family, (int)a->genus, a->species);
}

std::string DescribeCreatureActivity(Agent* self) {
	auto* ca = dynamic_cast<CreatureAgent*>(self);
	Creature* cr = ca ? ca->getCreature() : nullptr;
	if (!cr)
		return {};
	if (!cr->isAlive())
		return "Dead";
	if (cr->isAsleep())
		return "Asleep";
	Agent* focus = cr->getAttentionFocus().get();
	if (focus == self || (focus && focus->isDying()))
		focus = nullptr;
	if (!focus)
		return "Not paying attention to anything";
	std::string s = fmt::format("{} {}", actionWords(cr->getDecisionId()), describe(focus));
	if (!s.empty())
		s[0] = (char)toupper((unsigned char)s[0]);
	return s;
}

// the shorter way from x0 to x1 in the creature's metaroom, if it wraps
static MetaRoom* s_room = nullptr;
static float wrapDelta(float x0, float x1) {
	float d = x1 - x0;
	MetaRoom* m = s_room;
	if (m && m->wraparound()) {
		float w = (float)m->width();
		if (d > w / 2)
			d -= w;
		else if (d < -w / 2)
			d += w;
	}
	return d;
}

void DrawEyeView() {
	// developer aid: OPENC2E_OPEN_EYE_VIEW=1 selects the first norn and
	// opens this view (for headless snapshots)
	static bool test_opened = false;
	if (!test_opened && getenv("OPENC2E_OPEN_EYE_VIEW")) {
		for (auto& a : world.agents) {
			if (a && dynamic_cast<CreatureAgent*>(a.get()) && a->genus == 1) {
				world.selectedcreature = a;
				s_eye_view_open = test_opened = true;
				break;
			}
		}
	}
	if (!s_eye_view_open)
		return;
	if (!world.selectedcreature) {
		s_eye_view_open = false;
		return;
	}
	if (!s_camera)
		s_camera = std::make_unique<EyeViewCamera>();
	if (!s_target)
		s_target = get_backend()->newRenderTarget(VIEW_W, VIEW_H);

	Agent* c = world.selectedcreature.get();
	Creature* cr = nullptr;
	if (auto* ca = dynamic_cast<CreatureAgent*>(c))
		cr = ca->getCreature();
	Agent* focus = cr ? cr->getAttentionFocus().get() : nullptr;
	if (focus == c || (focus && focus->isDying()))
		focus = nullptr;
	int decision = cr ? cr->getDecisionId() : -1;

	// where to look
	float crx = c->x + c->getWidth() / 2.0f, cry = c->y + c->getHeight() / 2.0f;
	s_room = world.map->metaRoomAt((unsigned int)std::max(0.0f, crx), (unsigned int)std::max(0.0f, cry));
	float tx = crx, ty = cry;
	if (focus) {
		float fdx = wrapDelta(crx, focus->x + focus->getWidth() / 2.0f);
		float fdy = (focus->y + focus->getHeight() / 2.0f) - cry;
		if (std::fabs(fdx) < VIEW_W - 2 * FRAME_MARGIN && std::fabs(fdy) < VIEW_H - 2 * FRAME_MARGIN) {
			tx = crx + fdx / 2;
			ty = cry + fdy / 2;
		} else {
			tx = crx + fdx;
			ty = cry + fdy;
		}
	}
	// glide there (jump when a new creature is chosen or the target is far)
	float gdx = wrapDelta(s_cx, tx), gdy = ty - s_cy;
	if (s_followed != world.selectedcreature || std::fabs(gdx) > 2000 || std::fabs(gdy) > 2000) {
		s_followed = world.selectedcreature;
		s_cx = tx;
		s_cy = ty;
	} else {
		s_cx += gdx * 0.18f;
		s_cy += gdy * 0.18f;
	}
	s_camera->moveToGlobal((int)std::lround(s_cx) - VIEW_W / 2, (int)std::lround(s_cy) - VIEW_H / 2, jump);
	world.drawWorld(s_camera.get(), s_target.get());

	ImGui::SetNextWindowSize(ImVec2(VIEW_W + 16, VIEW_H + 62), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Creature's View", &s_eye_view_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {
		void* tex = s_target->getNativeTexture();
		if (tex) {
			ImGui::Image((ImTextureID)(uintptr_t)tex, ImVec2(VIEW_W, VIEW_H));
			// corner brackets round the object, if it's in view
			if (focus) {
				ImVec2 o = ImGui::GetItemRectMin();
				float x0 = o.x + wrapDelta((float)s_camera->getX(), focus->x) - 3;
				float y0 = o.y + (focus->y - (float)s_camera->getY()) - 3;
				float x1 = x0 + focus->getWidth() + 6, y1 = y0 + focus->getHeight() + 6;
				float k = std::min(10.0f, std::min(x1 - x0, y1 - y0) / 3);
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->PushClipRect(o, ImVec2(o.x + VIEW_W, o.y + VIEW_H), true);
				ImU32 col = IM_COL32(255, 230, 120, 230), shade = IM_COL32(0, 0, 0, 140);
				for (int pass = 0; pass < 2; pass++) {
					float d = pass == 0 ? 1.0f : 0.0f;
					ImU32 cc = pass == 0 ? shade : col;
					auto corner = [&](float x, float y, float sx, float sy) {
						dl->AddLine(ImVec2(x + d, y + d), ImVec2(x + sx * k + d, y + d), cc, 2.0f);
						dl->AddLine(ImVec2(x + d, y + d), ImVec2(x + d, y + sy * k + d), cc, 2.0f);
					};
					corner(x0, y0, 1, 1);
					corner(x1, y0, -1, 1);
					corner(x0, y1, 1, -1);
					corner(x1, y1, -1, -1);
				}
				dl->PopClipRect();
			}
		}
		if (cr && cr->isAsleep())
			ImGui::TextUnformatted("Asleep");
		else if (focus)
			ImGui::TextWrapped("%s %s", actionWords(decision), describe(focus).c_str());
		else
			ImGui::TextUnformatted("Not paying attention to anything");
	}
	ImGui::End();
}

} // namespace Openc2eImgui
