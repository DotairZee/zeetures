#include "C2StatusBar.h"

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
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Day %u of the season's 4", world.dayofseason % 4 + 1);
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
		// TODO: norn health/drive/heartbeat
		if (!world.selectedcreature) {
			ImGuiUtils::Image(s_healthicons_texs[0]);
			ImGuiUtils::Image(s_hearticons_texs[0]);
		} else {
			if (CreatureAgent* ca = dynamic_cast<CreatureAgent*>(world.selectedcreature.get())) {
				oldCreature* c = dynamic_cast<oldCreature*>(ca->getCreature());
				assert(c); // this is c2, after all..

				if (c->isAlive()) {
					// this life force indicator shows the level of Glycogen
					// TODO: this does not update instantly w/Glycogen in real
					// c2, so perhaps it's actually another (related) data point?
					unsigned int x = c->getChemical(72) / 64;
					ImGuiUtils::Image(s_healthicons_texs[x + 2]); // 2-5 (1-4 bars)
					// TODO: flash indicator when norn is dying

					// TODO: heartbeat animation
					ImGuiUtils::Image(s_hearticons_texs[1]); // 1-3 = beating heart, large to small
				} else {
					// dead
					ImGuiUtils::Image(s_healthicons_texs[1]);
					ImGuiUtils::Image(s_hearticons_texs[4]); // blue heart
				}
				// TODO: drive
			}
		}

		s_last_statusbar_rightside_width = ImGui::GetCursorPosX() - start_x;
		s_last_health_width = ImGui::GetCursorPosX() - health_x;

		s_last_c2statusbar_height = ImGui::GetWindowSize().y;
		ImGui::End();
	}
}

} // namespace Openc2eImgui