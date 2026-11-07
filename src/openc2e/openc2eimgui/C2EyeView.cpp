#include "C2EyeView.h"

#include "Agent.h"
#include "Camera.h"
#include "Engine.h"
#include "ImGuiUtils.h"
#include "World.h"
#include "common/backend/Backend.h"

#include <imgui.h>
#include <memory>

namespace Openc2eImgui {

// A small window showing the world around the selected creature, following
// it, like Creatures 2's "creature's eye view".

static constexpr int VIEW_W = 320;
static constexpr int VIEW_H = 240;

class EyeViewCamera : public Camera {
  public:
	unsigned int getWidth() const override { return VIEW_W; }
	unsigned int getHeight() const override { return VIEW_H; }
};

static bool s_eye_view_open = false;
static std::unique_ptr<EyeViewCamera> s_camera;
static std::shared_ptr<RenderTarget> s_target;

bool IsEyeViewOpen() {
	return s_eye_view_open;
}

void SetEyeViewOpen(bool open) {
	s_eye_view_open = open;
}

void DrawEyeView() {
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

	// centre on the selected creature and draw the world into our texture
	Agent* c = world.selectedcreature.get();
	int cx = (int)(c->x + c->getWidth() / 2.0f) - VIEW_W / 2;
	int cy = (int)(c->y + c->getHeight() / 2.0f) - VIEW_H / 2;
	s_camera->moveToGlobal(cx, cy, jump);
	world.drawWorld(s_camera.get(), s_target.get());

	ImGui::SetNextWindowSize(ImVec2(VIEW_W + 16, VIEW_H + 40), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Creature's Eye View", &s_eye_view_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {
		void* tex = s_target->getNativeTexture();
		if (tex)
			ImGui::Image((ImTextureID)(uintptr_t)tex, ImVec2(VIEW_W, VIEW_H));
	}
	ImGui::End();
}

} // namespace Openc2eImgui
