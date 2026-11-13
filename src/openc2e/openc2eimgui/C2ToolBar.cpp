#include "C2ToolBar.h"

#include "C2KitsInternal.h"
#include "HelpWindow.h"
#include "creatures/CreatureSpeech.h"

#include <SDL3/SDL_misc.h>

#include "AgentInjector.h"
#include "C2Kits.h"
#include "C2EyeView.h"
#include "WorldChooser.h"
#include "common/backend/Backend.h"
#include "WorldSave.h"
#include "MusicManager.h"
#include "SoundManager.h"
#include "C2StatusBar.h"
#include "C2ScrollBars.h"
#include "creatures/CreatureAgent.h"
#include "creatures/Creature.h"
#include "Camera.h"
#include "Engine.h"
#include "Hatchery.h"
#include "ImportCreature.h"
#include "ImGuiUtils.h"
#include "MainMenu.h"
#include "PointerAgent.h"
#include "SpritePart.h"
#include "World.h"
#include "Scriptorium.h"
#include "caosVM.h"
#include "fileformats/ImageUtils.h"
#include "fileformats/peFile.h"

#include <fmt/core.h>
#include <imgui.h>
#include <vector>
#include <imgui_internal.h>

using namespace ImGuiUtils;

namespace Openc2eImgui {

static int s_last_c2toolbar_height = 0;

// View > Toolbars, as in Creatures 2
static bool s_show_standard = true;
static bool s_show_pointer = true;
static bool s_show_applets = true;
static bool s_show_favorites = true;

int GetC2ToolBarHeight() {
	return s_last_c2toolbar_height;
}


// Moves the camera to a favorite place (stops following a creature). Places
// store the camera's top-left scroll position, like the world file's own
// starting scroll position.
static void GoToFavoritePlace(int i) {
	if (i < 0 || i >= (int)world.favoriteplaces.size())
		return;
	auto& f = world.favoriteplaces[i];
	engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
	engine.camera->moveToGlobal(f.x, f.y, jump);
}

// Selects the next living creature after the current selection and centers
// the camera on it.
static void SelectNextCreature() {
	std::vector<std::shared_ptr<Agent>> creatures;
	for (auto& a : world.agents) {
		if (!a)
			continue;
		CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a.get());
		if (ca && ca->getCreature() && ca->getCreature()->isAlive())
			creatures.push_back(a);
	}
	if (creatures.empty())
		return;
	size_t next = 0;
	std::shared_ptr<Agent> current = world.selectedcreature.lock();
	for (size_t i = 0; i < creatures.size(); i++) {
		if (creatures[i] == current)
			next = (i + 1) % creatures.size();
	}
	std::shared_ptr<Agent> c = creatures[next];
	world.selectCreature(c);
	int cx = (int)(c->x + c->getWidth() / 2.0f) - (int)engine.camera->getWidth() / 2;
	int cy = (int)(c->y + c->getHeight() / 2.0f) - (int)engine.camera->getHeight() / 2;
	engine.camera->moveToGlobal(cx, cy, jump);
}

static const char* GenusName(int genus) {
	switch (genus) {
		case 1: return "Norn";
		case 2: return "Grendel";
		case 3: return "Ettin";
		default: return "Creature";
	}
}

// This fork's version, shown in Help > About openC2e (raise it for releases)
#define OPENC2E_FORK_VERSION "0.1.0"
#define OPENC2E_WEB "https://openc2e.github.io/"
static bool s_open_about = false;

// Add Favourite Place (the Camera menu or the + by Places) opens the naming
// popup on the menu bar's next draw
static bool s_open_add_place = false;

// a world has six favourite place slots
static bool HasFreePlace() {
	if (world.favoriteplaces.size() < 6)
		return true;
	for (auto& f : world.favoriteplaces)
		if (f.name.empty())
			return true;
	return false;
}

// The Creatures 2 menu bar (layout read from the game's own menu resource).
// Items that openc2e can't do yet are shown grayed out, as C2 itself does.
static void DrawC2MenuBar() {
	static bool s_open_new_world = false;
	static bool s_open_save_as = false;
	static bool s_open_save_message = false;
	static std::string s_save_message;

	if (ImGui::BeginMenu("File")) {
		// worlds (C2 used a separate World Switcher; openc2e restarts into the chosen world)
		if (ImGui::MenuItem("New World..."))
			s_open_new_world = true;
		auto worlds = ListWorlds();
		if (ImGui::BeginMenu("Open World", worlds.size() > 1 || (worlds.size() == 1 && worlds[0].name != engine.currentworld))) {
			for (auto& w : worlds) {
				bool current = w.name == engine.currentworld;
				if (ImGui::MenuItem(w.name.c_str(), nullptr, current, !current))
					RelaunchIntoWorld(w.name);
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Save World", nullptr, false, !engine.currentworlddir.empty())) {
			std::string err;
			s_save_message = WorldSave::SaveWorldState(engine.currentworlddir, err) ? "World saved." : err;
			s_open_save_message = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Saves your creatures (bodies and minds) and eggs. Other objects still start as in the world's original file.");
		if (ImGui::MenuItem("Save World As...", nullptr, false, !engine.currentworlddir.empty()))
			s_open_save_as = true;
		// openc2e addition (owner, 2026-11-11): saves every few minutes
		if (ImGui::BeginMenu("Autosave")) {
			int cur = autosaveMinutes();
			for (int m : {1, 5, 10, 15, 30})
				if (ImGui::MenuItem(fmt::format("Every {} minute{}", m, m == 1 ? "" : "s").c_str(), nullptr, cur == m))
					setAutosaveMinutes(m);
			if (ImGui::MenuItem("Only when quitting", nullptr, cur == 0))
				setAutosaveMinutes(0);
			ImGui::EndMenu();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("How often the world saves itself (it always saves when you quit).");
		ImGui::Separator();
		ImGui::MenuItem("Export Current Creature...", nullptr, false, false);
		ImportCreatureMenuItem();
		ImGui::Separator();
		if (ImGui::BeginMenu("Sound")) {
			bool music = !engine.musicmanager->isMuted();
			if (ImGui::MenuItem("Ambient Music", nullptr, music)) {
				engine.musicmanager->setMuted(music);
				engine.musicmanager->setMIDIMuted(music);
			}
			bool world_sounds = !soundmanager.isMuted();
			if (ImGui::MenuItem("World Sounds", nullptr, world_sounds))
				soundmanager.setMuted(world_sounds);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Machines, objects, creatures' noises: every sound but music and speech");
			bool bibble = !soundmanager.areVoicesMutedByPlayer();
			if (ImGui::MenuItem("Bibble", nullptr, bibble))
				soundmanager.setVoicesMutedByPlayer(bibble);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Creatures' speech");
			ImGui::EndMenu();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Exit"))
			world.quitting = true;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("View")) {
		if (ImGui::MenuItem("Creature's View", nullptr, IsEyeViewOpen(), (bool)world.selectedcreature))
			SetEyeViewOpen(!IsEyeViewOpen());
		ImGui::Separator();
		// not in C2's menu; openc2e addition (owner, 2026-11-09)
		if (ImGui::MenuItem("Zoom In", "Ctrl+Wheel", false, engine.zoom < 3.0f))
			engine.zoomStep(1);
		if (ImGui::MenuItem("Zoom Out", "Ctrl+Wheel", false, engine.zoom > 0.5f))
			engine.zoomStep(-1);
		if (ImGui::MenuItem(fmt::format("Actual Size ({:.0f}%)###actualsize", engine.zoom * 100).c_str(), "Ctrl+0", false, engine.zoom != 1.0f))
			engine.setZoom(1.0f);
		ImGui::Separator();
		if (ImGui::BeginMenu("Toolbars")) {
			ImGui::MenuItem("Standard", nullptr, &s_show_standard);
			ImGui::MenuItem("Pointer", nullptr, &s_show_pointer);
			ImGui::MenuItem("Applets", nullptr, &s_show_applets);
			ImGui::MenuItem("Favourites", nullptr, &s_show_favorites);
			ImGui::Separator();
			ImGui::MenuItem("Text Labels", nullptr, false, false);
			ImGui::EndMenu();
		}
		// not in C2's menu; openc2e addition (owner, 2026-11-11)
		bool bubbles = speechBubblesShown();
		if (ImGui::MenuItem("Speech Bubbles", nullptr, &bubbles))
			setSpeechBubblesShown(bubbles);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Creatures' words in bubbles (their voices go on; Bibble under File > Sound)");
		bool statusbar = IsC2StatusBarShown();
		if (ImGui::MenuItem("Status Bar", nullptr, &statusbar))
			SetC2StatusBarShown(statusbar);
		ImGui::MenuItem("ToolTips for Objects", nullptr, false, false);
		bool scrollbars = AreC2ScrollBarsShown();
		if (ImGui::MenuItem("Scroll Bars", nullptr, &scrollbars)) // not in C2's menu; openc2e addition
			SetC2ScrollBarsShown(scrollbars);
		ImGui::Separator();
		ImGui::MenuItem("Full Screen", "F12", false, false);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Camera")) {
		bool tracking = world.selectedcreature && engine.camera->trackedAgent() == world.selectedcreature;
		if (ImGui::MenuItem("Track Norn", nullptr, tracking, (bool)world.selectedcreature)) {
			if (tracking)
				engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
			else
				engine.camera->trackAgent(world.selectedcreature, 0, 0, flexible, none);
		}
		ImGui::MenuItem("Norn Indicator", nullptr, false, false);
		ImGui::MenuItem("Smooth Scrolling", nullptr, false, false);
		ImGui::Separator();
		bool hasfree = false, hasnamed = false;
		for (auto& f : world.favoriteplaces) {
			if (f.name.empty())
				hasfree = true;
			else
				hasnamed = true;
		}
		if (world.favoriteplaces.size() < 6)
			hasfree = true;
		if (ImGui::MenuItem("Add Favourite Place...", nullptr, false, hasfree))
			s_open_add_place = true;
		if (ImGui::BeginMenu("Remove Favourite Place...", hasnamed)) {
			for (auto& f : world.favoriteplaces) {
				if (!f.name.empty() && ImGui::MenuItem(f.name.c_str())) {
					f.name.clear();
					f.x = f.y = 0;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Tools")) {
		// in the toolbar's order, with its groups (owner, 2026-11-12)
		if (ImGui::MenuItem("Hatchery", nullptr, false, IsHatcheryEnabled()))
			SetHatcheryOpen(true);
		if (ImGui::MenuItem("Owner's Kit", nullptr, false, IsC2KitAvailable("owners")))
			OpenC2Kit("owners");
		if (ImGui::MenuItem("Health Kit", nullptr, false, IsC2KitAvailable("health")))
			OpenC2Kit("health");
		if (ImGui::MenuItem("Graveyard Kit", nullptr, false, IsC2KitAvailable("graveyard")))
			OpenC2Kit("graveyard");
		ImGui::Separator();
		if (ImGui::MenuItem("Breeder's Kit", nullptr, false, IsC2KitAvailable("breeders")))
			OpenC2Kit("breeders");
		if (ImGui::MenuItem("Science Kit", nullptr, false, IsC2KitAvailable("science")))
			OpenC2Kit("science");
		if (ImGui::MenuItem("Mind Kit", nullptr, false, IsC2KitAvailable("mind")))
			OpenC2Kit("mind");
		if (ImGui::MenuItem("Observation Kit", nullptr, false, IsC2KitAvailable("observation")))
			OpenC2Kit("observation");
		ImGui::Separator();
		if (ImGui::MenuItem("Agent Injector", nullptr, false, IsAgentInjectorEnabled())) // openc2e's
			SetAgentInjectorOpen(true);
		if (ImGui::MenuItem("Chronicles Kit", nullptr, false, IsC2KitAvailable("chronicles")))
			OpenC2Kit("chronicles");
		if (ImGui::MenuItem("Ecology Kit", nullptr, false, IsC2KitAvailable("ecology")))
			OpenC2Kit("ecology");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Creatures")) {
		int n = 0;
		std::shared_ptr<Agent> current = world.selectedcreature.lock();
		for (auto& a : world.agents) {
			if (!a)
				continue;
			CreatureAgent* ca = dynamic_cast<CreatureAgent*>(a.get());
			if (!ca || !ca->getCreature() || !ca->getCreature()->isAlive())
				continue;
			n++;
			std::string given = KitGivenName(a.get());
			std::string label = given.empty() ? fmt::format("{} {} ({})", GenusName(a->genus), n, ca->getCreature()->isFemale() ? "female" : "male")
											   : fmt::format("{} ({} {})##{}", given, ca->getCreature()->isFemale() ? "female" : "male", GenusName(a->genus), n);
			if (ImGui::MenuItem(label.c_str(), nullptr, a == current)) {
				world.selectCreature(a);
				int cx = (int)(a->x + a->getWidth() / 2.0f) - (int)engine.camera->getWidth() / 2;
				int cy = (int)(a->y + a->getHeight() / 2.0f) - (int)engine.camera->getHeight() / 2;
				engine.camera->moveToGlobal(cx, cy, jump);
			}
		}
		if (n == 0)
			ImGui::MenuItem("None Available", nullptr, false, false);
		// (the life stage chooser is in the Owner's Kit, Registration;
		// owner, 2026-11-12)
		ImGui::EndMenu();
	}
	// Help: what the toolbar's Help and Web buttons do, in words, and About
	// (C2's items we cannot offer are left out; owner, 2026-11-12)
	if (ImGui::BeginMenu("Help")) {
		if (ImGui::MenuItem("Help Topics"))
			SetHelpOpen(true);
		if (ImGui::MenuItem("openc2e on the Web"))
			SDL_OpenURL(OPENC2E_WEB);
		ImGui::Separator();
		if (ImGui::MenuItem("About openC2e..."))
			s_open_about = true;
		ImGui::EndMenu();
	}

	// About openC2e (developer aid: OPENC2E_OPEN_KITS="about" opens it)
	static bool s_test_about = getenv("OPENC2E_OPEN_KITS") && strstr(getenv("OPENC2E_OPEN_KITS"), "about");
	if (s_test_about && world.worldtickcount > 5) {
		s_test_about = false;
		s_open_about = true;
	}
	if (s_open_about) {
		ImGui::OpenPopup("About openC2e");
		s_open_about = false;
	}
	if (ImGui::BeginPopupModal("About openC2e", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::SetWindowFontScale(1.6f);
		ImGui::TextUnformatted("openC2e");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::Text("Version %s (built %s)", OPENC2E_FORK_VERSION, __DATE__);
		ImGui::TextUnformatted("by DotairZee");
		ImGui::Spacing();
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 420);
		ImGui::TextUnformatted("Creatures 2 with creatures that think for themselves: a mind of their own, words, temperament and "
							   "relationships, breeding, and a full set of kits.");
		ImGui::Spacing();
		ImGui::TextDisabled("A fork of openc2e, the open-source Creatures engine, by its contributors. openc2e is free software "
							"under the GNU Lesser General Public License 2.1, and so are this fork's changes to it.");
		ImGui::TextDisabled("The game's art, sounds and worlds come from your own copy of Creatures 2.");
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		if (ImGui::Button("openc2e on the Web"))
			SDL_OpenURL(OPENC2E_WEB);
		ImGui::SameLine();
		if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Escape))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	// Save World As: a new world folder from the same starting world file,
	// with the current state saved into it; the session continues there
	ImportCreatureTick();
	if (s_open_save_as) {
		ImGui::OpenPopup("Save World As");
		s_open_save_as = false;
	}
	if (ImGui::BeginPopupModal("Save World As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char s_name[64] = "";
		static std::string s_error;
		ImGui::Text("Name for the new world:");
		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
			s_error.clear();
		}
		bool enter = ImGui::InputText("##saveasname", s_name, sizeof(s_name), ImGuiInputTextFlags_EnterReturnsTrue);
		if (!s_error.empty())
			ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%s", s_error.c_str());
		if (ImGui::Button("Save") || enter) {
			if (CreateWorld(s_name, WorldFile(engine.currentworlddir), s_error)) {
				auto w = FindWorld(s_name);
				if (w && WorldSave::SaveWorldState(w->dir, s_error)) {
					engine.currentworld = w->name;
					engine.currentworlddir = w->dir;
					get_backend()->setWindowTitle(w->name + " - Creatures 2");
					s_name[0] = 0;
					ImGui::CloseCurrentPopup();
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			s_name[0] = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (s_open_save_message) {
		ImGui::OpenPopup("Save World##message");
		s_open_save_message = false;
	}
	if (ImGui::BeginPopupModal("Save World##message", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(s_save_message.c_str());
		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	// New World: create one, then restart into it
	if (s_open_new_world) {
		ImGui::OpenPopup("New World");
		DrawNewWorldForm(true);
		s_open_new_world = false;
	}
	if (ImGui::BeginPopupModal("New World", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		std::string created = DrawNewWorldForm(false);
		ImGui::SameLine();
		bool cancel = ImGui::Button("Cancel");
		if (!created.empty()) {
			ImGui::CloseCurrentPopup();
			RelaunchIntoWorld(created);
		} else if (cancel) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Add Favorite Place: name the current camera position
	if (s_open_add_place) {
		ImGui::OpenPopup("Add Favourite Place");
		s_open_add_place = false;
	}
	if (ImGui::BeginPopupModal("Add Favourite Place", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		static char s_name[64] = "";
		ImGui::Text("Name for this place:");
		if (ImGui::IsWindowAppearing())
			ImGui::SetKeyboardFocusHere();
		bool enter = ImGui::InputText("##placename", s_name, sizeof(s_name), ImGuiInputTextFlags_EnterReturnsTrue);
		if ((ImGui::Button("OK") || enter) && s_name[0]) {
			while (world.favoriteplaces.size() < 6)
				world.favoriteplaces.push_back({"", 0, 0});
			for (auto& f : world.favoriteplaces) {
				if (f.name.empty()) {
					f.name = s_name;
					f.x = (int)engine.camera->getX();
					f.y = (int)engine.camera->getY();
					break;
				}
			}
			s_name[0] = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			s_name[0] = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void DrawC2Toolbar() {
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 0));

	if (ImGui::Begin("MainMenuBar", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
		if (ImGui::BeginMenuBar()) {
			DrawC2MenuBar();
			ImGui::EndMenuBar();
		}

		static Texture s_stdicons_tex;
		if (!s_stdicons_tex) {
			s_stdicons_tex = GetTextureFromExeFileWithTransparentTopLeft(0xe3);
		}

		TextureRect icon_next{s_stdicons_tex, 0, 0, 22, 21};
		TextureRect icon_eyeview{s_stdicons_tex, 22, 0, 22, 21};
		TextureRect icon_track{s_stdicons_tex, 44, 0, 22, 21};
		TextureRect icon_halo{s_stdicons_tex, 66, 0, 22, 21};
		TextureRect icon_play{s_stdicons_tex, 88, 0, 22, 21};
		TextureRect icon_pause{s_stdicons_tex, 110, 0, 22, 21};
		TextureRect icon_help{s_stdicons_tex, 132, 0, 22, 21};
		TextureRect icon_web{s_stdicons_tex, 154, 0, 22, 21};

		static Texture s_handicons_tex;
		if (!s_handicons_tex) {
			s_handicons_tex = GetTextureFromExeFileWithTransparentTopLeft(0xe4);
		}

		TextureRect icon_invisible{s_handicons_tex, 0, 0, 22, 21};
		TextureRect icon_teach{s_handicons_tex, 22, 0, 22, 21};
		TextureRect icon_push{s_handicons_tex, 44, 0, 22, 21};

		ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Horizontal;

		if (s_show_standard) {
		if (ImageButton(icon_next, true)) {
			SelectNextCreature();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Next Creature: select the next creature in the world");
		if (ImageButton(icon_eyeview, (bool)world.selectedcreature))
			SetEyeViewOpen(!IsEyeViewOpen());
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Creature's Eye View: see the world as the selected creature sees it");
		if (ImageButton(icon_track, (bool)world.selectedcreature)) {
			// toggle: follow the selected creature, or stop following
			if (engine.camera->trackedAgent() == world.selectedcreature)
				engine.camera->trackAgent(AgentRef(), 0, 0, brittle, none);
			else
				engine.camera->trackAgent(world.selectedcreature, 0, 0, flexible, none);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(engine.camera->trackedAgent() && engine.camera->trackedAgent() == world.selectedcreature ? "Follow: stop following the selected creature" : "Follow: keep the view on the selected creature");
		// the halo over the selected creature: the game's own system macro
		// (world-file script 0 1 0 0) makes or removes the halo object
		// (2 1 3), whose timer script keeps it over the creature's head
		bool halo_on = false;
		for (auto& a : world.agents)
			if (a && a->family == 2 && a->genus == 1 && a->species == 3)
				halo_on = true;
		std::shared_ptr<script> halo_macro = world.scriptorium->getScript(0, 1, 0, 0);
		if (ImageButton(icon_halo, (bool)world.selectedcreature && halo_macro)) {
			caosVM vm(0);
			try {
				vm.runEntirely(halo_macro);
			} catch (std::exception& e) {
				fmt::print("* Halo macro failed: {}\n", e.what());
			}
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(halo_on ? "Halo: hide the halo" : "Halo: show a halo over the selected creature");
		ImGui::Separator();
		if (ImageButton(icon_play, world.paused)) {
			world.paused = false;
		};
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Play: let the world run");
		if (ImageButton(icon_pause, !world.paused)) {
			world.paused = true;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Pause: stop the world");
		ImGui::Separator();
		if (ImageButton(icon_help, true))
			SetHelpOpen(true);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Help: how to play here, and what this version adds");
		if (ImageButton(icon_web, true))
			SDL_OpenURL(OPENC2E_WEB);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Web: openc2e on the web (the open-source engine this runs on)");
		ImGui::Separator();
		}
		if (s_show_pointer) {
		// the Pointer toolbar: an invisible hand (creatures do not see it)
		// as a switch, and the hand's two modes: teach (a click tickles or
		// slaps) and push (a click shoves a creature away from the hand)
		auto toggled = [](bool on, auto icon) {
			if (on)
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
			bool clicked = ImageButton(icon, true);
			if (on)
				ImGui::PopStyleColor();
			return clicked;
		};
		if (toggled(world.handInvisible, icon_invisible)) {
			world.handInvisible = !world.handInvisible;
			// see-through at once, even while the world is paused
			if (world.hand())
				if (auto* sp = dynamic_cast<SpritePart*>(world.hand()->part(0)))
					sp->setStippled(world.handInvisible);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(world.handInvisible ? "Invisible Hand: creatures can't see the hand (click to show it again)" : "Invisible Hand: hide the hand from creatures");
		if (toggled(!world.handPush, icon_teach))
			world.handPush = false;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Teach: click a creature's head to tickle it, lower down to slap it");
		if (toggled(world.handPush, icon_push))
			world.handPush = true;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Push: click a creature to shove it away from the hand");
		ImGui::Separator();
		}

		static Texture s_appleticons_tex;
		if (!s_appleticons_tex) {
			s_appleticons_tex = GetTextureFromExeFileWithTransparentTopLeft(0xe6);
		}

		TextureRect icon_hatchery{s_appleticons_tex, 0, 0, 22, 21};
		TextureRect icon_unknown_maybe_ecologykit{s_appleticons_tex, 22, 0, 22, 21};
		TextureRect icon_ownerskit{s_appleticons_tex, 44, 0, 22, 21};
		TextureRect icon_healthkit{s_appleticons_tex, 66, 0, 22, 21};
		TextureRect icon_unknown_maybe_sciencekit{s_appleticons_tex, 88, 0, 22, 21};
		TextureRect icon_unknown_maybe_breederskit{s_appleticons_tex, 110, 0, 22, 21};
		TextureRect icon_unknown_maybe_observationkit{s_appleticons_tex, 132, 0, 22, 21};
		TextureRect icon_agent_injector{s_appleticons_tex, 154, 0, 22, 21};
		TextureRect icon_unknown_maybe_historykit{s_appleticons_tex, 176, 0, 22, 21};
		TextureRect icon_graveyard{s_appleticons_tex, 198, 0, 22, 21};
		TextureRect icon_unknown_globe{s_appleticons_tex, 220, 0, 22, 21};
		TextureRect icon_unknown_magnifyingglass{s_appleticons_tex, 242, 0, 22, 21};
		TextureRect icon_unknown_maybe_neurosciencekit{s_appleticons_tex, 264, 0, 22, 21};

		if (s_show_applets) {
		if (ImageButton(icon_hatchery, IsHatcheryEnabled())) {
			SetHatcheryOpen(true);
		};
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Hatchery: choose eggs to hatch");
		if (ImageButton(icon_ownerskit, IsC2KitAvailable("owners")))
			OpenC2Kit("owners");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Owner's Kit: name, photos, family, temperament, relationships and life story");
		if (ImageButton(icon_healthkit, IsC2KitAvailable("health")))
			OpenC2Kit("health");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Health Kit: fitness, mind, drives and medicine");
		if (ImageButton(icon_graveyard, IsC2KitAvailable("graveyard")))
			OpenC2Kit("graveyard");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Graveyard Kit: the gravestones of the creatures that have died");
		ImGui::Separator();
		if (ImageButton(icon_unknown_maybe_breederskit, IsC2KitAvailable("breeders")))
			OpenC2Kit("breeders");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Breeder's Kit: fertility and pregnancy, aphrodisiacs and breeds");
		if (ImageButton(icon_unknown_maybe_sciencekit, IsC2KitAvailable("science")))
			OpenC2Kit("science");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Science Kit: body, genes, biochemistry and the syringe");
		// (C2's Neuroscience Kit button opens our Mind Kit: our creatures
		// have no neurons to show; owner, 2026-11-11)
		if (ImageButton(icon_unknown_maybe_neurosciencekit, IsC2KitAvailable("mind")))
			OpenC2Kit("mind");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Mind Kit: what the creature feels, is weighing, has learned and remembers");
		if (ImageButton(icon_unknown_maybe_observationkit, IsC2KitAvailable("observation")))
			OpenC2Kit("observation");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Observation Kit: every creature at a glance, and alarms");
		ImGui::Separator();
		if (ImageButton(icon_agent_injector, IsAgentInjectorEnabled())) {
			SetAgentInjectorOpen(true);
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Agent Injector: add objects to the world");
		if (ImageButton(icon_unknown_maybe_historykit, IsC2KitAvailable("chronicles")))
			OpenC2Kit("chronicles");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Chronicles Kit: the world's history and statistics");
		if (ImageButton(icon_unknown_maybe_ecologykit, IsC2KitAvailable("ecology")))
			OpenC2Kit("ecology");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Ecology Kit: the world's rooms (light, warmth, air, soil), watching rooms, and a census");
		}

		if (s_show_favorites) {
		ImGui::NewLine();

		static Texture s_favtoolbaricons_tex;
		if (!s_favtoolbaricons_tex) {
			s_favtoolbaricons_tex = GetTextureFromExeFileWithTransparentTopLeft(0xe5);
		}
		TextureRect icon_say{s_favtoolbaricons_tex, 0, 0, 22, 21};

		// ImGui::GetCurrentWindow()->DC.LayoutType = ImGuiLayoutType_Vertical;
		ImGui::Text("Speech History:");
		ImGui::SetNextItemWidth(150);
		// what creatures and the hand said lately (newest first); pick a
		// line and the lips button has the hand say it again
		static std::string s_line;
		if (ImGui::BeginCombo("##Speech History:", s_line.c_str())) {
			const auto& hist = speechHistory();
			if (hist.empty())
				ImGui::TextDisabled("Nothing said yet");
			for (size_t i = 0; i < hist.size(); i++) {
				const auto& h = hist[i];
				std::string who = !h.speaker ? "someone" : h.speaker.get() == (Agent*)world.hand() ? "You" : KitCreatureName(h.speaker.get());
				if (ImGui::Selectable(fmt::format("{}: {}##{}", who, h.text, i).c_str(), false))
					s_line = h.text;
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Speech History: what creatures and you said lately");
		if (ImageButton(icon_say, !s_line.empty()))
			handSays(s_line);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Say: the hand says the chosen line again (creatures nearby hear it)");
		ImGui::Separator();

		ImGui::Text("Places:");
		ImGui::SetNextItemWidth(150);
		static int s_place = 0;
		std::vector<int> places; // indexes of named slots
		for (size_t i = 0; i < world.favoriteplaces.size(); i++)
			if (!world.favoriteplaces[i].name.empty())
				places.push_back((int)i);
		if (s_place >= (int)world.favoriteplaces.size() || (s_place < (int)world.favoriteplaces.size() && world.favoriteplaces[s_place].name.empty()))
			s_place = places.empty() ? 0 : places[0];
		const char* preview = places.empty() ? "" : world.favoriteplaces[s_place].name.c_str();
		bool placesOpen = ImGui::BeginCombo("##Places:", preview);
		if (ImGui::IsItemHovered() && !placesOpen)
			ImGui::SetTooltip("Places: your favourite places; choose one to go there");
		if (placesOpen) {
			for (int i : places) {
				if (ImGui::Selectable(world.favoriteplaces[i].name.c_str(), i == s_place)) {
					s_place = i;
					GoToFavoritePlace(i);
				}
			}
			ImGui::EndCombo();
		}
		// no Go button: choosing a place goes there; + adds one, as the
		// Camera menu does (owner, 2026-11-12)
		{
			bool free = HasFreePlace();
			ImGui::BeginDisabled(!free);
			float side = ImGui::GetFrameHeight(); // square, as tall as the Places list
			if (ImGui::Button("+##addplace", ImVec2(side, side)))
				s_open_add_place = true;
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(free ? "Add a favourite place: name the spot the view shows now" : "All six favourite places are named; remove one in the Camera menu first");
		}
		// zoom, as a dropdown beside Places (owner, 2026-11-09; also
		// Ctrl+wheel and the View menu)
		ImGui::Text("Zoom:");
		ImGui::SetNextItemWidth(70);
		static const float s_zooms[] = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f};
		std::string zoomLabel = fmt::format("{:.0f}%", engine.zoom * 100);
		if (ImGui::BeginCombo("##Zoom", zoomLabel.c_str())) {
			for (float z : s_zooms)
				if (ImGui::Selectable(fmt::format("{:.0f}%", z * 100).c_str(), std::fabs(engine.zoom - z) < 0.01f))
					engine.setZoom(z);
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Zoom: the size of the view (also Ctrl+wheel; Ctrl+0 for actual size)");
		}

		s_last_c2toolbar_height = ImGui::GetWindowSize().y;
		ImGui::End();
	}
}

} // namespace Openc2eImgui