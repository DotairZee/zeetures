#include "C2StatusBar.h"
#include "C2KitsInternal.h"

#include "C2Kits.h"
#include "AgentHelpers.h"
#include "Camera.h"
#include "Engine.h"
#include "ImGuiUtils.h"
#include "MetaRoom.h"
#include "Room.h"
#include "World.h"
#include "creatures/CreatureAgent.h"
#include "creatures/oldCreature.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>
#include <optional>

using namespace ImGuiUtils;

namespace Openc2eImgui {

static std::optional<int> getTemperatureNearSelectedCreature() {
	if (world.selectedcreature) {
		auto room = roomContainingAgent(world.selectedcreature);
		if (room) {
			return room->temp;
		}
	}
	return {};
}

static std::optional<int> getTemperatureAtCenterOfCamera() {
	auto room = engine.camera->getMetaRoom()->roomAt(engine.camera->getXCentre(), engine.camera->getYCentre());
	if (room) {
		return room->temp;
	}
	return {};
}

static int s_last_c2statusbar_height = 0;

static bool s_show_c2statusbar = true;

bool IsC2StatusBarShown() {
	return s_show_c2statusbar;
}

void SetC2StatusBarShown(bool shown) {
	s_show_c2statusbar = shown;
}

int GetC2StatusBarHeight() {
	return s_show_c2statusbar ? s_last_c2statusbar_height : 0;
}

void DrawC2StatusBar() {
	if (!s_show_c2statusbar)
		return;
	ImGuiIO& io = ImGui::GetIO();

	ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - s_last_c2statusbar_height));
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 0));
	if (ImGui::Begin("Statusbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		static std::array<Texture, 4> s_season_icons_texs;
		if (!s_season_icons_texs[0]) {
			s_season_icons_texs[0] = GetTextureFromExeFile(0x98);
			s_season_icons_texs[1] = GetTextureFromExeFile(0x99);
			s_season_icons_texs[2] = GetTextureFromExeFile(0x9a);
			s_season_icons_texs[3] = GetTextureFromExeFile(0x9b);
		}

		ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Horizontal;

		// The left end (owner, 2026-11-11): three pictures, each named on
		// hover (the season, the time of day, how warm it is with the
		// temperature), then the day and year as text.
		if (world.season < s_season_icons_texs.size()) {
			ImGuiUtils::Image(s_season_icons_texs[world.season]);
			// (it looks like weather, but it is C2's picture of the season)
			static const char* const s_season_names[4] = {"Spring", "Summer", "Autumn", "Winter"};
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", s_season_names[world.season]);
		}

		static std::array<Texture, 5> s_timeofday_icons_texs;
		if (!s_timeofday_icons_texs[0]) {
			s_timeofday_icons_texs[0] = GetTextureFromExeFile(0xa3);
			s_timeofday_icons_texs[1] = GetTextureFromExeFile(0xc4);
			s_timeofday_icons_texs[2] = GetTextureFromExeFile(0xc2);
			s_timeofday_icons_texs[3] = GetTextureFromExeFile(0xc3);
			s_timeofday_icons_texs[4] = GetTextureFromExeFile(0xc5);
		}
		if (world.timeofday < s_timeofday_icons_texs.size()) {
			ImGuiUtils::Image(s_timeofday_icons_texs[world.timeofday]);
			// (C2 has no names built in; the owner's words)
			static const char* const s_timeofday_names[5] = {"Dawn", "Morning", "Midday", "Dusk", "Night"};
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", s_timeofday_names[world.timeofday]);
		}

		static std::array<Texture, 5> s_temperature_icons_texs;
		if (!s_temperature_icons_texs[0]) {
			s_temperature_icons_texs[0] = GetTextureFromExeFile(0xa4);
			s_temperature_icons_texs[1] = GetTextureFromExeFile(0xa5);
			s_temperature_icons_texs[2] = GetTextureFromExeFile(0xa6);
			s_temperature_icons_texs[3] = GetTextureFromExeFile(0xa7);
			s_temperature_icons_texs[4] = GetTextureFromExeFile(0xa8);
		}

		// prefer the room the selected creature is in
		std::optional<int> temperature = getTemperatureNearSelectedCreature();
		if (!temperature) {
			// then try the room at the center of the camera
			temperature = getTemperatureAtCenterOfCamera();
			if (!temperature) {
				// TODO: c2 seems to try closest room as a last resort?
			}
		}
		if (temperature) {
			int temp = temperature.value();
			unsigned int tempiconid = 4;
			if (temp < 255) {
				tempiconid = temp / 64; // boundaries at 64, 128, 192 and 255
			}
			if (tempiconid < s_temperature_icons_texs.size()) {
				ImGuiUtils::Image(s_temperature_icons_texs[tempiconid]);
				// (0 = -15 C = 5 F, 48 = 0 C = 32 F, 255 = 65 C = 149 F)
				int temp_celsius = ((temp * 80) / 255) - 15; // signed: rooms below 0 C are common
				const char* feel = temp_celsius < 0 ? "Freezing" : temp_celsius < 10 ? "Cold" : temp_celsius < 18 ? "Cool" : temp_celsius < 28 ? "Mild" : temp_celsius < 36 ? "Warm" : "Hot";
				std::string number = Openc2eImgui::KitFahrenheit() ? fmt::format("{}°F", (int)std::lround(temp_celsius * 9 / 5.0 + 32)) : fmt::format("{}°C", temp_celsius);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s: %s\n(click for %s)", feel, number.c_str(), Openc2eImgui::KitFahrenheit() ? "Celsius" : "Fahrenheit");
				if (ImGui::IsItemClicked())
					Openc2eImgui::KitToggleFahrenheit();
			}
		}

		// the day of the season (openc2e addition, owner 2026-11-11): C2's
		// seasons are four days long
		ImGui::Text("Day %u", world.dayofseason % 4 + 1);
		ImGui::Text("Year: %03i", world.year);

		static std::array<Texture, 6> s_healthicons_texs;
		if (!s_healthicons_texs[0]) {
			s_healthicons_texs[0] = GetTextureFromExeFileWithTransparentTopLeft(0xe8); // disabled (gray)
			s_healthicons_texs[1] = GetTextureFromExeFileWithTransparentTopLeft(0xc7); // 0/4
			s_healthicons_texs[2] = GetTextureFromExeFileWithTransparentTopLeft(0xac); // 1/4
			s_healthicons_texs[3] = GetTextureFromExeFileWithTransparentTopLeft(0xad); // 2/4
			s_healthicons_texs[4] = GetTextureFromExeFileWithTransparentTopLeft(0xab); // 3/4
			s_healthicons_texs[5] = GetTextureFromExeFileWithTransparentTopLeft(0xaa); // 4/4
		}

		static std::array<Texture, 5> s_hearticons_texs;
		if (!s_hearticons_texs[0]) {
			s_hearticons_texs[0] = GetTextureFromExeFileWithTransparentTopLeft(0xe9); // disabled (gray)
			s_hearticons_texs[1] = GetTextureFromExeFileWithTransparentTopLeft(0xae); // large
			s_hearticons_texs[2] = GetTextureFromExeFileWithTransparentTopLeft(0xaf); // medium
			s_hearticons_texs[3] = GetTextureFromExeFileWithTransparentTopLeft(0xb0); // small
			s_hearticons_texs[4] = GetTextureFromExeFileWithTransparentTopLeft(0xe7); // dead (blue)
		}

		// minimized kits, as chips that bring them back
		Openc2eImgui::DrawC2KitChips();

		// births, deaths and pregnancies (newest on the right), then health:
		// the notices get the room the left side and the health leave
		static int s_last_statusbar_rightside_width = 0;
		static float s_last_health_width = 0;
		const float left_end = ImGui::GetCursorPosX();
		ImGui::SetCursorPosX(std::max(left_end, ImGui::GetWindowContentRegionMax().x - s_last_statusbar_rightside_width));
		const int start_x = ImGui::GetCursorPosX();

		Openc2eImgui::DrawLifeNotices(ImGui::GetWindowContentRegionMax().x - left_end - s_last_health_width - ImGui::GetStyle().ItemSpacing.x * 2);

		const float health_x = ImGui::GetCursorPosX();
		ImGui::Text("Health:");
		// The selected creature's health in C2's pictures (owner, 2026-11-11:
		// make them real and say what they show). The bar, skull to smile,
		// is its weakest organ's strength (what failing kills it), flashing
		// while it is unconscious or an organ is failing; the heart beats,
		// faster with adrenaline, slow while unconscious, blue when dead.
		const char* noone = "Health: choose a creature to see how it is";
		CreatureAgent* ca = world.selectedcreature ? dynamic_cast<CreatureAgent*>(world.selectedcreature.get()) : nullptr;
		oldCreature* c = ca ? dynamic_cast<oldCreature*>(ca->getCreature()) : nullptr;
		if (!c) {
			ImGuiUtils::Image(s_healthicons_texs[0]);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", noone);
			ImGuiUtils::Image(s_hearticons_texs[0]);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", noone);
		} else {
			std::string name = Openc2eImgui::KitCreatureName(world.selectedcreature.get());
			if (c->isAlive()) {
				float worst = 1.0f, sum = 0;
				int n = 0;
				if (auto* c2 = dynamic_cast<c2Creature*>(c))
					for (auto& o : c2->getOrgans()) {
						float start = o->getInitialLifeforce();
						if (start <= 0)
							continue;
						float h = std::clamp(o->getShortTermLifeforce() / start, 0.0f, 1.0f);
						worst = std::min(worst, h);
						sum += h;
						n++;
					}
				float avg = n ? sum / n : 1.0f;
				unsigned int bars = (unsigned int)std::clamp((int)std::ceil(worst * 4) - 1, 0, 3);
				bool alarm = c->isUnconscious() || worst < 0.25f;
				bool flashOff = alarm && std::fmod(ImGui::GetTime(), 0.8) < 0.4;
				ImGuiUtils::Image(s_healthicons_texs[flashOff ? 1 : bars + 2]);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s's health: its weakest organ is at %.0f%% (all organs %.0f%% on average)%s", name.c_str(), worst * 100, avg * 100,
						c->isUnconscious() ? "\nUnconscious" : worst < 0.25f ? "\nAn organ is failing" : "");
				float adrenaline = c->getChemical(172) / 255.0f;
				float perSecond = c->isUnconscious() ? 0.5f : 1.0f + 1.6f * adrenaline;
				double phase = std::fmod(ImGui::GetTime() * perSecond, 1.0);
				ImGuiUtils::Image(s_hearticons_texs[phase < 0.15 ? 1 : phase < 0.3 ? 2 : 3]); // large, medium, small
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s's heartbeat: %s, about %.0f a minute", name.c_str(),
						c->isUnconscious() ? "faint" : adrenaline > 0.6f ? "racing" : adrenaline > 0.25f ? "quickened" : "calm", perSecond * 60);
			} else {
				ImGuiUtils::Image(s_healthicons_texs[1]);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s has died", name.c_str());
				ImGuiUtils::Image(s_hearticons_texs[4]); // blue heart
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s has died", name.c_str());
			}
		}

		s_last_statusbar_rightside_width = ImGui::GetCursorPosX() - start_x;
		s_last_health_width = ImGui::GetCursorPosX() - health_x;

		s_last_c2statusbar_height = ImGui::GetWindowSize().y;
		ImGui::End();
	}
}

} // namespace Openc2eImgui